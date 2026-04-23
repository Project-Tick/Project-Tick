from typing import Any

import structlog
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from app.models import Pipeline, PipelineStatus, PipelineTrigger
from app.schemas.pipelines import (
    PipelineResponse,
    PipelineSummary,
    PipelineType,
)

logger = structlog.get_logger(__name__)


class PipelineService:
    def __init__(self):
        pass

    async def list_pipelines_with_filters(
        self,
        db: AsyncSession,
        app_id: str | None = None,
        pipeline_type: PipelineType = PipelineType.BUILD,
        status: PipelineStatus | None = None,
        triggered_by: PipelineTrigger | None = None,
        target_repo: str | None = None,
        limit: int = 10,
    ) -> list[Pipeline]:
        limit = min(max(1, limit), 100)

        stmt = select(Pipeline).order_by(Pipeline.created_at.desc())

        if app_id:
            stmt = stmt.where(Pipeline.app_id.startswith(app_id))

        if status:
            stmt = stmt.where(Pipeline.status == status)

        if triggered_by:
            stmt = stmt.where(Pipeline.triggered_by == triggered_by)

        if target_repo:
            stmt = stmt.where(Pipeline.flat_manager_repo == target_repo)

        stmt = stmt.limit(limit)
        result = await db.execute(stmt)
        pipelines = list(result.scalars().all())

        return pipelines

    def pipeline_to_summary(self, pipeline: Pipeline) -> PipelineSummary:
        return PipelineSummary(
            id=str(pipeline.id),
            app_id=pipeline.app_id,
            type=PipelineType.BUILD,
            status=pipeline.status,
            repo=str(pipeline.flat_manager_repo)
            if pipeline.flat_manager_repo is not None
            else None,
            triggered_by=pipeline.triggered_by,
            build_id=pipeline.build_id,
            created_at=pipeline.created_at,
            started_at=pipeline.started_at,
            finished_at=pipeline.finished_at,
        )

    def pipeline_to_response(self, pipeline: Pipeline) -> PipelineResponse:
        return PipelineResponse(
            id=str(pipeline.id),
            app_id=pipeline.app_id,
            status=pipeline.status,
            repo=str(pipeline.flat_manager_repo)
            if pipeline.flat_manager_repo is not None
            else None,
            params=pipeline.params,
            triggered_by=pipeline.triggered_by,
            log_url=pipeline.log_url,
            build_id=pipeline.build_id,
            total_cost=pipeline.total_cost,
            created_at=pipeline.created_at,
            started_at=pipeline.started_at,
            finished_at=pipeline.finished_at,
        )

    def validate_status(self, status: Any) -> PipelineStatus:
        try:
            return PipelineStatus(status)
        except ValueError:
            valid_values = [s.value for s in PipelineStatus]
            raise ValueError(
                f"Invalid status value: {status}. Valid values are: {valid_values}"
            )

    def validate_trigger_filter(self, triggered_by: Any) -> PipelineTrigger:
        try:
            return PipelineTrigger(triggered_by)
        except ValueError:
            valid_values = [t.value for t in PipelineTrigger]
            raise ValueError(
                f"Invalid triggered_by value: {triggered_by}. Valid values are: {valid_values}"
            )

    async def trigger_manual_pipeline(
        self, app_id: str, params: dict[str, Any]
    ) -> dict[str, Any]:
        from app.pipelines import BuildPipeline

        build_pipeline = BuildPipeline()

        pipeline = await build_pipeline.create_pipeline(
            app_id=app_id,
            params=params,
            webhook_event_id=None,
        )

        from app.database import get_db

        async with get_db() as db:
            db_pipeline = await db.get(Pipeline, pipeline.id)
            if db_pipeline is None:
                raise ValueError(f"Pipeline {pipeline.id} not found")
            db_pipeline.triggered_by = PipelineTrigger.MANUAL
            await db.flush()
            pipeline = db_pipeline

        pipeline = await build_pipeline.prepare_pipeline_for_start(pipeline.id)
        await build_pipeline.supersede_conflicting_test_pipelines(pipeline.id)
        should_queue = await build_pipeline.should_queue_test_build(pipeline.id)

        if not should_queue:
            pipeline = await build_pipeline.start_pipeline(
                pipeline_id=pipeline.id,
            )

        return {
            "status": "created",
            "pipeline_id": str(pipeline.id),
            "app_id": pipeline.app_id,
            "pipeline_status": pipeline.status.value,
        }
