import hashlib
import hmac
import re
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
from app.pipelines.build import (
    BuildPipeline,
    app_build_types,
    cancel_pipeline,
    get_target_repo_for_branch,
    resolve_pipeline_target_repo,
)
from app.services.github_actions import GitHubActionsService
from app.utils.github import (
    add_issue_comment,
    close_github_issue,
    create_github_tag,
    create_pr_comment,
    get_github_client,
    get_workflow_run_title,
    is_issue_edited,
    update_commit_status,
)

logger = structlog.get_logger(__name__)

webhooks_router = APIRouter(prefix="/api/webhooks", tags=["webhooks"])

DISABLED_TEST_BUILDS_MSG = (
    "🚧 Test builds are currently disabled. Once the maintenance is over, this build "
    "can be retried by posting a `bot, build` comment. Please refer to "
    "{statuspage_url} for updates."
)

BOT_BUILD_COMMANDS = ("bot, build", "bot, retry")
BOT_COMMANDS = BOT_BUILD_COMMANDS + ("bot, ping admins", "bot, cancel")
GITLAB_NOTE_COMMANDS = BOT_COMMANDS + ("bot, status", "bot, help")
GITLAB_ISSUE_COMMANDS = ("bot, retry", "bot, help")

BUILD_FAILURE_ISSUE_RE = re.compile(
    r"The (?P<target_repo>\w+) build pipeline for `(?P<app_id>[^`]+)` "
    r"(?:failed|was cancelled)\.\s+"
    r"Commit SHA: (?P<sha>[0-9a-fA-F]+)\s+"
    r"(?:Ref: (?P<ref>\S+)\s+)?"
    r"Build log: (?P<log_url>\S+)"
    r"(?:\s+GitHub repository: (?P<github_repo>\S+))?"
    r"(?:\s+Source repository: (?P<source_repository>\S+))?",
    re.IGNORECASE | re.MULTILINE | re.DOTALL,
)

JOB_FAILURE_ISSUE_RE = re.compile(
    r"The (?P<job_type>[\w-]+) job for `(?P<app_id>[^`]+)` failed in the "
    r"(?P<target_repo>\w+) repository\.\s+"
    r"\*\*Build Information:\*\*\s+"
    r"- Commit SHA: (?P<sha>[0-9a-fA-F]+).*?"
    r"(?:- Ref: (?P<ref>\S+).*?)?"
    r"- Build log: (?P<log_url>\S+)"
    r"(?:.*?GitHub repository: (?P<github_repo>\S+))?"
    r"(?:.*?Source repository: (?P<source_repository>\S+))?",
    re.IGNORECASE | re.MULTILINE | re.DOTALL,
)


def filter_bot_command_text(
    text: str,
    commands: tuple[str, ...] = BOT_COMMANDS,
) -> str:
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
            for command in commands
        ):
            continue
        filtered_lines.append(line)

    return "\n".join(filtered_lines)


def has_bot_command(text: str, command: str) -> bool:
    return command.lower() in filter_bot_command_text(text).lower()


def extract_bot_command(
    text: str,
    commands: tuple[str, ...] = BOT_COMMANDS,
) -> str | None:
    filtered_text = filter_bot_command_text(text, commands).lower()
    earliest_match: tuple[int, str] | None = None

    for command in commands:
        index = filtered_text.find(command)
        if index == -1:
            continue
        if earliest_match is None or index < earliest_match[0]:
            earliest_match = (index, command)

    return earliest_match[1] if earliest_match else None


def is_build_bot_command(command: str | None) -> bool:
    return command in BOT_BUILD_COMMANDS


def get_gitlab_admin_mention() -> str:
    mention = settings.gitlab_admin_mention or settings.admin_team
    if not mention:
        return "@admins"
    return mention if mention.startswith("@") else f"@{mention}"


def get_default_source_repository(repo_name: str) -> str | None:
    normalized_repo_name = repo_name.strip().lower()
    if normalized_repo_name == "project-tick/project-tick":
        return f"{settings.gitlab_base_url.rstrip('/')}/project-tick/project-tick.git"

    if "/" in repo_name:
        return f"https://github.com/{repo_name}.git"

    return None


def build_github_dispatch_params(
    repo_name: str,
    source_event_name: str,
    source_ref: str,
    source_sha: str | None,
    actor_login: str,
    extra_inputs: dict[str, str] | None = None,
    source_repository: str | None = None,
) -> dict[str, Any]:
    dispatch_repo_full_name = f"{settings.github_org}/{settings.github_ci_repo}"
    source_repo_name = repo_name if "/" in repo_name else dispatch_repo_full_name
    resolved_source_repository = source_repository

    dispatch_inputs = {
        "force-all": "false",
        "event-name": source_event_name,
        "event-actor": actor_login,
        "source-ref": source_ref,
        "source-sha": source_sha or "",
    }
    if not resolved_source_repository:
        resolved_source_repository = get_default_source_repository(source_repo_name)
    if resolved_source_repository:
        dispatch_inputs["source-repository"] = resolved_source_repository
    if extra_inputs:
        dispatch_inputs.update(extra_inputs)

    return {
        "dispatch_owner": settings.github_org,
        "dispatch_repo": settings.github_ci_repo,
        "dispatch_workflow_id": settings.github_ci_workflow,
        "dispatch_ref": settings.github_ci_ref,
        "dispatch_inputs": {
            key: value
            for key, value in dispatch_inputs.items()
            if value is not None and value != ""
        },
    }


def build_gitlab_issue_routing_params(repo_name: str) -> dict[str, str]:
    if not settings.gitlab_base_url:
        return {}

    repo_slug = repo_name.split("/", 1)[-1].strip()
    native_repo = f"{settings.github_org}/{settings.github_ci_repo}".lower()

    if repo_name.lower() == native_repo and repo_slug:
        namespace = settings.github_org.lower()
        project_path = f"{namespace}/{repo_slug.lower()}"
    else:
        # Non-native repos fall back to the CI project as the GitLab notification target
        project_path = f"{settings.github_org.lower()}/{settings.github_ci_repo.lower()}"

    return {
        "gitlab_base_url": settings.gitlab_base_url,
        "gitlab_project_path": project_path,
    }


