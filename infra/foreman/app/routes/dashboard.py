from datetime import datetime, timezone
from pathlib import Path
from typing import Any
from urllib.parse import urlparse

from fastapi import APIRouter, Request
from fastapi.responses import HTMLResponse, RedirectResponse
from fastapi.templating import Jinja2Templates
from sqlalchemy import case, func, select

from app.config import settings
from app.database import get_db
from app.models import ChangeEvent, Pipeline, PipelineStatus
from app.pipelines.build import resolve_pipeline_target_repo
from app.services.change_events import get_change_targets, get_recent_change_events

dashboard_router = APIRouter(tags=["dashboard"])

templates = Jinja2Templates(directory=Path(__file__).parent.parent / "templates")


def _get_string(value: Any) -> str:
    return value if isinstance(value, str) else ""


def _normalize_ref(ref: str) -> str:
    if ref.startswith("refs/heads/"):
        return ref.removeprefix("refs/heads/")
    if ref.startswith("refs/tags/"):
        return ref.removeprefix("refs/tags/")
    if ref.startswith("refs/merge-requests/") and ref.endswith("/head"):
        parts = ref.split("/")
        if len(parts) >= 4:
            return f"mr-{parts[2]}"
    return ref


def _get_source_repository_url(pipeline: Pipeline) -> str:
    params = dict(pipeline.params or {})
    dispatch_inputs = params.get("dispatch_inputs")
    if isinstance(dispatch_inputs, dict):
        source_repository = _get_string(dispatch_inputs.get("source-repository"))
        if source_repository:
            return source_repository

    git_http_url = _get_string(params.get("gitlab_source_repository"))
    if git_http_url:
        return git_http_url

    git_repo = _get_string(params.get("repo"))
    if git_repo:
        return f"https://github.com/{git_repo}"

    return ""


def _get_source_ref(pipeline: Pipeline) -> str:
    params = dict(pipeline.params or {})
    dispatch_inputs = params.get("dispatch_inputs")
    if isinstance(dispatch_inputs, dict):
        source_ref = _get_string(dispatch_inputs.get("source-ref"))
        if source_ref:
            return source_ref

    for key in ("gitlab_source_branch", "pr_target_branch", "gitlab_target_branch", "ref"):
        value = _get_string(params.get(key))
        if value:
            return value

    return ""


def get_pipeline_target_repo(pipeline: Pipeline) -> str:
    return pipeline.flat_manager_repo or resolve_pipeline_target_repo(pipeline)


def _get_gitlab_merge_request_url(pipeline: Pipeline) -> str:
    params = dict(pipeline.params or {})
    gitlab_base_url = _get_string(params.get("gitlab_base_url")).rstrip("/")
    gitlab_project_path = _get_string(params.get("gitlab_project_path"))
    merge_request_iid = _get_string(params.get("gitlab_merge_request_iid"))

    if gitlab_base_url and gitlab_project_path and merge_request_iid:
        return f"{gitlab_base_url}/{gitlab_project_path}/-/merge_requests/{merge_request_iid}"

    return ""


def get_pipeline_request_label(pipeline: Pipeline) -> str:
    params = dict(pipeline.params or {})
    merge_request_iid = _get_string(params.get("gitlab_merge_request_iid"))
    pr_number = _get_string(params.get("pr_number"))

    if merge_request_iid:
        return f"MR !{merge_request_iid}"
    if pr_number:
        return f"PR #{pr_number}"

    source_ref = _normalize_ref(_get_source_ref(pipeline))
    if source_ref and source_ref != pipeline.app_id:
        return source_ref

    return pipeline.app_id


def get_pipeline_request_url(pipeline: Pipeline) -> str:
    params = dict(pipeline.params or {})
    git_repo = _get_string(params.get("repo"))
    pr_number = _get_string(params.get("pr_number"))

    gitlab_merge_request_url = _get_gitlab_merge_request_url(pipeline)
    if gitlab_merge_request_url:
        return gitlab_merge_request_url
    if git_repo and pr_number:
        return f"https://github.com/{git_repo}/pull/{pr_number}"

    return ""


