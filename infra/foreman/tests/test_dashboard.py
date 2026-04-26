import uuid
from datetime import datetime, timedelta
from unittest.mock import AsyncMock, patch

from app.models import Pipeline, PipelineStatus
from app.routes.dashboard import get_pipeline_request_label


def make_pipeline(**overrides):
    now = datetime.now()
    defaults = {
        "id": uuid.uuid4(),
        "app_id": "gitlab-mr-17",
        "status": PipelineStatus.SUCCEEDED,
        "flat_manager_repo": "stable",
        "params": {
            "gitlab_base_url": "https://git.projecttick.org",
            "gitlab_project_path": "project-tick/project-tick",
            "gitlab_merge_request_iid": "17",
            "gitlab_source_branch": "feature/gitlab-build",
            "gitlab_target_branch": "master",
            "pr_target_branch": "master",
            "dispatch_inputs": {
                "source-repository": "https://git.projecttick.org/project-tick/project-tick.git",
                "source-ref": "refs/merge-requests/17/head",
            },
        },
        "created_at": now - timedelta(minutes=10),
        "started_at": now - timedelta(minutes=5),
        "finished_at": now,
        "log_url": "https://github.com/Project-Tick/Project-Tick/actions/runs/123",
        "build_id": 123,
        "commit_job_id": None,
        "publish_job_id": None,
        "update_repo_job_id": None,
    }
    defaults.update(overrides)
    return Pipeline(**defaults)


def test_builds_table_places_succeeded_pipeline_in_completed(client):
    pipeline = make_pipeline()

    with patch(
        "app.routes.dashboard.get_recent_pipelines",
        new=AsyncMock(return_value=[pipeline]),
    ):
        response = client.get("/api/htmx/builds")

    assert response.status_code == 200
    assert "Completed" in response.text
    assert 'badge badge-succeeded' in response.text
    assert '>succeeded</a>' in response.text


def test_builds_table_shows_request_and_source_for_gitlab_pipeline(client):
    pipeline = make_pipeline(flat_manager_repo=None)

    with patch(
        "app.routes.dashboard.get_recent_pipelines",
        new=AsyncMock(return_value=[pipeline]),
    ):
        response = client.get("/api/htmx/builds")

    assert response.status_code == 200
    assert "MR !17" in response.text
    assert "project-tick/project-tick@mr-17" in response.text
    assert "test" in response.text


def test_builds_table_shows_pipeline_identity_bits(client):
    pipeline = make_pipeline(
        id=uuid.UUID("12345678-1234-5678-1234-567812345678"),
        params={
            "gitlab_base_url": "https://git.projecttick.org",
            "gitlab_project_path": "project-tick/project-tick",
            "gitlab_merge_request_iid": "17",
            "gitlab_source_branch": "feature/gitlab-build",
            "gitlab_target_branch": "master",
            "pr_target_branch": "master",
            "build_type": "large",
            "dispatch_inputs": {
                "source-repository": "https://git.projecttick.org/project-tick/project-tick.git",
                "source-ref": "refs/merge-requests/17/head",
            },
        },
    )

    with patch(
        "app.routes.dashboard.get_recent_pipelines",
        new=AsyncMock(return_value=[pipeline]),
    ):
        response = client.get("/api/htmx/builds")

    assert response.status_code == 200
    assert "gitlab-mr-17" in response.text
    assert "large" in response.text
    assert "#12345678" in response.text


def test_builds_table_places_superseded_pipeline_in_replaced_section(client):
    pipeline = make_pipeline(status=PipelineStatus.SUPERSEDED)

    with patch(
        "app.routes.dashboard.get_recent_pipelines",
        new=AsyncMock(return_value=[pipeline]),
    ):
        response = client.get("/api/htmx/builds")

    assert response.status_code == 200
    assert "Replaced / cancelling" in response.text
    assert '>replaced</a>' in response.text
    assert "Completed <span class=\"section-count\">1</span>" not in response.text


def test_builds_table_groups_pipelines_by_workflow(client):
    ci_pipeline = make_pipeline(
        params={
            "dispatch_workflow_id": "ci.yml",
            "dispatch_inputs": {
                "source-repository": "https://github.com/project-tick/Project-Tick.git",
                "source-ref": "refs/heads/master",
            },
        },
    )
    action_pipeline = make_pipeline(
        id=uuid.uuid4(),
        status=PipelineStatus.RUNNING,
        params={
            "workflow_name": "Action CI",
            "gitlab_project_path": "project-tick/project-tick",
            "gitlab_base_url": "https://git.projecttick.org",
        },
    )

    with patch(
        "app.routes.dashboard.get_recent_pipelines",
        new=AsyncMock(return_value=[ci_pipeline, action_pipeline]),
    ):
        response = client.get("/api/htmx/builds")

    assert response.status_code == 200
    assert "<th colspan=\"6\">CI <span class=\"section-count\">1</span></th>" in response.text
    assert "<th colspan=\"6\">Action CI <span class=\"section-count\">1</span></th>" in response.text


def test_get_pipeline_request_label_prefers_explicit_workflow_name_over_branch():
    pipeline = make_pipeline(
        params={
            "dispatch_workflow_id": "ci.yml",
            "dispatch_inputs": {
                "source-repository": "https://github.com/project-tick/Project-Tick.git",
                "source-ref": "refs/heads/master",
            },
        },
    )

    assert get_pipeline_request_label(pipeline) == "CI"


def test_reproducible_route_redirects_to_dashboard(client):
    response = client.get("/reproducible", follow_redirects=False)

    assert response.status_code == 302
    assert response.headers["location"] == "/"


def test_app_status_groups_builds_by_target_repo(client):
    stable_pipeline = make_pipeline(flat_manager_repo="stable")
    beta_pipeline = make_pipeline(
        id=uuid.uuid4(),
        flat_manager_repo="beta",
        params={
            "gitlab_base_url": "https://git.projecttick.org",
            "gitlab_project_path": "project-tick/project-tick",
            "gitlab_merge_request_iid": "17",
            "gitlab_source_branch": "feature/gitlab-build",
            "gitlab_target_branch": "beta",
            "pr_target_branch": "beta",
            "dispatch_inputs": {
                "source-repository": "https://git.projecttick.org/project-tick/project-tick.git",
                "source-ref": "refs/heads/beta",
            },
        },
    )

    with patch(
        "app.routes.dashboard.get_app_builds",
        new=AsyncMock(return_value=[stable_pipeline, beta_pipeline]),
    ):
        response = client.get("/status/gitlab-mr-17")

    assert response.status_code == 200
    assert "Build status of MR !17" in response.text
    assert "Latest stable builds" in response.text
    assert "Latest beta builds" in response.text
    assert "project-tick/project-tick@mr-17" in response.text
    assert "project-tick/project-tick@beta" in response.text
    assert "Reproducible builds" not in response.text
