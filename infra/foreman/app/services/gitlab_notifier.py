from urllib.parse import quote

import httpx
import structlog

from app.config import settings
from app.models import Pipeline

logger = structlog.get_logger(__name__)


class GitLabNotifier:
    status_name = "github-actions/CI"

    @staticmethod
    def _get_param(pipeline: Pipeline, key: str) -> str:
        value = (pipeline.params or {}).get(key, "")
        return value if isinstance(value, str) else ""

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
            "name": self.status_name,
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
        await self._update_commit_status(
            pipeline,
            state="running",
            description="Workflow running on GitHub Actions",
            target_url=log_url,
        )
        await self._create_merge_request_note(
            pipeline,
            f"🚧 [Test build started]({log_url}).",
        )

    async def handle_build_completion(
        self,
        pipeline: Pipeline,
        status: str,
    ) -> None:
        log_url = pipeline.log_url or ""

        if status == "success":
            state = "success"
            description = "Workflow succeeded on GitHub Actions"
            note = (
                f"✅ [Test build succeeded]({log_url})."
                if log_url
                else "✅ Test build succeeded."
            )
        elif status == "cancelled":
            state = "canceled"
            description = "Workflow cancelled on GitHub Actions"
            note = (
                f"⏹️ [Test build was cancelled]({log_url})."
                if log_url
                else "⏹️ Test build was cancelled."
            )
        else:
            state = "failed"
            description = "Workflow failed on GitHub Actions"
            note = (
                f"❌ [Test build failed]({log_url})."
                if log_url
                else "❌ Test build failed."
            )

        await self._update_commit_status(
            pipeline,
            state=state,
            description=description,
            target_url=log_url or None,
        )
        await self._create_merge_request_note(pipeline, note)