def get_gitlab_merge_request_data(payload: dict[str, Any]) -> dict[str, Any]:
    merge_request = payload.get("merge_request")
    if isinstance(merge_request, dict) and merge_request:
        return merge_request

    if payload.get("object_kind") == "merge_request":
        object_attributes = payload.get("object_attributes")
        if isinstance(object_attributes, dict):
            return object_attributes

    return {}


def get_gitlab_merge_request_iid(payload: dict[str, Any]) -> str:
    merge_request = get_gitlab_merge_request_data(payload)
    merge_request_iid = merge_request.get("iid")
    return str(merge_request_iid) if merge_request_iid is not None else ""


def get_gitlab_merge_request_source_sha(payload: dict[str, Any]) -> str:
    merge_request = get_gitlab_merge_request_data(payload)
    last_commit = merge_request.get("last_commit")
    if isinstance(last_commit, dict):
        last_commit_id = last_commit.get("id")
        if last_commit_id is not None:
            return str(last_commit_id)

    source_sha = merge_request.get("last_commit_id") or merge_request.get("sha")
    return str(source_sha) if source_sha is not None else ""


def get_gitlab_merge_request_oldrev(payload: dict[str, Any]) -> str:
    merge_request = get_gitlab_merge_request_data(payload)
    oldrev = merge_request.get("oldrev")
    return str(oldrev) if oldrev is not None else ""


def is_gitlab_merge_request_draft(payload: dict[str, Any]) -> bool:
    merge_request = get_gitlab_merge_request_data(payload)

    for key in ("draft", "work_in_progress"):
        draft_value = merge_request.get(key)
        if isinstance(draft_value, bool):
            return draft_value

    title = str(merge_request.get("title", "")).lower()
    return title.startswith(("draft:", "wip:"))


def should_autostart_gitlab_merge_request_build(payload: dict[str, Any]) -> bool:
    if payload.get("object_kind") != "merge_request":
        return False

    merge_request = get_gitlab_merge_request_data(payload)
    state = str(merge_request.get("state", "")).lower()
    if state in ("closed", "merged"):
        return False

    action = str(merge_request.get("action", "")).lower()
    if action in ("open", "opened", "reopen", "reopened"):
        return True
    if action != "update":
        return False

    oldrev = get_gitlab_merge_request_oldrev(payload)
    if oldrev and oldrev != ("0" * 40):
        return True

    changes = payload.get("changes")
    if not isinstance(changes, dict):
        return False

    return any(key in changes for key in ("last_commit", "source_branch", "target_branch"))


def should_autostart_gitlab_push_build(payload: dict[str, Any]) -> bool:
    if payload.get("object_kind") not in ("push", "tag_push"):
        return False

    ref = str(payload.get("ref", ""))
    after_sha = str(payload.get("after", ""))

    if payload.get("deleted") or after_sha == ("0" * 40):
        return False

    if ref.startswith("refs/tags/"):
        return True

    return ref in {"refs/heads/master", "refs/heads/lts"}


def get_gitlab_push_source_sha(payload: dict[str, Any]) -> str:
    for key in ("checkout_sha", "after"):
        value = str(payload.get(key, ""))
        if value and value != ("0" * 40):
            return value

    return ""


def describe_target_branch(target_branch: str) -> str:
    normalized_target_branch = target_branch.strip() or settings.github_ci_ref
    target_repo = get_target_repo_for_branch(normalized_target_branch)

    if target_repo == "stable":
        return f"target branch `{normalized_target_branch}` (stable)"
    if target_repo == "lts":
        return f"target branch `{normalized_target_branch}` (lts)"
    if target_repo == "beta":
        return f"target branch `{normalized_target_branch}`"

    return f"target branch `{normalized_target_branch}`"


def extract_run_id_from_log_url(log_url: str | None) -> str | None:
    if not log_url:
        return None

    run_id = log_url.rstrip("/").split("/")[-1]
    return run_id or None


def _default_ref_for_target_repo(target_repo: str) -> str:
    if target_repo == "lts":
        return "refs/heads/lts"
    return "refs/heads/master"


async def parse_failure_issue(issue_body: str, git_repo: str) -> dict[str, Any] | None:
    if not issue_body:
        return None

    for pattern, issue_type in (
        (BUILD_FAILURE_ISSUE_RE, "build_failure"),
        (JOB_FAILURE_ISSUE_RE, "job_failure"),
    ):
        match = pattern.search(issue_body.strip())
        if not match:
            continue

        parsed = {key: value for key, value in match.groupdict().items() if value}
        target_repo = str(parsed.get("target_repo", "stable")).lower()
        ref = parsed.get("ref") or _default_ref_for_target_repo(target_repo)

        result: dict[str, Any] = {
            "app_id": parsed.get("app_id") or git_repo.rsplit("/", 1)[-1],
            "sha": parsed["sha"],
            "repo": git_repo,
            "ref": ref,
            "flat_manager_repo": target_repo,
            "issue_type": issue_type,
            "log_url": parsed.get("log_url", ""),
        }

        if parsed.get("github_repo"):
            result["github_repo"] = parsed["github_repo"]
        if parsed.get("source_repository"):
            result["source_repository"] = parsed["source_repository"]
        if issue_type == "job_failure" and parsed.get("job_type"):
            result["job_type"] = parsed["job_type"]

        return result

    return None


