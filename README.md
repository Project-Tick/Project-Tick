# Project Tick Infra Image Repack

This repository uses **40 separate Dockerfiles**. Each Dockerfile has its own fixed base image.

Target format:
- `ghcr.io/project-tick-infra/images/<target_name>:<target_tag>`

## Repository Layout
- `dockerfiles/*.Dockerfile` (40 files, one base image per file)
- `.github/workflows/build.yml`
- `README.md`

## Workflow
`Repack Existing Images` runs automatically on:
- Push to `main` (when Dockerfiles/workflow/README change)
- Daily schedule (`03:17 UTC`)

The workflow builds and pushes the **Qt6-compatible set** on each run (currently 35 targets).

## Installed Dependencies
Each image is rebuilt with:
- Qt toolchain (validated in Docker build)
- The Linux dependency set you provided (mapped per package manager family)

Package profile highlights:
- `apt`: Qt6 packages are required; build fails if unavailable
- `dnf`: CRB/PowerTools and EPEL probing + package-name fallbacks for CentOS/RHEL family
- `apk`: Alpine-specific package mapping (no `libsystemd-dev`)
- `zypper`/`yum`/`pacman`/`xbps`/`nix`/`emerge`: mapped equivalents + Qt6 verification

## Notes
- Qt6 is mandatory. If Qt6 packages/tools are unavailable, Docker build fails (no Qt5 fallback).
- Some older bases are excluded from the active matrix because they do not provide Qt6 reliably.
- If an upstream image tag disappears, update the corresponding file in `dockerfiles/`.
