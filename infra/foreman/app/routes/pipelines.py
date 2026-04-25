import uuid
from collections.abc import Awaitable, Callable
from typing import Annotated, Any

import structlog
from fastapi import APIRouter, Depends, HTTPException, Response
from fastapi import status as http_status
from fastapi.security import HTTPAuthorizationCredentials, HTTPBearer

from app.config import settings
from app.database import get_db
from app.models import Pipeline, PipelineStatus, PipelineTrigger
from app.pipelines import BuildPipeline
from app.schemas.pipelines import (
    ExternalPipelineRegistrationRequest,
    ExternalPipelineRegistrationResponse,
    PipelineResponse,
    PipelineSummary,
    PipelineTriggerRequest,
    PipelineType,
)
from app.services import pipeline_service
from app.services.workflow_dispatch import (
    WorkflowEventContext,
    create_pipelines_for_rules,
    get_matching_workflow_rules,
)
from app.services.schedule_dispatch import dispatch_named_schedule

logger = structlog.get_logger(__name__)
pipelines_router = APIRouter(prefix="/api", tags=["pipelines"])
security = HTTPBearer()


async def get_verified_build_pipeline(
    pipeline_id: uuid.UUID,
    credentials: HTTPAuthorizationCredentials = Depends(security),
) -> BuildPipeline:
    build_pipeline = BuildPipeline()

    try:
        await build_pipeline.verify_callback_token(
            pipeline_id=pipeline_id,
            token=credentials.credentials,
        )
    except ValueError as e:
        if "not found" in str(e):
            raise HTTPException(
                status_code=http_status.HTTP_404_NOT_FOUND,
                detail=str(e),
            )
        raise HTTPException(
            status_code=http_status.HTTP_401_UNAUTHORIZED,
            detail="Invalid callback token",
        )

    return build_pipeline


VerifiedBuildPipeline = Annotated[BuildPipeline, Depends(get_verified_build_pipeline)]


async def execute_callback_handler(
    handler: Callable[..., Awaitable[tuple[Any, dict[str, Any]]]],
    pipeline_id: uuid.UUID,
    data: dict[str, Any],
    conflict_messages: list[str] | None = None,
) -> dict[str, Any]:
    try:
        _, updates = await handler(pipeline_id=pipeline_id, callback_data=data)
    except ValueError as e:
        error_str = str(e)
        if conflict_messages:
            for msg in conflict_messages:
                if msg in error_str:
                    raise HTTPException(
                        status_code=http_status.HTTP_409_CONFLICT,
                        detail=msg,
                    )
        raise HTTPException(
            status_code=http_status.HTTP_400_BAD_REQUEST,
            detail=error_str,
        )

    return {
        "status": "ok",
        "pipeline_id": str(pipeline_id),
        **updates,
    }


async def verify_token(
    credentials: HTTPAuthorizationCredentials = Depends(security),
):
    if credentials.credentials != settings.admin_token:
        raise HTTPException(
            status_code=http_status.HTTP_401_UNAUTHORIZED,
            detail="Invalid API token",
        )
    return credentials.credentials


@pipelines_router.post(
    "/pipelines",
    response_model=dict[str, Any],
    status_code=http_status.HTTP_201_CREATED,
)
async def trigger_pipeline(
    data: PipelineTriggerRequest,
    token: str = Depends(verify_token),
):
    try:
        result = await pipeline_service.trigger_manual_pipeline(
            app_id=data.app_id,
            params=data.params,
        )
        return result
    except ValueError as e:
        if "not found" in str(e).lower():
            raise HTTPException(
                status_code=http_status.HTTP_404_NOT_FOUND,
                detail=str(e),
            )
        raise


@pipelines_router.post(
    "/pipelines/external",
    response_model=ExternalPipelineRegistrationResponse,
    status_code=http_status.HTTP_201_CREATED,
)
async def register_external_pipeline(
    data: ExternalPipelineRegistrationRequest,
    token: str = Depends(verify_token),
):
    build_pipeline = BuildPipeline()

    try:
        pipeline = await build_pipeline.register_external_pipeline(
            app_id=data.app_id,
            params=data.params,
        )
    except ValueError as e:
        if "not found" in str(e).lower():
            raise HTTPException(
                status_code=http_status.HTTP_404_NOT_FOUND,
                detail=str(e),
            )
        raise HTTPException(
            status_code=http_status.HTTP_400_BAD_REQUEST,
            detail=str(e),
        )

    return {
        "status": "registered",
        "pipeline_id": str(pipeline.id),
        "app_id": pipeline.app_id,
        "pipeline_status": pipeline.status,
        "callback_url": f"{settings.base_url}/api/pipelines/{pipeline.id}/callback",
        "callback_token": pipeline.callback_token,
    }