async def validate_retry_permissions(git_repo: str, comment_author: str) -> bool:
    if not settings.github_status_token or not git_repo or "/" not in git_repo:
        return False

    owner, _repo = git_repo.split("/", 1)
    headers = {
        "Authorization": f"Bearer {settings.github_status_token}",
        "Accept": "application/vnd.github+json",
    }

    async with httpx.AsyncClient(timeout=10.0) as client:
        collaborator_response = await client.get(
            f"https://api.github.com/repos/{git_repo}/collaborators/{comment_author}",
            headers=headers,
        )
        if collaborator_response.status_code == status.HTTP_204_NO_CONTENT:
            return True

        org_response = await client.get(
            f"https://api.github.com/orgs/{owner}/members/{comment_author}",
            headers=headers,
        )
        return org_response.status_code == status.HTTP_204_NO_CONTENT


def build_cancel_provider_data(pipeline: Pipeline) -> dict[str, Any] | None:
    provider_data = dict(pipeline.provider_data) if pipeline.provider_data else {}

    if not provider_data.get("run_id"):
        run_id = extract_run_id_from_log_url(pipeline.log_url)
        if run_id:
            provider_data["run_id"] = run_id

    repo = (pipeline.params or {}).get("repo")
    if isinstance(repo, str) and "/" in repo:
        owner, repo_name = repo.split("/", 1)
        provider_data.setdefault("owner", owner)
        provider_data.setdefault("repo", repo_name)

    return provider_data or None


def get_gitlab_note_command(payload: dict[str, Any]) -> str | None:
    if payload.get("object_kind") != "note":
        return None

    object_attributes = payload.get("object_attributes", {})
    merge_request = get_gitlab_merge_request_data(payload)
    action = object_attributes.get("action")

    if object_attributes.get("noteable_type") != "MergeRequest":
        return None
    if object_attributes.get("system") or object_attributes.get("internal"):
        return None
    if action not in (None, "", "create", "update"):
        return None
    if merge_request.get("state") in ("closed", "merged"):
        return None

    note = object_attributes.get("note", "")
    return extract_bot_command(note, GITLAB_NOTE_COMMANDS)


def get_gitlab_issue_command(payload: dict[str, Any]) -> str | None:
    if payload.get("object_kind") != "note":
        return None

    object_attributes = payload.get("object_attributes", {})
    issue = payload.get("issue", {})
    action = object_attributes.get("action")

    if object_attributes.get("noteable_type") != "Issue":
        return None
    if object_attributes.get("system") or object_attributes.get("internal"):
        return None
    if action not in (None, "", "create", "update"):
        return None
    if issue.get("state") in ("closed", "merged"):
        return None

    note = object_attributes.get("note", "")
    return extract_bot_command(note, GITLAB_ISSUE_COMMANDS)


async def list_gitlab_merge_request_pipelines(
    merge_request_iid: str,
    limit: int = 5,
) -> list[Pipeline]:
    async with get_db(use_replica=True) as db:
        query = (
            select(Pipeline)
            .where(Pipeline.app_id == f"gitlab-mr-{merge_request_iid}")
            .order_by(Pipeline.created_at.desc())
            .limit(limit)
        )
        result = await db.execute(query)
        return list(result.scalars().all())


def format_gitlab_pipeline_status_note(pipelines: list[Pipeline]) -> str:
    if not pipelines:
        return "No pipelines recorded for this merge request yet."

    lines = ["Recent pipelines:"]

    for pipeline in pipelines:
        target_repo = pipeline.flat_manager_repo or resolve_pipeline_target_repo(pipeline)
        line = f"- `{pipeline.status.value}` • `{target_repo}`"
        if pipeline.log_url:
            line += f" • {pipeline.log_url}"
        lines.append(line)

    return "\n".join(lines)


def get_gitlab_help_note() -> str:
    return "\n".join(
        [
            "Available commands:",
            "- `bot, build` or `bot, retry` — Queue a GitHub Actions build for this MR",
            "- `bot, cancel` — Cancel active builds for this MR",
            "- `bot, status` — Show recent pipeline status for this MR",
            "- `bot, ping admins` — Mention the configured admin group",
            "- `bot, help` — Show this help message",
        ]
    )


def get_gitlab_merge_request_ref(payload: dict[str, Any]) -> str:
    merge_request = get_gitlab_merge_request_data(payload)
    merge_request_iid = merge_request.get("iid")
    if merge_request_iid:
        return f"refs/merge-requests/{merge_request_iid}/head"

    source_branch = merge_request.get("source_branch", "")
    if source_branch:
        return f"refs/heads/{source_branch}"

    return ""


def get_gitlab_project_path(payload: dict[str, Any]) -> str:
    merge_request = get_gitlab_merge_request_data(payload)
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
    merge_request = get_gitlab_merge_request_data(payload)
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

    merge_request_iid = get_gitlab_merge_request_iid(payload)
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


def build_gitlab_merge_request_initial_note(should_queue: bool) -> str:
    return (
        "🚧 Test build queued — waiting for capacity."
        if should_queue
        else "🚧 Test build enqueued."
    )


