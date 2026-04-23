import secrets
import uuid
from datetime import datetime, timezone
from typing import Any, Optional

import structlog
from pydantic import BaseModel
from sqlalchemy import select, text
from sqlalchemy.ext.asyncio import AsyncSession

from app.config import settings
from app.database import get_db
from app.models import Pipeline, PipelineStatus
from app.services import github_actions_service
from app.services.github_actions import GitHubActionsService
from app.services.github_notifier import GitHubNotifier

logger = structlog.get_logger(__name__)


class CallbackData(BaseModel):
    status: Optional[str] = None
    log_url: Optional[str] = None
    app_id: Optional[str] = None
    cost: Optional[float] = None


app_build_types: dict[str, str] = {}

FAST_BUILD_P90_THRESHOLD_MINUTES = 20.0
FAST_BUILD_MIN_BUILDS = 3
FAST_BUILD_LOOKBACK_DAYS = 90
SPOT_BUILD_TYPES = ("medium", "large")


async def get_app_p90_build_time(db: AsyncSession, app_id: str) -> float | None:
    query = text("""
        SELECT PERCENTILE_CONT(0.9) WITHIN GROUP (
            ORDER BY EXTRACT(EPOCH FROM (finished_at - started_at))/60
        ) as p90_min
        FROM pipeline
        WHERE app_id = :app_id
          AND status = 'succeeded'
          AND finished_at IS NOT NULL
          AND started_at IS NOT NULL
          AND (params->>'build_type' IS NULL OR params->>'build_type' != 'large')
          AND started_at > NOW() - INTERVAL '1 day' * :lookback_days
        HAVING COUNT(*) >= :min_builds
    """)
    result = await db.execute(
        query,
        {
            "app_id": app_id,
            "min_builds": FAST_BUILD_MIN_BUILDS,
            "lookback_days": FAST_BUILD_LOOKBACK_DAYS,
        },
    )
    row = result.first()
    if row is None or row[0] is None:
        return None
    return float(row[0])


async def determine_build_type(db: AsyncSession, app_id: str) -> str:
    if app_id in app_build_types:
        return app_build_types[app_id]

    p90 = await get_app_p90_build_time(db, app_id)
    if p90 is not None and p90 <= FAST_BUILD_P90_THRESHOLD_MINUTES:
        return "default"

    return "medium"


async def _validate_and_prepare_callback(
    validator_class: type,
    callback_data: dict[str, Any],
    pipeline_id: uuid.UUID,
    db: AsyncSession,
) -> tuple[Pipeline, CallbackData, str]:
    validator = validator_class()
    parsed_data = validator.validate_and_parse(callback_data)
    assert parsed_data.status is not None

    pipeline = await db.get(Pipeline, pipeline_id)
    if not pipeline:
        raise ValueError(f"Pipeline {pipeline_id} not found")

    if pipeline.status in [
        PipelineStatus.SUCCEEDED,
        PipelineStatus.PUBLISHED,
        PipelineStatus.CANCELLED,
    ]:
        raise ValueError("Pipeline status already finalized")

    status_value = parsed_data.status.lower()
    if status_value not in ["success", "failure", "cancelled"]:
        raise ValueError("status must be 'success', 'failure', or 'cancelled'")

    return pipeline, parsed_data, status_value


async def cancel_pipeline(
    pipeline_id: uuid.UUID,
    build_id: int | None,
    provider_data: dict[str, Any] | None,
    github_actions: GitHubActionsService | None = None,
) -> None:
    provider_data_dict = provider_data or {}
    run_id = provider_data_dict.get("run_id")
    if run_id and github_actions:
        try:
            await github_actions.cancel(str(pipeline_id), provider_data_dict)
            logger.info(
                "Cancelled GitHub Actions run for cancelled pipeline",
                run_id=run_id,
                pipeline_id=str(pipeline_id),
            )
        except Exception as e:
            logger.warning(
                "Failed to cancel GitHub Actions run",
                run_id=run_id,
                pipeline_id=str(pipeline_id),
                error=str(e),
            )