def get_pipeline_source_label(pipeline: Pipeline) -> str:
    source_repository_url = _get_source_repository_url(pipeline)
    source_ref = _normalize_ref(_get_source_ref(pipeline))

    if source_repository_url:
        parsed = urlparse(source_repository_url)
        repository_label = parsed.path.removeprefix("/").removesuffix(".git") or source_repository_url
        return f"{repository_label}@{source_ref}" if source_ref else repository_label

    gitlab_project_path = _get_string(dict(pipeline.params or {}).get("gitlab_project_path"))
    if gitlab_project_path:
        return f"{gitlab_project_path}@{source_ref}" if source_ref else gitlab_project_path

    return "-"


def get_pipeline_source_url(pipeline: Pipeline) -> str:
    params = dict(pipeline.params or {})
    git_repo = _get_string(params.get("repo"))
    sha = _get_string(params.get("sha"))

    gitlab_merge_request_url = _get_gitlab_merge_request_url(pipeline)
    if gitlab_merge_request_url:
        return gitlab_merge_request_url

    source_repository_url = _get_source_repository_url(pipeline)
    if source_repository_url:
        return source_repository_url.removesuffix(".git")

    if git_repo and sha:
        return f"https://github.com/{git_repo}/commit/{sha}"

    return ""


def _humanize_workflow_label(value: str) -> str:
    normalized = value.strip()
    if not normalized:
        return "Build"

    if normalized.endswith((".yml", ".yaml")):
        normalized = normalized.rsplit(".", maxsplit=1)[0]

    if normalized.lower() == "ci":
        return "CI"

    words = normalized.replace("-", " ").replace("_", " ").split()
    acronyms = {"ci", "mr", "pr", "qa", "api"}
    return " ".join(
        word.upper() if word.lower() in acronyms else word.capitalize()
        for word in words
    )


def get_pipeline_workflow_identity(pipeline: Pipeline) -> tuple[str, str]:
    params = dict(pipeline.params or {})

    workflow_name = _get_string(params.get("workflow_name")).strip()
    if workflow_name:
        return workflow_name.lower(), workflow_name

    for key in ("dispatch_workflow_id", "workflow_id"):
        workflow_id = _get_string(params.get(key)).strip()
        if workflow_id:
            return workflow_id.lower(), _humanize_workflow_label(workflow_id)

    provider_data = dict(pipeline.provider_data or {})
    workflow_id = _get_string(provider_data.get("workflow_id")).strip()
    if workflow_id:
        return workflow_id.lower(), _humanize_workflow_label(workflow_id)

    return "build", "Build"


def get_pipeline_status_display(pipeline: Pipeline) -> dict[str, str | None]:
    status_value = pipeline.status.value
    status_url = pipeline.log_url or None
    badge_class = f"badge-{status_value}"
    label = status_value

    if status_value in {"committed", "publishing"}:
        badge_class = f"badge-{status_value}-{get_pipeline_target_repo(pipeline)}"
    elif status_value == "succeeded":
        badge_class = "badge-succeeded"
    elif status_value == "superseded":
        label = "replaced"

    return {
        "label": label,
        "badge_class": badge_class,
        "url": status_url,
    }


def get_pipeline_build_type(pipeline: Pipeline) -> str:
    params = dict(pipeline.params or {})
    return _get_string(params.get("build_type"))


def get_pipeline_identity_bits(pipeline: Pipeline) -> list[str]:
    identity = [pipeline.app_id]
    build_type = get_pipeline_build_type(pipeline)
    if build_type:
        identity.append(build_type)
    identity.append(f"#{str(pipeline.id).split('-', maxsplit=1)[0]}")
    return identity


def format_time(dt: datetime | None) -> str:
    if dt is None:
        return "-"

    now = datetime.now(timezone.utc)
    if dt.tzinfo is None:
        dt = dt.replace(tzinfo=timezone.utc)

    diff = now - dt
    seconds = diff.total_seconds()

    if seconds < 60:
        return "just now"
    elif seconds < 3600:
        minutes = int(seconds / 60)
        return f"{minutes} minute{'s' if minutes != 1 else ''} ago"
    elif seconds < 86400:
        hours = int(seconds / 3600)
        return f"{hours} hour{'s' if hours != 1 else ''} ago"
    elif seconds < 604800:
        days = int(seconds / 86400)
        return f"{days} day{'s' if days != 1 else ''} ago"
    else:
        return dt.strftime("%Y-%m-%d %H:%M")


