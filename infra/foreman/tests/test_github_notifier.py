import uuid
from datetime import datetime
from unittest.mock import AsyncMock, patch

import pytest

from app.models import Pipeline, PipelineStatus, PipelineTrigger
from app.services.github_notifier import GitHubNotifier


@pytest.fixture
def github_notifier():
    return GitHubNotifier()


@pytest.fixture
def mock_pipeline():
    return Pipeline(
        id=uuid.uuid4(),
        app_id="org.test.App",
        status=PipelineStatus.RUNNING,
        params={
            "sha": "abc123def456",
            "repo": "project-tick/org.test.App",
            "pr_number": "42",
        },
        triggered_by=PipelineTrigger.WEBHOOK,
        build_id=123,
        flat_manager_repo="test",
        log_url="https://example.com/logs/123",
        created_at=datetime.now(),
    )


@pytest.mark.asyncio
async def test_notify_build_status_success(github_notifier, mock_pipeline):
    with patch("app.services.github_notifier.update_commit_status") as mock_update:
        await github_notifier.notify_build_status(
            mock_pipeline, "success", log_url="https://example.com/custom-log"
        )

        mock_update.assert_called_once_with(
            sha="abc123def456",
            state="success",
            git_repo="project-tick/org.test.App",
            description="Build succeeded",
            target_url="https://example.com/custom-log",
        )


@pytest.mark.asyncio
async def test_notify_build_status_failure(github_notifier, mock_pipeline):
    with patch("app.services.github_notifier.update_commit_status") as mock_update:
        await github_notifier.notify_build_status(mock_pipeline, "failure")

        mock_update.assert_called_once_with(
            sha="abc123def456",
            state="failure",
            git_repo="project-tick/org.test.App",
            description="Build failed",
            target_url="https://example.com/logs/123",
        )


@pytest.mark.asyncio
async def test_notify_build_status_cancelled(github_notifier, mock_pipeline):
    with patch("app.services.github_notifier.update_commit_status") as mock_update:
        await github_notifier.notify_build_status(mock_pipeline, "cancelled")

        mock_update.assert_called_once_with(
            sha="abc123def456",
            state="failure",
            git_repo="project-tick/org.test.App",
            description="Build cancelled",
            target_url="https://example.com/logs/123",
        )


@pytest.mark.asyncio
async def test_notify_build_status_committed(github_notifier, mock_pipeline):
    with patch("app.services.github_notifier.update_commit_status") as mock_update:
        await github_notifier.notify_build_status(
            mock_pipeline, "committed", log_url="https://example.com/custom-log"
        )

        mock_update.assert_called_once_with(
            sha="abc123def456",
            state="success",
            git_repo="project-tick/org.test.App",
            description="Build ready",
            target_url="https://example.com/custom-log",
        )


@pytest.mark.asyncio
async def test_notify_build_status_committing(github_notifier, mock_pipeline):
    with patch("app.services.github_notifier.update_commit_status") as mock_update:
        await github_notifier.notify_build_status(mock_pipeline, "committing")

        mock_update.assert_called_once_with(
            sha="abc123def456",
            state="pending",
            git_repo="project-tick/org.test.App",
            description="Committing build...",
            target_url="https://example.com/logs/123",
        )


@pytest.mark.asyncio
async def test_notify_build_status_unknown(github_notifier, mock_pipeline):
    with patch("app.services.github_notifier.update_commit_status") as mock_update:
        await github_notifier.notify_build_status(mock_pipeline, "unknown_status")

        mock_update.assert_called_once_with(
            sha="abc123def456",
            state="failure",
            git_repo="project-tick/org.test.App",
            description="Build status: unknown_status.",
            target_url="https://example.com/logs/123",
        )


@pytest.mark.asyncio
async def test_notify_build_status_missing_params(github_notifier, mock_pipeline):
    mock_pipeline.params = {}

    with patch("app.services.github_notifier.update_commit_status") as mock_update:
        await github_notifier.notify_build_status(mock_pipeline, "success")

        mock_update.assert_not_called()


@pytest.mark.asyncio
async def test_notify_build_status_no_log_url(github_notifier, mock_pipeline):
    mock_pipeline.log_url = None

    with patch("app.services.github_notifier.update_commit_status") as mock_update:
        await github_notifier.notify_build_status(mock_pipeline, "success")

        mock_update.assert_called_once()
        assert mock_update.call_args[1]["target_url"] == ""


