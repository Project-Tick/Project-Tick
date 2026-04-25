import hashlib
import hmac
import uuid
from datetime import datetime, timezone
from urllib.parse import quote, urlparse

import httpx
import structlog
from fastapi import APIRouter, Header, HTTPException, Request, status
from sqlalchemy import select, text
from typing import Any
from app.config import settings
from app.database import get_db
from app.models.pipeline import Pipeline, PipelineStatus
from app.models.webhook_event import WebhookEvent, WebhookSource
from app.pipelines.build import BuildPipeline, app_build_types, cancel_pipeline
from app.services.github_actions import GitHubActionsService
from app.utils.github import (
    create_pr_comment,
    get_github_client,
    update_commit_status,
)

logger = structlog.get_logger(__name__)

webhooks_router = APIRouter(prefix="/api/webhooks", tags=["webhooks"])

DISABLED_TEST_BUILDS_MSG = (
    "🚧 Test builds are currently disabled. Once the maintenance is over, this build "
    "can be retried by posting a `bot, build` comment. Please refer to "
    "{statuspage_url} for updates."
)

BOT_COMMANDS = ("bot, build", "bot, ping admins", "bot, cancel")


def filter_bot_command_text(text: str) -> str:
    filtered_lines = []
    for line in text.splitlines():
        stripped_line = line.lstrip()
        if stripped_line.startswith(">"):
            continue
        if stripped_line.startswith(("`", "<code>")) and stripped_line.rstrip().endswith(
            ("`", "</code>")
        ):
            continue
        if any(
            f"`{command}`" in line or f"<code>{command}</code>" in line
            for command in BOT_COMMANDS
        ):
            continue
        filtered_lines.append(line)

    return "\n".join(filtered_lines)


def has_bot_command(text: str, command: str) -> bool:
    return command.lower() in filter_bot_command_text(text).lower()


def is_gitlab_build_note(payload: dict[str, Any]) -> bool:
    if payload.get("object_kind") != "note":
        return False

    object_attributes = payload.get("object_attributes", {})
    merge_request = payload.get("merge_request", {})
    action = object_attributes.get("action")

    if object_attributes.get("noteable_type") != "MergeRequest":
        return False
    if object_attributes.get("system") or object_attributes.get("internal"):
        return False
    if action not in (None, "", "create", "update"):
        return False
    if merge_request.get("state") in ("closed", "merged"):
        return False

    note = object_attributes.get("note", "")
    return has_bot_command(note, "bot, build")


def get_gitlab_merge_request_ref(payload: dict[str, Any]) -> str:
    merge_request = payload.get("merge_request", {})
    merge_request_iid = merge_request.get("iid")
    if merge_request_iid:
        return f"refs/merge-requests/{merge_request_iid}/head"

    source_branch = merge_request.get("source_branch", "")
    if source_branch:
        return f"refs/heads/{source_branch}"

    return ""


def get_gitlab_project_path(payload: dict[str, Any]) -> str:
    merge_request = payload.get("merge_request", {})
    project = payload.get("project", {})

    for path in (
        project.get("path_with_namespace", ""),
        merge_request.get("target", {}).get("path_with_namespace", ""),
        merge_request.get("source", {}).get("path_with_namespace", ""),
    ):
        if path:
            return path

    return ""


def get_gitlab_base_url(payload: dict[str, Any]) -> str:
    merge_request = payload.get("merge_request", {})
    project = payload.get("project", {})

    for repository_url in (
        project.get("git_http_url", ""),
        merge_request.get("target", {}).get("git_http_url", ""),
        merge_request.get("source", {}).get("git_http_url", ""),
    ):
        if not repository_url:
            continue

        parsed_url = urlparse(repository_url)
        if parsed_url.scheme and parsed_url.netloc:
            return f"{parsed_url.scheme}://{parsed_url.netloc}"

    return ""


