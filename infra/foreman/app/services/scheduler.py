from __future__ import annotations

import asyncio
import os
from datetime import datetime, timezone

import structlog

from app.config import ForemanScheduleEntry, settings
from app.services.schedule_dispatch import dispatch_named_schedule

logger = structlog.get_logger(__name__)


def _matches_cron_value(expr: str, value: int, *, minimum: int, maximum: int) -> bool:
    if expr == "*":
        return True

    for part in expr.split(","):
        part = part.strip()
        if not part:
            continue

        step = 1
        base = part
        if "/" in part:
            base, step_str = part.split("/", maxsplit=1)
            step = int(step_str)
            if step <= 0:
                return False

        if base == "*":
            if (value - minimum) % step == 0:
                return True
            continue

        if "-" in base:
            start_str, end_str = base.split("-", maxsplit=1)
            start = int(start_str)
            end = int(end_str)
            if start <= value <= end and (value - start) % step == 0:
                return True
            continue

        candidate = int(base)
        if maximum == 6 and candidate == 7:
            candidate = 0
        if candidate == value:
            return True

    return False


def cron_matches(schedule: ForemanScheduleEntry, when: datetime) -> bool:
    fields = schedule.cron.split()
    if len(fields) != 5:
        return False

    minute, hour, day, month, weekday = fields
    cron_weekday = (when.weekday() + 1) % 7

    return all(
        (
            _matches_cron_value(minute, when.minute, minimum=0, maximum=59),
            _matches_cron_value(hour, when.hour, minimum=0, maximum=23),
            _matches_cron_value(day, when.day, minimum=1, maximum=31),
            _matches_cron_value(month, when.month, minimum=1, maximum=12),
            _matches_cron_value(weekday, cron_weekday, minimum=0, maximum=6),
        )
    )


class ForemanScheduler:
    def __init__(self) -> None:
        self._task: asyncio.Task[None] | None = None
        self._last_tick: dict[str, str] = {}

    def should_run(self) -> bool:
        if not settings.scheduler_enabled:
            return False
        if os.environ.get("PYTEST_CURRENT_TEST"):
            return False
        if settings.database_url.startswith("sqlite+aiosqlite:///:memory:"):
            return False
        return True

    async def start(self) -> None:
        if not self.should_run() or self._task is not None:
            return
        self._task = asyncio.create_task(self._run(), name="foreman-scheduler")
        logger.info("Foreman scheduler started")

    async def stop(self) -> None:
        if self._task is None:
            return
        self._task.cancel()
        try:
            await self._task
        except asyncio.CancelledError:
            pass
        finally:
            self._task = None
            logger.info("Foreman scheduler stopped")

    async def _run(self) -> None:
        while True:
            now = datetime.now(timezone.utc).replace(second=0, microsecond=0)
            await self._tick(now)
            await asyncio.sleep(30)

    async def _tick(self, now: datetime) -> None:
        current_slot = now.isoformat()
        for schedule in settings.foreman_schedules:
            if not schedule.enabled or not cron_matches(schedule, now):
                continue
            if self._last_tick.get(schedule.name) == current_slot:
                continue

            self._last_tick[schedule.name] = current_slot
            try:
                pipeline_ids = await dispatch_named_schedule(schedule.name)
                logger.info(
                    "Foreman schedule dispatched",
                    schedule_name=schedule.name,
                    pipeline_ids=[str(pipeline_id) for pipeline_id in pipeline_ids],
                )
            except Exception as error:
                logger.warning(
                    "Foreman schedule dispatch failed",
                    schedule_name=schedule.name,
                    error=str(error),
                )