@pytest.mark.asyncio
async def test_notify_build_started(github_notifier, mock_pipeline):
    log_url = "https://example.com/logs/456"

    with patch("app.services.github_notifier.update_commit_status") as mock_update:
        await github_notifier.notify_build_started(mock_pipeline, log_url)

        mock_update.assert_called_once_with(
            sha="abc123def456",
            state="pending",
            git_repo="project-tick/org.test.App",
            description="Build in progress",
            target_url=log_url,
        )


@pytest.mark.asyncio
async def test_notify_build_started_missing_params(github_notifier, mock_pipeline):
    mock_pipeline.params = {"sha": "abc123"}

    with patch("app.services.github_notifier.update_commit_status") as mock_update:
        await github_notifier.notify_build_started(mock_pipeline, "http://log")

        mock_update.assert_not_called()


@pytest.mark.asyncio
async def test_notify_pr_build_started(github_notifier, mock_pipeline):
    log_url = "https://example.com/logs/789"

    with patch("app.services.github_notifier.create_pr_comment") as mock_comment:
        await github_notifier.notify_pr_build_started(mock_pipeline, log_url)

        mock_comment.assert_called_once_with(
            git_repo="project-tick/org.test.App",
            pr_number=42,
            comment=f"🚧 Started [test build]({log_url}).",
        )


@pytest.mark.asyncio
async def test_notify_pr_build_started_invalid_pr_number(
    github_notifier, mock_pipeline
):
    mock_pipeline.params["pr_number"] = "not_a_number"

    with patch("app.services.github_notifier.create_pr_comment") as mock_comment:
        await github_notifier.notify_pr_build_started(mock_pipeline, "http://log")

        mock_comment.assert_not_called()


@pytest.mark.asyncio
async def test_notify_pr_build_complete_success_with_arches(
    github_notifier, mock_pipeline
):
    with (
        patch(
            "app.services.github_notifier.get_linter_warning_messages",
            return_value=[],
        ),
        patch(
            "app.services.github_notifier.get_build_job_arches",
            return_value=["x86_64", "aarch64"],
        ),
        patch("app.services.github_notifier.create_pr_comment") as mock_comment,
    ):
        await github_notifier.notify_pr_build_complete(mock_pipeline, "success")

        mock_comment.assert_called_once()
        comment = mock_comment.call_args[1]["comment"]
        assert "✅" in comment
        assert "Test build succeeded" in comment
        assert "aarch64 and x86_64" in comment


@pytest.mark.asyncio
async def test_notify_pr_build_complete_success_with_linter_warnings(
    github_notifier, mock_pipeline
):
    with (
        patch(
            "app.services.github_notifier.get_linter_warning_messages",
            return_value=["warning one", "warning two"],
        ),
        patch(
            "app.services.github_notifier.get_build_job_arches",
            return_value=["x86_64"],
        ),
        patch("app.services.github_notifier.create_pr_comment") as mock_comment,
    ):
        await github_notifier.notify_pr_build_complete(mock_pipeline, "success")

        comment = mock_comment.call_args[1]["comment"]
        assert "⚠️  Linter warnings" in comment
        assert "warning one" in comment


@pytest.mark.asyncio
async def test_notify_pr_build_complete_committed(github_notifier, mock_pipeline):
    with (
        patch(
            "app.services.github_notifier.get_linter_warning_messages",
            return_value=[],
        ),
        patch(
            "app.services.github_notifier.get_build_job_arches",
            return_value=["x86_64"],
        ),
        patch("app.services.github_notifier.create_pr_comment") as mock_comment,
    ):
        await github_notifier.notify_pr_build_complete(mock_pipeline, "committed")

        mock_comment.assert_called_once()
        comment = mock_comment.call_args[1]["comment"]
        assert "✅" in comment
        assert "Test build succeeded" in comment


