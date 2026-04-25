# Project Tick — CI/CD Pipeline

## Overview

Project Tick uses a multi-layered CI/CD pipeline that orchestrates builds,
tests, security scans, and releases across all sub-projects in the monorepo.
The pipeline combines GitHub Actions, Nix-based tooling, and the custom
tickborg distributed CI system.

---

## Architecture

### Three-Layer CI Strategy

```
Layer 1: GitHub Actions (ci.yml orchestrator)
    ├── Event classification and change detection
    ├── Per-project workflow dispatch
    └── Release and publishing workflows

Layer 2: CI Tooling (ci/ directory)
    ├── treefmt (multi-language formatting)
    ├── codeowners-validator
    ├── commit linting (Conventional Commits)
    └── Pinned Nix environment

Layer 3: tickborg (ofborg/ distributed CI)
    ├── RabbitMQ-based job distribution
    ├── Multi-platform build execution
    └── GitHub check run reporting
```

---

## GitHub Actions — The Orchestrator

### ci.yml — Monolithic Gate

The primary CI workflow (`ci.yml`) is the single entry point for all CI
activity. Every push, pull request, merge queue entry, tag push, and manual
dispatch flows through this workflow.

#### Trigger Events

```yaml
on:
  push:
    branches: ["**"]
    tags: ["*"]
  pull_request:
    types: [opened, synchronize, reopened, ready_for_review]
  pull_request_target:
    types: [closed, labeled]
  merge_group:
    types: [checks_requested]
  workflow_dispatch:
    inputs:
      force-all:
        description: "Force run all project CI pipelines"
        type: boolean
        default: false
      build-type:
        description: "Build configuration for meshmc/forgewrapper"
        type: choice
        options: [Debug, Release]
        default: Debug
```

#### Permissions

The orchestrator runs with minimal permissions:

```yaml
permissions:
  contents: read
```

#### Concurrency Control

```yaml
concurrency:
  group: >-
    ci-${{
      github.event_name == 'merge_group' && github.event.merge_group.head_ref ||
      github.event_name == 'pull_request' && format('pr-{0}', github.event.pull_request.number) ||
      github.ref
    }}
  cancel-in-progress: ${{ github.event_name != 'merge_group' }}
```

| Event | Concurrency Group | Cancel In-Progress |
|-------|-------------------|--------------------|
| Merge queue | `ci-<head_ref>` | No |
| Pull request | `ci-pr-<number>` | Yes |
| Push | `ci-<ref>` | Yes |

Merge queue runs are never cancelled to maintain queue integrity.

#### Stage 0: Gate & Triage

The `gate` job is the first job that runs. It:

1. **Classifies the event:** push, PR, merge queue, tag, backport, dependabot,
   scheduled, etc.
2. **Detects changed files:** Maps file paths to sub-project flags
3. **Sets run level:** `minimal`, `standard`, or `full`
4. **Exports output variables** for downstream jobs:
   - Event classification flags (`is_push`, `is_pr`, `is_merge_queue`, etc.)
   - Per-project change flags (`meshmc_changed`, `neozip_changed`, etc.)
   - Run level for downstream decisions

Draft PRs are automatically skipped:
```yaml
if: >-
  !(github.event_name == 'pull_request' && github.event.pull_request.draft)
```

### ci-lint.yml — Lint & Checks

Called from `ci.yml` before builds start. Runs commit message validation and
formatting checks.

#### Commit Message Linting

Uses `ci/github-script/lint-commits.js` via `actions/github-script`:

```yaml
- name: Lint commit messages
  uses: actions/github-script@v7
```

The linter validates Conventional Commits format:
```
type(scope): description
```

Valid types: `feat`, `fix`, `docs`, `style`, `refactor`, `perf`, `test`,
`build`, `ci`, `chore`, `revert`.

#### Security Hardening

All CI jobs use `step-security/harden-runner`:

```yaml
- name: Harden runner
  uses: step-security/harden-runner@v2
  with:
    egress-policy: audit
```

---

## Workflow Inventory

### Per-Project CI Workflows

Each sub-project has dedicated CI workflows that build, test, and analyze
the component:

#### MeshMC (7 workflows)

| Workflow | Purpose |
|----------|---------|
| `meshmc-build.yml` | Multi-platform build matrix |
| `meshmc-codeql.yml` | CodeQL security analysis |
| `meshmc-container.yml` | Container (Docker/Podman) build |
| `meshmc-nix.yml` | Nix build verification |
| `meshmc-backport.yml` | Automated backport PR creation |
| `meshmc-blocked-prs.yml` | Track and manage blocked PRs |
| `meshmc-merge-blocking-pr.yml` | Merge queue blocking logic |
| `meshmc-flake-update.yml` | Automated Nix flake update |

#### NeoZip (12 workflows)