async def trigger_gitlab_merge_request_build(
    payload: dict[str, Any],
    delivery_id: str,
    command: str | None = None,
) -> dict[str, Any]:
    merge_request = get_gitlab_merge_request_data(payload)
    merge_request_iid = get_gitlab_merge_request_iid(payload)
    source_info = merge_request.get("source", {})
    source_repository = str(
        source_info.get("git_http_url")
        or payload.get("project", {}).get("git_http_url", "")
    )
    source_ref = get_gitlab_merge_request_ref(payload)
    source_sha = get_gitlab_merge_request_source_sha(payload)
    target_branch = str(
        merge_request.get("target_branch", settings.github_ci_ref)
        or settings.github_ci_ref
    )
    source_branch = str(merge_request.get("source_branch", ""))
    merge_request_title = str(merge_request.get("title", ""))
    oldrev = get_gitlab_merge_request_oldrev(payload)
    gitlab_project_path = get_gitlab_project_path(payload)
    gitlab_base_url = get_gitlab_base_url(payload)

    if settings.ff_disable_test_builds:
        await create_gitlab_merge_request_note(
            payload,
            DISABLED_TEST_BUILDS_MSG.format(statuspage_url=settings.statuspage_url),
        )
        response = {
            "message": "GitLab webhook received",
            "event_id": delivery_id,
        }
        if command:
            response["command"] = command
        return response

    if not merge_request_iid:
        raise HTTPException(
            status_code=status.HTTP_422_UNPROCESSABLE_CONTENT,
            detail="Missing merge request IID in GitLab payload.",
        )
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

    diff_refs = merge_request.get("diff_refs")
    pr_base_sha = ""
    if isinstance(diff_refs, dict):
        base_sha = diff_refs.get("base_sha")
        if base_sha is not None:
            pr_base_sha = str(base_sha)

    dispatch_repo = f"{settings.github_org}/{settings.github_ci_repo}"
    dispatch_extra_inputs = {
        "pr-number": merge_request_iid,
        "pr-head-sha": source_sha,
        "pr-title": merge_request_title,
        "head-ref": source_branch,
        "base-ref": target_branch,
        "pr-draft": "true" if is_gitlab_merge_request_draft(payload) else "false",
    }
    if pr_base_sha:
        dispatch_extra_inputs["pr-base-sha"] = pr_base_sha
    if oldrev:
        dispatch_extra_inputs["before-sha"] = oldrev

    params = {
        "repo": dispatch_repo,
        "ref": source_ref,
        "sha": source_sha,
        "use_spot": False,
        "build_type": "default",
        "event_name": "pull_request",
        "actor": str(payload.get("user", {}).get("username", "")),
        "head_ref": source_branch,
        "pr_target_branch": target_branch,
        "pr_head_sha": source_sha,
        "pr_title": merge_request_title,
        "pr_draft": "true" if is_gitlab_merge_request_draft(payload) else "false",
        "dispatch_owner": settings.github_org,
        "dispatch_repo": settings.github_ci_repo,
        "dispatch_workflow_id": settings.github_ci_workflow,
        "dispatch_ref": settings.github_ci_ref,
        "gitlab_base_url": gitlab_base_url,
        "gitlab_project_path": gitlab_project_path,
        "gitlab_merge_request_iid": merge_request_iid,
        "gitlab_source_branch": source_branch,
        "gitlab_target_branch": target_branch,
        "gitlab_source_sha": source_sha,
    }
    if oldrev:
        params["before_sha"] = oldrev
    if pr_base_sha:
        params["pr_base_sha"] = pr_base_sha
    params.update(
        build_github_dispatch_params(
            repo_name=dispatch_repo,
            source_event_name="pull_request",
            source_ref=source_ref,
            source_sha=source_sha,
            actor_login=str(payload.get("user", {}).get("username", "")),
            extra_inputs=dispatch_extra_inputs,
            source_repository=source_repository,
        )
    )

    build_pipeline = BuildPipeline()
    pipeline = await build_pipeline.create_pipeline(
        app_id=f"gitlab-mr-{merge_request_iid}",
        params=params,
        webhook_event_id=None,
    )
    pipeline = await build_pipeline.prepare_pipeline_for_start(pipeline.id)
    await build_pipeline.supersede_conflicting_test_pipelines(pipeline.id)
    should_queue = await build_pipeline.should_queue_test_build(pipeline.id)

    if not should_queue:
        pipeline = await build_pipeline.start_pipeline(pipeline.id)

    await create_gitlab_merge_request_note(
        payload,
        build_gitlab_merge_request_initial_note(should_queue),
    )

    response = {
        "message": "GitLab webhook received",
        "event_id": delivery_id,
        "pipeline_id": str(pipeline.id),
        "pipeline_status": pipeline.status.value,
        "target_repo": pipeline.flat_manager_repo or resolve_pipeline_target_repo(pipeline),
    }
    if command:
        response["command"] = command
    return response