@pytest.mark.asyncio
@pytest.mark.parametrize("flag_enabled", [True, False])
async def test_notify_pr_build_complete_failure(
    github_notifier, mock_pipeline, flag_enabled
):
    with (
        patch(
            "app.services.github_notifier.settings.ff_admin_ping_comment",
            flag_enabled,
        ),
        patch("app.services.github_notifier.create_pr_comment") as mock_comment,
    ):
        await github_notifier.notify_pr_build_complete(mock_pipeline, "failure")

        expected_comment = (
            "❌ [Test build](https://example.com/logs/123) failed.\n\n"
            "<details><summary>Help</summary>\n\n"
            "- <code>bot, build</code> - Restart the test build\n"
        )
        if flag_enabled:
            expected_comment += (
                f"- <code>bot, ping admins</code> - Contact {mock_pipeline.app_id}\n"
            )
        expected_comment += "</details>"

        mock_comment.assert_called_once()
        comment = mock_comment.call_args[1]["comment"]
        assert "❌" in comment
        assert "failed" in comment


@pytest.mark.asyncio
async def test_notify_pr_build_complete_cancelled(github_notifier, mock_pipeline):
    with (
        patch("app.services.github_notifier.settings.ff_admin_ping_comment", True),
        patch("app.services.github_notifier.create_pr_comment") as mock_comment,
    ):
        await github_notifier.notify_pr_build_complete(mock_pipeline, "cancelled")

        mock_comment.assert_called_once()
        comment = mock_comment.call_args[1]["comment"]
        assert "❌" in comment
        assert "cancelled" in comment


@pytest.mark.asyncio
async def test_notify_pr_build_complete_missing_params(github_notifier, mock_pipeline):
    mock_pipeline.params = {"sha": "abc123"}

    with patch("app.services.github_notifier.create_pr_comment") as mock_comment:
        await github_notifier.notify_pr_build_complete(mock_pipeline, "success")

        mock_comment.assert_not_called()


@pytest.mark.asyncio
async def test_handle_build_completion_success_with_pr(github_notifier, mock_pipeline):
    with patch.object(github_notifier, "notify_build_status") as mock_status:
        with patch.object(github_notifier, "notify_pr_build_complete") as mock_pr:
            await github_notifier.handle_build_completion(mock_pipeline, "success")

            mock_status.assert_called_once_with(mock_pipeline, "success")
            mock_pr.assert_called_once_with(mock_pipeline, "success")


@pytest.mark.asyncio
async def test_handle_build_completion_no_pr(github_notifier, mock_pipeline):
    mock_pipeline.params = {"sha": "abc123", "repo": "project-tick/test"}

    with patch.object(github_notifier, "notify_build_status") as mock_status:
        with patch.object(github_notifier, "notify_pr_build_complete") as mock_pr:
            await github_notifier.handle_build_completion(mock_pipeline, "success")

            mock_status.assert_called_once_with(mock_pipeline, "success")
            mock_pr.assert_not_called()


@pytest.mark.asyncio
async def test_handle_build_completion_failure(github_notifier, mock_pipeline):
    with patch.object(github_notifier, "notify_build_status") as mock_status:
        with patch.object(github_notifier, "notify_pr_build_complete") as mock_pr:
            await github_notifier.handle_build_completion(mock_pipeline, "failure")

            mock_status.assert_called_once_with(mock_pipeline, "failure")
            mock_pr.assert_called_once_with(mock_pipeline, "failure")


@pytest.mark.asyncio
async def test_handle_build_completion_cancelled(github_notifier, mock_pipeline):
    with patch.object(github_notifier, "notify_build_status") as mock_status:
        with patch.object(github_notifier, "notify_pr_build_complete") as mock_pr:
            await github_notifier.handle_build_completion(mock_pipeline, "cancelled")

            mock_status.assert_called_once_with(mock_pipeline, "cancelled")
            mock_pr.assert_called_once_with(mock_pipeline, "cancelled")


@pytest.mark.asyncio
async def test_handle_build_completion_failure_creates_stable_issue(github_notifier, mock_pipeline):
    mock_pipeline.params = {"sha": "abc123", "repo": "project-tick/org.test.App"}
    mock_pipeline.flat_manager_repo = "stable"

    with patch.object(github_notifier, "notify_build_status") as mock_status:
        with patch.object(github_notifier, "notify_pr_build_complete") as mock_pr:
            with patch("app.services.github_notifier.create_github_issue", AsyncMock()) as mock_issue:
                await github_notifier.handle_build_completion(mock_pipeline, "failure")

                mock_status.assert_called_once_with(mock_pipeline, "failure")
                mock_pr.assert_not_called()
                mock_issue.assert_awaited_once()
                assert "bot, retry" in mock_issue.await_args.kwargs["body"]


