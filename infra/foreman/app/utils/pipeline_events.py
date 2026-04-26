from app.models import Pipeline


def is_scheduled_native_github_ci(pipeline: Pipeline) -> bool:
    params = dict(pipeline.params or {})
    if not params.get("native_github_ci"):
        return False

    event_name = str(
        params.get("github_event_name") or params.get("event_name") or ""
    ).strip().lower()
    if event_name == "schedule":
        return True

    # Catch manual workflow_dispatch runs of the Scheduled CI workflow
    workflow_name = str(params.get("workflow_name") or "").strip()
    return workflow_name == "Scheduled CI"