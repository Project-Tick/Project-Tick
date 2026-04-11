# Project Tick — Mono-Repo Architecture

## Architectural Philosophy

Project Tick is structured as a unified monorepo where each top-level directory
represents an independent component. This architecture provides:

- **Atomic cross-project changes** — A single commit can update a library and
  every consumer simultaneously, eliminating version skew.
- **Unified CI** — One orchestrator workflow (`ci.yml`) detects which
  sub-projects are affected by a change and dispatches builds accordingly.
- **Shared tooling** — Nix flakes, lefthook hooks, REUSE compliance, and
  code formatting apply uniformly across the entire tree.
- **Independent buildability** — Despite living in one repository, each
  sub-project maintains its own build system and can be built in isolation.

---

## Repository Layout

```
Project-Tick/
├── .github/                    # GitHub Actions, issue templates, CODEOWNERS
│   ├── workflows/              # 50+ CI workflow files
│   ├── ISSUE_TEMPLATE/         # Bug report, suggestion, RFC templates
│   ├── CODEOWNERS              # Ownership mapping for review routing
│   ├── dco.yml                 # DCO bot configuration
│   └── pull_request_template.md
│
├── LICENSES/                   # 20 SPDX-compliant license texts
├── REUSE.toml                  # Path-to-license mapping
├── CONTRIBUTING.md             # Contribution guidelines, CLA, DCO
├── SECURITY.md                 # Vulnerability reporting policy
├── TRADEMARK.md                # Trademark and brand usage policy
├── CODE_OF_CONDUCT.md          # Code of Conduct v2
├── README.md                   # Root README
│
├── flake.nix                   # Top-level Nix flake (dev shells, LLVM 22)
├── flake.lock                  # Pinned Nix inputs
├── bootstrap.sh                # Linux/macOS dependency bootstrap
├── bootstrap.cmd               # Windows dependency bootstrap
├── lefthook.yml                # Git hooks (REUSE lint, checkpatch)
│
├── meshmc/                     # MeshMC launcher (C++23, Qt 6, CMake)
├── mnv/                        # MNV text editor (C, Autotools/CMake)
├── cgit/                       # cgit Git web interface (C, Make)
│
├── neozip/                     # Compression library (C, CMake)
├── json4cpp/                   # JSON library (C++, CMake/Meson)
├── tomlplusplus/               # TOML library (C++17, Meson/CMake)
├── libnbtplusplus/             # NBT library (C++, CMake)
├── cmark/                      # Markdown library (C, CMake)
├── genqrcode/                  # QR code library (C, CMake/Autotools)
├── forgewrapper/               # Forge bootstrap (Java, Gradle)
│
├── corebinutils/               # BSD utility ports (C, Make)
│
├── meta/                       # Metadata generator (Python, Poetry)
├── ofborg/                     # tickborg CI bot (Rust, Cargo)
├── images4docker/              # Docker build environments (Dockerfile)
├── ci/                         # CI tooling (Nix, JavaScript)
├── hooks/                      # Git hook scripts
│
├── archived/                   # Deprecated sub-projects
│   ├── projt-launcher/
│   ├── projt-modpack/
│   ├── projt-minicraft-modpack/
│   └── ptlibzippy/
│
└── docs/                       # Documentation
    └── handbook/               # Developer handbook by component
```

---

## Dependency Graph

### Compile-Time Dependencies

MeshMC is the primary integration point. It consumes most of the library
sub-projects either directly or indirectly:

```
meshmc
├─── json4cpp          # JSON configuration parsing
│    └── (header-only, no transitive deps)
│
├─── tomlplusplus      # TOML instance/mod configuration
│    └── (header-only, no transitive deps)
│
├─── libnbtplusplus    # Minecraft world/data NBT parsing
│    └── zlib          # Compressed NBT support
│
├─── neozip            # General compression (zlib-compatible API)
│    └── (CPU intrinsics, no library deps)
│
├─── cmark             # Markdown changelog/news rendering
│    └── (no deps)
│
├─── genqrcode         # QR code generation for account linking
│    └── libpng        # PNG output (optional, for CLI tool)
│
├─── forgewrapper      # Runtime: Forge mod loader bootstrap
│    └── (Java SPI, no compile-time deps from meshmc)
│
├─── Qt 6              # External: GUI framework
│    ├── Core, Widgets, Concurrent
│    ├── Network, NetworkAuth
│    ├── Test, Xml
│    └── QuaZip (Qt 6) # ZIP archive handling
│
├─── libarchive        # External: Archive extraction
└─── ECM               # External: Extra CMake Modules
```