@pytest.mark.asyncio
async def test_handle_build_completion_failure_does_not_create_stable_issue_for_schedule(
    github_notifier, mock_pipeline
):
    mock_pipeline.params = {
        "sha": "abc123",
        "repo": "project-tick/org.test.App",
        "native_github_ci": "true",
        "github_event_name": "schedule",
        "schedule": "0 3 * * *",
    }
    mock_pipeline.flat_manager_repo = "stable"

    with patch.object(github_notifier, "notify_build_status") as mock_status:
        with patch.object(github_notifier, "notify_pr_build_complete") as mock_pr:
            with patch("app.services.github_notifier.create_github_issue", AsyncMock()) as mock_issue:
                await github_notifier.handle_build_completion(mock_pipeline, "failure")

                mock_status.assert_called_once_with(mock_pipeline, "failure")
                mock_pr.assert_not_called()
                mock_issue.assert_not_called()


@pytest.mark.asyncio
async def test_handle_build_completion_failure_delegates_stable_issue_to_gitlab(
    github_notifier, mock_pipeline
):
    mock_pipeline.params = {
        "sha": "abc123",
        "repo": "Project-Tick/Project-Tick",
        "gitlab_base_url": "https://git.projecttick.org",
        "gitlab_project_path": "project-tick/project-tick",
    }
    mock_pipeline.flat_manager_repo = "stable"

    with patch.object(github_notifier, "notify_build_status") as mock_status:
        with patch.object(github_notifier, "notify_pr_build_complete") as mock_pr:
            with patch(
                "app.services.gitlab_notifier.GitLabNotifier._handle_stable_issue_lifecycle",
                AsyncMock(),
            ) as mock_gitlab_issue:
                with patch(
                    "app.services.github_notifier.create_github_issue",
                    AsyncMock(),
                ) as mock_issue:
                    await github_notifier.handle_build_completion(mock_pipeline, "failure")

                    mock_status.assert_called_once_with(mock_pipeline, "failure")
                    mock_pr.assert_not_called()
                    mock_gitlab_issue.assert_awaited_once_with(mock_pipeline, "failure")
                    mock_issue.assert_not_called()


@pytest.mark.asyncio
async def test_handle_build_completion_success_closes_retry_issue(github_notifier, mock_pipeline):
    mock_pipeline.params = {
        "sha": "abc123",
        "repo": "project-tick/org.test.App",
        "retry_from_issue": "17",
    }
    mock_pipeline.flat_manager_repo = "stable"

    with patch.object(github_notifier, "notify_build_status") as mock_status:
        with patch.object(github_notifier, "notify_pr_build_complete") as mock_pr:
            with patch("app.services.github_notifier.add_issue_comment", AsyncMock()) as mock_comment:
                with patch("app.services.github_notifier.close_github_issue", AsyncMock()) as mock_close:
                    await github_notifier.handle_build_completion(mock_pipeline, "success")

                    mock_status.assert_called_once_with(mock_pipeline, "success")
                    mock_pr.assert_not_called()
                    mock_comment.assert_awaited_once()
                    mock_close.assert_awaited_once_with(
                        git_repo="project-tick/org.test.App",
                        issue_number=17,
                    )


@pytest.mark.asyncio
async def test_handle_build_started(github_notifier, mock_pipeline):
    log_url = "https://example.com/new-log"

    with patch.object(github_notifier, "notify_build_started") as mock_started:
        with patch.object(github_notifier, "notify_pr_build_started") as mock_pr:
            await github_notifier.handle_build_started(mock_pipeline, log_url)

            mock_started.assert_called_once_with(mock_pipeline, log_url)
            mock_pr.assert_called_once_with(mock_pipeline, log_url)


@pytest.mark.asyncio
async def test_handle_build_started_no_pr(github_notifier, mock_pipeline):
    mock_pipeline.params = {"sha": "abc123", "repo": "project-tick/test"}
    log_url = "https://example.com/new-log"

    with patch.object(github_notifier, "notify_build_started") as mock_started:
        with patch.object(github_notifier, "notify_pr_build_started") as mock_pr:
            await github_notifier.handle_build_started(mock_pipeline, log_url)

            mock_started.assert_called_once_with(mock_pipeline, log_url)
            mock_pr.assert_not_called()