| Workflow | Purpose |
|----------|---------|
| `neozip-cmake.yml` | CMake build matrix |
| `neozip-configure.yml` | Autotools (configure) build |
| `neozip-analyze.yml` | Static analysis |
| `neozip-codeql.yml` | CodeQL security scanning |
| `neozip-fuzz.yml` | Fuzz testing |
| `neozip-lint.yml` | Code linting |
| `neozip-libpng.yml` | libpng integration test |
| `neozip-link.yml` | Link validation |
| `neozip-osb.yml` | OpenSSF Scorecard |
| `neozip-pigz.yml` | pigz compatibility test |
| `neozip-pkgcheck.yml` | Package check |
| `neozip-release.yml` | Release workflow |

#### json4cpp (7 workflows)

| Workflow | Purpose |
|----------|---------|
| `json4cpp-ci.yml` | Primary CI |
| `json4cpp-fuzz.yml` | Fuzz testing |
| `json4cpp-amalgam.yml` | Amalgamation (single-header) build |
| `json4cpp-amalgam-comment.yml` | Amalgamation PR comment |
| `json4cpp-flawfinder.yml` | Flawfinder static analysis |
| `json4cpp-semgrep.yml` | Semgrep security scanning |
| `json4cpp-publish-docs.yml` | Documentation publishing |

#### Other Sub-Projects

| Workflow | Sub-Project | Purpose |
|----------|------------|---------|
| `cmark-ci.yml` | cmark | Build and test |
| `cmark-fuzz.yml` | cmark | Fuzz testing |
| `tomlplusplus-ci.yml` | tomlplusplus | Build and test |
| `tomlplusplus-fuzz.yml` | tomlplusplus | Fuzz testing |
| `tomlplusplus-gh-pages.yml` | tomlplusplus | Documentation deployment |
| `mnv-ci.yml` | mnv | Build and test |
| `mnv-codeql.yml` | mnv | CodeQL analysis |
| `mnv-coverity.yml` | mnv | Coverity scan |
| `mnv-link-check.yml` | mnv | Documentation link check |
| `cgit-ci.yml` | cgit | Build and test |
| `corebinutils-ci.yml` | corebinutils | Build and test |
| `forgewrapper-build.yml` | forgewrapper | Gradle build |
| `libnbtplusplus-ci.yml` | libnbtplusplus | Build and test |
| `genqrcode-ci.yml` | genqrcode | Build and test |
| `images4docker-build.yml` | images4docker | Docker image build |

### Release & Publishing Workflows

| Workflow | Purpose |
|----------|---------|
| `release-sources.yml` | Build MeshMC release artifacts and source bundles |
| `meshmc-publish.yml` | Publish MeshMC artifacts |
| `neozip-release.yml` | Create NeoZip releases |

### Repository Maintenance Workflows

| Workflow | Purpose |
|----------|---------|
| `repo-dependency-review.yml` | Scan dependency changes for vulnerabilities |
| `repo-labeler.yml` | Auto-label PRs by changed paths |
| `repo-scorecards.yml` | OpenSSF Scorecard compliance tracking |
| `repo-stale.yml` | Mark and close stale issues/PRs |

---

## Change Detection

The CI orchestrator maps changed file paths to sub-project flags:

| Path Pattern | Flag | Sub-Project |
|-------------|------|-------------|
| `meshmc/**` | `meshmc_changed` | MeshMC |
| `neozip/**` | `neozip_changed` | NeoZip |
| `json4cpp/**` | `json4cpp_changed` | json4cpp |
| `tomlplusplus/**` | `tomlplusplus_changed` | tomlplusplus |
| `libnbtplusplus/**` | `libnbt_changed` | libnbt++ |
| `cmark/**` | `cmark_changed` | cmark |
| `genqrcode/**` | `genqrcode_changed` | genqrcode |
| `forgewrapper/**` | `forgewrapper_changed` | ForgeWrapper |
| `cgit/**` | `cgit_changed` | cgit |
| `corebinutils/**` | `corebinutils_changed` | CoreBinUtils |
| `mnv/**` | `mnv_changed` | MNV |
| `ofborg/**` | `ofborg_changed` | tickborg |
| `meta/**` | `meta_changed` | Meta |
| `images4docker/**` | `docker_changed` | Images4Docker |
| `ci/**` | `ci_changed` | CI tooling |
| `archived/**` | `archived_changed` | Archived |

### Force-All Mode

All projects are built when:
- `force-all` is set to `true` in a manual dispatch
- The event is a merge queue entry (`is_merge_queue`)

---

## tickborg — Distributed CI

tickborg is a RabbitMQ-based distributed CI system adapted from NixOS's
ofborg. It runs alongside GitHub Actions to provide:

### Capabilities

1. **Automatic change detection** — Detects changed sub-projects in PRs based
   on file paths and commit scopes