class BuildPipeline:
    def __init__(self):
        self.provider = github_actions_service

    @staticmethod
    def is_spot_build_type(build_type: str | None) -> bool:
        return build_type in SPOT_BUILD_TYPES

    async def create_pipeline(
        self,
        app_id: str,
        params: dict[str, Any],
        webhook_event_id: uuid.UUID | None = None,
    ) -> Pipeline:
        async with get_db() as db:
            pipeline = Pipeline(
                app_id=app_id,
                params=params,
                webhook_event_id=webhook_event_id,
                provider_data={},
            )
            db.add(pipeline)
            await db.flush()
            await db.commit()
            return pipeline

    async def _prepare_pipeline_metadata(
        self,
        db: AsyncSession,
        pipeline: Pipeline,
    ) -> tuple[str, str]:
        params = dict(pipeline.params or {})
        stored_build_type = params.get("build_type")
        if stored_build_type is None:
            build_type = await determine_build_type(db, pipeline.app_id)
            params["build_type"] = build_type
        else:
            build_type = str(stored_build_type)

        pipeline.params = params

        flat_manager_repo: str
        if pipeline.flat_manager_repo is None:
            flat_manager_repo = "test"
            pipeline.flat_manager_repo = flat_manager_repo
        else:
            flat_manager_repo = pipeline.flat_manager_repo

        return build_type, flat_manager_repo

    async def prepare_pipeline_for_start(
        self,
        pipeline_id: uuid.UUID,
    ) -> Pipeline:
        async with get_db() as db:
            pipeline = await db.get(Pipeline, pipeline_id)
            if not pipeline:
                raise ValueError(f"Pipeline {pipeline_id} not found")

            await self._prepare_pipeline_metadata(db, pipeline)
            await db.commit()
            return pipeline

    async def _count_running_spot_builds(self, db: AsyncSession) -> int:
        query = text("""
            SELECT COUNT(*)
            FROM pipeline
            WHERE status = :status
              AND params->>'build_type' = ANY(:spot_types)
        """)
        result = await db.execute(
            query,
            {
                "status": PipelineStatus.RUNNING.value,
                "spot_types": list(SPOT_BUILD_TYPES),
            },
        )
        return int(result.scalar() or 0)

    async def _can_start_test_spot_build(self, db: AsyncSession) -> bool:
        if settings.max_concurrent_builds == 0:
            return True

        running_spot_builds = await self._count_running_spot_builds(db)
        return running_spot_builds < settings.max_concurrent_builds

    async def supersede_conflicting_test_pipelines(
        self, pipeline_id: uuid.UUID
    ) -> None:
        async with get_db() as db:
            pipeline = await db.get(Pipeline, pipeline_id)
            if not pipeline:
                raise ValueError(f"Pipeline {pipeline_id} not found")

            await self._supersede_conflicting_pipelines(
                db=db,
                pipeline=pipeline,
            )
            await db.commit()

    async def should_queue_test_build(self, pipeline_id: uuid.UUID) -> bool:
        async with get_db() as db:
            pipeline = await db.get(Pipeline, pipeline_id)
            if not pipeline:
                raise ValueError(f"Pipeline {pipeline_id} not found")

            if not self.is_spot_build_type((pipeline.params or {}).get("build_type")):
                return False

            return not await self._can_start_test_spot_build(db)

    async def _supersede_conflicting_pipelines(
        self,
        db: AsyncSession,
        pipeline: Pipeline,
    ) -> None:
        ref = (pipeline.params or {}).get("ref")
        if not ref:
            return

        query = (
            select(Pipeline)
            .where(
                Pipeline.app_id == pipeline.app_id,
                Pipeline.status.in_([PipelineStatus.RUNNING, PipelineStatus.PENDING]),
                Pipeline.id != pipeline.id,
            )
            .where(text("params->>'ref' = :ref"))
            .params(ref=ref)
        )
        result = await db.execute(query)
        conflicting = list(result.scalars().all())

        for old_pipeline in conflicting:
            old_pipeline.status = PipelineStatus.SUPERSEDED
            logger.info(
                "Superseded conflicting pipeline at start time",
                superseded_pipeline_id=str(old_pipeline.id),
                new_pipeline_id=str(pipeline.id),
                app_id=pipeline.app_id,
            )
            await cancel_pipeline(
                old_pipeline.id,
                old_pipeline.build_id,
                old_pipeline.provider_data,
                github_actions=GitHubActionsService(),
            )

    async def start_pipeline(
        self,
        pipeline_id: uuid.UUID,
    ) -> Pipeline:
        async with get_db() as db:
            pipeline = await db.get(Pipeline, pipeline_id)
            if not pipeline:
                raise ValueError(f"Pipeline {pipeline_id} not found")

            if pipeline.status != PipelineStatus.PENDING:
                raise ValueError(f"Pipeline {pipeline_id} is not in PENDING state")

            pipeline.status = PipelineStatus.RUNNING
            pipeline.started_at = datetime.now(tz=timezone.utc)

            params = dict(pipeline.params or {})
            stored_build_type = params.get("build_type")
            if stored_build_type is None or pipeline.flat_manager_repo is None:
                build_type, _ = await self._prepare_pipeline_metadata(db, pipeline)
            else:
                build_type = str(stored_build_type)

            await self._supersede_conflicting_pipelines(db, pipeline)

            workflow_id = pipeline.params.get("workflow_id", "build.yml")

            inputs = {
                "app_id": pipeline.app_id,
                "git_ref": pipeline.params.get("ref", "master"),
                "callback_url": f"{settings.base_url}/api/pipelines/{pipeline.id}/callback",
                "callback_token": pipeline.callback_token,
                "build_type": build_type,
                "spot": "true" if pipeline.params.get("use_spot", True) else "false",
                "pr_target_branch": pipeline.params.get("pr_target_branch", "master"),
            }

            job_data = {
                "app_id": pipeline.app_id,
                "job_type": "build",
                "params": {
                    "owner": settings.github_org,
                    "repo": settings.workflow_repo,
                    "workflow_id": workflow_id,
                    "ref": "main",
                    "inputs": inputs,
                },
            }

            provider_result = await self.provider.dispatch(
                str(pipeline.id), str(pipeline.id), job_data
            )

            pipeline.provider_data = provider_result

            await db.commit()
            return pipeline

    async def start_pending_builds(self) -> list[uuid.UUID]:
        query = """
            SELECT id
            FROM pipeline
            WHERE status = :pending_status
              AND flat_manager_repo = 'test'
              AND params->>'build_type' = ANY(:spot_types)
            ORDER BY created_at ASC
        """

        params: dict[str, Any] = {
            "pending_status": PipelineStatus.PENDING.value,
            "spot_types": list(SPOT_BUILD_TYPES),
        }
        limit: int | None = None
        async with get_db() as db:
            if settings.max_concurrent_builds > 0:
                running_spot_builds = await self._count_running_spot_builds(db)
                remaining_capacity = max(
                    settings.max_concurrent_builds - running_spot_builds,
                    0,
                )
                if remaining_capacity == 0:
                    return []
                limit = remaining_capacity

            if limit is not None:
                query += "\nLIMIT :limit"
                params["limit"] = limit

            result = await db.execute(text(query), params)
            rows = result.fetchall()
            pending_pipeline_ids = [row[0] for row in rows]

        started_pipeline_ids: list[uuid.UUID] = []
        for pending_pipeline_id in pending_pipeline_ids:
            try:
                started_pipeline = await self.start_pipeline(pending_pipeline_id)
                started_pipeline_ids.append(started_pipeline.id)
            except ValueError:
                logger.info(
                    "Skipping pending pipeline that is no longer startable",
                    pipeline_id=str(pending_pipeline_id),
                )
            except Exception as e:
                logger.error(
                    "Failed to start queued pipeline",
                    pipeline_id=str(pending_pipeline_id),
                    error=str(e),
                )

        return started_pipeline_ids

    async def handle_metadata_callback(
        self,
        pipeline_id: uuid.UUID,
        callback_data: dict[str, Any],
    ) -> tuple[Pipeline, dict[str, Any]]:
        from app.services.callback import MetadataCallbackValidator

        validator = MetadataCallbackValidator()
        parsed_data = validator.validate_and_parse(callback_data)

        async with get_db() as db:
            pipeline = await db.get(Pipeline, pipeline_id)
            if not pipeline:
                raise ValueError(f"Pipeline {pipeline_id} not found")

            updates: dict[str, Any] = {}

            if parsed_data.app_id and pipeline.app_id != parsed_data.app_id:
                pipeline.app_id = parsed_data.app_id
                updates["app_id"] = pipeline.app_id

            await db.commit()
            return pipeline, updates

    async def handle_log_url_callback(
        self,
        pipeline_id: uuid.UUID,
        callback_data: dict[str, Any],
    ) -> tuple[Pipeline, dict[str, Any]]:
        from app.services.callback import LogUrlCallbackValidator

        validator = LogUrlCallbackValidator()
        parsed_data = validator.validate_and_parse(callback_data)
        assert parsed_data.log_url is not None

        async with get_db() as db:
            pipeline = await db.get(Pipeline, pipeline_id)
            if not pipeline:
                raise ValueError(f"Pipeline {pipeline_id} not found")

            if pipeline.log_url:
                raise ValueError("Log URL already set")

            updates: dict[str, Any] = {}
            pipeline.log_url = parsed_data.log_url
            updates["log_url"] = pipeline.log_url

            provider_data = dict(pipeline.provider_data or {})

            try:
                run_id = parsed_data.log_url.rstrip("/").split("/")[-1]
                provider_data["run_id"] = run_id
                pipeline.provider_data = provider_data
            except (IndexError, AttributeError):
                logger.warning(
                    "Failed to extract run_id from log_url",
                    log_url=parsed_data.log_url,
                    pipeline_id=str(pipeline_id),
                )

            await db.commit()

            if pipeline.status == PipelineStatus.CANCELLED:
                run_id = provider_data.get("run_id")
                if run_id:
                    try:
                        github_actions = GitHubActionsService()
                        await github_actions.cancel(str(pipeline.id), provider_data)
                        logger.info(
                            "Cancelled GitHub Actions run for already-cancelled pipeline",
                            run_id=run_id,
                            pipeline_id=str(pipeline_id),
                        )
                    except Exception as e:
                        logger.warning(
                            "Failed to cancel GitHub Actions run for cancelled pipeline",
                            run_id=run_id,
                            pipeline_id=str(pipeline_id),
                            error=str(e),
                        )
                return pipeline, updates

            github_notifier = GitHubNotifier()
            await github_notifier.handle_build_started(pipeline, parsed_data.log_url)

            return pipeline, updates

    async def handle_status_callback(
        self,
        pipeline_id: uuid.UUID,
        callback_data: dict[str, Any],
    ) -> tuple[Pipeline, dict[str, Any]]:
        from app.services.callback import StatusCallbackValidator

        async with get_db() as db:
            pipeline, parsed_data, status_value = await _validate_and_prepare_callback(
                StatusCallbackValidator, callback_data, pipeline_id, db
            )

            updates: dict[str, Any] = {}

            match status_value:
                case "success":
                    pipeline.status = PipelineStatus.SUCCEEDED
                    pipeline.finished_at = datetime.now(tz=timezone.utc)
                case "failure":
                    github_actions = GitHubActionsService()
                    try:
                        was_cancelled = await github_actions.check_run_was_cancelled(
                            pipeline.provider_data
                        )
                        if was_cancelled:
                            logger.info(
                                "Build reclassified from failed to cancelled",
                                pipeline_id=str(pipeline_id),
                            )
                            pipeline.status = PipelineStatus.CANCELLED
                            status_value = "cancelled"
                        else:
                            pipeline.status = PipelineStatus.FAILED
                    except Exception as e:
                        logger.warning(
                            "Failed to check if build was cancelled, treating as failed",
                            pipeline_id=str(pipeline_id),
                            error=str(e),
                        )
                        pipeline.status = PipelineStatus.FAILED
                    pipeline.finished_at = datetime.now(tz=timezone.utc)
                case "cancelled":
                    pipeline.status = PipelineStatus.CANCELLED
                    pipeline.finished_at = datetime.now(tz=timezone.utc)

            await db.commit()

            github_notifier = GitHubNotifier()
            await github_notifier.handle_build_completion(pipeline, status_value)

            updates["pipeline_status"] = status_value
            await self.start_pending_builds()
            return pipeline, updates

    async def handle_cost_callback(
        self,
        pipeline_id: uuid.UUID,
        callback_data: dict[str, Any],
    ) -> tuple[Pipeline, dict[str, Any]]:
        from app.services.callback import CostCallbackValidator

        validator = CostCallbackValidator()
        parsed_data = validator.validate_and_parse(callback_data)
        assert parsed_data.cost is not None

        async with get_db() as db:
            pipeline = await db.get(Pipeline, pipeline_id)
            if not pipeline:
                raise ValueError(f"Pipeline {pipeline_id} not found")

            pipeline.total_cost = (pipeline.total_cost or 0) + parsed_data.cost

            await db.commit()

            updates: dict[str, Any] = {"total_cost": pipeline.total_cost}
            return pipeline, updates

    async def verify_callback_token(
        self, pipeline_id: uuid.UUID, token: str
    ) -> Pipeline:
        async with get_db() as db:
            pipeline = await db.get(Pipeline, pipeline_id)
            if not pipeline:
                raise ValueError(f"Pipeline {pipeline_id} not found")

            if not secrets.compare_digest(token, pipeline.callback_token):
                raise ValueError("Invalid callback token")

            return pipeline