### Runtime Dependencies

```
meshmc (running)
├─── forgewrapper.jar       # Loaded at Minecraft launch for Forge ≥1.13
├─── meta/ JSON manifests   # Fetched over HTTP for version discovery
│    ├── Mojang versions
│    ├── Forge / NeoForge versions
│    ├── Fabric / Quilt versions
│    └── Java runtime versions
├─── JDK 17+               # For running Minecraft
└─── System zlib / neozip   # Linked at build time
```

### CI Dependencies

```
ci.yml (orchestrator)
├─── ci/github-script/       # JavaScript: commit lint, PR prep, reviews
│    ├── lint-commits.js     # Conventional Commits validation
│    ├── prepare.js          # PR validation
│    ├── reviews.js          # Review state management
│    └── withRateLimit.js    # GitHub API rate limiting
│
├─── ci/default.nix          # Nix: treefmt, codeowners-validator
│    ├── treefmt-nix         # Multi-language formatting
│    │   ├── actionlint      # GitHub Actions YAML lint
│    │   ├── biome           # JavaScript/TypeScript formatting
│    │   ├── nixfmt          # Nix formatting
│    │   ├── yamlfmt         # YAML formatting
│    │   └── zizmor          # GitHub Actions security scanning
│    └── codeowners-validator # CODEOWNERS file validation
│
├─── ci/pinned.json          # Pinned Nixpkgs revision
│
├─── images4docker/          # Docker build environments (40 distros)
│
└─── ofborg/tickborg/        # Distributed CI bot
     ├── RabbitMQ            # Message queue
     └── GitHub API          # Check runs, PR comments
```

### Full Dependency Matrix

| Consumer | json4cpp | toml++ | libnbt++ | neozip | cmark | genqrcode | forgewrapper | meta | Qt 6 |
|----------|----------|--------|----------|--------|-------|-----------|--------------|------|------|
| meshmc | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ (runtime) | ✓ (runtime, HTTP) | ✓ |
| meta | — | — | — | — | — | — | — | — | — |
| tickborg | — | — | — | — | — | — | — | — | — |
| corebinutils | — | — | — | — | — | — | — | — | — |
| mnv | — | — | — | — | — | — | — | — | — |
| cgit | — | — | — | — | — | — | — | — | — |

The library sub-projects (json4cpp, tomlplusplus, libnbtplusplus, neozip,
cmark, genqrcode) are consumed exclusively by MeshMC within the monorepo.
External consumers can also use them independently.

---

## Build System Architecture

Each sub-project uses the build system best suited to its upstream lineage:

```
                  ┌─────────────────────────┐
                  │   Nix Flake (top-level) │
                  │   Development Shells    │
                  └────────────┬────────────┘
                               │
          ┌────────────────────┼────────────────────┐
          │                    │                    │
    ┌─────▼─────┐        ┌─────▼─────┐        ┌─────▼─────┐
    │   CMake   │        │   Other   │        │  Package  │
    │  Projects │        │  Systems  │        │  Managers │
    └─────┬─────┘        └─────┬─────┘        └─────┬─────┘
          │                    │                    │
  ┌───────┼────────┐    ┌──────┼──────┐             ├─────┐
  │       │        │    │      │      │             │     │
meshmc  neozip  cmark  toml++ cgit  corebinutils  meta  tickborg
json4  genqr   libnbt  (Meson)(Make) (Make)      (Poetry)(Cargo)
(CMake)(CMake) (CMake)  mnv   forgewrapper
                       (Auto) (Gradle)
```

### CMake Projects (Ninja Multi-Config)

MeshMC and its library dependencies use CMake with the Ninja Multi-Config
generator. MeshMC ships `CMakePresets.json` with platform-specific presets:

| Preset | Platform | Toolchain |
|--------|----------|-----------|
| `linux` | Linux | System compiler |
| `macos` | macOS | vcpkg |
| `macos_universal` | macOS Universal | x86_64 + arm64 |
| `windows_mingw` | Windows | MinGW |
| `windows_msvc` | Windows | MSVC + vcpkg |

All presets share a hidden `base` preset that enforces:
- Ninja Multi-Config generator
- Build directory: `build/`
- Install directory: `install/`
- LTO enabled by default

### CMake Compiler Requirements

