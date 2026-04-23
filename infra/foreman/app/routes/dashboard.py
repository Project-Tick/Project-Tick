from datetime import datetime, timezone
from pathlib import Path

from fastapi import APIRouter, Request
from fastapi.responses import HTMLResponse
from fastapi.templating import Jinja2Templates
from sqlalchemy import case, func, select

from app.database import get_db
from app.models import Pipeline, PipelineStatus

dashboard_router = APIRouter(tags=["dashboard"])

templates = Jinja2Templates(directory=Path(__file__).parent.parent / "templates")


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


def group_pipelines(
    pipelines: list[Pipeline],
) -> dict[str, list[Pipeline]]:
    in_progress: list[Pipeline] = []
    completed: list[Pipeline] = []

    in_progress_statuses = {
        PipelineStatus.RUNNING,
        PipelineStatus.SUCCEEDED,
    }

    for p in pipelines:
        if p.status in in_progress_statuses:
            in_progress.append(p)
        else:
            completed.append(p)

    return {
        "in_progress": in_progress,
        "completed": completed,
    }


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

    return templates.TemplateResponse(
        request=request,
        name="dashboard.html",
        context={
            "grouped_pipelines": grouped,
            "statuses": [
                PipelineStatus.RUNNING,
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

    return templates.TemplateResponse(
        request=request,
        name="partials/builds.html",
        context={
            "grouped_pipelines": grouped,
        },
    )


@dashboard_router.get("/status/{app_id}", response_class=HTMLResponse)
async def app_status(request: Request, app_id: str):
    recent_builds = await get_app_builds(app_id, limit=10)

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
            "recent_builds": recent_builds,
            "chart_data": chart_data,
        },
    )