async def trigger_gitlab_push_build(
    payload: dict[str, Any],
    delivery_id: str,
) -> dict[str, Any]:
    project = payload.get("project", {})
    project_path = get_gitlab_project_path(payload)
    gitlab_base_url = get_gitlab_base_url(payload)
    source_repository = str(project.get("git_http_url", ""))
    ref = str(payload.get("ref", ""))
    source_sha = get_gitlab_push_source_sha(payload)
    actor_login = str(
        payload.get("user_username")
        or payload.get("user_name")
        or payload.get("user", {}).get("username", "")
    )
    head_ref = ref.removeprefix("refs/heads/") if ref.startswith("refs/heads/") else ""

    if not source_repository and gitlab_base_url and project_path:
        source_repository = f"{gitlab_base_url}/{project_path}.git"

    if not source_repository:
        raise HTTPException(
            status_code=status.HTTP_422_UNPROCESSABLE_CONTENT,
            detail="Missing source repository URL in GitLab push payload.",
        )
    if not ref:
        raise HTTPException(
            status_code=status.HTTP_422_UNPROCESSABLE_CONTENT,
            detail="Missing ref in GitLab push payload.",
        )
    if not source_sha:
        raise HTTPException(
            status_code=status.HTTP_422_UNPROCESSABLE_CONTENT,
            detail="Missing commit SHA in GitLab push payload.",
        )
    if not gitlab_base_url or not project_path:
        raise HTTPException(
            status_code=status.HTTP_422_UNPROCESSABLE_CONTENT,
            detail="Missing GitLab routing fields in push payload.",
        )

    dispatch_repo = f"{settings.github_org}/{settings.github_ci_repo}"
    params = {
        "repo": dispatch_repo,
        "ref": ref,
        "sha": source_sha,
        "push": "true",
        "event_name": "push",
        "actor": actor_login,
        "head_ref": head_ref,
        "dispatch_owner": settings.github_org,
        "dispatch_repo": settings.github_ci_repo,
        "dispatch_workflow_id": settings.github_ci_workflow,
        "dispatch_ref": settings.github_ci_ref,
        "gitlab_base_url": gitlab_base_url,
        "gitlab_project_path": project_path,
        "gitlab_source_sha": source_sha,
    }

    if head_ref:
        params["gitlab_source_branch"] = head_ref
        params["gitlab_target_branch"] = head_ref
        params["pr_target_branch"] = head_ref

    params.update(
        build_github_dispatch_params(
            repo_name=dispatch_repo,
            source_event_name="push",
            source_ref=ref,
            source_sha=source_sha,
            actor_login=actor_login,
            extra_inputs={
                "before-sha": str(payload.get("before", "")),
                "head-ref": head_ref,
            },
            source_repository=source_repository,
        )
    )

    build_pipeline = BuildPipeline()
    pipeline = await build_pipeline.create_pipeline(
        app_id=(
            settings.github_ci_repo
            if project_path.lower()
            == f"{settings.github_org}/{settings.github_ci_repo}".lower()
            else project_path.rsplit("/", 1)[-1]
        ),
        params=params,
        webhook_event_id=None,
    )
    pipeline = await build_pipeline.prepare_pipeline_for_start(pipeline.id)
    await build_pipeline.supersede_conflicting_test_pipelines(pipeline.id)
    should_queue = await build_pipeline.should_queue_test_build(pipeline.id)

    if not should_queue:
        pipeline = await build_pipeline.start_pipeline(pipeline.id)

    response = {
        "message": "GitLab webhook received",
        "event_id": delivery_id,
        "pipeline_id": str(pipeline.id),
        "pipeline_status": pipeline.status.value,
        "target_repo": pipeline.flat_manager_repo or resolve_pipeline_target_repo(pipeline),
    }

    # For master pushes: auto-create a vBETA tag and dispatch a beta release pipeline.
    # Beta tags are never created manually — Foreman owns them entirely.
    if ref == "refs/heads/master" and source_sha:
        beta_tag_name = "vBETA" + datetime.now(timezone.utc).strftime("%Y%m%d%H%M")
        beta_ref = f"refs/tags/{beta_tag_name}"

        await create_github_tag(
            f"{settings.github_org}/{settings.github_ci_repo}",
            beta_tag_name,
            source_sha,
        )

        beta_params: dict[str, Any] = {
            "repo": dispatch_repo,
            "ref": beta_ref,
            "sha": source_sha,
            "push": "true",
            "event_name": "push",
            "actor": actor_login,
            "head_ref": "",
            "dispatch_owner": settings.github_org,
            "dispatch_repo": settings.github_ci_repo,
            "dispatch_workflow_id": settings.github_ci_workflow,
            "dispatch_ref": settings.github_ci_ref,
            "gitlab_base_url": gitlab_base_url,
            "gitlab_project_path": project_path,
            "gitlab_source_sha": source_sha,
        }
        beta_params.update(
            build_github_dispatch_params(
                repo_name=dispatch_repo,
                source_event_name="push",
                source_ref=beta_ref,
                source_sha=source_sha,
                actor_login=actor_login,
                source_repository=source_repository,
            )
        )

        beta_pipeline = await build_pipeline.create_pipeline(
            app_id=settings.github_ci_repo,
            params=beta_params,
            webhook_event_id=None,
        )
        beta_pipeline = await build_pipeline.prepare_pipeline_for_start(beta_pipeline.id)
        beta_pipeline = await build_pipeline.start_pipeline(beta_pipeline.id)
        response["beta_pipeline_id"] = str(beta_pipeline.id)
        response["beta_tag"] = beta_tag_name

    return response


def _get_gitlab_issue_iid(payload: dict[str, Any]) -> str:
    issue = payload.get("issue", {})
    issue_iid = issue.get("iid")
    return str(issue_iid) if issue_iid is not None else ""


async def create_gitlab_issue_note(
    payload: dict[str, Any],
    note: str,
    issue_iid: str | None = None,
) -> bool:
    if not settings.gitlab_api_token:
        logger.info("Skipping GitLab issue note because GITLAB_API_TOKEN is not configured")
        return False

    project_path = get_gitlab_project_path(payload)
    gitlab_base_url = get_gitlab_base_url(payload)
    resolved_issue_iid = issue_iid or _get_gitlab_issue_iid(payload)

    if not resolved_issue_iid or not project_path or not gitlab_base_url:
        logger.warning(
            "Skipping GitLab issue note because payload is missing required routing fields",
            has_issue_iid=bool(resolved_issue_iid),
            has_project_path=bool(project_path),
            has_gitlab_base_url=bool(gitlab_base_url),
        )
        return False

    endpoint = (
        f"{gitlab_base_url}/api/v4/projects/{quote(project_path, safe='')}/"
        f"issues/{resolved_issue_iid}/notes"
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
            "GitLab issue note creation failed",
            endpoint=endpoint,
            status_code=error.response.status_code,
            response_text=error.response.text[:400],
        )
    except httpx.HTTPError as error:
        logger.warning(
            "GitLab issue note request failed",
            endpoint=endpoint,
            error=str(error),
        )

    return False


async def close_gitlab_issue(payload: dict[str, Any], issue_iid: str | None = None) -> bool:
    if not settings.gitlab_api_token:
        return False

    project_path = get_gitlab_project_path(payload)
    gitlab_base_url = get_gitlab_base_url(payload)
    resolved_issue_iid = issue_iid or _get_gitlab_issue_iid(payload)

    if not resolved_issue_iid or not project_path or not gitlab_base_url:
        return False

    endpoint = (
        f"{gitlab_base_url}/api/v4/projects/{quote(project_path, safe='')}/"
        f"issues/{resolved_issue_iid}"
    )

    try:
        async with httpx.AsyncClient(timeout=10.0) as client:
            response = await client.put(
                endpoint,
                headers={"PRIVATE-TOKEN": settings.gitlab_api_token},
                data={"state_event": "close"},
            )
            response.raise_for_status()
        return True
    except httpx.HTTPError as error:
        logger.warning(
            "GitLab issue close request failed",
            endpoint=endpoint,
            error=str(error),
        )
        return False