2. **Native build system execution** — Builds each project using its own build
   system (CMake, Meson, Make, Cargo, Gradle, Autotools)
3. **Multi-platform support** — Builds on 7 platform/architecture combinations
4. **GitHub integration** — Posts results as check runs and PR comments

### Platform Matrix

| Platform | Runner | Architecture |
|----------|--------|-------------|
| `x86_64-linux` | `ubuntu-latest` | x86_64 |
| `aarch64-linux` | `ubuntu-24.04-arm` | ARM64 |
| `x86_64-darwin` | `macos-15` | x86_64 |
| `aarch64-darwin` | `macos-15` | Apple Silicon |
| `x86_64-windows` | `windows-2025` | x86_64 |
| `aarch64-windows` | `windows-2025` | ARM64 |
| `x86_64-freebsd` | `ubuntu-latest` (VM) | x86_64 |

### Bot Commands

tickborg responds to `@tickbot` commands in PR comments:

```
@tickbot build meshmc neozip cmark     # Build specific projects
@tickbot test meshmc                    # Run tests for a project
@tickbot eval                           # Full evaluation (detect + label)
```

### WIP Suppression

PRs with titles starting with `WIP:` or containing `[WIP]` suppress
automatic builds.

### Commit-Based Triggers

tickborg reads Conventional Commits scopes to determine builds:

| Commit Message | Triggered Build |
|---------------|-----------------|
| `feat(meshmc): add chunk loading` | meshmc |
| `fix(neozip): handle empty archives` | neozip |
| `cmark: fix buffer overflow` | cmark |
| `chore(ci): update workflow` | (no build) |

---

## CI Tooling (ci/ Directory)

### Directory Structure

```
ci/
├── OWNERS                        # Code ownership
├── README.md                     # CI documentation
├── default.nix                   # Nix CI entry point
├── pinned.json                   # Pinned Nixpkgs revision
├── update-pinned.sh              # Update pinned.json
├── supportedBranches.js          # Branch classification
├── codeowners-validator/
│   ├── default.nix               # Build codeowners-validator
│   ├── owners-file-name.patch    # Patch for file naming
│   └── permissions.patch         # Patch for permissions
└── github-script/
    ├── run                       # CLI entry (local testing)
    ├── lint-commits.js           # Conventional Commits linter
    ├── prepare.js                # PR preparation/validation
    ├── reviews.js                # Review state management
    ├── get-pr-commit-details.js  # Extract PR commit info
    ├── withRateLimit.js          # GitHub API rate limiting
    ├── package.json              # npm dependencies
    └── shell.nix                 # Nix development shell
```

### treefmt Configuration

The CI `default.nix` configures treefmt with these formatters:

| Formatter | Language/Format | Settings |
|-----------|----------------|----------|
| `actionlint` | GitHub Actions YAML | Default |
| `biome` | JavaScript/TypeScript | Single quotes, no semicolons, editorconfig |
| `keep-sorted` | Any (annotated blocks) | Default |
| `nixfmt` | Nix | RFC style |
| `yamlfmt` | YAML | Retain line breaks |
| `zizmor` | GitHub Actions security | Default |

Files matching `*.min.js` are excluded from biome formatting.

### Pinned Nixpkgs

`ci/pinned.json` contains content-addressed references to:
- `nixpkgs` — The Nixpkgs revision used for CI tools
- `treefmt-nix` — The treefmt-nix revision

Updated via:
```bash
./ci/update-pinned.sh
```

### Local CI Testing

CI scripts can be tested locally:

```bash
cd ci/github-script
nix-shell     # or: nix develop
gh auth login
./run lint-commits <owner> <repo> <pr-number>
./run prepare <owner> <repo> <pr-number>
```

---

## Docker Images (images4docker)

### Purpose

The `images4docker/` directory provides 40 Dockerfiles for building MeshMC
across different Linux distributions and versions. Each image includes the
Qt 6 toolchain and all MeshMC build dependencies.

### Image Registry

Images are published to:
```
ghcr.io/project-tick-infra/images/<target_name>:<target_tag>
```

### Build Schedule

The `images4docker-build.yml` workflow runs:
- On push to `main` (when Dockerfiles, workflow, or README change)
- On a daily schedule at **03:17 UTC**

Currently 35 targets are actively built (Qt6-compatible set).

### Supported Package Managers

| Package Manager | Distributions |
|----------------|---------------|
| `apt` | Debian, Ubuntu |
| `dnf` | Fedora, RHEL, CentOS |
| `apk` | Alpine |
| `zypper` | openSUSE, SLES |
| `yum` | Older CentOS/RHEL |
| `pacman` | Arch Linux |
| `xbps` | Void Linux |
| `nix` | NixOS |
| `emerge` | Gentoo |

