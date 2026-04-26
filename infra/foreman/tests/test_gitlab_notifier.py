import uuid
from datetime import datetime
from unittest.mock import AsyncMock

import pytest

from app.models import Pipeline, PipelineStatus, PipelineTrigger
from app.services.gitlab_notifier import GitLabNotifier


@pytest.fixture
def gitlab_notifier():
    return GitLabNotifier()


@pytest.fixture
def mock_pipeline():
    return Pipeline(
        id=uuid.uuid4(),
        app_id="gitlab-mr-17",
        status=PipelineStatus.RUNNING,
        params={
            "gitlab_base_url": "https://git.projecttick.org",
            "gitlab_project_path": "project-tick/project-tick",
            "gitlab_merge_request_iid": "17",
            "gitlab_source_sha": "abcdef1234567890",
            "gitlab_source_branch": "feature/gitlab-build",
            "gitlab_target_branch": "master",
        },
        triggered_by=PipelineTrigger.WEBHOOK,
        flat_manager_repo="stable",
        log_url="https://github.com/Project-Tick/Project-Tick/actions/runs/123",
        created_at=datetime.now(),
        provider_data={},
        callback_token="test-token",
    )


def test_get_status_name_uses_target_branch(gitlab_notifier, mock_pipeline):
    assert gitlab_notifier._get_status_name(mock_pipeline) == "github-actions/CI/master"


@pytest.mark.asyncio
async def test_handle_build_started_includes_target_label(gitlab_notifier, mock_pipeline):
    gitlab_notifier._update_commit_status = AsyncMock(return_value=True)
    gitlab_notifier._create_merge_request_note = AsyncMock(return_value=True)

    await gitlab_notifier.handle_build_started(mock_pipeline, mock_pipeline.log_url or "")

    assert gitlab_notifier._update_commit_status.await_args.kwargs["description"] == (
        "Workflow running for master (stable) on GitHub Actions"
    )
    assert gitlab_notifier._create_merge_request_note.await_args.args[1] == (
        "🚧 [Build for master (stable) started]"
        f"({mock_pipeline.log_url})."
    )


@pytest.mark.asyncio
async def test_handle_build_completion_uses_target_label_for_beta(gitlab_notifier, mock_pipeline):
    gitlab_notifier._update_commit_status = AsyncMock(return_value=True)
    gitlab_notifier._create_merge_request_note = AsyncMock(return_value=True)
    mock_pipeline.flat_manager_repo = "beta"
    mock_pipeline.params["gitlab_target_branch"] = "beta"

    await gitlab_notifier.handle_build_completion(mock_pipeline, "success")

    assert gitlab_notifier._update_commit_status.await_args.kwargs["description"] == (
        "Workflow succeeded for beta on GitHub Actions"
    )
    assert gitlab_notifier._create_merge_request_note.await_args.args[1] == (
        "✅ [Build for beta succeeded]"
        f"({mock_pipeline.log_url})."
    )


@pytest.mark.asyncio
async def test_handle_build_completion_failure_creates_stable_issue(
    gitlab_notifier, mock_pipeline
):
    gitlab_notifier._update_commit_status = AsyncMock(return_value=True)
    gitlab_notifier._create_merge_request_note = AsyncMock(return_value=True)
    gitlab_notifier._create_issue = AsyncMock(return_value=("https://git/issue/1", 1))

    await gitlab_notifier.handle_build_completion(mock_pipeline, "failure")

    gitlab_notifier._create_issue.assert_awaited_once()
    assert "bot, retry" in gitlab_notifier._create_issue.await_args.args[2]


@pytest.mark.asyncio
async def test_handle_build_completion_failure_does_not_create_stable_issue_for_schedule(
    gitlab_notifier, mock_pipeline
):
    gitlab_notifier._update_commit_status = AsyncMock(return_value=True)
    gitlab_notifier._create_merge_request_note = AsyncMock(return_value=True)
    gitlab_notifier._create_issue = AsyncMock(return_value=("https://git/issue/1", 1))
    mock_pipeline.params.update(
        {
            "native_github_ci": "true",
            "github_event_name": "schedule",
            "schedule": "0 3 * * *",
        }
    )

    await gitlab_notifier.handle_build_completion(mock_pipeline, "failure")

    gitlab_notifier._create_issue.assert_not_awaited()


@pytest.mark.asyncio
async def test_handle_build_completion_success_closes_retry_issue(
    gitlab_notifier, mock_pipeline
):
    gitlab_notifier._update_commit_status = AsyncMock(return_value=True)
    gitlab_notifier._create_merge_request_note = AsyncMock(return_value=True)
    gitlab_notifier._create_issue_note = AsyncMock(return_value=True)
    gitlab_notifier._close_issue = AsyncMock(return_value=True)
    mock_pipeline.params["retry_from_gitlab_issue_iid"] = "11"

    await gitlab_notifier.handle_build_completion(mock_pipeline, "success")

    gitlab_notifier._create_issue_note.assert_awaited_once()
    gitlab_notifier._close_issue.assert_awaited_once_with(mock_pipeline, "11")