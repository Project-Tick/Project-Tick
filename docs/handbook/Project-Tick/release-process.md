# Project Tick — Release Process

## Overview

Project Tick uses a per-component release methodology. Each sub-project
maintains its own version numbering, release cadence, and distribution
channels. Releases are triggered by Git tags and automated through GitHub
Actions workflows.

---

## Versioning Schemes

### Semantic Versioning (SemVer)

Most sub-projects follow [Semantic Versioning 2.0.0](https://semver.org/):

```
MAJOR.MINOR.PATCH[-PRERELEASE][+BUILD]
```

| Component       | Current Version | Source of Truth                          |
|-----------------|-----------------|-----------------------------------------|
| MeshMC          | 7.0.0           | `meshmc/CMakeLists.txt` (project())     |
| meta            | 0.0.5-1         | `meta/pyproject.toml` ([tool.poetry])   |
| neozip          | —               | `neozip/CMakeLists.txt`                 |
| json4cpp        | —               | `json4cpp/CMakeLists.txt`               |
| tomlplusplus    | —               | `tomlplusplus/meson.build`              |
| libnbtplusplus  | —               | `libnbtplusplus/CMakeLists.txt`         |
| forgewrapper    | —               | `forgewrapper/gradle.properties`        |
| cmark           | —               | `cmark/CMakeLists.txt`                  |
| genqrcode       | —               | `genqrcode/configure.ac`               |
| tickborg        | —               | `ofborg/Cargo.toml`                     |

### MeshMC Version Details

MeshMC's version is defined in its root `CMakeLists.txt`:

```cmake
project(MeshMC
  VERSION 7.0.0
  DESCRIPTION "MeshMC — Custom Minecraft Launcher"
  HOMEPAGE_URL "https://meshmc.org"
  LANGUAGES CXX C
)
```

The version is decomposed into CMAKE variables and compiled into the binary
via `buildconfig/`:
- `MeshMC_VERSION_MAJOR` — 7
- `MeshMC_VERSION_MINOR` — 0
- `MeshMC_VERSION_PATCH` — 0

### meta Version Details

The meta package uses Poetry versioning with a Debian-style suffix:

```toml
[tool.poetry]
name = "meta"
version = "0.0.5-1"
```

---

## Branch Strategy

### Branch Types

| Branch Pattern   | Purpose                       | Protected | CI Level    |
|------------------|-------------------------------|-----------|-------------|
| `master`         | Main development branch       | Yes       | Full        |
| `release-*`      | Release preparation branches  | Yes       | Full        |
| `staging-*`      | Integration testing branches  | No        | Partial     |
| `feature-*`      | Feature development           | No        | PR-only     |
| `fix-*`          | Bug fix branches              | No        | PR-only     |

### Branch Classification Logic

Branch classification is implemented in `ci/supportedBranches.js`:

```javascript
// Simplified representation of the classify() function:
function classify(branch) {
  if (branch === 'master')
    return { level: 'full', protected: true };
  if (branch.startsWith('release-'))
    return { level: 'full', protected: true };
  if (branch.startsWith('staging-'))
    return { level: 'partial', protected: false };
  return { level: 'pr-only', protected: false };
}
```

Protected branches cannot receive direct pushes; all changes must go through
pull requests with passing CI.

---

## Release Workflow

### Phase 1 — Feature Freeze

1. A release branch is created from `master`:
   ```bash
   git checkout -b release-7.1.0 master
   ```

2. Only bug fixes and documentation updates are merged into the release branch.

3. CI runs full validation on the release branch (same as `master`).

### Phase 2 — Version Bump

1. Update the version in the component's source of truth:
   - **MeshMC**: Edit `meshmc/CMakeLists.txt` `project(VERSION ...)`
   - **meta**: Edit `meta/pyproject.toml` `version = "..."`
   - **neozip**: Edit `neozip/CMakeLists.txt`
   - **Other CMake projects**: Edit the relevant `CMakeLists.txt`
   - **Meson projects**: Edit `meson.build`
   - **Gradle projects**: Edit `gradle.properties`
   - **Cargo projects**: Edit `Cargo.toml`

2. Update changelogs:
   - MeshMC maintains `meshmc/changelog.md`
   - Other components maintain changelogs in their directories

3. Commit the version bump:
   ```bash
   git add -A
   git commit -s -m "release: bump MeshMC to 7.1.0"
   ```

### Phase 3 — Tagging

Create an annotated Git tag:

```bash
git tag -a v7.1.0 -m "MeshMC 7.1.0"
git push origin v7.1.0
```

Tag naming conventions:
- **MeshMC**: `v<MAJOR>.<MINOR>.<PATCH>` (e.g., `v7.1.0`)
- **neozip**: `neozip-v<VERSION>` (e.g., `neozip-v2.2.3`)
- **json4cpp**: `json4cpp-v<VERSION>`
- **Other**: `<component>-v<VERSION>`

### Phase 4 — Automated Build & Publish

Pushing a tag triggers the corresponding release workflow:

| Tag Pattern          | Workflow                      | Artifacts                             |
|----------------------|-------------------------------|---------------------------------------|
| `v*`                 | `meshmc-release.yml`          | Linux/macOS/Windows binaries          |
| `v*`                 | `meshmc-publish.yml`          | Flathub, AUR, packaging repos         |
| `neozip-v*`          | `neozip-release.yml`          | Source archive, shared libraries      |
| `json4cpp-v*`        | (manual)                      | Updated single-header                 |
| `images4docker-v*`   | `images4docker-build.yml`     | Docker images to GHCR                 |

---

## MeshMC Release Details

### Build Matrix

MeshMC releases build for all supported platforms:

| Platform          | Compiler       | Qt Version | Output Format        |
|-------------------|----------------|------------|----------------------|
| Linux (x86_64)    | Clang 18+      | 6.x        | AppImage, tar.gz     |
| Linux (aarch64)   | Clang 18+      | 6.x        | AppImage, tar.gz     |
| macOS (x86_64)    | Apple Clang 16+| 6.x        | .dmg, .app           |
| macOS (aarch64)   | Apple Clang 16+| 6.x        | .dmg, .app           |
| Windows (x86_64)  | MSVC 17.10+    | 6.x        | .msi, portable .zip  |
| Windows (aarch64) | MSVC 17.10+    | 6.x        | .msi, portable .zip  |

### Release Workflow Steps

```
meshmc-release.yml:
  1. Checkout code at tag
  2. Set up dependencies (via .github/actions/meshmc/setup-dependencies/)
  3. Configure with CMake presets (Release mode)
  4. Build
  5. Run tests (ctest)
  6. Package (via .github/actions/meshmc/package/)
  7. Create GitHub Release with artifacts
  8. Upload checksums (SHA-256)
```

### Post-Release Publishing

```
meshmc-publish.yml:
  1. Download release artifacts
  2. Update Flathub manifest
  3. Update AUR PKGBUILD
  4. Update packaging repository
  5. Notify announcement channels
```

---

## neozip Release Details

neozip releases produce:
- Source tarball (`neozip-<version>.tar.gz`)
- Pre-built shared libraries for major platforms
- CMake package configuration files

The build matrix covers multiple architectures to validate SIMD
optimizations:
- x86_64 (SSE2, SSE4.2, AVX2, AVX-512)
- aarch64 (NEON, ARMv8 CRC)
- s390x (DFLTCC hardware deflate)
- ppc64le (VMX, VSX)
- riscv64 (RVV, ZBC) — when available

---

## Docker Image Releases

### images4docker Rebuild Cycle

Docker images are rebuilt on a scheduled basis:

```yaml
# .github/workflows/images4docker-build.yml
on:
  schedule:
    - cron: '17 3 * * *'    # Daily at 03:17 UTC
  push:
    branches: [master]
    paths: ['images4docker/**']
```

Each push to `master` that modifies `images4docker/` also triggers a rebuild.
All 40 Dockerfiles are built and pushed to GitHub Container Registry (GHCR).

---

## meta Release Details

The meta package uses Poetry for releases:

```bash
# Build distribution
poetry build

# Publish to PyPI (if applicable)
poetry publish
```

meta also defines CLI scripts in `pyproject.toml`:

```toml
[tool.poetry.scripts]
generate_fabric = "meta.run.generate_fabric:main"
generate_forge = "meta.run.generate_forge:main"
generate_mojang = "meta.run.generate_mojang:main"
generate_neoforge = "meta.run.generate_neoforge:main"
generate_quilt = "meta.run.generate_quilt:main"
generate_java = "meta.run.generate_java:main"
update_mojang = "meta.run.update_mojang:main"
```

---

## tickborg (ofborg) Release Details

tickborg is deployed as a Docker container:

```bash
# Build
docker build -t tickborg:latest ofborg/

# Deploy with docker-compose
cd ofborg && docker-compose up -d
```

The Rust workspace produces two binaries:
- `tickborg` — Main CI bot with AMQP integration
- `tickborg-simple-build` — Simplified builder for local testing

Deployment is managed via `ofborg/DEPLOY.md` and `ofborg/service.nix` for
NixOS.

---

## Hotfix Process

For critical security fixes or regressions:

1. **Branch from the release tag**:
   ```bash
   git checkout -b hotfix-7.0.1 v7.0.0
   ```

2. **Apply the minimal fix** — only the patch, no feature additions.

3. **Bump the PATCH version**.

4. **Tag and release**:
   ```bash
   git tag -a v7.0.1 -m "MeshMC 7.0.1 — Security hotfix"
   git push origin v7.0.1
   ```

5. **Cherry-pick to master**:
   ```bash
   git checkout master
   git cherry-pick <hotfix-commit>
   ```

---

## Pre-release / Development Builds

Development builds are produced on every push to `master`:

- MeshMC: `meshmc-ci.yml` produces nightly artifacts
- neozip: `neozip-ci.yml` attaches build artifacts
- Other components: CI produces artifacts accessible from workflow runs

Pre-release builds are not tagged and are identified by commit SHA or
workflow run number.

---

## Release Checklist

### Before Tagging

- [ ] All CI checks pass on the release branch
- [ ] Version number updated in source of truth
- [ ] Changelog updated with all changes since last release
- [ ] Security advisories addressed
- [ ] License compliance verified (`reuse lint` passes)
- [ ] Documentation updated for new features
- [ ] Breaking changes documented with migration guide

### After Tagging

- [ ] GitHub Release created with artifacts
- [ ] SHA-256 checksums published
- [ ] Package repositories updated (Flathub, AUR, etc.)
- [ ] Downstream dependencies notified
- [ ] Announcement published

### Post-Release

- [ ] Release branch merged back to `master` (if applicable)
- [ ] Confirm all distribution channels have the new version
- [ ] Monitor issue tracker for regression reports
- [ ] Begin next development cycle with version bump on `master`

---

## Dependency Updates

### Automated Updates

- **Renovate**: Configured in `meta/renovate.json` for automated dependency
  PRs in the meta sub-project
- **Nix flake**: `nix flake update` refreshes all Nix inputs
- **CI pinning**: `ci/update-pinned.sh` updates the pinned Nixpkgs revision

### Manual Updates

- **Git submodules**: `git submodule update --remote` for cgit's bundled Git
- **vcpkg**: Update `meshmc/vcpkg.json` and `meshmc/vcpkg-configuration.json`
- **Poetry lock**: `cd meta && poetry update`
- **Cargo lock**: `cd ofborg && cargo update`
