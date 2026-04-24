#!/usr/bin/env python3
"""
Project Tick merge action.

Triggered by a PR comment matching:
  /merge:<branch> HEAD=<40-char sha>
  /merge HEAD=<40-char sha>          (uses PR's base branch)

Steps:
1. Verify the PR HEAD SHA matches the one in the command.
2. Clone the internal git repository.
3. Add GitHub as the upstream remote and fetch.
4. Merge the given commit into the target branch with --no-ff.
5. Push to internal git.
"""

import json
import logging
import os
import re
import subprocess
import sys
import tempfile

import github

logging.basicConfig(level=logging.INFO, format="%(levelname)s: %(message)s")

# Matches: /merge         HEAD=abc123...
#          /merge:main    HEAD=abc123...
#          /merge:v1.0.x  HEAD=abc123...
MERGE_RE = re.compile(
    r"^/merge(?::(?P<branch>[A-Za-z0-9._/+\-]+))?\s+HEAD=(?P<sha>[0-9a-f]{40})\b",
    re.IGNORECASE | re.MULTILINE,
)


def run(cmd: list[str], cwd: str | None = None) -> None:
    logging.info("+ %s", " ".join(cmd))
    subprocess.run(cmd, check=True, cwd=cwd)


def setup_ssh(ssh_key: str) -> None:
    """Write the deploy key and configure SSH to use it for git.projecttick.org."""
    ssh_dir = os.path.expanduser("~/.ssh")
    os.makedirs(ssh_dir, mode=0o700, exist_ok=True)

    key_path = os.path.join(ssh_dir, "deploy_key")
    with open(key_path, "w") as f:
        f.write(ssh_key)
        if not ssh_key.endswith("\n"):
            f.write("\n")
    os.chmod(key_path, 0o600)

    config_path = os.path.join(ssh_dir, "config")
    with open(config_path, "a") as f:
        f.write(
            "\nHost git.projecttick.org\n"
            f"    IdentityFile {key_path}\n"
            "    StrictHostKeyChecking no\n"
        )


def post_comment(pr: github.PullRequest.PullRequest, msg: str) -> None:
    try:
        pr.create_issue_comment(msg)
    except github.GithubException as exc:
        logging.warning("Could not post comment: %s", exc)