@pipelines_router.post(
    "/schedules/{schedule_name}/dispatch",
    response_model=dict[str, Any],
    status_code=http_status.HTTP_202_ACCEPTED,
)
async def dispatch_schedule_workflows(
    schedule_name: str,
    token: str = Depends(verify_token),
):
    rules = get_matching_workflow_rules(trigger="schedule", schedule_name=schedule_name)
    if not rules:
        raise HTTPException(
            status_code=http_status.HTTP_404_NOT_FOUND,
            detail=f"No workflow dispatch rules matched schedule '{schedule_name}'.",
        )

    pipeline_ids = await dispatch_named_schedule(schedule_name)

    return {
        "status": "scheduled",
        "schedule_name": schedule_name,
        "pipeline_ids": [str(pipeline_id) for pipeline_id in pipeline_ids],
    }


@pipelines_router.get(
    "/pipelines",
    response_model=list[PipelineSummary],
    status_code=http_status.HTTP_200_OK,
)
async def list_pipelines(
    app_id: str | None = None,
    type: PipelineType = PipelineType.BUILD,
    status: PipelineStatus | str | None = None,
    triggered_by: PipelineTrigger | None = None,
    target_repo: str | None = None,
    limit: int | None = 10,
):
    pipeline_status: PipelineStatus | None = None

    if status:
        try:
            pipeline_status = pipeline_service.validate_status(status)
        except ValueError as e:
            raise HTTPException(
                status_code=http_status.HTTP_400_BAD_REQUEST,
                detail=str(e),
            )

    if triggered_by:
        try:
            triggered_by = pipeline_service.validate_trigger_filter(triggered_by)
        except ValueError as e:
            raise HTTPException(
                status_code=http_status.HTTP_400_BAD_REQUEST,
                detail=str(e),
            )

    async with get_db(use_replica=True) as db:
        pipelines = await pipeline_service.list_pipelines_with_filters(
            db=db,
            app_id=app_id,
            pipeline_type=type,
            status=pipeline_status,
            triggered_by=triggered_by,
            target_repo=target_repo,
            limit=limit or 10,
        )

        return [
            pipeline_service.pipeline_to_summary(pipeline) for pipeline in pipelines
        ]


@pipelines_router.get(
    "/pipelines/{pipeline_id}",
    response_model=PipelineResponse,
    status_code=http_status.HTTP_200_OK,
)
async def get_pipeline(
    pipeline_id: uuid.UUID,
):
    async with get_db(use_replica=True) as db:
        pipeline = await db.get(Pipeline, pipeline_id)
        if not pipeline:
            raise HTTPException(
                status_code=http_status.HTTP_404_NOT_FOUND,
                detail=f"Pipeline {pipeline_id} not found",
            )

        return pipeline_service.pipeline_to_response(pipeline)


@pipelines_router.post(
    "/pipelines/{pipeline_id}/callback/metadata",
    status_code=http_status.HTTP_200_OK,
)
async def pipeline_metadata_callback(
    pipeline_id: uuid.UUID,
    data: dict[str, Any],
    build_pipeline: VerifiedBuildPipeline,
):
    return await execute_callback_handler(
        build_pipeline.handle_metadata_callback,
        pipeline_id,
        data,
    )


@pipelines_router.post(
    "/pipelines/{pipeline_id}/callback/log_url",
    status_code=http_status.HTTP_200_OK,
)
async def pipeline_log_url_callback(
    pipeline_id: uuid.UUID,
    data: dict[str, Any],
    build_pipeline: VerifiedBuildPipeline,
):
    return await execute_callback_handler(
        build_pipeline.handle_log_url_callback,
        pipeline_id,
        data,
        conflict_messages=["Log URL already set"],
    )


@pipelines_router.post(
    "/pipelines/{pipeline_id}/callback/status",
    status_code=http_status.HTTP_200_OK,
)
async def pipeline_status_callback(
    pipeline_id: uuid.UUID,
    data: dict[str, Any],
    build_pipeline: VerifiedBuildPipeline,
):
    return await execute_callback_handler(
        build_pipeline.handle_status_callback,
        pipeline_id,
        data,
        conflict_messages=["Pipeline status already finalized"],
    )


