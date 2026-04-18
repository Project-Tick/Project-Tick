# MeshMC Build Guide

This document explains how to build MeshMC from source on all supported platforms.

## Table of Contents

- [Requirements](#requirements)
- [Quick Start (Bootstrap)](#quick-start-bootstrap)
- [Dependencies](#dependencies)
- [Cloning the Repository](#cloning-the-repository)
- [CMake Presets](#cmake-presets)
- [Building on Linux](#building-on-linux)
- [Building on macOS](#building-on-macos)
- [Building on Windows](#building-on-windows)
- [Building with Nix](#building-with-nix)
- [Building with Container (Podman/Docker)](#building-with-container-podmandocker)
- [Running Tests](#running-tests)
- [CMake Options](#cmake-options)
- [Troubleshooting](#troubleshooting)

## Requirements

- **CMake** >= 3.28
- **Ninja** (recommended generator)
- **C++ compiler** with C++23 support (GCC >= 13, Clang >= 17, MSVC >= 19.36)
- **Qt 6** (Core, Widgets, Concurrent, Network, NetworkAuth, Test, Xml)
- **Java Development Kit** (JDK 17) — for building Java launcher components
- **Git** — for submodule management

## Quick Start (Bootstrap)

The fastest way to get started is to use the bootstrap script. It automatically
detects your platform, installs missing dependencies, initializes submodules,
and sets up lefthook git hooks.

### Linux / macOS

```bash
../bootstrap.sh
```

Supported distributions: Debian, Ubuntu, Fedora, RHEL/CentOS, openSUSE, Arch Linux, macOS (via Homebrew).

### Windows

```cmd
..\bootstrap.cmd
```

Uses [Scoop](https://scoop.sh) for CLI tools and [vcpkg](https://github.com/microsoft/vcpkg) for C/C++ libraries.

## Dependencies

### Build Dependencies

| Dependency             | Purpose                         | pkg-config name    |
|------------------------|---------------------------------|--------------------|
| Qt 6 (Base)            | GUI framework                   | `Qt6Core`          |
| Qt 6 NetworkAuth       | OAuth2 authentication           | —                  |
| QuaZip (Qt6)           | ZIP archive support             | `quazip1-qt6`      |
| zlib                   | Compression                     | `zlib`             |
| Extra CMake Modules    | KDE CMake utilities             | `ECM`              |
| cmark                  | Markdown rendering              | —                  |
| tomlplusplus           | TOML parsing                    | —                  |
| libarchive             | Archive extraction              | —                  |
| libqrencode            | QR code generation              | —                  |
| scdoc                  | Man page generation (optional)  | —                  |

### Distro-Specific Package Names

<details>
<summary><strong>Debian / Ubuntu</strong></summary>

```bash
sudo apt-get install \
    cmake ninja-build extra-cmake-modules pkg-config \
    qt6-base-dev libquazip1-qt6-dev zlib1g-dev \
    libcmark-dev libarchive-dev libqrencode-dev libtomlplusplus-dev \
    scdoc
```

</details>

<details>
<summary><strong>Fedora</strong></summary>

```bash
sudo dnf install \
    cmake ninja-build extra-cmake-modules pkgconf \
    qt6-qtbase-devel quazip-qt6-devel zlib-devel \
    cmark-devel libarchive-devel qrencode-devel tomlplusplus-devel \
    scdoc
```

</details>

<details>
<summary><strong>Arch Linux</strong></summary>

```bash
sudo pacman -S --needed \
    cmake ninja extra-cmake-modules pkgconf \
    qt6-base quazip-qt6 zlib \
    cmark libarchive qrencode tomlplusplus \
    scdoc
```

</details>

<details>
<summary><strong>openSUSE</strong></summary>

```bash
sudo zypper install \
    cmake ninja extra-cmake-modules pkg-config \
    qt6-base-devel quazip-qt6-devel zlib-devel \
    cmark-devel libarchive-devel qrencode-devel tomlplusplus-devel \
    scdoc
```

</details>

<details>
<summary><strong>macOS (Homebrew)</strong></summary>

```bash
brew install \
    cmake ninja extra-cmake-modules \
    qt@6 quazip zlib \
    cmark libarchive qrencode tomlplusplus \
    scdoc
```

</details>

### Developer Tooling (Optional)

These are **not required** to build, but are used for development:

| Tool       | Purpose                       |
|------------|-------------------------------|
| npm        | Frontend tooling              |
| Go         | Installing lefthook           |
| lefthook   | Git hooks manager             |
| reuse       | REUSE license compliance      |
| clang-format | Code formatting              |
| clang-tidy | Static analysis               |

## Cloning the Repository

MeshMC uses git submodules. Make sure to clone recursively:

```bash
git clone --recursive https://github.com/Project-Tick/Project-Tick.git
cd Project-Tick/MeshMC
```

If you already cloned without `--recursive`, initialize submodules manually:

```bash
git submodule update --init --recursive
```

## Building Dependencies

You can use to build MeshMC dependencies for git repository, please use build-deps script.

Linux / macOS / Windows MinGW

```bash

./scripts/build-deps.sh

```

Windows MSVC

```pwsh

.\scripts\build-deps.ps1

```

## CMake Presets

MeshMC ships a `CMakePresets.json` with pre-configured presets for each platform.
All presets use the **Ninja Multi-Config** generator and output to the `build/`
directory with install prefix `install/`.

### Configure Presets

| Preset             | Platform                  | Notes                                      |
|--------------------|---------------------------|--------------------------------------------|
| `linux`            | Linux                     | Available only on Linux hosts               |
| `macos`            | macOS                     | Uses vcpkg toolchain (`$VCPKG_ROOT`)        |
| `macos_universal`  | macOS (Universal Binary)  | Builds for both x86_64 and arm64            |
| `windows_mingw`    | Windows (MinGW)           | Available only on Windows hosts             |
| `windows_msvc`     | Windows (MSVC)            | Uses vcpkg toolchain (`$VCPKG_ROOT`)        |

All presets inherit from a hidden `base` preset which sets:

- **Generator:** `Ninja Multi-Config`
- **Build directory:** `build/`
- **Install directory:** `install/`
- **LTO:** Enabled by default

### Build Presets

Each configure preset has a matching build preset with the same name:

| Preset             | Configure Preset   |
|--------------------|--------------------|
| `linux`            | `linux`            |
| `macos`            | `macos`            |
| `macos_universal`  | `macos_universal`  |
| `windows_mingw`    | `windows_mingw`    |
| `windows_msvc`     | `windows_msvc`     |

### Test Presets

Test presets share the same names. They are configured with verbose output on
failure and exclude example tests.

### Environment Variables

Some presets reference environment variables:

| Variable          | Used By                        | Purpose                           |
|-------------------|--------------------------------|-----------------------------------|
| `VCPKG_ROOT`      | `macos`, `macos_universal`, `windows_msvc` | Path to vcpkg installation  |
| `ARTIFACT_NAME`   | All (via `base`)               | Updater artifact identifier       |
| `BUILD_PLATFORM`  | All (via `base`)               | Platform identifier string        |

## Building on Linux

### Configure

```bash
cmake --preset linux
```

### Build

```bash
cmake --build --preset linux --config Release
```

For a debug build:

```bash
cmake --build --preset linux --config Debug
```

Since the generator is `Ninja Multi-Config`, you can switch between `Debug`,
`Release`, `RelWithDebInfo`, and `MinSizeRel` without re-configuring.

### Install

```bash
cmake --install build --config Release --prefix /usr/local
```

### Full One-Liner

```bash
cmake --preset linux && cmake --build --preset linux --config Release
```

## Building on macOS

### Prerequisites

Make sure `VCPKG_ROOT` is set:

```bash
export VCPKG_ROOT="$HOME/vcpkg"
```

### Standard Build (Native Architecture)

```bash
cmake --preset macos
cmake --build --preset macos --config Release
```

### Universal Binary (x86_64 + arm64)

```bash
cmake --preset macos_universal
cmake --build --preset macos_universal --config Release
```

This produces a fat binary supporting both Intel and Apple Silicon Macs.

### Install

```bash
cmake --install build --config Release
```

## Building on Windows

### Using MSVC

Requires Visual Studio with C++ workload and vcpkg:

```cmd
set VCPKG_ROOT=C:\path\to\vcpkg
cmake --preset windows_msvc
cmake --build --preset windows_msvc --config Release
```

### Using MinGW

```cmd
cmake --preset windows_mingw
cmake --build --preset windows_mingw --config Release
```

### Install

```cmd
cmake --install build --config Release
```

## Building with Nix

MeshMC provides a Nix flake for reproducible builds.

### Using the Nix Flake

```bash
# Build the package
nix build .#meshmc

# Enter the development shell
nix develop

# Inside the dev shell:
cd "$cmakeBuildDir"
ninjaBuildPhase
ninjaInstallPhase
```

### Without Flakes

```bash
nix-build
# or
nix-shell
```

### Binary Cache

A binary cache is available to speed up builds:

```
https://meshmc.cachix.org
```

The public key is:

```
meshmc.cachix.org-1:6ZNLcfqjVDKmN9/XNWGV3kcjBTL51v1v2V+cvanMkZA=
```

These are already configured in the flake's `nixConfig`.

## Building with Container (Podman/Docker)

A `Containerfile` (Debian-based) is provided for CI and reproducible builds.

### Build the Container Image

```bash
podman build -t meshmc-build .
```

### Run a Build Inside the Container

```bash
podman run --rm -it -v "$(pwd):/work:z" meshmc-build

# Inside the container:
git submodule update --init --recursive
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The container comes with Qt 6.10.2 (installed via `aqtinstall`), Clang, LLD,
Ninja, CMake, and all required dependencies pre-installed.

## Running Tests

### Using CTest Presets

```bash
# Configure first (if not done)
cmake --preset linux

# Build including test targets
cmake --build --preset linux --config Debug

# Run tests
ctest --preset linux --build-config Debug
```

### Running Tests Directly

```bash
cd build
ctest --output-on-failure
```

### Available Test Binaries

After building, individual test binaries are available in `build/`:

- `DownloadTask_test`
- `FileSystem_test`
- `GradleSpecifier_test`
- `GZip_test`
- `Index_test`
- `INIFile_test`
- `JavaVersion_test`
- `Library_test`
- `ModFolderModel_test`
- `MojangVersionFormat_test`
- `ParseUtils_test`
- `UpdateChecker_test`
- `sys_test`

## CMake Options

These options can be set during configuration with `-D<OPTION>=<VALUE>`:

| Option                         | Default  | Description                                |
|--------------------------------|----------|--------------------------------------------|
| `ENABLE_LTO`                   | `OFF`\*  | Enable Link Time Optimization              |
| `MeshMC_BUILD_PLATFORM`       | `""`     | Platform identifier string                 |
| `MeshMC_BUILD_ARTIFACT`       | `""`     | Artifact name for the updater              |
| `MeshMC_META_URL`             | (set)    | URL for meta server                        |
| `MeshMC_NEWS_RSS_URL`         | (set)    | URL for news RSS feed                      |
| `MeshMC_UPDATER_BASE`         | `""`     | Base URL for the updater                   |
| `MeshMC_NOTIFICATION_URL`     | (set)    | URL for notifications                      |
| `MeshMC_PASTE_EE_API_KEY`     | (set)    | paste.ee API key                           |
| `MeshMC_IMGUR_CLIENT_ID`      | (set)    | Imgur API client ID                        |
| `MeshMC_CURSEFORGE_API_KEY`   | (set)    | CurseForge API key                         |
| `BUILD_TESTING`                | `ON`     | Build unit tests                           |

> \* `ENABLE_LTO` defaults to `OFF` in the CMakeLists.txt, but presets set it
> to `ON`.

### Example

```bash
cmake --preset linux \
    -DENABLE_LTO=OFF \
    -DBUILD_TESTING=OFF \
    -DMeshMC_BUILD_PLATFORM="linux-x86_64"
```

## Troubleshooting

### In-Source Builds are Forbidden

CMake will refuse to configure if the source and build directories are the same.
Always use a separate build directory (the presets handle this automatically with `build/`).

### WSL is Not Supported

Building under Windows Subsystem for Linux is explicitly blocked. Use a native
Linux environment, the Windows presets, or a container.

### LTO Failures

If you get linker errors with LTO enabled, disable it:

```bash
cmake --preset linux -DENABLE_LTO=OFF
```

### Missing Qt

If CMake cannot find Qt, make sure the Qt 6 development packages are installed
and that Qt's bin directory is in your `PATH` (or set `CMAKE_PREFIX_PATH`):

```bash
cmake --preset linux -DCMAKE_PREFIX_PATH=/path/to/qt6
```

### Missing ECM (Extra CMake Modules)

ECM is required. Install it via your package manager:

- **Debian/Ubuntu:** `sudo apt-get install extra-cmake-modules`
- **Fedora:** `sudo dnf install extra-cmake-modules`
- **Arch:** `sudo pacman -S extra-cmake-modules`
- **macOS:** `brew install extra-cmake-modules`

### Submodule Errors

If you see errors about missing files in `libraries/`, ensure submodules are
initialized:

```bash
git submodule update --init --recursive
```