async def validate_gitlab_issue_retry_permissions(payload: dict[str, Any]) -> bool:
    if not settings.gitlab_api_token:
        return False

    project_path = get_gitlab_project_path(payload)
    gitlab_base_url = get_gitlab_base_url(payload)
    user_id = payload.get("user", {}).get("id")

    if not project_path or not gitlab_base_url or user_id is None:
        return False

    endpoint = (
        f"{gitlab_base_url}/api/v4/projects/{quote(project_path, safe='')}/"
        f"members/all/{user_id}"
    )

    try:
        async with httpx.AsyncClient(timeout=10.0) as client:
            response = await client.get(
                endpoint,
                headers={"PRIVATE-TOKEN": settings.gitlab_api_token},
            )
            if response.status_code != status.HTTP_200_OK:
                return False

            access_level = int(response.json().get("access_level", 0))
            return access_level >= 40
    except (httpx.HTTPError, TypeError, ValueError) as error:
        logger.warning(
            "GitLab issue retry permission check failed",
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
                or target_ref in ("master", "lts")
                or target_ref.startswith("branch/")
            )

    if "commits" in payload and ref:
        after_sha = str(payload.get("after", ""))
        if payload.get("deleted") or after_sha == ("0" * 40):
            return False

        if ref.startswith("refs/tags/"):
            return True

        if ref == "refs/heads/master":
            return True

    if "comment" in payload:
        comment_author = payload.get("comment", {}).get("user", {}).get("login")

        if comment_author in ("github-actions[bot]",):
            return False

        if extract_bot_command(comment):
            return True

    return False


