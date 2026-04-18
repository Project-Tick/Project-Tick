# Building MeshMC

## Prerequisites

### Required Tools

| Tool | Minimum Version | Purpose |
|---|---|---|
| CMake | 3.28 | Build system generator |
| Ninja | Any recent | Build executor (recommended) |
| C++ Compiler | GCC ≥ 13, Clang ≥ 17, MSVC ≥ 19.36 | C++23 compilation |
| JDK | 17 | Building Java launcher components |
| Git | Any recent | Submodule management |
| pkg-config | Any | Dependency discovery |

### Required Qt6 Modules

MeshMC requires Qt 6 with the following modules, as specified in the root `CMakeLists.txt`:

```cmake
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    Concurrent
    Network
    NetworkAuth
    Test
    Xml
)
```

### Required External Libraries

| Dependency | Purpose | pkg-config / CMake Name |
|---|---|---|
| Qt 6 Base | GUI framework | `Qt6Core`, `Qt6Widgets`, etc. |
| Qt 6 NetworkAuth | OAuth2 authentication | `Qt6NetworkAuth` |
| QuaZip (Qt6) | ZIP archive support | `quazip1-qt6` |
| zlib | Compression | `zlib` |
| Extra CMake Modules | KDE CMake utilities | `ECM` |
| cmark | Markdown rendering | — |
| tomlplusplus | TOML parsing | — |
| libarchive | Archive extraction | `LibArchive` (CMake) |
| libqrencode | QR code generation | — |
| scdoc | Man page generation (optional) | — |

## Quick Start with Bootstrap

The fastest path to building MeshMC is the bootstrap script at the repository root. It detects your platform, installs missing dependencies, initializes submodules, and configures lefthook git hooks.

### Linux / macOS

```bash
cd meshmc/
../bootstrap.sh
```

Supported distributions: Debian, Ubuntu, Fedora, RHEL/CentOS, openSUSE, Arch Linux, macOS (via Homebrew).

### Windows

```cmd
cd meshmc\
..\bootstrap.cmd
```

Uses Scoop for CLI tools and system libraries.

## Cloning the Repository

MeshMC uses git submodules (notably `libnbtplusplus`). Always clone recursively:

```bash
git clone --recursive https://github.com/Project-Tick/Project-Tick.git
cd Project-Tick/MeshMC
```

If you already cloned without `--recursive`:

```bash
git submodule update --init --recursive
```

The `libnbtplusplus` submodule lives at `../libnbtplusplus` relative to the `meshmc/` directory and is referenced in the root `CMakeLists.txt`:

```cmake
add_subdirectory(${CMAKE_SOURCE_DIR}/../libnbtplusplus libnbtplusplus)
```

## Distro-Specific Package Installation

### Debian / Ubuntu

```bash
sudo apt-get install \
    cmake ninja-build extra-cmake-modules pkg-config \
    qt6-base-dev libquazip1-qt6-dev zlib1g-dev \
    libcmark-dev libarchive-dev libqrencode-dev libtomlplusplus-dev \
    scdoc
```

### Fedora

```bash
sudo dnf install \
    cmake ninja-build extra-cmake-modules pkgconf \
    qt6-qtbase-devel quazip-qt6-devel zlib-devel \
    cmark-devel libarchive-devel qrencode-devel tomlplusplus-devel \
    scdoc
```

### Arch Linux

```bash
sudo pacman -S --needed \
    cmake ninja extra-cmake-modules pkgconf \
    qt6-base quazip-qt6 zlib \
    cmark libarchive qrencode tomlplusplus \
    scdoc
```

### openSUSE

```bash
sudo zypper install \
    cmake ninja extra-cmake-modules pkg-config \
    qt6-base-devel quazip-qt6-devel zlib-devel \
    cmark-devel libarchive-devel qrencode-devel tomlplusplus-devel \
    scdoc
```

### macOS (Homebrew)

```bash
brew install \
    cmake ninja extra-cmake-modules \
    qt@6 quazip zlib \
    cmark libarchive qrencode tomlplusplus \
    scdoc
```

### Windows

On Windows, `build-deps.ps1` automatically fetches and builds `extra-cmake-modules` and `libarchive` alongside the monorepo libraries. No separate package manager installation is required for these.\n\nOptionally install `pkg-config` via Chocolatey:\n\n```cmd\nchoco install pkgconfiglite -y\n```\n\nQt can be installed via the Qt Online Installer for full module support.

## CMake Presets

MeshMC ships `CMakePresets.json` with pre-configured presets for each platform. All presets use the **Ninja Multi-Config** generator and output to the `build/` directory.

### Available Configure Presets

| Preset | Platform | Notes |
|---|---|---|
| `linux` | Linux | Available only on Linux hosts |
| `macos` | macOS | Available only on macOS hosts |
| `macos_universal` | macOS (Universal Binary) | Builds for x86_64 + arm64 |
| `windows_mingw` | Windows (MinGW) | Available only on Windows hosts |
| `windows_msvc` | Windows (MSVC) | Available only on Windows hosts |

