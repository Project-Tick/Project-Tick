from pydantic import BaseModel, Field
from pydantic_settings import BaseSettings, SettingsConfigDict


class WorkflowDispatchRule(BaseModel):
    name: str
    workflow_id: str
    workflow_name: str | None = None
    owner: str | None = None
    repo: str | None = None
    ref: str | None = None
    app_id: str | None = None
    target_repo: str | None = None
    triggers: list[str] = Field(default_factory=list)
    branches: list[str] = Field(default_factory=list)
    tag_patterns: list[str] = Field(default_factory=list)
    schedule_names: list[str] = Field(default_factory=list)
    path_patterns: list[str] = Field(default_factory=list)
    ignore_path_patterns: list[str] = Field(default_factory=list)
    fixed_inputs: dict[str, str] = Field(default_factory=dict)
    github_status_context: str | None = None
    tag_input_name: str | None = None
    enabled: bool = True
    forward_source_repository: bool = False
    forward_source_ref: bool = False
    forward_source_sha: bool = False
    forward_before_sha: bool = False
    forward_event_metadata: bool = False
    forward_pr_metadata: bool = False

# It's going to stay like this because that's what I want, it's policy, don't interfere!
_PRODUCT_FORGE_OWNERS: dict[str, str] = {
    "cmark": "CMark-Infra",
    "corebinutils": "CoreBinutils-Infra",
    "meshmc": "MeshMC-MMCO-Infra",
    "mnv": "MNV-Infra",
    "json4cpp": "Json4Cpp-Infra",
    "cgit": "CGit-Infra",
    "forgewrapper": "ForgeWrapper-Infra",
    "genqrcode": "GenQRCode-Infra",
    "libnbtplusplus": "LibNBTPlusPlus-Infra",
    "neozip": "NeoZip-Infra",
    "tomlplusplus": "TOMLPlusPlus-Infra",
}

_PRODUCT_PATHS: dict[str, list[str]] = {
    "cmark": ["cmark/**", "**/cmark/**", ".github/workflows/cmark-*.yml"],
    "corebinutils": [
        "corebinutils/**",
        "**/corebinutils/**",
        ".github/workflows/corebinutils-*.yml",
    ],
    "meshmc": ["meshmc/**", "**/meshmc/**", ".github/workflows/meshmc-*.yml"],
    "mnv": ["mnv/**", "**/mnv/**", ".github/workflows/mnv-*.yml"],
    "json4cpp": ["json4cpp/**", "**/json4cpp/**", ".github/workflows/json4cpp-*.yml"],
    "cgit": ["cgit/**", "**/cgit/**", ".github/workflows/cgit-*.yml"],
    "forgewrapper": [
        "forgewrapper/**",
        "**/forgewrapper/**",
        ".github/workflows/forgewrapper-*.yml",
    ],
    "genqrcode": [
        "genqrcode/**",
        "**/genqrcode/**",
        ".github/workflows/genqrcode-*.yml",
    ],
    "libnbtplusplus": [
        "libnbtplusplus/**",
        "**/libnbtplusplus/**",
        ".github/workflows/libnbtplusplus-*.yml",
    ],
    "neozip": ["neozip/**", "**/neozip/**", ".github/workflows/neozip-*.yml"],
    "tomlplusplus": [
        "tomlplusplus/**",
        "**/tomlplusplus/**",
        ".github/workflows/tomlplusplus-*.yml",
    ],
}

