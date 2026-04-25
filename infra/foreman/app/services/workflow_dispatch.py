from __future__ import annotations

from dataclasses import dataclass
from fnmatch import fnmatch
import re
import uuid
from typing import Any

from app.config import WorkflowDispatchRule, settings
from app.pipelines.build import BuildPipeline


@dataclass
class WorkflowEventContext:
    source_repo_name: str = ""
    source_repository_url: str = ""
    source_ref: str = ""
    source_sha: str = ""
    before_sha: str = ""
    actor_login: str = ""
    event_name: str = ""
    pr_number: str = ""
    pr_base_sha: str = ""
    pr_head_sha: str = ""
    pr_title: str = ""
    pr_draft: str = ""
    head_ref: str = ""
    base_ref: str = ""
    tag_name: str = ""


def collect_changed_paths(payload: dict[str, Any]) -> set[str]:
    changed_paths: set[str] = set()

    for commit in payload.get("commits", []):
        if not isinstance(commit, dict):
            continue
        for key in ("added", "modified", "removed"):
            values = commit.get(key, [])
            if not isinstance(values, list):
                continue
            for path in values:
                if isinstance(path, str) and path:
                    changed_paths.add(path)

    return changed_paths


def get_branch_name(ref: str) -> str:
    if ref.startswith("refs/heads/"):
        return ref.removeprefix("refs/heads/")
    return ""


def get_tag_name(ref: str) -> str:
    if ref.startswith("refs/tags/"):
        return ref.removeprefix("refs/tags/")
    return ""


def _humanize_workflow_name(value: str) -> str:
    normalized = value.strip()
    if not normalized:
        return "Workflow"

    if normalized.endswith((".yml", ".yaml")):
        normalized = normalized.rsplit(".", maxsplit=1)[0]

    words = normalized.replace("-", " ").replace("_", " ").split()
    acronyms = {"ci", "mr", "pr", "qa", "api"}
    return " ".join(
        word.upper() if word.lower() in acronyms else word.capitalize()
        for word in words
    )


def _slugify(value: str) -> str:
    slug = re.sub(r"[^a-z0-9]+", "-", value.lower()).strip("-")
    return slug or "workflow"


def match_workflow_rule(
    rule: WorkflowDispatchRule,
    *,
    trigger: str,
    ref: str = "",
    branch_name: str = "",
    changed_paths: set[str] | None = None,
    schedule_name: str = "",
) -> bool:
    if not rule.enabled or trigger not in rule.triggers:
        return False

    if trigger == "schedule":
        if rule.schedule_names and schedule_name not in rule.schedule_names:
            return False
        return True

    if trigger == "github_tag_push":
        tag_name = get_tag_name(ref)
        if not tag_name:
            return False
        if rule.tag_patterns and not any(fnmatch(tag_name, pattern) for pattern in rule.tag_patterns):
            return False
    elif rule.branches:
        effective_branch = branch_name or get_branch_name(ref)
        if effective_branch not in rule.branches:
            return False

    resolved_changed_paths = changed_paths or set()
    if rule.path_patterns:
        if not resolved_changed_paths:
            return False
        if not any(
            fnmatch(path, pattern)
            for path in resolved_changed_paths
            for pattern in rule.path_patterns
        ):
            return False

    if rule.ignore_path_patterns and resolved_changed_paths:
        if not any(
            not any(fnmatch(path, pattern) for pattern in rule.ignore_path_patterns)
            for path in resolved_changed_paths
        ):
            return False

    return True


def get_matching_workflow_rules(
    *,
    trigger: str,
    ref: str = "",
    branch_name: str = "",
    changed_paths: set[str] | None = None,
    schedule_name: str = "",
) -> list[WorkflowDispatchRule]:
    return [
        rule
        for rule in settings.workflow_dispatch_rules
        if match_workflow_rule(
            rule,
            trigger=trigger,
            ref=ref,
            branch_name=branch_name,
            changed_paths=changed_paths,
            schedule_name=schedule_name,
        )
    ]