| Compiler | Minimum Version | Standard |
|----------|----------------|----------|
| GCC | 13 | C++23 |
| Clang | 17 | C++23 |
| MSVC | 19.36 | C++23 |

CMake minimum version: **3.28**

### Meson Project (tomlplusplus)

toml++ uses Meson as its primary build system with CMake as an alternative.
The Meson build supports both header-only and compiled modes.

### Make Projects (cgit, corebinutils)

cgit uses a traditional `Makefile` that first builds a bundled version of Git,
then builds cgit itself. The Makefile supports `NO_LUA=1` and
`LUA_PKGCONFIG=luaXX` options.

corebinutils uses a `./configure && make` workflow with `config.mk` for
toolchain configuration. It selects musl-gcc by default and falls back to
system gcc/clang.

### Autotools Projects (mnv, genqrcode, neozip)

MNV supports both CMake and traditional Autotools (`./configure && make`).
GenQRCode uses Autotools (`autogen.sh` → `./configure` → `make`).
NeoZip supports both CMake and a `./configure` script.

### Gradle Project (forgewrapper)

ForgeWrapper uses Gradle for building. The project includes a `gradlew`
wrapper script and uses JPMS (Java Platform Module System) via the
`jigsaw/` directory.

### Cargo Workspace (tickborg)

The `ofborg/` directory contains a Cargo workspace with two crates:
- `tickborg` — The main CI bot
- `tickborg-simple-build` — Simplified build runner

The workspace uses Rust 2021 edition with `resolver = "2"`.

### Poetry Project (meta)

The `meta/` component uses Poetry for Python dependency management. Key
dependencies include `requests`, `cachecontrol`, `pydantic`, and `filelock`.
It provides CLI entry points for generating and updating version metadata
for each supported mod loader.

---

## CI/CD Architecture

### Orchestrator Pattern

Project Tick uses a single monolithic CI orchestrator (`ci.yml`) that gates
all other workflows. The orchestrator:

1. **Classifies the event** — Push, PR, merge queue, tag, scheduled, or manual
2. **Detects changed files** — Maps file paths to affected sub-projects
3. **Determines run level** — `minimal`, `standard`, or `full`
4. **Dispatches per-project builds** — Only builds what changed

```
Event (push/PR/merge_queue/tag)
    │
    ▼
┌──────────┐
│   Gate   │ ── classify event, detect changes, set run level
└────┬─────┘
     │
     ├──► Lint & Checks (commit messages, formatting, CODEOWNERS)
     │
     ├──► meshmc-build.yml       (if meshmc/ changed)
     ├──► neozip-ci.yml          (if neozip/ changed)
     ├──► cmark-ci.yml           (if cmark/ changed)
     ├──► json4cpp-ci.yml        (if json4cpp/ changed)
     ├──► tomlplusplus-ci.yml    (if tomlplusplus/ changed)
     ├──► libnbtplusplus-ci.yml  (if libnbtplusplus/ changed)
     ├──► genqrcode-ci.yml       (if genqrcode/ changed)
     ├──► forgewrapper-build.yml (if forgewrapper/ changed)
     ├──► cgit-ci.yml            (if cgit/ changed)
     ├──► corebinutils-ci.yml    (if corebinutils/ changed)
     ├──► mnv-ci.yml             (if mnv/ changed)
     │
     └──► Release workflows      (if tag push)
          ├── meshmc-release.yml
          ├── meshmc-publish.yml
          └── neozip-release.yml
```

### Workflow Inventory

The `.github/workflows/` directory contains 50+ workflow files:

**Core CI:**
- `ci.yml` — Monolithic orchestrator
- `ci-lint.yml` — Commit message and formatting checks
- `ci-schedule.yml` — Scheduled jobs

**Per-Project CI:**
- `meshmc-build.yml`, `meshmc-codeql.yml`, `meshmc-container.yml`, `meshmc-nix.yml`
- `neozip-ci.yml`, `neozip-cmake.yml`, `neozip-configure.yml`, `neozip-analyze.yml`, `neozip-codeql.yml`, `neozip-fuzz.yml`, `neozip-lint.yml`
- `json4cpp-ci.yml`, `json4cpp-fuzz.yml`, `json4cpp-amalgam.yml`, `json4cpp-flawfinder.yml`, `json4cpp-semgrep.yml`
- `cmark-ci.yml`, `cmark-fuzz.yml`
- `tomlplusplus-ci.yml`, `tomlplusplus-fuzz.yml`
- `mnv-ci.yml`, `mnv-codeql.yml`, `mnv-coverity.yml`
- `cgit-ci.yml`, `corebinutils-ci.yml`
- `forgewrapper-build.yml`, `libnbtplusplus-ci.yml`, `genqrcode-ci.yml`