# Each product owns a discrete set of distributed workflow files that live in
# its own GitHub org repo (e.g. MeshMC-MMCO-Infra/foreman). When Foreman sees a
# push touching the product's paths it dispatches every workflow in this list
# in parallel ("the rain"). Callbacks come back as start/finish events.
_PRODUCT_WORKFLOWS: dict[str, list[tuple[str, str]]] = {
    "cmark": [
        ("cmark-ci.yml", "CI"),
        ("cmark-fuzz.yml", "Fuzz"),
    ],
    "corebinutils": [
        ("corebinutils-ci.yml", "CI"),
    ],
    "meshmc": [
        ("meshmc-build.yml", "Build"),
        ("meshmc-nix.yml", "Nix Build"),
        ("meshmc-codeql.yml", "CodeQL"),
        ("meshmc-container.yml", "Container Build"),
    ],
    "mnv": [
        ("mnv-ci.yml", "CI"),
        ("mnv-codeql.yml", "CodeQL"),
        ("mnv-link-check.yml", "Link Check"),
    ],
    "json4cpp": [
        ("json4cpp-ci.yml", "CI"),
        ("json4cpp-amalgam.yml", "Amalgam"),
        ("json4cpp-fuzz.yml", "Fuzz"),
        ("json4cpp-publish-docs.yml", "Publish Docs"),
    ],
    "cgit": [
        ("cgit-ci.yml", "CI"),
    ],
    "forgewrapper": [
        ("forgewrapper-build.yml", "Build"),
    ],
    "genqrcode": [
        ("genqrcode-ci.yml", "CI"),
    ],
    "libnbtplusplus": [
        ("libnbtplusplus-ci.yml", "CI"),
    ],
    "neozip": [
        ("neozip-analyze.yml", "Analyze"),
        ("neozip-cmake.yml", "CMake"),
        ("neozip-codeql.yml", "CodeQL"),
        ("neozip-configure.yml", "Configure"),
        ("neozip-fuzz.yml", "Fuzz"),
        ("neozip-libpng.yml", "libpng"),
        ("neozip-link.yml", "Link"),
        ("neozip-lint.yml", "Lint"),
        ("neozip-osb.yml", "OSB"),
        ("neozip-pigz.yml", "pigz"),
        ("neozip-pkgcheck.yml", "Pkgcheck"),
    ],
    "tomlplusplus": [
        ("tomlplusplus-ci.yml", "CI"),
        ("tomlplusplus-fuzz.yml", "Fuzz"),
    ],
}


def _build_product_rules() -> list["WorkflowDispatchRule"]:
    rules: list[WorkflowDispatchRule] = []
    for product, workflows in _PRODUCT_WORKFLOWS.items():
        owner = _PRODUCT_FORGE_OWNERS[product]
        paths = _PRODUCT_PATHS[product]
        for workflow_id, workflow_name in workflows:
            slug = workflow_id.removesuffix(".yml")
            base_kwargs = dict(
                workflow_id=workflow_id,
                workflow_name=f"{product.upper()} {workflow_name}",
                owner=owner,
                repo="foreman",
                ref="master",
                target_repo="workflow",
                path_patterns=paths,
                forward_source_repository=True,
                forward_source_ref=True,
                forward_source_sha=True,
                forward_event_metadata=True,
                github_status_context=f"{product}/{slug}",
            )
            rules.append(
                WorkflowDispatchRule(
                    name=f"{product}-pr-{slug}",
                    triggers=["github_pull_request", "github_pull_request_comment", "gitlab_merge_request"],
                    forward_pr_metadata=True,
                    **base_kwargs,
                )
            )
            rules.append(
                WorkflowDispatchRule(
                    name=f"{product}-push-{slug}",
                    triggers=["github_push"],
                    branches=["master", "beta"],
                    forward_before_sha=True,
                    **base_kwargs,
                )
            )
            rules.append(
                WorkflowDispatchRule(
                    name=f"{product}-retry-{slug}",
                    triggers=["github_issue_retry", "gitlab_issue_retry"],
                    **base_kwargs,
                )
            )
    return rules