async def cancel_gitlab_merge_request_pipelines(merge_request_iid: str) -> int:
    app_id = f"gitlab-mr-{merge_request_iid}"
    pipelines_to_cancel: list[tuple[uuid.UUID, int | None, dict[str, Any] | None]] = []

    async with get_db() as db:
        query = select(Pipeline).where(
            Pipeline.app_id == app_id,
            Pipeline.status.in_([PipelineStatus.PENDING, PipelineStatus.RUNNING]),
        )
        result = await db.execute(query)
        active_pipelines = list(result.scalars().all())

        if not active_pipelines:
            return 0

        for pipeline in active_pipelines:
            pipeline.status = PipelineStatus.CANCELLED
            pipeline.finished_at = datetime.now(timezone.utc)

        pipelines_to_cancel = [
            (
                pipeline.id,
                pipeline.build_id,
                build_cancel_provider_data(pipeline),
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

    return len(pipelines_to_cancel)


async def handle_issue_retry(
    git_repo: str,
    issue_number: int,
    issue_body: str,
    comment_author: str,
    webhook_event_id: uuid.UUID | None,
) -> uuid.UUID | None:
    if not await validate_retry_permissions(git_repo, comment_author):
        await add_issue_comment(
            git_repo=git_repo,
            issue_number=issue_number,
            comment=(
                f"User `{comment_author}` does not have permission to retry stable builds "
                "from issues."
            ),
        )
        return None

    issue_edited = await is_issue_edited(git_repo, issue_number)
    if issue_edited:
        await add_issue_comment(
            git_repo=git_repo,
            issue_number=issue_number,
            comment=(
                "This issue was edited after it was opened. Could not safely parse build "
                "parameters for retry."
            ),
        )
        return None

    parsed_issue = await parse_failure_issue(issue_body, git_repo)
    if parsed_issue is None:
        await add_issue_comment(
            git_repo=git_repo,
            issue_number=issue_number,
            comment="Could not parse build parameters from this failure issue.",
        )
        return None

    run_id = extract_run_id_from_log_url(parsed_issue.get("log_url"))
    run_title = None
    if run_id and run_id.isdigit():
        run_title = await get_workflow_run_title(int(run_id))

    retry_repo = str(parsed_issue.get("github_repo") or git_repo)
    retry_ref = str(parsed_issue["ref"])
    params: dict[str, Any] = {
        "repo": retry_repo,
        "ref": retry_ref,
        "sha": parsed_issue["sha"],
        "target_repo": parsed_issue["flat_manager_repo"],
        "build_type": "default",
        "use_spot": False,
        "retry_count": 1,
        "retry_from_issue": issue_number,
        "retry_command_source": "github_issue",
    }

    if parsed_issue.get("job_type"):
        params["retry_job_type"] = parsed_issue["job_type"]
    if run_title:
        params["retry_from_run_title"] = run_title

    native_github_ci_repo = f"{settings.github_org}/{settings.github_ci_repo}"
    if retry_repo.lower() == native_github_ci_repo.lower():
        head_ref = (
            retry_ref.removeprefix("refs/heads/")
            if retry_ref.startswith("refs/heads/")
            else ""
        )
        params.update(
            {
                "event_name": "push",
                "actor": comment_author,
                "head_ref": head_ref,
            }
        )
        params.update(
            build_github_dispatch_params(
                repo_name=retry_repo,
                source_event_name="push",
                source_ref=retry_ref,
                source_sha=parsed_issue["sha"],
                actor_login=comment_author,
                extra_inputs={
                    "head-ref": head_ref,
                },
            )
        )

    pipeline_service = BuildPipeline()
    pipeline = await pipeline_service.create_pipeline(
        app_id=str(parsed_issue.get("app_id") or git_repo.rsplit("/", 1)[-1]),
        params=params,
        webhook_event_id=webhook_event_id,
    )
    pipeline = await pipeline_service.prepare_pipeline_for_start(pipeline.id)
    await pipeline_service.supersede_conflicting_test_pipelines(pipeline.id)
    should_queue_test_build = await pipeline_service.should_queue_test_build(
        pipeline.id
    )

    target_url = f"{settings.base_url}/api/pipelines/{pipeline.id}"
    commit_sha = pipeline.params.get("sha")
    pipeline_repo = pipeline.params.get("repo")

    if commit_sha and pipeline_repo:
        await update_commit_status(
            sha=commit_sha,
            state="pending",
            git_repo=pipeline_repo,
            description=(
                "Stable build retry queued — waiting for capacity"
                if should_queue_test_build
                else "Stable build retry enqueued"
            ),
            target_url=target_url,
        )

    if not should_queue_test_build:
        pipeline = await pipeline_service.start_pipeline(pipeline.id)

    await add_issue_comment(
        git_repo=git_repo,
        issue_number=issue_number,
        comment=(
            "🔁 Stable build retry queued — waiting for capacity."
            if should_queue_test_build
            else "🔁 Stable build retry enqueued."
        )
        + f"\n\nTrack progress: {target_url}",
    )

    return pipeline.id


async def handle_gitlab_issue_retry(
    payload: dict[str, Any],
    command: str,
    delivery_id: str,
) -> dict[str, Any]:
    issue_iid = _get_gitlab_issue_iid(payload)
    if not issue_iid:
        raise HTTPException(
            status_code=status.HTTP_422_UNPROCESSABLE_CONTENT,
            detail="Missing issue IID in GitLab payload.",
        )

    if command == "bot, help":
        await create_gitlab_issue_note(
            payload,
            "Available commands:\n- `bot, retry` — Retry the stable build referenced by this issue",
            issue_iid=issue_iid,
        )
        return {
            "message": "GitLab webhook received",
            "command": command,
        }

    comment_author = payload.get("user", {}).get("username", "")
    if not await validate_gitlab_issue_retry_permissions(payload):
        await create_gitlab_issue_note(
            payload,
            (
                f"User `{comment_author}` does not have permission to retry stable builds "
                "from issues."
            ),
            issue_iid=issue_iid,
        )
        return {
            "message": "GitLab webhook received",
            "command": command,
        }

    parsed_issue = await parse_failure_issue(
        payload.get("issue", {}).get("description", ""),
        get_gitlab_project_path(payload),
    )
    if parsed_issue is None:
        await create_gitlab_issue_note(
            payload,
            "Could not parse build parameters from this failure issue.",
            issue_iid=issue_iid,
        )
        return {
            "message": "GitLab webhook received",
            "command": command,
        }

    gitlab_base_url = get_gitlab_base_url(payload)
    project_path = get_gitlab_project_path(payload)
    source_repository = parsed_issue.get("source_repository")
    if not source_repository and gitlab_base_url and project_path:
        source_repository = f"{gitlab_base_url}/{project_path}.git"

    ref = parsed_issue["ref"]
    source_branch = (
        ref.removeprefix("refs/heads/") if ref.startswith("refs/heads/") else ""
    )

    params: dict[str, Any] = {
        "ref": ref,
        "sha": parsed_issue["sha"],
        "repo": parsed_issue.get("github_repo") or parsed_issue["repo"],
        "target_repo": parsed_issue["flat_manager_repo"],
        "build_type": "default",
        "use_spot": False,
        "retry_count": 1,
        "retry_from_gitlab_issue_iid": issue_iid,
        "retry_command_source": "gitlab_issue",
        "gitlab_base_url": gitlab_base_url,
        "gitlab_project_path": project_path,
        "gitlab_issue_iid": issue_iid,
        "gitlab_source_sha": parsed_issue["sha"],
        "gitlab_source_branch": source_branch,
        "gitlab_target_branch": source_branch or settings.github_ci_ref,
        "pr_target_branch": source_branch or settings.github_ci_ref,
        "dispatch_owner": settings.github_org,
        "dispatch_repo": settings.github_ci_repo,
        "dispatch_workflow_id": settings.github_ci_workflow,
        "dispatch_ref": settings.github_ci_ref,
        "dispatch_inputs": {
            "force-all": "false",
            "source-repository": source_repository,
            "source-ref": ref,
        },
    }

    pipeline_service = BuildPipeline()
    pipeline = await pipeline_service.create_pipeline(
        app_id=str(parsed_issue.get("app_id") or project_path.rsplit("/", 1)[-1]),
        params=params,
        webhook_event_id=None,
    )
    pipeline = await pipeline_service.prepare_pipeline_for_start(pipeline.id)
    await pipeline_service.supersede_conflicting_test_pipelines(pipeline.id)
    should_queue_test_build = await pipeline_service.should_queue_test_build(
        pipeline.id
    )

    if not should_queue_test_build:
        pipeline = await pipeline_service.start_pipeline(pipeline.id)

    await create_gitlab_issue_note(
        payload,
        (
            "🔁 Stable build retry queued — waiting for capacity."
            if should_queue_test_build
            else "🔁 Stable build retry enqueued."
        )
        + f"\n\nTrack progress: {settings.base_url}/api/pipelines/{pipeline.id}",
        issue_iid=issue_iid,
    )

    return {
        "message": "GitLab webhook received",
        "event_id": delivery_id,
        "command": command,
        "pipeline_id": str(pipeline.id),
        "pipeline_status": pipeline.status.value,
        "target_repo": pipeline.flat_manager_repo or parsed_issue["flat_manager_repo"],
    }


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

    if x_gitlab_event in ("Push Hook", "Tag Push Hook"):
        if not should_autostart_gitlab_push_build(payload):
            return {"message": "Webhook received but ignored due to push filter."}
        return await trigger_gitlab_push_build(
            payload,
            x_gitlab_event_uuid or str(uuid.uuid4()),
        )

    if x_gitlab_event == "Merge Request Hook":
        if not should_autostart_gitlab_merge_request_build(payload):
            return {"message": "Webhook received but ignored due to merge request filter."}
        return await trigger_gitlab_merge_request_build(
            payload,
            x_gitlab_event_uuid or str(uuid.uuid4()),
        )

    if x_gitlab_event != "Note Hook":
        return {"message": "Webhook received but ignored due to event type filter."}

    issue_command = get_gitlab_issue_command(payload)
    if issue_command:
        return await handle_gitlab_issue_retry(
            payload,
            issue_command,
            x_gitlab_event_uuid or str(uuid.uuid4()),
        )

    command = get_gitlab_note_command(payload)

    if not command:
        return {"message": "Webhook received but ignored due to note filter."}

    if command == "bot, ping admins":
        if settings.ff_admin_ping_comment:
            note = f"Contacted admins: cc {get_gitlab_admin_mention()}"
        else:
            note = "Admin ping is disabled by configuration."

        await create_gitlab_merge_request_note(payload, note)
        return {
            "message": "GitLab webhook received",
            "command": command,
        }

    merge_request_iid = get_gitlab_merge_request_iid(payload)

    if command == "bot, help":
        await create_gitlab_merge_request_note(payload, get_gitlab_help_note())
        return {
            "message": "GitLab webhook received",
            "command": command,
        }

    if command == "bot, status":
        if not merge_request_iid:
            raise HTTPException(
                status_code=status.HTTP_422_UNPROCESSABLE_CONTENT,
                detail="Missing merge request IID in GitLab payload.",
            )

        pipelines = await list_gitlab_merge_request_pipelines(merge_request_iid)
        await create_gitlab_merge_request_note(
            payload,
            format_gitlab_pipeline_status_note(pipelines),
        )
        return {
            "message": "GitLab webhook received",
            "command": command,
            "pipeline_count": len(pipelines),
        }

    if command == "bot, cancel":
        if not merge_request_iid:
            raise HTTPException(
                status_code=status.HTTP_422_UNPROCESSABLE_CONTENT,
                detail="Missing merge request IID in GitLab payload.",
            )

        cancelled_count = await cancel_gitlab_merge_request_pipelines(
            merge_request_iid
        )
        note = (
            f"Cancelled {cancelled_count} active build(s)."
            if cancelled_count
            else "No active builds found to cancel."
        )
        await create_gitlab_merge_request_note(payload, note)
        return {
            "message": "GitLab webhook received",
            "command": command,
            "cancelled_count": cancelled_count,
        }

    return await trigger_gitlab_merge_request_build(
        payload,
        x_gitlab_event_uuid or str(uuid.uuid4()),
        command=command,
    )


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

    native_repo = f"{settings.github_org}/{settings.github_ci_repo}".lower()
    if is_push_event and repo_name.lower() == native_repo:
        return {
            "message": "Webhook received but ignored because GitLab push hooks are authoritative for this repository."
        }

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
        pr_head = pr.get("head", {})
        pr_base = pr.get("base", {})

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

        sha = pr_head.get("sha")
        pr_head_ref = str(pr_head.get("ref", ""))
        pr_base_ref = str(pr_base.get("ref", "master"))
        params.update(
            {
                "ref": f"refs/pull/{pr_number}/head",
                "pr_number": str(pr_number) if pr_number is not None else "",
                "action": str(payload.get("action", "")),
                "pr_target_branch": pr_base_ref,
            }
        )
        params.update(build_gitlab_issue_routing_params(event.repository))
        params.update(
            build_github_dispatch_params(
                repo_name=event.repository,
                source_event_name="pull_request",
                source_ref=params["ref"],
                source_sha=sha,
                actor_login=event.actor,
                extra_inputs={
                    "pr-number": params["pr_number"],
                    "pr-base-sha": str(pr_base.get("sha", "")),
                    "pr-head-sha": str(pr_head.get("sha", "")),
                    "pr-title": str(pr.get("title", "")),
                    "head-ref": pr_head_ref,
                    "base-ref": pr_base_ref,
                    "pr-draft": "true" if pr.get("draft") else "false",
                },
            )
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
        params.update(build_gitlab_issue_routing_params(event.repository))
        params.update(
            build_github_dispatch_params(
                repo_name=event.repository,
                source_event_name="push",
                source_ref=ref,
                source_sha=sha,
                actor_login=event.actor,
                extra_inputs={
                    "before-sha": str(payload.get("before", "")),
                    "head-ref": ref.removeprefix("refs/heads/")
                    if ref.startswith("refs/heads/")
                    else "",
                },
            )
        )

    elif "comment" in payload:
        comment_body = payload.get("comment", {}).get("body", "")
        command = extract_bot_command(comment_body)
        issue = payload.get("issue", {})
        issue_number = issue.get("number")
        comment_author = payload.get("comment", {}).get("user", {}).get("login", "")
        pr_url = issue.get("pull_request", {}).get("url", "")
        repo = event.repository

        if command == "bot, ping admins":
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

        elif command == "bot, cancel":
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
                        build_cancel_provider_data(pipeline),
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

        elif command == "bot, retry" and not pr_url and issue_number is not None:
            return await handle_issue_retry(
                git_repo=repo,
                issue_number=issue_number,
                issue_body=str(issue.get("body", "")),
                comment_author=comment_author,
                webhook_event_id=event.id,
            )

        elif is_build_bot_command(command):
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
            pr_data: dict[str, Any] = {}
            pr_head: dict[str, Any] = {}
            pr_base: dict[str, Any] = {}
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
                pr_head = pr_data.get("head", {})
                pr_base = pr_data.get("base", {})
                sha = pr_head.get("sha")
                pr_target_branch = pr_base.get("ref", "master")

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
            params.update(build_gitlab_issue_routing_params(repo))
            params.update(
                build_github_dispatch_params(
                    repo_name=repo,
                    source_event_name="pull_request",
                    source_ref=pr_ref,
                    source_sha=sha,
                    actor_login=event.actor,
                    extra_inputs={
                        "pr-number": str(issue_number),
                        "pr-base-sha": str(pr_base.get("sha", "")),
                        "pr-head-sha": str(pr_head.get("sha", "")),
                        "pr-title": str(pr_data.get("title", "")),
                        "head-ref": str(pr_head.get("ref", "")),
                        "base-ref": pr_target_branch,
                        "pr-draft": "true" if pr_data.get("draft") else "false",
                    },
                )
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