async def create_gitlab_merge_request_note(
    payload: dict[str, Any],
    note: str,
) -> bool:
    if not settings.gitlab_api_token:
        logger.info("Skipping GitLab merge request note because GITLAB_API_TOKEN is not configured")
        return False

    merge_request_iid = payload.get("merge_request", {}).get("iid")
    project_path = get_gitlab_project_path(payload)
    gitlab_base_url = get_gitlab_base_url(payload)

    if not merge_request_iid or not project_path or not gitlab_base_url:
        logger.warning(
            "Skipping GitLab merge request note because payload is missing required routing fields",
            has_merge_request_iid=bool(merge_request_iid),
            has_project_path=bool(project_path),
            has_gitlab_base_url=bool(gitlab_base_url),
        )
        return False

    endpoint = (
        f"{gitlab_base_url}/api/v4/projects/{quote(project_path, safe='')}/"
        f"merge_requests/{merge_request_iid}/notes"
    )

    try:
        async with httpx.AsyncClient(timeout=10.0) as client:
            response = await client.post(
                endpoint,
                headers={"PRIVATE-TOKEN": settings.gitlab_api_token},
                data={"body": note},
            )
            response.raise_for_status()
        return True
    except httpx.HTTPStatusError as error:
        logger.warning(
            "GitLab merge request note creation failed",
            endpoint=endpoint,
            status_code=error.response.status_code,
            response_text=error.response.text[:400],
        )
    except httpx.HTTPError as error:
        logger.warning(
            "GitLab merge request note request failed",
            endpoint=endpoint,
            error=str(error),
        )

    return False


def should_store_event(payload: dict) -> bool:
    ref = payload.get("ref", "")
    comment = payload.get("comment", {}).get("body", "")

    if "pull_request" in payload:
        pr_action = payload.get("action", "")
        target_ref = payload.get("pull_request", {}).get("base", {}).get("ref")
        if pr_action in ["opened", "synchronize", "reopened"]:
            return (
                not target_ref
                or target_ref in ("master", "beta")
                or target_ref.startswith("branch/")
            )

    if "commits" in payload and ref:
        if ref in (
            "refs/heads/master",
            "refs/heads/beta",
        ) or ref.startswith("refs/heads/branch/"):
            return True

    if "comment" in payload:
        comment_author = payload.get("comment", {}).get("user", {}).get("login")

        if comment_author in ("github-actions[bot]",):
            return False

        filtered_comment = filter_bot_command_text(comment)

        if "bot, build" in filtered_comment.lower():
            return True

        if "bot, ping admins" in filtered_comment.lower():
            return True

        if "bot, cancel" in filtered_comment.lower():
            return True

    return False


