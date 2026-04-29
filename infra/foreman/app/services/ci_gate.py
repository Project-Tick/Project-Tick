import asyncio
import re
import shutil
from pathlib import Path
from tempfile import TemporaryDirectory
from typing import Any

import structlog

from app.config import settings
from app.models import Pipeline
from app.schemas.pipelines import GitHubCIGatePlanRequest

logger = structlog.get_logger(__name__)

PROJECTS = (
    "archived",
    "cgit",
    "ci",
    "cmark",
    "corebinutils",
    "forgewrapper",
    "genqrcode",
    "hooks",
    "images4docker",
    "json4cpp",
    "libnbtplusplus",
    "meshmc",
    "meta",
    "mnv",
    "neozip",
    "tomlplusplus",
)

KNOWN_TOP_LEVEL_DIRS = set(PROJECTS) | {".github", "LICENSES"}
PR_EVENTS = {"pull_request", "pull_request_target"}
RELEASE_TAG_PREFIXES = ("meshmc", "neozip", "mnv", "cmark", "forgewrapper")


def _string(value: Any) -> str:
    if value is None:
        return ""
    return str(value).strip()


def _truthy(value: Any) -> bool:
    if isinstance(value, bool):
        return value
    if isinstance(value, (int, float)):
        return bool(value)
    return _string(value).lower() in {"1", "true", "yes", "on"}


def _labels(value: Any) -> list[str]:
    if isinstance(value, list):
        return [item for item in (_string(item) for item in value) if item]
    return []