def format_duration(started_at: datetime | None, finished_at: datetime | None) -> str:
    if started_at is None:
        return "-"
    if finished_at is None:
        return "In progress"

    diff = finished_at - started_at
    seconds = int(diff.total_seconds())

    if seconds < 60:
        return f"{seconds}s"
    elif seconds < 3600:
        minutes = seconds // 60
        secs = seconds % 60
        return f"{minutes}m {secs}s"
    else:
        hours = seconds // 3600
        minutes = (seconds % 3600) // 60
        return f"{hours}h {minutes}m"


templates.env.globals["format_time"] = format_time  # ty: ignore[invalid-assignment]
templates.env.globals["format_duration"] = format_duration  # ty: ignore[invalid-assignment]
templates.env.globals["get_pipeline_request_label"] = get_pipeline_request_label  # ty: ignore[invalid-assignment]
templates.env.globals["get_pipeline_request_url"] = get_pipeline_request_url  # ty: ignore[invalid-assignment]
templates.env.globals["get_pipeline_source_label"] = get_pipeline_source_label  # ty: ignore[invalid-assignment]
templates.env.globals["get_pipeline_source_url"] = get_pipeline_source_url  # ty: ignore[invalid-assignment]
templates.env.globals["get_pipeline_target_repo"] = get_pipeline_target_repo  # ty: ignore[invalid-assignment]
templates.env.globals["get_pipeline_status_display"] = get_pipeline_status_display  # ty: ignore[invalid-assignment]
templates.env.globals["get_pipeline_identity_bits"] = get_pipeline_identity_bits  # ty: ignore[invalid-assignment]
templates.env.globals["get_change_targets"] = get_change_targets  # ty: ignore[invalid-assignment]


def group_pipelines(
    pipelines: list[Pipeline],
) -> dict[str, list[Pipeline]]:
    in_progress: list[Pipeline] = []
    superseded: list[Pipeline] = []
    completed: list[Pipeline] = []

    in_progress_statuses = {
        PipelineStatus.PENDING,
        PipelineStatus.RUNNING,
    }

    for p in pipelines:
        if p.status in in_progress_statuses:
            in_progress.append(p)
        elif p.status == PipelineStatus.SUPERSEDED:
            superseded.append(p)
        else:
            completed.append(p)

    return {
        "in_progress": in_progress,
        "superseded": superseded,
        "completed": completed,
    }


def group_pipelines_by_workflow(
    pipelines: list[Pipeline],
) -> list[dict[str, Any]]:
    ordered_groups: list[dict[str, Any]] = []
    groups_by_key: dict[str, dict[str, Any]] = {}

    for pipeline in pipelines:
        workflow_key, workflow_label = get_pipeline_workflow_identity(pipeline)
        group = groups_by_key.get(workflow_key)
        if group is None:
            group = {
                "key": workflow_key,
                "label": workflow_label,
                "pipelines": [],
            }
            groups_by_key[workflow_key] = group
            ordered_groups.append(group)

        group["pipelines"].append(pipeline)

    return [
        {
            "key": group["key"],
            "label": group["label"],
            "total": len(group["pipelines"]),
            "grouped_pipelines": group_pipelines(group["pipelines"]),
        }
        for group in ordered_groups
    ]


async def get_recent_pipelines(
    status: str | None = None,
    app_id: str | None = None,
    target: str | None = None,
    date_from: str | None = None,
    date_to: str | None = None,
) -> list[Pipeline]:
    limit = 25 if app_id and not (date_from or date_to) else 50

    async with get_db(use_replica=True) as db:
        query = (
            select(Pipeline)
            .order_by(
                case(
                    (
                        Pipeline.status == PipelineStatus.RUNNING,
                        0,
                    ),
                    else_=1,
                ),
                func.coalesce(Pipeline.finished_at, Pipeline.created_at).desc(),
            )
            .limit(limit)
        )

        if status:
            try:
                pipeline_status = PipelineStatus(status)
                query = query.where(Pipeline.status == pipeline_status)
            except ValueError:
                pass

        if app_id:
            query = query.where(Pipeline.app_id.ilike(f"%{app_id}%"))

        if target:
            query = query.where(Pipeline.flat_manager_repo == target)

        if date_from:
            try:
                from_date = datetime.strptime(date_from, "%Y-%m-%dT%H:%M")
                query = query.where(Pipeline.started_at >= from_date)
            except ValueError:
                pass

        if date_to:
            try:
                to_date = datetime.strptime(date_to, "%Y-%m-%dT%H:%M")
                query = query.where(Pipeline.started_at <= to_date)
            except ValueError:
                pass

        result = await db.execute(query)
        return list(result.scalars().all())