def build_dispatch_params_for_rule(
    rule: WorkflowDispatchRule,
    context: WorkflowEventContext,
) -> dict[str, Any]:
    owner = rule.owner or settings.github_org
    repo = rule.repo or settings.github_ci_repo
    workflow_ref = rule.ref or settings.github_ci_ref
    workflow_name = rule.workflow_name or _humanize_workflow_name(rule.workflow_id)
    status_context = rule.github_status_context or _slugify(rule.name)

    inputs = {
        key: value
        for key, value in rule.fixed_inputs.items()
        if value is not None and value != ""
    }

    dispatch_repo_full_name = f"{owner}/{repo}".lower()
    source_repository_url = context.source_repository_url
    if (
        not source_repository_url
        and context.source_repo_name
        and context.source_repo_name.lower() != dispatch_repo_full_name
    ):
        source_repository_url = f"https://github.com/{context.source_repo_name}.git"

    if rule.forward_source_repository and source_repository_url:
        inputs["source-repository"] = source_repository_url
    if rule.forward_source_ref and context.source_ref:
        inputs["source-ref"] = context.source_ref
    if rule.forward_source_sha and context.source_sha:
        inputs["source-sha"] = context.source_sha
    if rule.forward_before_sha and context.before_sha:
        inputs["before-sha"] = context.before_sha

    if rule.forward_event_metadata:
        if context.event_name:
            inputs["event-name"] = context.event_name
        if context.actor_login:
            inputs["event-actor"] = context.actor_login
        if context.head_ref:
            inputs["head-ref"] = context.head_ref
        if context.base_ref:
            inputs["base-ref"] = context.base_ref

    if rule.forward_pr_metadata:
        if context.pr_number:
            inputs["pr-number"] = context.pr_number
        if context.pr_base_sha:
            inputs["pr-base-sha"] = context.pr_base_sha
        if context.pr_head_sha:
            inputs["pr-head-sha"] = context.pr_head_sha
        if context.pr_title:
            inputs["pr-title"] = context.pr_title
        if context.pr_draft:
            inputs["pr-draft"] = context.pr_draft

    if rule.tag_input_name and context.tag_name:
        inputs[rule.tag_input_name] = context.tag_name

    params: dict[str, Any] = {
        "dispatch_owner": owner,
        "dispatch_repo": repo,
        "dispatch_workflow_id": rule.workflow_id,
        "dispatch_ref": workflow_ref,
        "dispatch_inputs": inputs,
        "workflow_name": workflow_name,
        "github_status_context": status_context,
    }

    if rule.target_repo:
        params["target_repo"] = rule.target_repo

    return params


def resolve_app_id_for_rule(rule: WorkflowDispatchRule, fallback_app_id: str) -> str:
    if rule.app_id:
        return rule.app_id
    return fallback_app_id or _slugify(rule.name)


async def create_pipelines_for_rules(
    *,
    rules: list[WorkflowDispatchRule],
    base_params: dict[str, Any],
    context: WorkflowEventContext,
    fallback_app_id: str,
    webhook_event_id: uuid.UUID | None,
) -> list[uuid.UUID]:
    if not rules:
        return []

    pipeline_service = BuildPipeline()
    pipeline_ids: list[uuid.UUID] = []

    for rule in rules:
        params = dict(base_params)
        params.update(build_dispatch_params_for_rule(rule, context))

        pipeline = await pipeline_service.create_pipeline(
            app_id=resolve_app_id_for_rule(rule, fallback_app_id),
            params=params,
            webhook_event_id=webhook_event_id,
        )
        pipeline = await pipeline_service.prepare_pipeline_for_start(pipeline.id)
        await pipeline_service.supersede_conflicting_test_pipelines(pipeline.id)
        should_queue = await pipeline_service.should_queue_test_build(pipeline.id)
        if not should_queue:
            pipeline = await pipeline_service.start_pipeline(pipeline.id)
        pipeline_ids.append(pipeline.id)

    return pipeline_ids