@webhooks_router.post(
    "/gitlab",
    status_code=status.HTTP_202_ACCEPTED,
)
async def receive_gitlab_webhook(
    request: Request,
    x_gitlab_event: str | None = Header(None, description="GitLab event name"),
    x_gitlab_event_uuid: str | None = Header(
        None, description="GitLab event UUID"
    ),
    x_gitlab_token: str | None = Header(None, description="GitLab webhook token"),
):
    if settings.gitlab_webhook_secret:
        if not x_gitlab_token or not hmac.compare_digest(
            settings.gitlab_webhook_secret, x_gitlab_token
        ):
            raise HTTPException(
                status_code=status.HTTP_401_UNAUTHORIZED,
                detail="Invalid GitLab webhook token.",
            )

    try:
        payload = await request.json()
    except Exception:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="Invalid JSON payload.",
        )

    if x_gitlab_event != "Note Hook":
        return {"message": "Webhook received but ignored due to event type filter."}

    if not is_gitlab_build_note(payload):
        return {"message": "Webhook received but ignored due to note filter."}

    merge_request = payload.get("merge_request", {})
    source_info = merge_request.get("source", {})
    source_repository = source_info.get("git_http_url") or payload.get(
        "project", {}
    ).get("git_http_url", "")
    source_ref = get_gitlab_merge_request_ref(payload)

    if not source_repository:
        raise HTTPException(
            status_code=status.HTTP_422_UNPROCESSABLE_CONTENT,
            detail="Missing source repository URL in GitLab payload.",
        )
    if not source_ref:
        raise HTTPException(
            status_code=status.HTTP_422_UNPROCESSABLE_CONTENT,
            detail="Missing source ref in GitLab payload.",
        )

    merge_request_iid = str(merge_request.get("iid", ""))
    source_sha = merge_request.get("last_commit", {}).get("id", "")
    gitlab_project_path = get_gitlab_project_path(payload)
    gitlab_base_url = get_gitlab_base_url(payload)

    if not merge_request_iid:
        raise HTTPException(
            status_code=status.HTTP_422_UNPROCESSABLE_CONTENT,
            detail="Missing merge request IID in GitLab payload.",
        )
    if not source_sha:
        raise HTTPException(
            status_code=status.HTTP_422_UNPROCESSABLE_CONTENT,
            detail="Missing merge request commit SHA in GitLab payload.",
        )
    if not gitlab_project_path or not gitlab_base_url:
        raise HTTPException(
            status_code=status.HTTP_422_UNPROCESSABLE_CONTENT,
            detail="Missing GitLab routing fields in payload.",
        )

    delivery_id = x_gitlab_event_uuid or str(uuid.uuid4())
    build_pipeline = BuildPipeline()
    pipeline = await build_pipeline.create_pipeline(
        app_id=f"gitlab-mr-{merge_request_iid}",
        params={
            "ref": source_ref,
            "use_spot": False,
            "build_type": "default",
            "dispatch_owner": settings.github_org,
            "dispatch_repo": settings.github_ci_repo,
            "dispatch_workflow_id": settings.github_ci_workflow,
            "dispatch_ref": settings.github_ci_ref,
            "dispatch_inputs": {
                "force-all": "false",
                "source-repository": source_repository,
                "source-ref": source_ref,
            },
            "gitlab_base_url": gitlab_base_url,
            "gitlab_project_path": gitlab_project_path,
            "gitlab_merge_request_iid": merge_request_iid,
            "gitlab_source_branch": merge_request.get("source_branch", ""),
            "gitlab_source_sha": source_sha,
        },
        webhook_event_id=None,
    )
    pipeline = await build_pipeline.prepare_pipeline_for_start(pipeline.id)
    await build_pipeline.supersede_conflicting_test_pipelines(pipeline.id)
    should_queue = await build_pipeline.should_queue_test_build(pipeline.id)

    if not should_queue:
        pipeline = await build_pipeline.start_pipeline(pipeline.id)

    return {
        "message": "GitLab webhook received",
        "event_id": delivery_id,
        "pipeline_id": str(pipeline.id),
        "pipeline_status": pipeline.status.value,
    }


async def is_submodule_only_pr(
    payload: dict[str, Any], github_token: str | None = None
) -> bool:
    repo, number = (
        payload.get("repository", {}).get("full_name"),
        payload.get("pull_request", {}).get("number"),
    )

    if not (repo and number):
        return False

    # Public API, token is only required if requests exceed some per
    # minute limit
    url = f"https://api.github.com/repos/{repo}/pulls/{number}/files"
    headers = {}
    if github_token:
        headers["Authorization"] = f"Bearer {github_token}"

    try:
        async with httpx.AsyncClient() as client:
            r = await client.get(url, headers=headers)
            r.raise_for_status()
            files = r.json()
    except (httpx.HTTPError, ValueError) as err:
        logger.error(
            "Error fetching PR file details from GitHub",
            error=str(err),
            repo=repo,
            pr_number=number,
        )
        return False

    if not files:
        return False

    return all(
        "patch" in f and f["patch"] and "Subproject commit" in f["patch"] for f in files
    )


