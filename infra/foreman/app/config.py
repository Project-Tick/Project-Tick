from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    admin_token: str = "raeVenga1eez3Geeca"
    base_url: str = "http://localhost:8000"
    database_url: str = "postgresql+psycopg://postgres:postgres@db:5432/test_db"
    database_replica_url: str | None = None
    debug: bool = False
    gitlab_api_token: str | None = None
    gitlab_webhook_secret: str = "test_gitlab_webhook_secret"
    github_token: str = "test_github_token"
    github_status_token: str = "test_github_status_token"
    github_webhook_secret: str = "test_webhook_secret"
    github_org: str = "project-tick"
    github_ci_repo: str = "Project-Tick"
    github_ci_workflow: str = "ci.yml"
    github_ci_ref: str = "master"
    workflow_repo: str = "foreman"
    admin_team: str = "project-tick/maintainers"
    gitlab_admin_mention: str | None = None
    statuspage_url: str = "https://status.projecttick.org"
    sentry_dsn: str | None = None
    ff_admin_ping_comment: bool = True
    ff_disable_test_builds: bool = False
    max_concurrent_builds: int = 15

    model_config = SettingsConfigDict(
        env_file=".env",
        env_file_encoding="utf-8",
    )


settings = Settings()