**Release & Publishing:**
- `meshmc-release.yml`, `meshmc-publish.yml`
- `neozip-release.yml`
- `images4docker-build.yml`
- `tomlplusplus-gh-pages.yml`, `json4cpp-publish-docs.yml`

**Repository Maintenance:**
- `repo-dependency-review.yml`, `repo-labeler.yml`, `repo-scorecards.yml`, `repo-stale.yml`
- `meshmc-backport.yml`, `meshmc-blocked-prs.yml`, `meshmc-merge-blocking-pr.yml`
- `meshmc-flake-update.yml`

### Concurrency Control

The CI orchestrator uses a concurrency key that varies by event type:

| Event | Concurrency Group |
|-------|-------------------|
| Merge queue | `ci-<merge_group_head_ref>` |
| Pull request | `ci-pr-<PR_number>` |
| Push | `ci-<ref>` |

In-progress runs are cancelled for pushes and PRs but **not** for merge queue
entries, ensuring merge queue integrity.

---

## Branch Strategy

Branch classification is defined in `ci/supportedBranches.js`:

| Branch Pattern | Type | Priority | Description |
|----------------|------|----------|-------------|
| `master` | development / primary | 0 (highest) | Main development branch |
| `release-*` | development / primary | 1 | Release branches (e.g., `release-7.0`) |
| `staging-*` | development / secondary | 2 | Pre-release staging |
| `staging-next-*` | development / secondary | 3 | Next staging cycle |
| `feature-*` | wip | — | Feature development |
| `fix-*` | wip | — | Bug fixes |
| `backport-*` | wip | — | Cherry-picks to release branches |
| `revert-*` | wip | — | Reverted changes |
| `wip-*` | wip | — | Work in progress |
| `dependabot-*` | wip | — | Automated dependency updates |

Version tags follow: `<major>.<minor>.<patch>` (e.g., `7.0.0`).

---

## Shared Infrastructure

### Nix Flake (Top-Level)

The root `flake.nix` provides a development shell for the entire monorepo:

- **Toolchain:** LLVM 22 (Clang, clang-tidy)
- **clang-tidy-diff:** Wrapped Python script for incremental analysis
- **Submodule initialization:** Automatic via `shellHook`
- **Systems:** All `lib.systems.flakeExposed` (x86_64, aarch64 on Linux/macOS
  and other exotic platforms)

### CI Nix Configuration

The `ci/default.nix` provides:

- **treefmt** — Multi-language formatter with:
  - `actionlint` — GitHub Actions YAML validation
  - `biome` — JavaScript formatting (single quotes, no semicolons)
  - `nixfmt` — Nix formatting (RFC style)
  - `yamlfmt` — YAML formatting (retain line breaks)
  - `zizmor` — GitHub Actions security scanning
  - `keep-sorted` — Sort blocks marked with `keep-sorted` comments
- **codeowners-validator** — Validates the CODEOWNERS file

### Lefthook Git Hooks

Pre-commit hooks configured in `lefthook.yml`:

1. **reuse-lint** — Validates REUSE compliance. If missing licenses are
   detected, downloads them and stages the fix automatically.
2. **checkpatch** — Runs `scripts/checkpatch.pl` on staged C/C++ and CMake
   diffs. Skipped during merge and rebase operations.

Pre-push hooks:
1. **reuse-lint** — Final REUSE compliance check before push.

### Bootstrap Scripts

`bootstrap.sh` (Linux/macOS) and `bootstrap.cmd` (Windows) handle first-time
setup:

- Detect the host distribution (Debian, Ubuntu, Fedora, RHEL, openSUSE, Arch,
  macOS)
- Install required dependencies via the native package manager
- Initialize and update Git submodules
- Install and configure lefthook

The bootstrap scripts check for:
- Build tools: npm, Go, lefthook, reuse
- Libraries: Qt6Core, quazip1-qt6, zlib, ECM (via pkg-config)

---

## Security Architecture

### Supply Chain

- All Nix inputs are pinned with content hashes (`flake.lock`, `ci/pinned.json`)
- GitHub Actions use pinned action versions with SHA references
- `step-security/harden-runner` is used in CI workflows
- `repo-dependency-review.yml` scans dependency changes
- `repo-scorecards.yml` tracks OpenSSF Scorecard compliance