@webhooks_router.post(
    "/github",
    status_code=status.HTTP_202_ACCEPTED,
)
async def receive_github_webhook(
    request: Request,
    x_github_delivery: str | None = Header(None, description="GitHub delivery GUID"),
    x_hub_signature_256: str | None = Header(
        None, description="GitHub webhook signature"
    ),
):
    if not x_github_delivery:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="Missing X-GitHub-Delivery header.",
        )

    try:
        delivery_id = uuid.UUID(x_github_delivery)
    except ValueError:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="Invalid X-GitHub-Delivery header format (must be a UUID).",
        )

    if settings.github_webhook_secret:
        if not x_hub_signature_256:
            raise HTTPException(
                status_code=status.HTTP_401_UNAUTHORIZED,
                detail="Missing X-Hub-Signature-256 header.",
            )

        body = await request.body()
        secret = settings.github_webhook_secret.encode()
        signature = hmac.new(secret, body, hashlib.sha256).hexdigest()
        expected_signature = f"sha256={signature}"

        if not hmac.compare_digest(expected_signature, x_hub_signature_256):
            raise HTTPException(
                status_code=status.HTTP_401_UNAUTHORIZED,
                detail="Invalid signature.",
            )

    try:
        payload = await request.json()
    except Exception:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST, detail="Invalid JSON payload."
        )

    try:
        repo_name = payload["repository"]["full_name"]
        actor_login = payload["sender"]["login"]
    except KeyError as e:
        raise HTTPException(
            status_code=status.HTTP_422_UNPROCESSABLE_CONTENT,
            detail=f"Missing expected key in GitHub payload: {e}",
        )

    ignored_repos: list[str] = []
    is_pr_event = "pull_request" in payload and payload.get("action") in [
        "opened",
        "synchronize",
        "reopened",
    ]
    is_push_event = "commits" in payload and payload.get("ref", "")

    if repo_name in ignored_repos and (is_pr_event or is_push_event):
        return {"message": "Webhook received but ignored due to repository filter."}

    if is_pr_event:
        if repo_name.split("/")[1] in app_build_types:
            return {
                "message": "Pull request webhook received but ignored due to large app."
            }

        if actor_login in ("dependabot[bot]", "renovate[bot]"):
            return {"message": "Webhook received but ignored due to actor filter."}

        if actor_login in ("github-actions[bot]",) and await is_submodule_only_pr(
            payload
        ):
            return {"message": "Webhook received but ignored due to PR changes filter."}

    event = WebhookEvent(
        id=delivery_id,
        source=WebhookSource.GITHUB,
        payload=payload,
        repository=repo_name,
        actor=actor_login,
    )

    pipeline_id = None
    if should_store_event(payload):
        try:
            async with get_db() as db:
                db.add(event)
                await db.commit()

            pipeline_id = await create_pipeline(event)

        except Exception as e:
            logger.error(
                "Database error",
                error=str(e),
                event_id=str(event.id) if event else None,
            )
            raise HTTPException(
                status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
                detail=f"Database error occurred while processing webhook: {e}",
            )

    response = {"message": "Webhook received", "event_id": str(event.id)}
    if pipeline_id:
        response["pipeline_id"] = str(pipeline_id)

    return response