@pipelines_router.post(
    "/pipelines/{pipeline_id}/callback/cost",
    status_code=http_status.HTTP_200_OK,
)
async def pipeline_cost_callback(
    pipeline_id: uuid.UUID,
    data: dict[str, Any],
    build_pipeline: VerifiedBuildPipeline,
):
    return await execute_callback_handler(
        build_pipeline.handle_cost_callback,
        pipeline_id,
        data,
    )


@pipelines_router.post(
    "/pipelines/{pipeline_id}/admin-callback/{callback_kind}",
    status_code=http_status.HTTP_200_OK,
)
async def pipeline_admin_callback(
    pipeline_id: uuid.UUID,
    callback_kind: str,
    data: dict[str, Any],
    token: str = Depends(verify_token),
):
    build_pipeline = BuildPipeline()

    handlers: dict[
        str,
        tuple[Callable[..., Awaitable[tuple[Any, dict[str, Any]]]], list[str] | None],
    ] = {
        "metadata": (build_pipeline.handle_metadata_callback, None),
        "log_url": (build_pipeline.handle_log_url_callback, ["Log URL already set"]),
        "status": (
            build_pipeline.handle_status_callback,
            ["Pipeline status already finalized"],
        ),
        "cost": (build_pipeline.handle_cost_callback, None),
    }

    if callback_kind not in handlers:
        raise HTTPException(
            status_code=http_status.HTTP_404_NOT_FOUND,
            detail=f"Unknown callback type: {callback_kind}",
        )

    handler, conflict_messages = handlers[callback_kind]
    return await execute_callback_handler(
        handler,
        pipeline_id,
        data,
        conflict_messages=conflict_messages,
    )


@pipelines_router.get(
    "/pipelines/{pipeline_id}/log_url",
)
async def redirect_to_log_url(
    pipeline_id: uuid.UUID,
    response: Response,
):
    async with get_db(use_replica=True) as db:
        pipeline = await db.get(Pipeline, pipeline_id)
        if not pipeline:
            raise HTTPException(
                status_code=http_status.HTTP_404_NOT_FOUND,
                detail=f"Pipeline {pipeline_id} not found",
            )

        if not pipeline.log_url:
            response.status_code = http_status.HTTP_202_ACCEPTED
            response.headers["Retry-After"] = "10"
            return {"detail": "Log URL not available yet"}

        response.status_code = http_status.HTTP_307_TEMPORARY_REDIRECT
        response.headers["Location"] = pipeline.log_url
        return {}


@pipelines_router.post(
    "/github-tasks/process",
    status_code=http_status.HTTP_200_OK,
)
async def process_github_tasks(
    token: str = Depends(verify_token),
):
    from app.services.github_task import GitHubTaskService

    async with get_db() as db:
        service = GitHubTaskService()
        processed = await service.process_pending_tasks(db)
        await db.commit()

        return {
            "status": "completed",
            "processed_tasks": processed,
        }


@pipelines_router.post(
    "/github-tasks/cleanup",
    status_code=http_status.HTTP_200_OK,
)
async def cleanup_github_tasks(
    token: str = Depends(verify_token),
    days: int = 7,
):
    from app.services.github_task import GitHubTaskService

    async with get_db() as db:
        service = GitHubTaskService()
        deleted = await service.cleanup_old_tasks(db, days)
        await db.commit()

        return {
            "status": "completed",
            "deleted_tasks": deleted,
        }


@pipelines_router.post(
    "/pipelines/cleanup-stale",
    status_code=http_status.HTTP_200_OK,
)
async def cleanup_stale_pipelines(
    token: str = Depends(verify_token),
    hours: int = 24,
):
    async with get_db() as db:
        from datetime import datetime, timedelta, timezone

        from sqlalchemy import select

        cutoff = datetime.now(tz=timezone.utc) - timedelta(hours=hours)

        query = select(Pipeline).where(
            Pipeline.status == PipelineStatus.RUNNING,
            Pipeline.created_at < cutoff,
        )
        result = await db.execute(query)
        stale_pipelines = list(result.scalars().all())

        cancelled_ids = []
        for pipeline in stale_pipelines:
            pipeline.status = PipelineStatus.CANCELLED
            pipeline.finished_at = datetime.now(tz=timezone.utc)
            cancelled_ids.append(str(pipeline.id))

        if stale_pipelines:
            await db.commit()
            logger.info(
                "Cancelled stale pipelines", count=len(cancelled_ids), ids=cancelled_ids
            )

        return {
            "status": "completed",
            "cancelled_pipelines": len(stale_pipelines),
        }