### Code Quality

- CodeQL analysis for meshmc, mnv, and neozip
- Fuzz testing for neozip, json4cpp, cmark, and tomlplusplus
- Semgrep and Flawfinder static analysis for json4cpp
- Coverity scanning for mnv
- clang-tidy checks enabled via `MeshMC_ENABLE_CLANG_TIDY` CMake option

### Compiler Hardening (MeshMC)

MeshMC's CMakeLists.txt enables:
- `-fstack-protector-strong --param=ssp-buffer-size=4` — Stack smashing protection
- `-O3 -D_FORTIFY_SOURCE=2` — Buffer overflow detection
- `-Wall -pedantic` — Comprehensive warnings
- ASLR and PIE via `CMAKE_POSITION_INDEPENDENT_CODE ON`

---

## Data Flow

### MeshMC Launch Sequence

```
User clicks "Launch" in MeshMC
    │
    ▼
MeshMC reads instance configuration
    │  (tomlplusplus for TOML, json4cpp for JSON)
    │
    ▼
MeshMC fetches version metadata
    │  (HTTP → meta/ JSON manifests)
    │
    ▼
MeshMC downloads/verifies game assets
    │  (neozip for decompression, libarchive for extraction)
    │
    ▼
MeshMC prepares launch environment
    │  (libnbtplusplus for world data if needed)
    │
    ▼
[If Forge ≥1.13] ForgeWrapper bootstraps Forge
    │  (Java SPI, installer extraction)
    │
    ▼
Minecraft process spawned with JDK 17+
```

### CI Build Flow

```
Developer pushes commit
    │
    ▼
ci.yml Gate job runs
    │  ─ classifies event type
    │  ─ detects changed files
    │  ─ maps to affected sub-projects
    │
    ▼
ci-lint.yml runs in parallel
    │  ─ Conventional Commits validation
    │  ─ treefmt formatting check
    │  ─ CODEOWNERS validation
    │
    ▼
Per-project CI dispatched
    │  ─ CMake configure + build + test
    │  ─ Multi-platform matrix
    │  ─ CodeQL / fuzz / static analysis
    │
    ▼
Results posted as GitHub check runs
    │
    ▼
[If tag push] Release workflow triggered
    ─ Build release binaries
    ─ Create GitHub release
    ─ Publish artifacts
```

### Metadata Generation Flow

```
meta/ update scripts run (cron or manual)
    │
    ├─► updateMojang    → fetches Mojang version manifest
    ├─► updateForge     → fetches Forge version list
    ├─► updateNeoForge  → fetches NeoForge version list
    ├─► updateFabric    → fetches Fabric loader versions
    ├─► updateQuilt     → fetches Quilt loader versions
    ├─► updateLiteloader → fetches LiteLoader versions
    └─► updateJava      → fetches Java runtime versions
    │
    ▼
generate scripts produce JSON manifests
    │
    ▼
Manifests deployed (git push or static hosting)
    │
    ▼
MeshMC reads manifests at startup
```

---

## Module Boundaries

### Interface Contracts

Each library sub-project provides well-defined interfaces:

| Library | Include Path | Namespace | API Style |
|---------|-------------|-----------|-----------|
| json4cpp | `<nlohmann/json.hpp>` | `nlohmann` | Header-only, template-based |
| tomlplusplus | `<toml++/toml.hpp>` | `toml` | Header-only, C++17 |
| libnbtplusplus | `<nbt/nbt.h>` | `nbt` | Compiled library, C++11 |
| neozip | `<zlib.h>` or `<zlib-ng.h>` | C API | Drop-in zlib replacement |
| cmark | `<cmark.h>` | C API | Compiled library |
| genqrcode | `<qrencode.h>` | C API | Compiled library |
| forgewrapper | Java SPI | `io.github.zekerzhayard.forgewrapper` | JAR, service provider |

### Versioning Independence

Each sub-project maintains its own version number:

| Project | Versioning | Current |
|---------|-----------|---------|
| meshmc | `MAJOR.MINOR.HOTFIX` (CMake) | 7.0.0 |
| meta | `MAJOR.MINOR.PATCH-REV` (pyproject.toml) | 0.0.5-1 |
| forgewrapper | Gradle `version` property | (see gradle.properties) |
| neozip | CMake project version | (follows zlib-ng) |
| Other libraries | Follow upstream versioning | — |

The monorepo does not impose a single version across sub-projects. Each
component releases independently based on its own cadence.
