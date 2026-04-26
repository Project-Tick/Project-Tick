from urllib.parse import quote

import httpx
import structlog

from app.config import settings
from app.models import Pipeline
from app.utils.pipeline_events import is_scheduled_native_github_ci
from app.utils.github import get_build_job_arches

logger = structlog.get_logger(__name__)


class GitLabNotifier:
    status_name = "github-actions/CI"

    @staticmethod
    def _get_param(pipeline: Pipeline, key: str) -> str:
        value = (pipeline.params or {}).get(key, "")
        return value if isinstance(value, str) else ""

    def _get_target_branch(self, pipeline: Pipeline) -> str:
        for key in ("gitlab_target_branch", "pr_target_branch"):
            value = self._get_param(pipeline, key)
            if value:
                return value

        return self._get_param(pipeline, "gitlab_source_branch") or "master"

    def _get_target_label(self, pipeline: Pipeline) -> str:
        target_branch = self._get_target_branch(pipeline)
        target_repo = pipeline.flat_manager_repo or "test"

        if target_repo == "stable":
            return f"{target_branch} (stable)"
        if target_repo == "beta":
            return target_branch

        return target_branch

    def _get_status_name(self, pipeline: Pipeline) -> str:
        return f"{self.status_name}/{self._get_target_branch(pipeline)}"

    async def _get_component_targets(self, pipeline: Pipeline) -> list[str]:
        log_url = pipeline.log_url or ""
        if not log_url:
            return []

        try:
            run_id = int(log_url.rstrip("/").split("/")[-1])
        except (ValueError, TypeError, IndexError):
            return []

        owner = ""
        repo = ""
        provider_data = dict(pipeline.provider_data or {})
        if provider_data.get("owner") and provider_data.get("repo"):
            owner = str(provider_data["owner"])
            repo = str(provider_data["repo"])
        else:
            source_repo = self._get_param(pipeline, "repo")
            if "/" in source_repo:
                owner, repo = source_repo.split("/", 1)

        if not owner or not repo:
            return []

        arches = await get_build_job_arches(run_id, owner=owner, repo=repo)
        return [f"{pipeline.app_id}/{arch}" for arch in sorted(arches)]

    async def _create_merge_request_note(
        self,
        pipeline: Pipeline,
        note: str,
    ) -> bool:
        if not settings.gitlab_api_token:
            logger.info(
                "Skipping GitLab merge request note because GITLAB_API_TOKEN is not configured",
                pipeline_id=str(pipeline.id),
            )
            return False

        gitlab_base_url = self._get_param(pipeline, "gitlab_base_url")
        project_path = self._get_param(pipeline, "gitlab_project_path")
        merge_request_iid = self._get_param(pipeline, "gitlab_merge_request_iid")

        if not gitlab_base_url or not project_path or not merge_request_iid:
            logger.warning(
                "Skipping GitLab merge request note because pipeline params are incomplete",
                pipeline_id=str(pipeline.id),
                has_gitlab_base_url=bool(gitlab_base_url),
                has_project_path=bool(project_path),
                has_merge_request_iid=bool(merge_request_iid),
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
                pipeline_id=str(pipeline.id),
                endpoint=endpoint,
                status_code=error.response.status_code,
                response_text=error.response.text[:400],
            )
        except httpx.HTTPError as error:
            logger.warning(
                "GitLab merge request note request failed",
                pipeline_id=str(pipeline.id),
                endpoint=endpoint,
                error=str(error),
            )

        return False

    async def _create_issue(
        self,
        pipeline: Pipeline,
        title: str,
        description: str,
    ) -> tuple[str, int] | None:
        if not settings.gitlab_api_token:
            return None

        gitlab_base_url = self._get_param(pipeline, "gitlab_base_url")
        project_path = self._get_param(pipeline, "gitlab_project_path")

        if not gitlab_base_url or not project_path:
            return None

        endpoint = (
            f"{gitlab_base_url}/api/v4/projects/{quote(project_path, safe='')}/issues"
        )

        try:
            async with httpx.AsyncClient(timeout=10.0) as client:
                response = await client.post(
                    endpoint,
                    headers={"PRIVATE-TOKEN": settings.gitlab_api_token},
                    data={"title": title, "description": description},
                )
                response.raise_for_status()
                payload = response.json()

            issue_url = payload.get("web_url", "")
            issue_iid = payload.get("iid")
            if issue_url and issue_iid is not None:
                return issue_url, int(issue_iid)
        except (httpx.HTTPError, ValueError, TypeError):
            logger.warning(
                "GitLab issue creation failed",
                pipeline_id=str(pipeline.id),
                endpoint=endpoint,
            )

        return None

    async def _create_issue_note(
        self,
        pipeline: Pipeline,
        issue_iid: str,
        note: str,
    ) -> bool:
        if not settings.gitlab_api_token:
            return False

        gitlab_base_url = self._get_param(pipeline, "gitlab_base_url")
        project_path = self._get_param(pipeline, "gitlab_project_path")
        if not gitlab_base_url or not project_path or not issue_iid:
            return False

        endpoint = (
            f"{gitlab_base_url}/api/v4/projects/{quote(project_path, safe='')}/"
            f"issues/{issue_iid}/notes"
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
        except httpx.HTTPError:
            logger.warning(
                "GitLab issue note creation failed",
                pipeline_id=str(pipeline.id),
                endpoint=endpoint,
            )
            return False

    async def _close_issue(self, pipeline: Pipeline, issue_iid: str) -> bool:
        if not settings.gitlab_api_token:
            return False

        gitlab_base_url = self._get_param(pipeline, "gitlab_base_url")
        project_path = self._get_param(pipeline, "gitlab_project_path")
        if not gitlab_base_url or not project_path or not issue_iid:
            return False

        endpoint = (
            f"{gitlab_base_url}/api/v4/projects/{quote(project_path, safe='')}/"
            f"issues/{issue_iid}"
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
        except httpx.HTTPError:
            logger.warning(
                "GitLab issue close failed",
                pipeline_id=str(pipeline.id),
                endpoint=endpoint,
            )
            return False

    def _build_failure_issue_title(self, pipeline: Pipeline) -> str:
        return f"Stable build failed for {pipeline.app_id}"

    def _build_failure_issue_body(self, pipeline: Pipeline) -> str:
        source_sha = self._get_param(pipeline, "gitlab_source_sha") or self._get_param(
            pipeline, "sha"
        )
        ref = self._get_param(pipeline, "ref") or f"refs/heads/{self._get_target_branch(pipeline)}"
        github_repo = self._get_param(pipeline, "repo")
        source_repository = ""

        gitlab_base_url = self._get_param(pipeline, "gitlab_base_url")
        project_path = self._get_param(pipeline, "gitlab_project_path")
        if gitlab_base_url and project_path:
            source_repository = f"{gitlab_base_url}/{project_path}.git"

        lines = [
            f"The stable build pipeline for `{pipeline.app_id}` failed.",
            "",
            f"Commit SHA: {source_sha}",
            f"Ref: {ref}",
        ]

        if pipeline.log_url:
            lines.append(f"Build log: {pipeline.log_url}")
        if github_repo:
            lines.append(f"GitHub repository: {github_repo}")
        if source_repository:
            lines.append(f"Source repository: {source_repository}")

        lines.extend(
            [
                "",
                "Please investigate this failure.",
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

        retry_issue_iid = self._get_param(pipeline, "retry_from_gitlab_issue_iid")
        log_url = pipeline.log_url or ""

        if status == "failure":
            if retry_issue_iid:
                note = (
                    f"❌ Stable retry failed.\n\nBuild log: {log_url}\n\n"
                    "Please investigate and comment `bot, retry` again if another retry is needed."
                )
                await self._create_issue_note(pipeline, retry_issue_iid, note)
            else:
                await self._create_issue(
                    pipeline,
                    self._build_failure_issue_title(pipeline),
                    self._build_failure_issue_body(pipeline),
                )
        elif status == "success" and retry_issue_iid:
            note = (
                f"✅ Stable retry succeeded.\n\nBuild log: {log_url}"
                if log_url
                else "✅ Stable retry succeeded."
            )
            await self._create_issue_note(pipeline, retry_issue_iid, note)
            await self._close_issue(pipeline, retry_issue_iid)
        elif status == "cancelled" and retry_issue_iid:
            note = (
                f"⏹️ Stable retry was cancelled.\n\nBuild log: {log_url}"
                if log_url
                else "⏹️ Stable retry was cancelled."
            )
            await self._create_issue_note(pipeline, retry_issue_iid, note)

    async def _update_commit_status(
        self,
        pipeline: Pipeline,
        state: str,
        description: str,
        target_url: str | None = None,
    ) -> bool:
        if not settings.gitlab_api_token:
            logger.info(
                "Skipping GitLab commit status update because GITLAB_API_TOKEN is not configured",
                pipeline_id=str(pipeline.id),
            )
            return False

        gitlab_base_url = self._get_param(pipeline, "gitlab_base_url")
        project_path = self._get_param(pipeline, "gitlab_project_path")
        source_sha = self._get_param(pipeline, "gitlab_source_sha")
        source_branch = self._get_param(pipeline, "gitlab_source_branch")

        if not gitlab_base_url or not project_path or not source_sha:
            logger.warning(
                "Skipping GitLab commit status update because pipeline params are incomplete",
                pipeline_id=str(pipeline.id),
                has_gitlab_base_url=bool(gitlab_base_url),
                has_project_path=bool(project_path),
                has_source_sha=bool(source_sha),
            )
            return False

        endpoint = (
            f"{gitlab_base_url}/api/v4/projects/{quote(project_path, safe='')}/"
            f"statuses/{source_sha}"
        )

        payload = {
            "state": state,
            "name": self._get_status_name(pipeline),
            "description": description,
        }

        if source_branch:
            payload["ref"] = source_branch

        if target_url:
            payload["target_url"] = target_url

        try:
            async with httpx.AsyncClient(timeout=10.0) as client:
                response = await client.post(
                    endpoint,
                    headers={"PRIVATE-TOKEN": settings.gitlab_api_token},
                    data=payload,
                )
                response.raise_for_status()
            return True
        except httpx.HTTPStatusError as error:
            logger.warning(
                "GitLab commit status update failed",
                pipeline_id=str(pipeline.id),
                endpoint=endpoint,
                status_code=error.response.status_code,
                response_text=error.response.text[:400],
            )
        except httpx.HTTPError as error:
            logger.warning(
                "GitLab commit status request failed",
                pipeline_id=str(pipeline.id),
                endpoint=endpoint,
                error=str(error),
            )

        return False

    async def handle_build_started(
        self,
        pipeline: Pipeline,
        log_url: str,
    ) -> None:
        target_label = self._get_target_label(pipeline)
        component_targets = await self._get_component_targets(pipeline)
        targets_suffix = (
            f"\n\nTargets: {', '.join(component_targets)}." if component_targets else ""
        )
        await self._update_commit_status(
            pipeline,
            state="running",
            description=f"Workflow running for {target_label} on GitHub Actions",
            target_url=log_url,
        )
        await self._create_merge_request_note(
            pipeline,
            f"🚧 [Build for {target_label} started]({log_url}).{targets_suffix}",
        )

    async def handle_build_completion(
        self,
        pipeline: Pipeline,
        status: str,
    ) -> None:
        log_url = pipeline.log_url or ""
        target_label = self._get_target_label(pipeline)
        component_targets = await self._get_component_targets(pipeline)
        targets_suffix = (
            f"\n\nTargets: {', '.join(component_targets)}." if component_targets else ""
        )

        if status == "success":
            state = "success"
            description = f"Workflow succeeded for {target_label} on GitHub Actions"
            note = (
                f"✅ [Build for {target_label} succeeded]({log_url}).{targets_suffix}"
                if log_url
                else f"✅ Build for {target_label} succeeded.{targets_suffix}"
            )
        elif status == "cancelled":
            state = "canceled"
            description = f"Workflow cancelled for {target_label} on GitHub Actions"
            note = (
                f"⏹️ [Build for {target_label} was cancelled]({log_url}).{targets_suffix}"
                if log_url
                else f"⏹️ Build for {target_label} was cancelled.{targets_suffix}"
            )
        else:
            state = "failed"
            description = f"Workflow failed for {target_label} on GitHub Actions"
            note = (
                f"❌ [Build for {target_label} failed]({log_url}).{targets_suffix}"
                if log_url
                else f"❌ Build for {target_label} failed.{targets_suffix}"
            )

        await self._update_commit_status(
            pipeline,
            state=state,
            description=description,
            target_url=log_url or None,
        )
        await self._create_merge_request_note(pipeline, note)
        await self._handle_stable_issue_lifecycle(pipeline, status)