All presets inherit from a hidden `base` preset which sets:
- **Generator:** `Ninja Multi-Config`
- **Build directory:** `build/`
- **Install directory:** `install/`
- **LTO:** Enabled by default (`ENABLE_LTO=ON`)

### Environment Variables

| Variable | Used By | Purpose |
|---|---|---|
| `ARTIFACT_NAME` | All (via `base`) | Updater artifact identifier |
| `BUILD_PLATFORM` | All (via `base`) | Platform identifier string |

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

Since the generator is `Ninja Multi-Config`, you can switch between `Debug`, `Release`, `RelWithDebInfo`, and `MinSizeRel` without re-configuring.

### Install

```bash
cmake --install build --config Release --prefix /usr/local
```

The install layout on Linux follows KDE conventions:
- Binary: `bin/meshmc`
- Libraries: `lib/`
- Data: `share/MeshMC/`
- Desktop file: `share/applications/org.projecttick.MeshMC.desktop`
- Metainfo: `share/metainfo/org.projecttick.MeshMC.metainfo.xml`
- Icon: `share/icons/hicolor/scalable/apps/org.projecttick.MeshMC.svg`
- MIME type: `share/mime/packages/org.projecttick.MeshMC.xml`

### Full One-Liner

```bash
cmake --preset linux && cmake --build --preset linux --config Release
```

## Building on macOS

### Prerequisites

Install dependencies via Homebrew:

```bash
brew install cmake ninja extra-cmake-modules qt@6 libarchive qrencode pkg-config
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

### Install

```bash
cmake --install build --config Release
```

The macOS install layout creates an application bundle:
- `MeshMC.app/Contents/MacOS/` — binaries and plugins
- `MeshMC.app/Contents/Frameworks/` — frameworks and libraries
- `MeshMC.app/Contents/Resources/` — icons, assets

### macOS-Specific Features

- **Sparkle updates** — macOS uses the Sparkle framework for native update UI. The public key and feed URL are configured via CMake:
  ```cmake
  set(MACOSX_SPARKLE_UPDATE_PUBLIC_KEY "C0eBoyDSoZbzgCMxQH9wH6kmjU2mPRmvhZZd9mHgqZQ=")
  set(MACOSX_SPARKLE_UPDATE_FEED_URL "https://projecttick.org/product/meshmc/appcast.xml")
  ```
- **Asset catalog** — Icons are compiled via `actool` when Xcode ≥ 26.0 is available (liquid glass icons)
- **Bundle metadata** — Info.plist values are set via `MACOSX_BUNDLE_*` CMake variables

## Building on Windows

### Using MSVC

Requires Visual Studio with C++ workload. Run `build-deps.ps1` first to fetch\nexternal dependencies (ECM, libarchive) and build all monorepo libraries.

```cmd
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

Windows install layout places everything in a flat directory structure.

### Windows-Specific Notes

- **MSVC C standard** — C11 is used instead of C23 for MSVC compatibility:
  ```cmake
  if(MSVC)
      set(CMAKE_C_STANDARD 11)
  else()
      set(CMAKE_C_STANDARD 23)
  endif()
  ```
- **NSIS installer** — An NSIS installer script is generated from `branding/win_install.nsi.in`
- **Visual C++ Redistributable** — The NSIS installer can download and install the VC++ runtime automatically
- **Resource file** — Windows executable metadata is provided via a `.rc` file generated from `branding/meshmc.rc.in`
- **Manifest** — A Windows application manifest is generated for DPI awareness and UAC settings

## Building with Nix

MeshMC provides a Nix flake for reproducible builds:

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

Public key:
```
meshmc.cachix.org-1:6ZNLcfqjVDKmN9/XNWGV3kcjBTL51v1v2V+cvanMkZA=
```

These are already configured in the flake's `nixConfig`.

## Building with Container (Podman/Docker)

A `Containerfile` (Debian-based) is provided for CI and reproducible builds:

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

The container comes with Qt 6.10.2 (installed via `aqtinstall`), Clang, LLD, Ninja, CMake, and all required dependencies pre-installed.

## CMake Options

These options can be set during configuration with `-D<OPTION>=<VALUE>`:

| Option | Default | Description |
|---|---|---|
| `ENABLE_LTO` | `OFF` (ON in presets) | Enable Link Time Optimization |
| `MeshMC_DISABLE_JAVA_DOWNLOADER` | `OFF` | Disable the built-in Java downloader feature |
| `MeshMC_ENABLE_CLANG_TIDY` | `OFF` | Run clang-tidy during compilation |
| `MeshMC_BUILD_PLATFORM` | `""` | Platform identifier string for notifications |
| `MeshMC_VERSION_BUILD` | `-1` | Build number (-1 for no build number) |
| `MeshMC_BUILD_ARTIFACT` | `""` | Artifact name for updater identification |
| `BUILD_TESTING` | `ON` | Build unit tests |

### URL Configuration Options

