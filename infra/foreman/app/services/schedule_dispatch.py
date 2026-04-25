from __future__ import annotations

import uuid

from app.config import settings
from app.services.workflow_dispatch import (
    WorkflowEventContext,
    create_pipelines_for_rules,
    get_matching_workflow_rules,
)


async def dispatch_named_schedule(schedule_name: str) -> list[uuid.UUID]:
    rules = get_matching_workflow_rules(
        trigger="schedule",
        schedule_name=schedule_name,
    )
    if not rules:
        return []

    return await create_pipelines_for_rules(
        rules=rules,
        base_params={
            "repo": f"{settings.github_org}/{settings.github_ci_repo}",
            "ref": f"schedule:{schedule_name}",
            "scheduled": "true",
            "gitlab_base_url": settings.failure_issue_gitlab_base_url
            or settings.gitlab_base_url,
            "gitlab_project_path": settings.failure_issue_gitlab_project,
        },
        context=WorkflowEventContext(event_name="schedule"),
        fallback_app_id=f"schedule-{schedule_name}",
        webhook_event_id=None,
    )