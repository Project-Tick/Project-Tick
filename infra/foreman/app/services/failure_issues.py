from __future__ import annotations

from urllib.parse import quote

import httpx
import structlog

from app.config import settings
from app.models import Pipeline

logger = structlog.get_logger(__name__)


def _get_string(value: object) -> str:
    return value if isinstance(value, str) else ""


def _get_dispatch_target(pipeline: Pipeline) -> str:
    params = dict(pipeline.params or {})
    owner = _get_string(params.get("dispatch_owner")) or settings.github_org
    repo = _get_string(params.get("dispatch_repo")) or settings.github_ci_repo
    workflow_id = _get_string(params.get("dispatch_workflow_id")) or _get_string(
        params.get("workflow_id")
    )
    if owner and repo and workflow_id:
        return f"{owner}/{repo}:{workflow_id}"
    return workflow_id or "unknown"


def _build_failure_issue_title(pipeline: Pipeline) -> str:
    workflow_name = _get_string(dict(pipeline.params or {}).get("workflow_name"))
    if workflow_name:
        return f"Workflow failed: {pipeline.app_id} ({workflow_name})"
    return f"Workflow failed: {pipeline.app_id}"


def _build_failure_issue_body(pipeline: Pipeline) -> str:
    params = dict(pipeline.params or {})
    dispatch_inputs = params.get("dispatch_inputs")
    source_repository = ""
    source_ref = ""
    source_sha = _get_string(params.get("sha"))
    if isinstance(dispatch_inputs, dict):
        source_repository = _get_string(dispatch_inputs.get("source-repository"))
        source_ref = _get_string(dispatch_inputs.get("source-ref"))
        source_sha = _get_string(dispatch_inputs.get("source-sha")) or source_sha

    lines = [
        f"The workflow pipeline for `{pipeline.app_id}` failed.",
        "",
        f"Pipeline ID: {pipeline.id}",
        f"Workflow target: {_get_dispatch_target(pipeline)}",
        f"Target repo class: {pipeline.flat_manager_repo or 'workflow'}",
    ]

    if source_sha:
        lines.append(f"Commit SHA: {source_sha}")
    if source_ref or _get_string(params.get('ref')):
        lines.append(f"Ref: {source_ref or _get_string(params.get('ref'))}")
    if _get_string(params.get("repo")):
        lines.append(f"GitHub repository: {_get_string(params.get('repo'))}")
    if source_repository:
        lines.append(f"Source repository: {source_repository}")
    if pipeline.log_url:
        lines.append(f"Build log: {pipeline.log_url}")
    if pipeline.started_at:
        lines.append(f"Started at: {pipeline.started_at.isoformat()}")
    if pipeline.finished_at:
        lines.append(f"Finished at: {pipeline.finished_at.isoformat()}")

    lines.extend(
        [
            "",
            "This issue was created automatically by Foreman.",
        ]
    )
    return "\n".join(lines)


class FailureIssueService:
    async def open_failure_issue(self, pipeline: Pipeline, status: str) -> tuple[str, int] | None:
        if status != "failure" or not settings.gitlab_api_token:
            return None

        project_path = settings.failure_issue_gitlab_project.strip()
        gitlab_base_url = (
            settings.failure_issue_gitlab_base_url or settings.gitlab_base_url
        ).rstrip("/")
        if not project_path or not gitlab_base_url:
            return None

        endpoint = (
            f"{gitlab_base_url}/api/v4/projects/{quote(project_path, safe='')}/issues"
        )
        try:
            async with httpx.AsyncClient(timeout=10.0) as client:
                response = await client.post(
                    endpoint,
                    headers={"PRIVATE-TOKEN": settings.gitlab_api_token},
                    data={
                        "title": _build_failure_issue_title(pipeline),
                        "description": _build_failure_issue_body(pipeline),
                    },
                )
                response.raise_for_status()
                payload = response.json()
            issue_url = payload.get("web_url", "")
            issue_iid = payload.get("iid")
            if issue_url and issue_iid is not None:
                return issue_url, int(issue_iid)
        except (httpx.HTTPError, ValueError, TypeError) as error:
            logger.warning(
                "GitLab failure issue creation failed",
                pipeline_id=str(pipeline.id),
                endpoint=endpoint,
                error=str(error),
            )
        return None