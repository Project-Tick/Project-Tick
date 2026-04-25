from __future__ import annotations

import uuid
from typing import Any

from sqlalchemy import select

from app.config import settings
from app.database import get_db
from app.models.change_event import ChangeEvent
from app.models.webhook_event import WebhookEvent
from app.config import WorkflowDispatchRule


def serialize_dispatch_rule(rule: WorkflowDispatchRule) -> dict[str, str]:
    owner = rule.owner or settings.github_org
    repo = rule.repo or settings.github_ci_repo
    return {
        "name": rule.name,
        "workflow_id": rule.workflow_id,
        "owner": owner,
        "repo": repo,
        "workflow_name": rule.workflow_name or rule.workflow_id,
    }


async def record_change_event(
    *,
    event: WebhookEvent,
    changed_paths: set[str],
    rules: list[WorkflowDispatchRule],
    pipeline_ids: list[uuid.UUID],
) -> ChangeEvent:
    async with get_db() as db:
        change_event = ChangeEvent(
            repository=event.repository,
            actor=event.actor,
            ref=str(event.payload.get("ref", "")),
            before_sha=str(event.payload.get("before", "")),
            after_sha=str(event.payload.get("after", "")),
            changed_paths=sorted(changed_paths),
            matched_rule_names=[rule.name for rule in rules],
            dispatched_workflows=[serialize_dispatch_rule(rule) for rule in rules],
            pipeline_ids=[str(pipeline_id) for pipeline_id in pipeline_ids],
            webhook_event_id=event.id,
        )
        db.add(change_event)
        await db.flush()
        await db.commit()
        return change_event


async def get_recent_change_events(limit: int | None = None) -> list[ChangeEvent]:
    async with get_db(use_replica=True) as db:
        query = select(ChangeEvent).order_by(ChangeEvent.created_at.desc())
        if limit is not None:
            query = query.limit(limit)
        result = await db.execute(query)
        return list(result.scalars().all())


def get_change_targets(change_event: ChangeEvent) -> list[str]:
    targets: list[str] = []
    for workflow in change_event.dispatched_workflows:
        if not isinstance(workflow, dict):
            continue
        owner = str(workflow.get("owner", "")).strip()
        repo = str(workflow.get("repo", "")).strip()
        workflow_id = str(workflow.get("workflow_id", "")).strip()
        if owner and repo and workflow_id:
            targets.append(f"{owner}/{repo}:{workflow_id}")
        elif workflow_id:
            targets.append(workflow_id)
    return targets