def main() -> None:
    # ── Read GitHub event ──────────────────────────────────────────────────────
    event_path = os.environ.get("GITHUB_EVENT_PATH", "")
    if not event_path:
        logging.error("GITHUB_EVENT_PATH is not set")
        sys.exit(1)

    with open(event_path) as f:
        event = json.load(f)

    if event.get("action") != "created":
        logging.info("Event action is %r, nothing to do", event.get("action"))
        sys.exit(0)

    issue = event.get("issue", {})
    if "pull_request" not in issue:
        logging.info("Comment is on an issue, not a pull request — skipping")
        sys.exit(0)

    comment_body: str = event.get("comment", {}).get("body", "")
    repo_full: str = event.get("repository", {}).get("full_name", "")
    repo_name: str = event.get("repository", {}).get("name", "")
    issue_number: int = issue.get("number", 0)

    if not issue_number or not repo_full:
        logging.error("Could not determine PR number or repository from event")
        sys.exit(1)

    # ── Parse /merge command ───────────────────────────────────────────────────
    match = MERGE_RE.search(comment_body)
    if not match:
        logging.info("No /merge command found in comment, nothing to do")
        sys.exit(0)

    target_branch: str | None = match.group("branch")
    expected_sha: str = match.group("sha").lower()

    logging.info(
        "/merge command: branch=%r sha=%s", target_branch or "(default)", expected_sha
    )

    # ── GitHub API ─────────────────────────────────────────────────────────────
    token = os.environ.get("INPUT_TOKEN") or os.environ.get("GITHUB_TOKEN", "")
    if not token:
        logging.error("No GitHub token available (INPUT_TOKEN / GITHUB_TOKEN)")
        sys.exit(1)

    gh = github.Github(auth=github.Auth.Token(token))

    try:
        gh_repo = gh.get_repo(repo_full)
        pr = gh_repo.get_pull(issue_number)
    except github.GithubException as exc:
        logging.error("Failed to fetch PR #%d from %s: %s", issue_number, repo_full, exc)
        sys.exit(1)

    # ── Verify SHA ─────────────────────────────────────────────────────────────
    actual_sha = pr.head.sha.lower()
    if actual_sha != expected_sha:
        msg = (
            f"❌ SHA mismatch: PR HEAD is `{actual_sha}` but the command "
            f"specified `{expected_sha}`. Update the command with the current HEAD SHA."
        )
        logging.error(msg)
        post_comment(pr, msg)
        sys.exit(1)

    # ── Resolve target branch ──────────────────────────────────────────────────
    if not target_branch:
        target_branch = pr.base.ref
        logging.info("No branch specified; using PR base branch: %s", target_branch)

    pr_head_ref = pr.head.ref
    logging.info(
        "Merging PR #%d (%s → %s)", issue_number, pr_head_ref, target_branch
    )

    # ── SSH setup ──────────────────────────────────────────────────────────────
    ssh_key = os.environ.get("INPUT_SSH_KEY", "")
    if ssh_key:
        setup_ssh(ssh_key)
    else:
        logging.warning("INPUT_SSH_KEY not set — assuming SSH agent is pre-configured")

    # ── Git identity ───────────────────────────────────────────────────────────
    run(["git", "config", "--global", "user.name", "Project Tick Bot"])
    run(["git", "config", "--global", "user.email", "bot@projecttick.org"])

    # ── Build remote URLs ──────────────────────────────────────────────────────
    internal_git_base = os.environ.get(
        "INPUT_INTERNAL_GIT_BASE",
        "git@git.projecttick.org/Project-Tick",
    )
    internal_git_url = f"{internal_git_base}/{repo_name}.git"
    github_url = f"https://x-access-token:{token}@github.com/{repo_full}.git"

    # ── Clone → merge → push ───────────────────────────────────────────────────
    with tempfile.TemporaryDirectory() as tmpdir:
        logging.info("Cloning internal git: %s", internal_git_url)
        run(["git", "clone", "--origin", "origin", internal_git_url, tmpdir])

        run(["git", "-C", tmpdir, "remote", "add", "upstream", github_url])
        run(["git", "-C", tmpdir, "fetch", "upstream"])

        # Check out the target branch from origin (must already exist)
        run(["git", "-C", tmpdir, "checkout", target_branch])

        # Sanity-check: the expected commit must be present after fetch
        probe = subprocess.run(
            ["git", "-C", tmpdir, "cat-file", "-e", expected_sha],
            capture_output=True,
        )
        if probe.returncode != 0:
            msg = f"❌ Commit `{expected_sha}` not found after fetching from GitHub."
            logging.error(msg)
            post_comment(pr, msg)
            sys.exit(1)

        # Merge
        merge_msg = f"Merge PR #{issue_number} ({pr_head_ref}) into {target_branch}"
        try:
            run(
                [
                    "git", "-C", tmpdir,
                    "merge", "--no-ff", expected_sha,
                    "-m", merge_msg,
                ]
            )
        except subprocess.CalledProcessError:
            msg = (
                f"❌ Merge conflict when merging into `{target_branch}`. "
                "Please resolve manually."
            )
            logging.error(msg)
            post_comment(pr, msg)
            sys.exit(1)

        # Push
        try:
            run(["git", "-C", tmpdir, "push", "origin", target_branch])
        except subprocess.CalledProcessError:
            msg = "❌ Failed to push to the internal git repository."
            logging.error(msg)
            post_comment(pr, msg)
            sys.exit(1)

    post_comment(
        pr,
        f"✅ PR #{issue_number} merged into `{target_branch}` on the internal git.",
    )
    logging.info("Done.")


if __name__ == "__main__":
    main()