class CIGateService:
    async def build_plan_from_request(
        self,
        request: GitHubCIGatePlanRequest,
    ) -> dict[str, Any]:
        try:
            context = self._build_context(request.model_dump())
            if context["event_name"] == "push" and context["source_ref"].startswith(
                "refs/tags/"
            ):
                changed_files, commit_message = [], ""
            else:
                (
                    changed_files,
                    commit_message,
                ) = await self._collect_changed_files_and_commit_message(context)
            return self._build_plan(context, changed_files, commit_message)
        except Exception as e:
            logger.exception("gate_plan_resolution_failed")
            # Hata durumunda güvenli bir "default" plan dön ki 502 vermesin
            return {"error": str(e), "jobs": {}, "force_all": False}

    async def build_plan_from_pipeline(self, pipeline: Pipeline) -> dict[str, Any]:
        context = self._build_context_from_pipeline(pipeline)
        # For tag pushes, skip git analysis — tags just reference existing commits
        if context["event_name"] == "push" and context["source_ref"].startswith(
            "refs/tags/"
        ):
            changed_files, commit_message = [], ""
        else:
            (
                changed_files,
                commit_message,
            ) = await self._collect_changed_files_and_commit_message(context)
        return self._build_plan(context, changed_files, commit_message)

    def _build_context(self, raw: dict[str, Any]) -> dict[str, Any]:
        repository = _string(raw.get("repository"))
        source_repository = _string(raw.get("source_repository"))
        source_ref = _string(raw.get("source_ref"))
        base_ref = _string(raw.get("base_ref") or raw.get("merge_group_base_ref"))
        build_type_input = _string(raw.get("build_type"))

        return {
            "workflow_name": _string(
                raw.get("workflow_name") or settings.github_ci_workflow
            ),
            "repository": repository,
            "source_repository": source_repository,
            "repo_url": self._resolve_repo_url(repository, source_repository),
            "source_ref": source_ref,
            "source_sha": _string(raw.get("source_sha")),
            "event_name": _string(raw.get("event_name")).lower(),
            "actor": _string(raw.get("actor")),
            "before_sha": _string(raw.get("before_sha")),
            "head_ref": _string(raw.get("head_ref")),
            "base_ref": base_ref,
            "pr_number": _string(raw.get("pr_number")),
            "pr_base_sha": _string(raw.get("pr_base_sha")),
            "pr_head_sha": _string(raw.get("pr_head_sha")),
            "pr_title": _string(raw.get("pr_title")),
            "pr_draft": _truthy(raw.get("pr_draft")),
            "pr_merged": _truthy(raw.get("pr_merged")),
            "pr_labels": _labels(raw.get("pr_labels")),
            "force_all": _truthy(raw.get("force_all")),
            "build_type_input": build_type_input,
            "schedule": _string(raw.get("schedule")),
        }

    def _build_context_from_pipeline(self, pipeline: Pipeline) -> dict[str, Any]:
        params = dict(pipeline.params or {})
        dispatch_inputs = params.get("dispatch_inputs")
        if not isinstance(dispatch_inputs, dict):
            dispatch_inputs = {}

        source_repository = _string(dispatch_inputs.get("source-repository"))
        source_ref = _string(dispatch_inputs.get("source-ref") or params.get("ref"))

        event_name = _string(
            params.get("github_event_name") or params.get("event_name")
        ).lower()
        if not event_name:
            if params.get("pr_number") or params.get("gitlab_merge_request_iid"):
                event_name = "pull_request"
            elif params.get("push"):
                event_name = "push"
            else:
                event_name = "workflow_dispatch"

        build_type_input = _string(dispatch_inputs.get("build-type"))
        if not build_type_input:
            stored_build_type = _string(params.get("build_type"))
            if stored_build_type in {"Debug", "Release"}:
                build_type_input = stored_build_type

        repo_name = _string(params.get("repo") or params.get("dispatch_repo"))

        return {
            "workflow_name": _string(
                params.get("workflow_name") or settings.github_ci_workflow
            ),
            "repository": repo_name,
            "source_repository": source_repository,
            "repo_url": self._resolve_repo_url(repo_name, source_repository),
            "source_ref": source_ref,
            "source_sha": _string(params.get("sha") or params.get("gitlab_source_sha")),
            "event_name": event_name,
            "actor": _string(params.get("actor")),
            "before_sha": _string(params.get("before_sha")),
            "head_ref": _string(
                params.get("head_ref") or params.get("gitlab_source_branch")
            ),
            "base_ref": _string(
                params.get("pr_target_branch")
                or params.get("gitlab_target_branch")
                or params.get("merge_group_base_ref")
            ),
            "pr_number": _string(
                params.get("pr_number") or params.get("gitlab_merge_request_iid")
            ),
            "pr_base_sha": _string(params.get("pr_base_sha")),
            "pr_head_sha": _string(
                params.get("pr_head_sha") or params.get("gitlab_source_sha")
            ),
            "pr_title": _string(params.get("pr_title")),
            "pr_draft": _truthy(params.get("pr_draft")),
            "pr_merged": _truthy(params.get("pr_merged")),
            "pr_labels": _labels(params.get("pr_labels")),
            "force_all": _truthy(params.get("force_all")),
            "build_type_input": build_type_input,
            "schedule": _string(params.get("schedule")),
        }

    def _resolve_repo_url(self, repository: str, source_repository: str) -> str:
        if source_repository:
            return source_repository
        if repository.startswith(("https://", "http://", "ssh://", "git@")):
            return repository
        if "/" in repository:
            return f"https://github.com/{repository}.git"
        return ""

    async def _collect_changed_files_and_commit_message(
        self,
        context: dict[str, Any],
    ) -> tuple[list[str], str]:
        commit_message = ""
        if context["event_name"] in PR_EVENTS and context["pr_title"]:
            commit_message = context["pr_title"]

        repo_url = context["repo_url"]
        if not repo_url:
            return ([], commit_message)

        if shutil.which("git") is None:
            raise RuntimeError("git is required to resolve CI gate plans")

        with TemporaryDirectory(prefix="foreman-ci-gate-") as temp_dir_name:
            temp_dir = Path(temp_dir_name)

            await self._run_git(temp_dir, "init")
            await self._run_git(temp_dir, "config", "advice.detachedHead", "false")
            await self._run_git(temp_dir, "remote", "add", "origin", repo_url)

            base_target, head_target = self._resolve_diff_targets(context)
            local_head = await self._fetch_target(
                temp_dir, head_target, "head", context
            )

            if base_target:
                local_base = await self._fetch_target(
                    temp_dir, base_target, "base", context
                )
                changed_files_output = await self._run_git(
                    temp_dir,
                    "diff",
                    "--name-only",
                    local_base,
                    local_head,
                )
            else:
                changed_files_output = await self._run_git(
                    temp_dir,
                    "diff-tree",
                    "--no-commit-id",
                    "--name-only",
                    "-r",
                    local_head,
                )

            if not commit_message:
                commit_message = await self._run_git(
                    temp_dir,
                    "log",
                    "-1",
                    "--format=%s",
                    local_head,
                )

        changed_files = [
            line for line in changed_files_output.splitlines() if line.strip()
        ]
        return (changed_files, commit_message.strip())

    def _resolve_diff_targets(self, context: dict[str, Any]) -> tuple[str | None, str]:
        source_ref = context["source_ref"]
        source_sha = context["source_sha"]
        base_ref = context["base_ref"]
        before_sha = context["before_sha"]
        event_name = context["event_name"]

        if event_name in PR_EVENTS and base_ref and source_ref:
            return (f"refs/heads/{base_ref}", source_ref)

        if event_name == "merge_group" and base_ref and source_ref:
            return (f"refs/heads/{base_ref}", source_ref)

        if (
            event_name == "push"
            and source_sha
            and before_sha
            and before_sha != "0" * 40
        ):
            return (before_sha, source_sha)

        if base_ref and source_ref and source_ref != f"refs/heads/{base_ref}":
            return (f"refs/heads/{base_ref}", source_ref)

        head_target = source_ref or source_sha
        if not head_target:
            raise ValueError("Missing source ref or SHA for CI gate analysis")

        return (None, head_target)

    async def _fetch_target(
        self, temp_dir: Path, target: str, name: str, context: dict[str, Any] = None
    ) -> str:
        local_ref = f"refs/foreman/{name}"

        # İlk deneme: Belirtilen hedefi (ref veya SHA) çek
        try:
            await self._run_git(
                temp_dir,
                "fetch",
                "--no-tags",
                "--depth",
                "200",
                "origin",
                f"{target}:{local_ref}",
            )
            return local_ref
        except RuntimeError:
            logger.warning("fetch_failed_trying_fallback", target=target)

        # İkinci deneme: Eğer target bir ref ise ve başarısız olduysa, SHA ile dene
        sha = context.get("source_sha") if context else None
        if sha and target != sha:
            try:
                await self._run_git(
                    temp_dir, "fetch", "--no-tags", "--depth", "200", "origin", sha
                )
                fetched_sha = await self._run_git(temp_dir, "rev-parse", "FETCH_HEAD")
                await self._run_git(
                    temp_dir, "update-ref", local_ref, fetched_sha.strip()
                )
                return local_ref
            except RuntimeError:
                logger.error("sha_fetch_failed", sha=sha)

        # Son çare: Hiçbir şey çekilemediyse hata fırlat ki boş plan üretilmesin
        raise RuntimeError(f"Could not fetch target {target} or fallback SHA")

    async def _run_git(self, temp_dir: Path, *args: str) -> str:
        process = await asyncio.create_subprocess_exec(
            "git",
            *args,
            cwd=str(temp_dir),
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
        )
        stdout, stderr = await process.communicate()

        if process.returncode != 0:
            raise RuntimeError(
                f"git {' '.join(args)} failed: {stderr.decode('utf-8', errors='ignore').strip()}"
            )

        return stdout.decode("utf-8", errors="ignore")

    def _build_plan(
        self,
        context: dict[str, Any],
        changed_files: list[str],
        commit_message: str,
    ) -> dict[str, Any]:
        ref = context["source_ref"]
        event_name = context["event_name"]
        actor = context["actor"]
        head_ref = context["head_ref"]
        base_ref = context["base_ref"]

        is_push = event_name == "push"
        is_pr = event_name in PR_EVENTS
        is_merge_queue = event_name == "merge_group"
        is_tag = ref.startswith("refs/tags/")
        _tag_name = ref.removeprefix("refs/tags/") if is_tag else ""
        is_beta_tag = is_tag and _tag_name.startswith("vBETA")
        is_lts_tag = is_tag and _tag_name.startswith("vLTS")
        if is_beta_tag:
            tag_channel = "beta"
        elif is_lts_tag:
            tag_channel = "lts"
        elif is_tag:
            tag_channel = "stable"
        else:
            tag_channel = ""
        is_release_tag = any(
            ref.startswith(f"refs/tags/{prefix}-") for prefix in RELEASE_TAG_PREFIXES
        )
        is_backport = head_ref.startswith("backport-") or head_ref.startswith(
            "backport/"
        )
        is_dependabot = actor == "dependabot[bot]"
        is_master = ref in {"refs/heads/master", "refs/heads/main"}
        is_scheduled = event_name == "schedule"
        workflow_enabled = not (
            (event_name == "pull_request_target" and not context["pr_merged"])
            or (event_name == "pull_request" and context["pr_draft"])
        )

        if is_merge_queue or context["force_all"]:
            run_level = "full"
        elif is_dependabot:
            run_level = "minimal"
        else:
            run_level = "standard"

        build_type = "Debug"
        if is_tag:
            build_type = "Release"
        elif context["build_type_input"] in {"Debug", "Release"}:
            build_type = context["build_type_input"]

        change_flags = {f"{project}_changed": False for project in PROJECTS}
        changed_projects: list[str] = []

        for project in PROJECTS:
            changed = any(
                path == project or path.startswith(f"{project}/")
                for path in changed_files
            )
            change_flags[f"{project}_changed"] = changed
            if changed:
                changed_projects.append(project)

        github_changed = any(
            path == ".github" or path.startswith(".github/") for path in changed_files
        )
        if github_changed:
            changed_projects.append(".github")

        root_changed = False
        for path in changed_files:
            top_level = path.split("/", 1)[0]
            if top_level not in KNOWN_TOP_LEVEL_DIRS:
                root_changed = True
                break

        if root_changed:
            changed_projects.append("root")

        if is_push and is_master and root_changed:
            change_flags["meshmc_changed"] = True
            if "meshmc" not in changed_projects:
                changed_projects.append("meshmc")

        commit_type = ""
        commit_scope = ""
        commit_subject = commit_message
        commit_breaking = False
        match = re.match(r"^([a-zA-Z]+)(\(([^)]*)\))?(!)?\:\ (.+)$", commit_message)
        if match:
            commit_type = match.group(1) or ""
            commit_scope = match.group(3) or ""
            commit_breaking = match.group(4) == "!"
            commit_subject = match.group(5) or commit_message

        jobs = {
            "lint": workflow_enabled and is_pr,
            "dependency_review": workflow_enabled and is_pr,
            "mnv": workflow_enabled
            and (change_flags["mnv_changed"] or run_level == "full"),
            "cgit": workflow_enabled
            and (change_flags["cgit_changed"] or run_level == "full"),
            "cmark": workflow_enabled
            and (change_flags["cmark_changed"] or run_level == "full"),
            "corebinutils": workflow_enabled
            and (change_flags["corebinutils_changed"] or run_level == "full"),
            "genqrcode": workflow_enabled
            and (change_flags["genqrcode_changed"] or run_level == "full"),
            "neozip": workflow_enabled
            and (change_flags["neozip_changed"] or run_level == "full"),
            "json4cpp": workflow_enabled
            and (change_flags["json4cpp_changed"] or run_level == "full"),
            "libnbtplusplus": workflow_enabled
            and (change_flags["libnbtplusplus_changed"] or run_level == "full"),
            "tomlplusplus": workflow_enabled
            and (change_flags["tomlplusplus_changed"] or run_level == "full"),
            "meshmc": workflow_enabled
            and (change_flags["meshmc_changed"] or run_level == "full"),
            "forgewrapper": workflow_enabled
            and (change_flags["forgewrapper_changed"] or run_level == "full"),
            "images4docker": workflow_enabled
            and (change_flags["images4docker_changed"] or run_level == "full"),
            "cmark_fuzz": workflow_enabled
            and run_level == "full"
            and (change_flags["cmark_changed"] or run_level == "full"),
            "json4cpp_fuzz": workflow_enabled
            and run_level == "full"
            and (change_flags["json4cpp_changed"] or run_level == "full"),
            "json4cpp_amalgam": workflow_enabled
            and (change_flags["json4cpp_changed"] or run_level == "full"),
            "tomlplusplus_fuzz": workflow_enabled
            and run_level == "full"
            and (change_flags["tomlplusplus_changed"] or run_level == "full"),
            "neozip_fuzz": workflow_enabled
            and run_level == "full"
            and (change_flags["neozip_changed"] or run_level == "full"),
            "meshmc_codeql": workflow_enabled
            and run_level == "full"
            and (change_flags["meshmc_changed"] or run_level == "full"),
            "mnv_codeql": workflow_enabled
            and run_level == "full"
            and (change_flags["mnv_changed"] or run_level == "full"),
            "neozip_codeql": workflow_enabled
            and run_level == "full"
            and (change_flags["neozip_changed"] or run_level == "full"),
            "meshmc_container": workflow_enabled
            and run_level == "full"
            and (change_flags["meshmc_changed"] or run_level == "full"),
            "meshmc_nix": workflow_enabled
            and run_level == "full"
            and (change_flags["meshmc_changed"] or run_level == "full"),
            "json4cpp_docs": workflow_enabled
            and is_master
            and is_push
            and change_flags["json4cpp_changed"],
            "backport": workflow_enabled
            and event_name == "pull_request_target"
            and context["pr_merged"]
            and "backport" in context["pr_labels"],
            "merge_blocking": workflow_enabled
            and event_name == "pull_request_target"
            and context["pr_merged"]
            and "status: blocking" in context["pr_labels"],
            "neozip_release": workflow_enabled and ref.startswith("refs/tags/neozip-"),
            "release_sources": workflow_enabled and is_tag,
            # For beta tags: source artifacts only — skip meshmc binary compilation.
            "beta_source_only": is_beta_tag,
        }

        if is_tag:
            jobs = {key: False for key in jobs} | {
                "release_sources": workflow_enabled,
                "beta_source_only": is_beta_tag,
            }

        return {
            "is_push": is_push,
            "is_pr": is_pr,
            "is_merge_queue": is_merge_queue,
            "is_tag": is_tag,
            "is_beta_tag": is_beta_tag,
            "is_lts_tag": is_lts_tag,
            "tag_channel": tag_channel,
            "is_release_tag": is_release_tag,
            "is_backport": is_backport,
            "is_dependabot": is_dependabot,
            "is_master": is_master,
            "is_scheduled": is_scheduled,
            "run_level": run_level,
            **change_flags,
            "github_changed": github_changed,
            "root_changed": root_changed,
            "changed_projects": ",".join(changed_projects),
            "changed_count": len(changed_projects),
            "commit_type": commit_type,
            "commit_scope": commit_scope,
            "commit_subject": commit_subject,
            "commit_breaking": commit_breaking,
            "commit_message": commit_message,
            "build_type": build_type,
            "jobs": jobs,
        }
