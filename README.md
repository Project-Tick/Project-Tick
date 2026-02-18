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

The workflow builds and pushes all 40 images on each run.

## Installed Dependencies
Each image is rebuilt with:
- Qt toolchain (validated in Docker build)
- The Linux dependency set you provided (mapped per package manager family)

Package profile highlights:
- `apt`: `dpkg-dev`, `ninja-build`, `scdoc`, `appstream`, `libxcb-cursor-dev`, `openjdk-21-jdk`, `lcov`, `libdbus-1-dev`, `libinih-dev`, `libsystemd-dev`, Qt packages, and `libtiff6/libtiff5` fallback logic
- `dnf`/`zypper`: equivalent dependency set + Qt packages
- `yum`/`pacman`/`xbps`/`nix`/`emerge`: mapped best-effort equivalents + Qt

## Notes
- If Qt tools are not found during build, Docker build fails.
- If an upstream image tag disappears, update the corresponding file in `dockerfiles/`.
