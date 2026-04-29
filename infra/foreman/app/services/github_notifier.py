import asyncio

import structlog

from app.config import settings
from app.models import Pipeline
from app.utils.github import (
    create_pr_comment,
    get_build_job_arches,
    get_linter_warning_messages,
    update_commit_status,
)
from app.utils.pipeline_events import is_scheduled_native_github_ci

logger = structlog.get_logger(__name__)

_DEPRECATION_MSG = (
    "GitHubNotifier is deprecated and will be removed in a future release. "
    "All build notifications (commit status, issue lifecycle, PR comments) are now "
    "handled by GitLabNotifier. GitHubNotifier only remains for GitHub PR comment "
    "delivery (notify_pr_build_started / notify_pr_build_complete) and GitHub commit "
    "status writes for GitHub-originated pipelines."
)


class GitHubNotifier:
    """
    .. deprecated::
        GitHubNotifier is deprecated. Use :class:`app.services.gitlab_notifier.GitLabNotifier`
        for all new notification paths. Only GitHub PR comment delivery and GitHub commit
        status updates for GitHub-sourced pipelines are still routed here.
        All stable issue lifecycle and failure issue creation now goes to GitLab.
    """

    def __init__(self):
        pass

    def _build_failure_issue_title(
        self,
        pipeline: Pipeline,
        status: str = "failure",
    ) -> str:
        outcome = "cancelled" if status == "cancelled" else "failed"
        return f"Stable build {outcome} for {pipeline.app_id}"

    def _build_failure_issue_body(
        self,
        pipeline: Pipeline,
        status: str = "failure",
    ) -> str:
        sha = pipeline.params.get("sha", "")
        ref = pipeline.params.get("ref", "refs/heads/master")
        git_repo = pipeline.params.get("repo", "")
        outcome = "was cancelled" if status == "cancelled" else "failed"
        investigation = (
            "Please investigate this cancellation."
            if status == "cancelled"
            else "Please investigate this failure."
        )

        lines = [
            f"The stable build pipeline for `{pipeline.app_id}` {outcome}.",
            "",
            f"Commit SHA: {sha}",
            f"Ref: {ref}",
        ]

        if pipeline.log_url:
            lines.append(f"Build log: {pipeline.log_url}")
        if git_repo:
            lines.append(f"GitHub repository: {git_repo}")

        lines.extend(
            [
                "",
                investigation,
                "To retry the stable build, comment `bot, retry` on this issue.",
            ]
        )

        return "\n".join(lines)

    async def _handle_stable_issue_lifecycle(
        self,
        pipeline: Pipeline,
        status: str,
    ) -> None:
        if (pipeline.flat_manager_repo or "") != "stable":
            return

        if is_scheduled_native_github_ci(pipeline):
            return

        # All stable issue lifecycle is handled on GitLab
        from app.services.gitlab_notifier import GitLabNotifier

        await GitLabNotifier()._handle_stable_issue_lifecycle(pipeline, status)

    async def notify_build_status(
        self,
        pipeline: Pipeline,
        status: str,
        log_url: str | None = None,
    ) -> None:
        app_id = pipeline.app_id
        sha = pipeline.params.get("sha")
        git_repo = pipeline.params.get("repo")

        if (
            not all([app_id, sha, git_repo])
            or not isinstance(sha, str)
            or not isinstance(git_repo, str)
        ):
            logger.info(
                "Missing required params for GitHub status update",
                pipeline_id=str(pipeline.id),
                has_app_id=bool(app_id),
                has_sha=bool(sha),
                has_git_repo=bool(git_repo),
            )
            return

        match status:
            case "success":
                description = "Build succeeded"
                github_state = "success"
            case "committing":
                description = "Committing build..."
                github_state = "pending"
            case "committed":
                description = "Build ready"
                github_state = "success"
            case "failure":
                description = "Build failed"
                github_state = "failure"
            case "cancelled":
                description = "Build cancelled"
                github_state = "failure"
            case _:
                description = f"Build status: {status}."
                github_state = "failure"

        target_url = log_url or pipeline.log_url or ""
        if not target_url:
            logger.warning(
                "log_url is unexpectedly None when setting final commit status",
                pipeline_id=str(pipeline.id),
            )

        await update_commit_status(
            sha=sha,
            state=github_state,
            git_repo=git_repo,
            description=description,
            target_url=target_url,
        )

    async def notify_build_started(
        self,
        pipeline: Pipeline,
        log_url: str,
    ) -> None:
        sha = pipeline.params.get("sha")
        git_repo = pipeline.params.get("repo")

        if (
            not all([sha, git_repo])
            or not isinstance(sha, str)
            or not isinstance(git_repo, str)
        ):
            logger.info(
                "Missing required params for GitHub status update",
                pipeline_id=str(pipeline.id),
                has_sha=bool(sha),
                has_git_repo=bool(git_repo),
            )
            return

        await update_commit_status(
            sha=sha,
            state="pending",
            git_repo=git_repo,
            description="Build in progress",
            target_url=log_url,
        )

    async def notify_pr_build_started(
        self,
        pipeline: Pipeline,
        log_url: str,
    ) -> None:
        pr_number_str = pipeline.params.get("pr_number")
        git_repo = pipeline.params.get("repo")

        if not pr_number_str or not git_repo:
            logger.error(
                "Missing required params for PR comment",
                pipeline_id=str(pipeline.id),
                has_pr_number=bool(pr_number_str),
                has_git_repo=bool(git_repo),
            )
            return

        try:
            pr_number = int(pr_number_str)
            comment = f"🚧 Started [test build]({log_url})."
            await create_pr_comment(
                git_repo=git_repo,
                pr_number=pr_number,
                comment=comment,
            )
        except ValueError:
            logger.error(
                "Invalid PR number. Skipping PR comment",
                pr_number=pr_number_str,
                pipeline_id=str(pipeline.id),
            )
        except Exception as e:
            logger.error(
                "Error creating 'Started' PR comment",
                pipeline_id=str(pipeline.id),
                error=str(e),
            )

    async def notify_pr_build_complete(
        self,
        pipeline: Pipeline,
        status: str,
    ) -> None:
        pr_number_str = pipeline.params.get("pr_number")
        git_repo = pipeline.params.get("repo")

        if not pr_number_str or not git_repo:
            logger.error(
                "Missing required params for PR comment",
                pipeline_id=str(pipeline.id),
                has_pr_number=bool(pr_number_str),
                has_git_repo=bool(git_repo),
            )
            return

        try:
            pr_number = int(pr_number_str)
            log_url = pipeline.log_url
            comment = ""
            footnote = (
                "<details><summary>Help</summary>\n\n"
                "- <code>bot, build</code> - Restart the test build\n"
            )

            if settings.ff_admin_ping_comment:
                footnote += (
                    f"- <code>bot, ping admins</code> - Contact {settings.admin_team}\n"
                )

            footnote += "</details>"

            run_id = None
            if log_url:
                try:
                    run_id = int(log_url.rstrip("/").split("/")[-1])
                except (ValueError, TypeError, IndexError):
                    logger.warning(
                        "Failed to extract run_id from log_url", log_url=log_url
                    )

            linter_warnings: list[str] = []
            build_job_arches: list[str] = []

            if run_id is not None:
                linter_warnings, build_job_arches = await asyncio.gather(
                    get_linter_warning_messages(run_id),
                    get_build_job_arches(run_id),
                )

            arch_info_comment = ""
            if build_job_arches:
                sorted_arches = sorted(build_job_arches)
                plural = "s" if len(build_job_arches) > 1 else ""
                arch_text = ""
                if len(build_job_arches) == 1:
                    arch_text = sorted_arches[0]
                elif len(build_job_arches) == 2:
                    arch_text = " and ".join(sorted_arches)
                elif len(build_job_arches) > 2:
                    arch_text = (
                        ", ".join(sorted_arches[:-1]) + ", and " + sorted_arches[-1]
                    )
                arch_info_comment = f"\n\n*Built for {arch_text} architecture{plural}.*"

            if status == "success" or status == "committed":
                comment = f"✅ [Test build succeeded]({log_url}).{arch_info_comment}"
                if linter_warnings:
                    warnings_text = "\n".join(f"- {w.strip()}" for w in linter_warnings)
                    comment += (
                        "\n\n⚠️  Linter warnings:\n\n"
                        "_Warnings can be promoted to errors in the future. Please try to resolve them._\n\n"
                        f"{warnings_text}"
                    )
            elif status == "failure":
                comment = f"❌ [Test build]({log_url}) failed.\n\n{footnote}"
            elif status == "cancelled":
                comment = f"❌ [Test build]({log_url}) was cancelled.\n\n{footnote}"

            if comment:
                await create_pr_comment(
                    git_repo=git_repo,
                    pr_number=pr_number,
                    comment=comment,
                )
        except ValueError:
            logger.error(
                "Invalid PR number. Skipping final PR comment.",
                pr_number=pr_number_str,
                pipeline_id=str(pipeline.id),
            )
        except Exception as e:
            logger.error(
                "Error creating final PR comment",
                pipeline_id=str(pipeline.id),
                error=str(e),
            )

    async def handle_build_completion(
        self,
        pipeline: Pipeline,
        status: str,
    ) -> None:
        await self.notify_build_status(pipeline, status)

        if pipeline.params.get("pr_number"):
            await self.notify_pr_build_complete(pipeline, status)

        await self._handle_stable_issue_lifecycle(pipeline, status)

    async def handle_build_started(
        self,
        pipeline: Pipeline,
        log_url: str,
    ) -> None:
        await self.notify_build_started(pipeline, log_url)

        if pipeline.params.get("pr_number"):
            await self.notify_pr_build_started(pipeline, log_url)