def default_workflow_dispatch_rules() -> list[WorkflowDispatchRule]:
    rules: list[WorkflowDispatchRule] = list(_build_product_rules())
    rules += [
        WorkflowDispatchRule(
            name="github-master-infra-action",
            workflow_id="infra-action-ci.yml",
            workflow_name="Action CI",
            triggers=["github_push"],
            branches=["master"],
            target_repo="workflow",
            github_status_context="infra-action-ci",
            path_patterns=[
                ".github/actions/**",
                ".github/workflows/infra-action-ci.yml",
                ".github/workflows/infra-test.yml",
            ],
            ignore_path_patterns=[".gitignore", "LICENSE", "README.md", "action.yml"],
            forward_source_repository=True,
            forward_source_ref=True,
        ),
        WorkflowDispatchRule(
            name="github-master-infra-test",
            workflow_id="infra-test.yml",
            workflow_name="Test build with published image",
            triggers=["github_push"],
            branches=["master"],
            target_repo="workflow",
            github_status_context="infra-test",
            path_patterns=[
                ".github/actions/**",
                ".github/workflows/infra-action-ci.yml",
                ".github/workflows/infra-test.yml",
            ],
        ),
        WorkflowDispatchRule(
            name="github-master-scorecards",
            workflow_id="repo-scorecards.yml",
            workflow_name="Scorecard supply-chain security",
            triggers=["github_push"],
            branches=["master"],
            target_repo="workflow",
            github_status_context="repo-scorecards",
            path_patterns=[
                ".github/workflows/repo-scorecards.yml",
                ".github/workflows/repo-dependency-review.yml",
                "SECURITY",
                "SECURITY.md",
                "README",
                "README.md",
            ],
            forward_source_repository=True,
            forward_source_ref=True,
        ),
        WorkflowDispatchRule(
            name="github-master-meshmc-flatpak-deploy",
            workflow_id="meshmc-flatpak-deploy.yml",
            workflow_name="Deploy to repository",
            triggers=["github_push"],
            branches=["master"],
            target_repo="workflow",
            github_status_context="meshmc-flatpak-deploy",
            path_patterns=[
                "scripts/**",
                "shared-modules/**",
                "static/**",
                ".github/workflows/meshmc-flatpak-build.yml",
                ".github/workflows/meshmc-flatpak-deploy.yml",
            ],
            forward_source_repository=True,
            forward_source_ref=True,
        ),
        WorkflowDispatchRule(
            name="github-tag-release-sources",
            workflow_id="release-sources.yml",
            workflow_name="Release: Package Source Archives",
            triggers=["github_tag_push"],
            tag_patterns=["*"],
            target_repo="workflow",
            github_status_context="release-sources",
            tag_input_name="version",
            forward_source_repository=True,
            forward_source_ref=True,
            forward_source_sha=True,
        ),
        # Schedule rules — Foreman's internal cron service ticks each named
        # schedule and dispatches the underlying standalone workflow file in
        # the main repo. ci-schedule.yml has been retired.
        WorkflowDispatchRule(
            name="schedule-stale",
            workflow_id="repo-stale.yml",
            workflow_name="Stale Issues & PRs",
            triggers=["schedule"],
            schedule_names=["daily", "stale"],
            app_id="schedule-stale",
            target_repo="workflow",
            github_status_context="repo-stale",
        ),
        WorkflowDispatchRule(
            name="schedule-docker-images",
            workflow_id="images4docker-build.yml",
            workflow_name="Docker Image Rebuild",
            triggers=["schedule"],
            schedule_names=["daily", "docker-images"],
            app_id="schedule-docker-images",
            target_repo="workflow",
            github_status_context="docker-images",
        ),
        WorkflowDispatchRule(
            name="schedule-mnv-coverity",
            workflow_id="mnv-coverity.yml",
            workflow_name="MNV Coverity",
            triggers=["schedule"],
            schedule_names=["daily", "mnv-coverity"],
            app_id="schedule-mnv-coverity",
            target_repo="workflow",
            github_status_context="mnv-coverity",
        ),
        WorkflowDispatchRule(
            name="schedule-mnv-link-check",
            workflow_id="mnv-link-check.yml",
            workflow_name="MNV Link Check",
            triggers=["schedule"],
            schedule_names=["weekly-sun", "mnv-link-check"],
            app_id="schedule-mnv-link-check",
            target_repo="workflow",
            github_status_context="mnv-link-check",
        ),
        WorkflowDispatchRule(
            name="schedule-mnv-codeql",
            workflow_id="mnv-codeql.yml",
            workflow_name="MNV CodeQL",
            triggers=["schedule"],
            schedule_names=["weekly-sun", "mnv-codeql"],
            app_id="schedule-mnv-codeql",
            target_repo="workflow",
            github_status_context="mnv-codeql",
        ),
        WorkflowDispatchRule(
            name="schedule-neozip-codeql",
            workflow_id="neozip-codeql.yml",
            workflow_name="NeoZip CodeQL",
            triggers=["schedule"],
            schedule_names=["weekly-sun", "neozip-codeql"],
            app_id="schedule-neozip-codeql",
            target_repo="workflow",
            github_status_context="neozip-codeql",
        ),
        WorkflowDispatchRule(
            name="schedule-meshmc-flake-update",
            workflow_id="meshmc-flake-update.yml",
            workflow_name="MeshMC Flake Update",
            triggers=["schedule"],
            schedule_names=["weekly-sun", "meshmc-flake-update"],
            app_id="schedule-meshmc-flake-update",
            target_repo="workflow",
            github_status_context="meshmc-flake-update",
        ),
        WorkflowDispatchRule(
            name="schedule-scorecards",
            workflow_id="repo-scorecards.yml",
            workflow_name="OpenSSF Scorecard",
            triggers=["schedule"],
            schedule_names=["weekly-sun", "scorecards"],
            app_id="schedule-scorecards",
            target_repo="workflow",
            github_status_context="repo-scorecards",
        ),
        WorkflowDispatchRule(
            name="schedule-json4cpp-flawfinder",
            workflow_id="json4cpp-flawfinder.yml",
            workflow_name="JSON4CPP Flawfinder",
            triggers=["schedule"],
            schedule_names=["weekly-wed", "json4cpp-flawfinder"],
            app_id="schedule-json4cpp-flawfinder",
            target_repo="workflow",
            github_status_context="json4cpp-flawfinder",
        ),
        WorkflowDispatchRule(
            name="schedule-json4cpp-semgrep",
            workflow_id="json4cpp-semgrep.yml",
            workflow_name="JSON4CPP Semgrep",
            triggers=["schedule"],
            schedule_names=["weekly-thu", "json4cpp-semgrep"],
            app_id="schedule-json4cpp-semgrep",
            target_repo="workflow",
            github_status_context="json4cpp-semgrep",
        ),
        WorkflowDispatchRule(
            name="schedule-infra-action",
            workflow_id="infra-action-ci.yml",
            workflow_name="Action CI",
            triggers=["schedule"],
            schedule_names=["weekly-sun", "infra-action-ci"],
            app_id="schedule-infra-action-ci",
            target_repo="workflow",
            github_status_context="infra-action-ci",
        ),
        WorkflowDispatchRule(
            name="schedule-infra-docker",
            workflow_id="infra-docker.yml",
            workflow_name="Docker images",
            triggers=["schedule"],
            schedule_names=["weekly-sun", "infra-docker"],
            app_id="schedule-infra-docker",
            target_repo="workflow",
            github_status_context="infra-docker",
        ),
        WorkflowDispatchRule(
            name="schedule-meshmc-flatpak-update",
            workflow_id="meshmc-flatpak-update.yml",
            workflow_name="MeshMC Flatpak Update",
            triggers=["schedule"],
            schedule_names=["daily", "meshmc-flatpak-update"],
            app_id="schedule-meshmc-flatpak-update",
            target_repo="workflow",
            github_status_context="meshmc-flatpak-update",
        ),
        WorkflowDispatchRule(
            name="schedule-meshmc-flatpak-deploy",
            workflow_id="meshmc-flatpak-deploy.yml",
            workflow_name="MeshMC Flatpak Deploy",
            triggers=["schedule"],
            schedule_names=["daily", "meshmc-flatpak-deploy"],
            app_id="schedule-meshmc-flatpak-deploy",
            target_repo="workflow",
            github_status_context="meshmc-flatpak-deploy",
        ),
    ]
    return rules