### Qt 6 Requirement

Qt 6 is **mandatory** for all images. If Qt 6 packages are unavailable on a
given distribution, the Docker build fails intentionally — there is no Qt 5
fallback. This ensures all CI builds use a consistent Qt version.

---

## Security Scanning

### CodeQL

CodeQL analysis runs for security-critical components:

| Component | Schedule | Languages |
|-----------|----------|-----------|
| meshmc | Per-PR, scheduled | C++ |
| mnv | Per-PR, scheduled | C |
| neozip | Per-PR, scheduled | C |

### Fuzz Testing

Continuous fuzz testing for parser and compression libraries:

| Component | Infrastructure | Workflow |
|-----------|---------------|----------|
| neozip | OSS-Fuzz + custom | `neozip-fuzz.yml` |
| json4cpp | OSS-Fuzz + custom | `json4cpp-fuzz.yml` |
| cmark | Custom fuzzers | `cmark-fuzz.yml` |
| tomlplusplus | Custom fuzzers | `tomlplusplus-fuzz.yml` |

### Static Analysis

| Tool | Component | Workflow |
|------|-----------|----------|
| Semgrep | json4cpp | `json4cpp-semgrep.yml` |
| Flawfinder | json4cpp | `json4cpp-flawfinder.yml` |
| Coverity | mnv | `mnv-coverity.yml` |
| clang-tidy | meshmc | Via `MeshMC_ENABLE_CLANG_TIDY` |

### Dependency Review

`repo-dependency-review.yml` scans dependency changes in PRs for known
vulnerabilities using GitHub's dependency review action.

### OpenSSF Scorecard

`repo-scorecards.yml` tracks the project's OpenSSF Scorecard score, measuring
security practices across dimensions like branch protection, dependency
updates, fuzzing, and signed releases.

---

## Release Pipeline

### MeshMC Releases

1. A release tag is pushed (e.g., `7.0.0`)
2. Foreman dispatches `release-sources.yml`
3. `release-sources.yml`:
  - Builds release binaries for all platforms
  - Packages source archives and deployment bundles
  - Creates the GitHub release draft with artifacts
4. `meshmc-publish.yml`:
   - Publishes artifacts to distribution channels

### NeoZip Releases

Similar tag-triggered flow via `neozip-release.yml`.

### Documentation Deployment

- `tomlplusplus-gh-pages.yml` — Deploys toml++ documentation to GitHub Pages
- `json4cpp-publish-docs.yml` — Publishes json4cpp API documentation

---

## Branch Classification

The `ci/supportedBranches.js` module classifies branches for CI decisions:

```javascript
const typeConfig = {
  master: ['development', 'primary'],
  release: ['development', 'primary'],
  staging: ['development', 'secondary'],
  'staging-next': ['development', 'secondary'],
  feature: ['wip'],
  fix: ['wip'],
  backport: ['wip'],
  revert: ['wip'],
  wip: ['wip'],
  dependabot: ['wip'],
}
```

Branch ordering (for base branch detection):
```javascript
const orderConfig = {
  master: 0,       // Highest priority
  release: 1,
  staging: 2,
  'staging-next': 3,
}
```

The `classify()` function parses branch names to extract:
- `prefix` — Branch type prefix
- `version` — Optional version number (e.g., `7.0`)
- `stable` — Whether the branch has a version (release branch)
- `type` — Classification from `typeConfig`
- `order` — Priority for base branch detection

---

## DCO Enforcement

The `.github/dco.yml` configuration:

```yaml
allowRemediationCommits:
  individual: false
```

This means:
- Every commit must have a `Signed-off-by` tag
- Remediation commits (adding sign-off after the fact) are **not** allowed
- Contributors must either sign off each commit individually or use
  `git rebase --signoff` to retroactively sign all commits

---

## Environment Variables

### Shared CI Environment

```yaml
env:
  CI: true
  FORCE_ALL: ${{ github.event.inputs.force-all == 'true' || github.event_name == 'merge_group' }}
```

### Per-Workflow Variables

Individual workflows may set additional variables specific to their build
systems (CMake flags, Cargo features, Gradle properties, etc.).

---

## Monitoring and Diagnostics

### CI Health Indicators

| Indicator | Source |
|-----------|--------|
| Build status badge | `meshmc-build.yml` badge in README |
| OpenSSF Scorecard | `repo-scorecards.yml` |
| Code coverage | Per-project coverage workflows |
| Dependency freshness | Dependabot/Renovate alerts |
| Stale issue count | `repo-stale.yml` |

### Debugging Failed Builds

1. Check the GitHub Actions run log
2. Identify which job failed (gate, lint, or per-project build)
3. For commit lint failures: fix commit message format
4. For build failures: reproduce locally using the same build system
5. For formatting failures: run treefmt locally via `nix develop -f ci/`
