import uuid
from datetime import datetime

import pytest

from app.models import Pipeline, PipelineStatus, PipelineTrigger
from app.services.ci_gate import CIGateService


class StubCIGateService(CIGateService):
    def __init__(self, changed_files: list[str], commit_message: str):
        super().__init__()
        self.changed_files = changed_files
        self.commit_message = commit_message
        self.contexts: list[dict[str, object]] = []

    async def _collect_changed_files_and_commit_message(self, context):
        self.contexts.append(context)
        return (self.changed_files, self.commit_message)


@pytest.mark.asyncio
async def test_build_plan_from_request_enables_pr_jobs_and_changed_projects():
    service = StubCIGateService(
        changed_files=["mnv/main.c", ".github/workflows/ci.yml"],
        commit_message="feat(mnv): improve parser",
    )

    plan = await service.build_plan_from_request(
        type(
            "Request",
            (),
            {
                "model_dump": lambda self: {
                    "event_name": "pull_request",
                    "repository": "Project-Tick/Project-Tick",
                    "source_ref": "refs/pull/42/head",
                    "base_ref": "master",
                    "pr_title": "feat(mnv): improve parser",
                }
            },
        )()
    )

    assert plan["is_pr"] is True
    assert plan["run_level"] == "standard"
    assert plan["mnv_changed"] is True
    assert plan["github_changed"] is True
    assert plan["changed_projects"] == "mnv,.github"
    assert plan["commit_type"] == "feat"
    assert plan["commit_scope"] == "mnv"
    assert plan["jobs"]["lint"] is True
    assert plan["jobs"]["dependency_review"] is True
    assert plan["jobs"]["mnv"] is True
    assert plan["jobs"]["cgit"] is False


@pytest.mark.asyncio
async def test_build_plan_from_request_downgrades_master_root_only_run_and_marks_meshmc():
    service = StubCIGateService(
        changed_files=["README.md"],
        commit_message="docs: refresh readme",
    )

    plan = await service.build_plan_from_request(
        type(
            "Request",
            (),
            {
                "model_dump": lambda self: {
                    "event_name": "push",
                    "repository": "Project-Tick/Project-Tick",
                    "source_ref": "refs/heads/master",
                    "source_sha": "abc123",
                    "actor": "dev",
                }
            },
        )()
    )

    assert plan["run_level"] == "standard"
    assert plan["root_changed"] is True
    assert plan["meshmc_changed"] is True
    assert plan["jobs"]["meshmc"] is True
    assert plan["jobs"]["mnv"] is False


@pytest.mark.asyncio
async def test_build_plan_from_request_keeps_master_meshmc_and_root_push_targeted():
    service = StubCIGateService(
        changed_files=["meshmc/updater/CMakeLists.txt", "README.md"],
        commit_message="build(meshmc): tweak updater",
    )

    plan = await service.build_plan_from_request(
        type(
            "Request",
            (),
            {
                "model_dump": lambda self: {
                    "event_name": "push",
                    "repository": "Project-Tick/Project-Tick",
                    "source_ref": "refs/heads/master",
                    "source_sha": "abc123",
                    "actor": "dev",
                }
            },
        )()
    )

    assert plan["run_level"] == "standard"
    assert plan["root_changed"] is True
    assert plan["meshmc_changed"] is True
    assert plan["changed_projects"] == "meshmc,root"
    assert plan["jobs"]["meshmc"] is True
    assert plan["jobs"]["meshmc_codeql"] is False
    assert plan["jobs"]["mnv"] is False


@pytest.mark.asyncio
async def test_build_plan_from_request_disables_all_jobs_for_unmerged_pull_request_target():
    service = StubCIGateService(
        changed_files=["mnv/main.c"],
        commit_message="fix: no-op",
    )

    plan = await service.build_plan_from_request(
        type(
            "Request",
            (),
            {
                "model_dump": lambda self: {
                    "event_name": "pull_request_target",
                    "repository": "Project-Tick/Project-Tick",
                    "source_ref": "refs/pull/42/head",
                    "base_ref": "master",
                    "pr_merged": False,
                    "pr_labels": ["backport"],
                }
            },
        )()
    )

    assert all(not enabled for enabled in plan["jobs"].values())


@pytest.mark.asyncio
async def test_build_plan_from_pipeline_infers_gitlab_merge_request_as_pr():
    service = StubCIGateService(
        changed_files=["json4cpp/src/value.cpp"],
        commit_message="fix(json4cpp): handle edge case",
    )
    pipeline = Pipeline(
        id=uuid.uuid4(),
        app_id="gitlab-mr-17",
        status=PipelineStatus.RUNNING,
        params={
            "dispatch_inputs": {
                "source-repository": "https://git.projecttick.org/project-tick/project-tick.git",
                "source-ref": "refs/merge-requests/17/head",
            },
            "gitlab_merge_request_iid": "17",
            "gitlab_source_sha": "abcdef123",
            "gitlab_source_branch": "feature/gate-plan",
            "gitlab_target_branch": "master",
            "pr_target_branch": "master",
        },
        triggered_by=PipelineTrigger.WEBHOOK,
        created_at=datetime.now(),
        provider_data={},
        callback_token="test-token",
    )

    plan = await service.build_plan_from_pipeline(pipeline)

    assert service.contexts[0]["event_name"] == "pull_request"
    assert (
        service.contexts[0]["source_repository"]
        == "https://git.projecttick.org/project-tick/project-tick.git"
    )
    assert plan["is_pr"] is True
    assert plan["jobs"]["lint"] is True
    assert plan["jobs"]["json4cpp"] is True


@pytest.mark.asyncio
async def test_build_plan_from_request_runs_release_sources_for_tags():
    service = StubCIGateService(
        changed_files=["README.md"],
        commit_message="release: publish archives",
    )

    plan = await service.build_plan_from_request(
        type(
            "Request",
            (),
            {
                "model_dump": lambda self: {
                    "event_name": "push",
                    "repository": "Project-Tick/Project-Tick",
                    "source_ref": "refs/tags/v1.2.3",
                    "source_sha": "abc123",
                }
            },
        )()
    )

    assert plan["is_tag"] is True
    assert plan["jobs"]["release_sources"] is True