These are typically set for custom/self-hosted deployments:

| Option | Default | Description |
|---|---|---|
| `MeshMC_META_URL` | `https://meta.projecttick.org/` | Metadata server URL |
| `MeshMC_NEWS_RSS_URL` | `https://projecttick.org/product/meshmc/feed.xml` | News RSS feed URL |
| `MeshMC_UPDATER_FEED_URL` | `""` | RSS feed URL for updater |
| `MeshMC_UPDATER_GITHUB_API_URL` | `""` | GitHub Releases API URL for update verification |
| `MeshMC_NOTIFICATION_URL` | `https://projecttick.org/` | Notification check URL |
| `MeshMC_BUG_TRACKER_URL` | `hhttps://bugs.projecttick.org/enter_bug.cgi?product=MeshMC` | Bug tracker URL |

### API Key Options

| Option | Description |
|---|---|
| `MeshMC_MICROSOFT_CLIENT_ID` | Azure AD application client ID for MSA login |
| `MeshMC_PASTE_EE_API_KEY` | paste.ee API key for log upload |
| `MeshMC_IMGUR_CLIENT_ID` | Imgur API client ID for screenshot upload |
| `MeshMC_CURSEFORGE_API_KEY` | CurseForge API key |
| `MeshMC_ANALYTICS_ID` | Google Analytics measurement ID |

## Compiler Flags

### GCC / Clang

```
-Wall -pedantic -Wno-deprecated-declarations
-fstack-protector-strong --param=ssp-buffer-size=4
-O3 -D_FORTIFY_SOURCE=2
-DQT_NO_DEPRECATED_WARNINGS=Y
```

### MSVC

```
/W4 /DQT_NO_DEPRECATED_WARNINGS=Y
```

### macOS-Specific

```
-stdlib=libc++
```

## Running Tests

### Using CTest Presets

```bash
cmake --preset linux
cmake --build --preset linux --config Debug
ctest --preset linux --build-config Debug
```

### Running Tests Directly

```bash
cd build
ctest --output-on-failure
```

### Available Test Binaries

After building, individual test binaries are available in `build/`:

| Test Binary | Tests |
|---|---|
| `DownloadTask_test` | Network download functionality |
| `FileSystem_test` | Filesystem utilities |
| `GradleSpecifier_test` | Maven coordinate parsing |
| `GZip_test` | GZip compression/decompression |
| `Index_test` | Metadata index |
| `INIFile_test` | INI file parsing |
| `JavaVersion_test` | Java version comparison |
| `Library_test` | Library descriptor |
| `ModFolderModel_test` | Mod folder model |
| `MojangVersionFormat_test` | Version JSON parsing |
| `ParseUtils_test` | Version string parsing |
| `UpdateChecker_test` | Update check system |
| `sys_test` | System information |

Tests use Qt's `QTest` framework and are integrated via ECM's `ecm_add_tests()`.

## Build Safety

The root `CMakeLists.txt` enforces several safety rules:

### No In-Source Builds

```cmake
string(COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${CMAKE_BUILD_DIR}" IS_IN_SOURCE_BUILD)
if(IS_IN_SOURCE_BUILD)
    message(FATAL_ERROR "You are building MeshMC in-source. ...")
endif()
```

### No WSL Builds

```cmake
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(CMAKE_HOST_SYSTEM_VERSION MATCHES ".*[Mm]icrosoft.*" OR
       CMAKE_HOST_SYSTEM_VERSION MATCHES ".*WSL.*")
        message(FATAL_ERROR "Building MeshMC is not supported in Linux-on-Windows distributions.")
    endif()
endif()
```

## Development Tooling

Optional tools for development:

| Tool | Purpose |
|---|---|
| `lefthook` | Git hooks manager (installed by bootstrap) |
| `reuse` | REUSE license compliance checker |
| `clang-format` | Code formatting (config in `.clang-format`) |
| `clang-tidy` | Static analysis (config in `.clang-tidy`) |
| `scdoc` | Man page generation |

## Troubleshooting

### Qt6 Not Found

If CMake cannot find Qt6, set the `Qt6_DIR` or `CMAKE_PREFIX_PATH`:

```bash
cmake --preset linux -DQt6_DIR=/usr/lib64/cmake/Qt6
# or
export CMAKE_PREFIX_PATH=/opt/qt6
cmake --preset linux
```

### ECM Not Found

Install `extra-cmake-modules`:

```bash
# Debian/Ubuntu
sudo apt-get install extra-cmake-modules
# Fedora
sudo dnf install extra-cmake-modules
# Arch
sudo pacman -S extra-cmake-modules
```

### Submodule Errors

If you see errors about missing `libnbtplusplus`:

```bash
git submodule update --init --recursive
```

### Missing Qt6 NetworkAuth

This module is not always included in distro Qt6 packages. Install separately:

```bash
# Debian/Ubuntu
sudo apt-get install qt6-networkauth-dev
# Fedora
sudo dnf install qt6-qtnetworkauth-devel
```