_DEFAULT_FOREMAN_SCHEDULES: list[dict[str, str]] = [
    {"name": "daily", "cron": "0 3 * * *"},
    {"name": "weekly-sun", "cron": "0 4 * * 0"},
    {"name": "weekly-wed", "cron": "0 14 * * 3"},
    {"name": "weekly-thu", "cron": "0 2 * * 4"},
]


class ForemanScheduleEntry(BaseModel):
    name: str
    cron: str
    enabled: bool = True


def default_foreman_schedules() -> list[ForemanScheduleEntry]:
    return [ForemanScheduleEntry(**entry) for entry in _DEFAULT_FOREMAN_SCHEDULES]



class Settings(BaseSettings):
    admin_token: str = "raeVenga1eez3Geeca"
    base_url: str = "http://localhost:8000"
    database_url: str = "postgresql+psycopg://postgres:postgres@db:5432/test_db"
    database_replica_url: str | None = None
    debug: bool = False
    gitlab_api_token: str | None = None
    gitlab_base_url: str = "https://git.projecttick.org"
    gitlab_webhook_secret: str = "test_gitlab_webhook_secret"
    github_token: str = "test_github_token"
    github_status_token: str = "test_github_status_token"
    github_webhook_secret: str = "test_webhook_secret"
    github_org: str = "project-tick"
    github_ci_repo: str = "Project-Tick"
    github_ci_ref: str = "master"
    workflow_repo: str = "foreman"
    admin_team: str = "project-tick/maintainers"
    gitlab_admin_mention: str | None = None
    statuspage_url: str = "https://status.projecttick.org"
    sentry_dsn: str | None = None
    ff_admin_ping_comment: bool = True
    ff_disable_test_builds: bool = False
    max_concurrent_builds: int = 15
    workflow_dispatch_rules: list[WorkflowDispatchRule] = Field(
        default_factory=default_workflow_dispatch_rules
    )
    foreman_schedules: list[ForemanScheduleEntry] = Field(
        default_factory=default_foreman_schedules
    )
    scheduler_enabled: bool = True
    # All pipeline failures open issues in this GitLab project (NOT GitHub).
    failure_issue_gitlab_project: str = "project-tick/project-tick"
    failure_issue_gitlab_base_url: str | None = None
    changes_history_limit: int = 200

    model_config = SettingsConfigDict(
        env_file=".env",
        env_file_encoding="utf-8",
    )


settings = Settings()