async def create_pipeline(event: WebhookEvent) -> uuid.UUID | None:
    payload = event.payload
    app_id = f"{event.repository.split('/')[-1]}"
    params: dict[str, Any] = {"repo": event.repository}
    sha = None

    if "pull_request" in payload and payload.get("action") in [
        "opened",
        "synchronize",
        "reopened",
    ]:
        pr = payload.get("pull_request", {})
        pr_state = pr.get("state")

        if pr_state == "closed":
            logger.info(
                "PR is closed, skipping pipeline creation",
                pr_number=pr.get("number"),
                repo=event.repository,
                action=payload.get("action"),
            )
            return None

        pr_number = pr.get("number")

        if settings.ff_disable_test_builds:
            logger.info(
                "Test builds are disabled, skipping PR pipeline creation",
                pr_number=pr_number,
                repo=event.repository,
            )
            await create_pr_comment(
                git_repo=event.repository,
                pr_number=pr_number,
                comment=DISABLED_TEST_BUILDS_MSG.format(
                    statuspage_url=settings.statuspage_url
                ),
            )
            return None

        sha = pr.get("head", {}).get("sha")
        params.update(
            {
                "ref": f"refs/pull/{pr_number}/head",
                "pr_number": str(pr_number) if pr_number is not None else "",
                "action": str(payload.get("action", "")),
                "pr_target_branch": pr.get("base", {}).get("ref", "master"),
            }
        )

    elif "commits" in payload and payload.get("ref", ""):
        ref = payload.get("ref", "")
        sha = payload.get("after")
        params.update(
            {
                "ref": ref,
                "push": "true",
            }
        )

    elif "comment" in payload:
        comment_body = payload.get("comment", {}).get("body", "").lower()
        issue = payload.get("issue", {})
        issue_number = issue.get("number")
        comment_author = payload.get("comment", {}).get("user", {}).get("login", "")
        pr_url = issue.get("pull_request", {}).get("url", "")
        repo = event.repository

        if "bot, ping admins" in comment_body:
            if issue_number is None:
                logger.error(
                    "Missing issue number for admin ping",
                    repo=repo,
                    comment_author=comment_author,
                )
            else:
                if settings.ff_admin_ping_comment:
                    logger.info(
                        "Handling admin ping", repo=repo, issue_number=issue_number
                    )
                    await create_pr_comment(
                        git_repo=repo,
                        pr_number=issue_number,
                        comment=f"Contacted admins: cc @{settings.admin_team}",
                    )
                else:
                    logger.info(
                        "Admin ping is disabled via settings",
                        repo=repo,
                        issue_number=issue_number,
                    )
            return None

        elif "bot, cancel" in comment_body:
            if not pr_url or issue_number is None:
                return None

            pr_ref = f"refs/pull/{issue_number}/head"
            pipelines_to_cancel: list[
                tuple[uuid.UUID, int | None, dict[str, Any] | None]
            ] = []

            async with get_db() as db:
                query = (
                    select(Pipeline)
                    .where(
                        Pipeline.app_id == app_id,
                        Pipeline.status.in_(
                            [PipelineStatus.PENDING, PipelineStatus.RUNNING]
                        ),
                        text("params->>'ref' = :ref"),
                    )
                    .params(ref=pr_ref)
                )
                result = await db.execute(query)
                active_pipelines = list(result.scalars().all())

                if not active_pipelines:
                    await create_pr_comment(
                        git_repo=repo,
                        pr_number=issue_number,
                        comment="No active builds found to cancel.",
                    )
                    return None

                for pipeline in active_pipelines:
                    pipeline.status = PipelineStatus.CANCELLED
                    pipeline.finished_at = datetime.now(timezone.utc)

                pipelines_to_cancel = [
                    (
                        pipeline.id,
                        pipeline.build_id,
                        dict(pipeline.provider_data)
                        if pipeline.provider_data
                        else None,
                    )
                    for pipeline in active_pipelines
                ]

                await db.commit()

            github_actions = GitHubActionsService()
            for pipeline_id, build_id, provider_data in pipelines_to_cancel:
                await cancel_pipeline(
                    pipeline_id,
                    build_id,
                    provider_data,
                    github_actions=github_actions,
                )

            count = len(active_pipelines)
            await create_pr_comment(
                git_repo=repo,
                pr_number=issue_number,
                comment=f"Cancelled {count} active build(s).",
            )

            logger.info(
                "Cancelled active pipelines via bot command",
                app_id=app_id,
                pr_number=issue_number,
                cancelled_count=count,
            )

            return None

        elif "bot, build" in comment_body:
            if not pr_url or issue_number is None:
                return None

            if settings.ff_disable_test_builds:
                logger.info(
                    "Test builds are disabled, skipping bot, build pipeline creation",
                    pr_number=issue_number,
                    repo=repo,
                )
                await create_pr_comment(
                    git_repo=repo,
                    pr_number=issue_number,
                    comment=DISABLED_TEST_BUILDS_MSG.format(
                        statuspage_url=settings.statuspage_url
                    ),
                )
                return None

            pr_ref = f"refs/pull/{issue_number}/head"
            pr_target_branch = "master"
            github_client = get_github_client()

            try:
                response = await github_client.request(
                    "get",
                    f"https://api.github.com/repos/{repo}/pulls/{issue_number}",
                    context={"repo": repo, "pr_number": issue_number},
                )
                if response is None:
                    return None
                pr_data = response.json()
                sha = pr_data.get("head", {}).get("sha")
                pr_target_branch = pr_data.get("base", {}).get("ref", "master")

                pr_state = pr_data.get("state")
                if pr_state in ["closed", "merged"]:
                    logger.info(
                        "PR is closed/merged, ignoring 'bot, build' command",
                        pr_number=issue_number,
                        repo=repo,
                        pr_state=pr_state,
                    )
                    await create_pr_comment(
                        git_repo=repo,
                        pr_number=issue_number,
                        comment="❌ Cannot build closed or merged PR. Please reopen the PR if you want to trigger a build.",
                    )
                    return None
            except httpx.RequestError as e:
                logger.error(
                    "Error fetching PR details from GitHub",
                    error=str(e),
                    repo=repo,
                    pr_number=issue_number,
                )
            except httpx.HTTPStatusError as e:
                logger.error(
                    "GitHub API error",
                    status_code=e.response.status_code,
                    response_text=e.response.text,
                    repo=repo,
                    pr_number=issue_number,
                )

            params.update(
                {
                    "pr_number": str(issue_number),
                    "ref": pr_ref,
                    "use_spot": False,
                    "pr_target_branch": pr_target_branch,
                }
            )

    if sha:
        params["sha"] = sha

    pipeline_service = BuildPipeline()
    pipeline = await pipeline_service.create_pipeline(
        app_id=app_id,
        params=params,
        webhook_event_id=event.id,
    )
    pipeline = await pipeline_service.prepare_pipeline_for_start(pipeline.id)
    await pipeline_service.supersede_conflicting_test_pipelines(pipeline.id)
    should_queue_test_build = await pipeline_service.should_queue_test_build(
        pipeline.id
    )

    commit_sha = pipeline.params.get("sha")
    git_repo = pipeline.params.get("repo")

    if commit_sha and git_repo:
        target_url = f"{settings.base_url}/api/pipelines/{pipeline.id}"
        description = (
            "Build queued — waiting for capacity"
            if should_queue_test_build
            else "Build enqueued"
        )
        try:
            await update_commit_status(
                sha=commit_sha,
                state="pending",
                git_repo=git_repo,
                description=description,
                target_url=target_url,
            )
        except Exception as e:
            logger.warning(
                "Error setting initial commit status",
                pipeline_id=str(pipeline.id),
                git_repo=git_repo,
                commit_sha=commit_sha,
                error=str(e),
            )
    elif commit_sha and not git_repo:
        logger.error(
            "Missing git_repo in params. Cannot update commit status.",
            pipeline_id=str(pipeline.id),
        )

    if not should_queue_test_build:
        pipeline = await pipeline_service.start_pipeline(pipeline_id=pipeline.id)

    pr_number_str = pipeline.params.get("pr_number")
    if pr_number_str and git_repo:
        try:
            pr_number = int(pr_number_str)
            comment = (
                "🚧 Test build queued — waiting for capacity."
                if should_queue_test_build
                else "🚧 Test build enqueued."
            )
            await create_pr_comment(
                git_repo=git_repo,
                pr_number=pr_number,
                comment=comment,
            )
        except ValueError:
            logger.error(
                "Invalid PR number. Skipping PR comment.",
                pr_number=pr_number_str,
                pipeline_id=str(pipeline.id),
            )
        except Exception as e:
            logger.error(
                "Error creating initial PR comment",
                pipeline_id=str(pipeline.id),
                error=str(e),
            )
    elif pr_number_str and not git_repo:
        logger.error(
            "Missing git_repo in params. Cannot create PR comment.",
            pipeline_id=str(pipeline.id),
        )

    return pipeline.id