async def get_app_builds(app_id: str, limit: int = 5) -> list[Pipeline]:
    async with get_db(use_replica=True) as db:
        query = (
            select(Pipeline)
            .where(Pipeline.app_id == app_id)
            .order_by(Pipeline.created_at.desc())
            .limit(limit)
        )
        result = await db.execute(query)
        return list(result.scalars().all())


def group_pipelines_by_target(pipelines: list[Pipeline]) -> dict[str, list[Pipeline]]:
    grouped = {"stable": [], "beta": [], "test": []}

    for pipeline in pipelines:
        grouped.setdefault(get_pipeline_target_repo(pipeline), []).append(pipeline)

    return grouped


async def get_recent_changes(limit: int | None = None) -> list[ChangeEvent]:
    return await get_recent_change_events(limit=limit)


@dashboard_router.get("/", response_class=HTMLResponse)
async def dashboard(
    request: Request,
    status: str | None = None,
    app_id: str | None = None,
    target: str | None = None,
    date_from: str | None = None,
    date_to: str | None = None,
):
    pipelines = await get_recent_pipelines(
        status=status,
        app_id=app_id,
        target=target,
        date_from=date_from,
        date_to=date_to,
    )

    grouped = group_pipelines(pipelines)
    workflow_groups = group_pipelines_by_workflow(pipelines)

    return templates.TemplateResponse(
        request=request,
        name="dashboard.html",
        context={
            "grouped_pipelines": grouped,
            "workflow_groups": workflow_groups,
            "statuses": [
                PipelineStatus.PENDING,
                PipelineStatus.RUNNING,
                PipelineStatus.SUPERSEDED,
                PipelineStatus.SUCCEEDED,
                PipelineStatus.CANCELLED,
                PipelineStatus.FAILED,
            ],
            "filters": {
                "status": status or "",
                "app_id": app_id or "",
                "target": target or "",
                "date_from": date_from or "",
                "date_to": date_to or "",
            },
        },
    )


@dashboard_router.get("/reproducible", include_in_schema=False)
async def redirect_reproducible():
    return RedirectResponse(url="/", status_code=302)


@dashboard_router.get("/changes", response_class=HTMLResponse)
async def changes_dashboard(request: Request):
    changes = await get_recent_changes(limit=settings.changes_history_limit)
    latest_change = changes[0] if changes else None

    return templates.TemplateResponse(
        request=request,
        name="changes.html",
        context={
            "changes": changes,
            "latest_change": latest_change,
        },
    )


@dashboard_router.get("/api/htmx/builds", response_class=HTMLResponse)
async def builds_table(
    request: Request,
    status: str | None = None,
    app_id: str | None = None,
    target: str | None = None,
    date_from: str | None = None,
    date_to: str | None = None,
):
    pipelines = await get_recent_pipelines(
        status=status,
        app_id=app_id,
        target=target,
        date_from=date_from,
        date_to=date_to,
    )

    grouped = group_pipelines(pipelines)
    workflow_groups = group_pipelines_by_workflow(pipelines)

    return templates.TemplateResponse(
        request=request,
        name="partials/builds.html",
        context={
            "grouped_pipelines": grouped,
            "workflow_groups": workflow_groups,
        },
    )


@dashboard_router.get("/status/{app_id}", response_class=HTMLResponse)
async def app_status(request: Request, app_id: str):
    recent_builds = await get_app_builds(app_id, limit=25)
    grouped_builds = group_pipelines_by_target(recent_builds)
    display_name = get_pipeline_request_label(recent_builds[0]) if recent_builds else app_id

    chart_data = [
        {
            "date": p.started_at.strftime("%Y-%m-%d %H:%M"),
            "duration": round((p.finished_at - p.started_at).total_seconds() / 60, 1),
        }
        for p in reversed(recent_builds)
        if p.started_at and p.finished_at
    ]

    return templates.TemplateResponse(
        request=request,
        name="app_status.html",
        context={
            "app_id": app_id,
            "display_name": display_name,
            "recent_builds": recent_builds,
            "stable_builds": grouped_builds.get("stable", []),
            "beta_builds": grouped_builds.get("beta", []),
            "test_builds": grouped_builds.get("test", []),
            "chart_data": chart_data,
        },
    )
