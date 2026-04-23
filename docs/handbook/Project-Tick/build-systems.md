# Project Tick — Build Systems

## Overview

Project Tick uses seven distinct build systems across its sub-projects, each
chosen to match the upstream heritage and language ecosystem of the component.
This document provides a comprehensive reference for each build system, common
patterns, and cross-cutting concerns.

---

## Build System Matrix

| Build System | Sub-Projects | Language | Configuration |
|-------------|-------------|----------|---------------|
| **CMake** | meshmc, neozip, cmark, genqrcode, json4cpp, libnbtplusplus, mnv | C/C++ | `CMakeLists.txt`, `CMakePresets.json` |
| **Meson** | tomlplusplus | C++ | `meson.build`, `meson_options.txt` |
| **Make (GNU Make)** | cgit, corebinutils | C | `Makefile`, `GNUmakefile` |
| **Autotools** | mnv, genqrcode, neozip | C | `configure.ac`, `Makefile.am`, `configure` |
| **Gradle** | forgewrapper | Java | `build.gradle`, `settings.gradle` |
| **Cargo** | tickborg | Rust | `Cargo.toml`, `Cargo.lock` |
| **Poetry** | meta | Python | `pyproject.toml`, `poetry.lock` |
| **Nix** | CI, dev shells, deployments | Multi | `flake.nix`, `default.nix` |

---

## CMake

CMake is the dominant build system in Project Tick, used by seven sub-projects.

### Minimum Versions

| Component | CMake Minimum | C++ Standard | C Standard |
|-----------|--------------|-------------|-----------|
| meshmc | 3.20 | C++23 | C23 (C11 on MSVC) |
| neozip | 3.14 | — | C11 |
| cmark | 3.5 | — | C99 |
| genqrcode | 3.5 | — | C99 |
| json4cpp | 3.1 | C++11 | — |
| libnbtplusplus | 3.15 | C++11 | — |
| mnv | 3.10 | — | C11 |

### MeshMC CMake Configuration

MeshMC has the most sophisticated CMake setup in the monorepo, including:

#### CMake Presets (`meshmc/CMakePresets.json`)

All presets inherit from a hidden `base` preset:

```json
{
  "name": "base",
  "hidden": true,
  "generator": "Ninja Multi-Config",
  "binaryDir": "build",
  "installDir": "install",
  "cacheVariables": {
    "ENABLE_LTO": "ON"
  }
}
```

Platform presets:

| Preset | OS | Toolchain | vcpkg |
|--------|-----|-----------|-------|
| `linux` | Linux | System | No |
| `macos` | macOS | System | Yes (`$VCPKG_ROOT`) |
| `macos_universal` | macOS | Universal (x86_64+arm64) | Yes |
| `windows_mingw` | Windows | MinGW | No |
| `windows_msvc` | Windows | MSVC | Yes (`$VCPKG_ROOT`) |

Usage:

```bash
# Configure
cmake --preset linux

# Build
cmake --build --preset linux

# Test (uses CTest)
cd build && ctest --output-on-failure

# Install
cmake --install build --config Release --prefix install
```

#### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `ENABLE_LTO` | OFF | Link Time Optimization |
| `MeshMC_DISABLE_JAVA_DOWNLOADER` | OFF | Disable Java auto-download |
| `MeshMC_ENABLE_CLANG_TIDY` | OFF | Run clang-tidy during build |

#### External Dependencies (find_package)

```cmake
find_package(Qt6 REQUIRED COMPONENTS
    Core Widgets Concurrent Network NetworkAuth Test Xml
)
find_package(ECM NO_MODULE REQUIRED)
find_package(LibArchive REQUIRED)
```

Additional Qt queries via `QMakeQuery`:
- `QT_INSTALL_PLUGINS` → Plugin directory
- `QT_INSTALL_LIBS` → Library directory
- `QT_INSTALL_LIBEXECS` → Libexec directory

#### Compiler Configuration

```cmake
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED true)
set(CMAKE_C_STANDARD 23)       # C11 on MSVC
set(CMAKE_AUTOMOC ON)           # Qt meta-object compiler
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
```

Compiler flags (GCC/Clang):
```
-Wall -pedantic -Wno-deprecated-declarations
-fstack-protector-strong --param=ssp-buffer-size=4
-O3 -D_FORTIFY_SOURCE=2
-DQT_NO_DEPRECATED_WARNINGS=Y
```

MSVC flags:
```
/W4 /DQT_NO_DEPRECATED_WARNINGS=Y
```

macOS additionally:
```
-stdlib=libc++
```

#### LTO (Link Time Optimization)

When `ENABLE_LTO` is ON, MeshMC uses `CheckIPOSupported`:

```cmake
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
if(ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL TRUE)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)
endif()
```

LTO is **not** enabled for Debug builds.

#### Versioning

```cmake
set(MeshMC_VERSION_MAJOR    7)
set(MeshMC_VERSION_MINOR    0)
set(MeshMC_VERSION_HOTFIX   0)
set(MeshMC_RELEASE_VERSION_NAME "7.0.0")
```

#### Build Targets

The meshmc CMake tree produces:
- Main executable (`meshmc`)
- Libraries in `libraries/` subdirectory
- Java JARs in `${PROJECT_BINARY_DIR}/jars`
- Tests (via ECMAddTests when `BUILD_TESTING` is ON)

### NeoZip CMake Configuration

NeoZip supports both CMake and traditional `./configure`:

```bash
# CMake
mkdir build && cd build
cmake .. -G Ninja \
    -DZLIB_COMPAT=ON \      # zlib-compatible API
    -DWITH_GTEST=ON         # Enable Google Test
ninja
ctest

# Autotools
./configure
make -j$(nproc)
make test
```

Key CMake variables:
- `ZLIB_COMPAT` — Build with zlib-compatible API
- `WITH_GTEST` — Build with Google Test
- `WITH_BENCHMARKS` — Build benchmarks
- Architecture-specific SIMD flags are auto-detected

### cmark CMake Configuration

```bash
mkdir build && cd build
cmake .. -G Ninja \
    -DCMARK_TESTS=ON \
    -DCMARK_SHARED=ON
ninja
ctest
```

### json4cpp CMake Configuration

json4cpp supports CMake, Meson (via `meson.build`), and Bazel:

```bash
mkdir build && cd build
cmake .. -G Ninja \
    -DJSON_BuildTests=ON
ninja
ctest
```

The library is header-only; the CMake build is primarily for tests.

### libnbt++ CMake Configuration

```bash
mkdir build && cd build
cmake .. \
    -DNBT_BUILD_SHARED=OFF \
    -DNBT_USE_ZLIB=ON \
    -DNBT_BUILD_TESTS=ON
make -j$(nproc)
ctest
```

### genqrcode CMake Configuration

```bash
mkdir build && cd build
cmake .. -G Ninja
ninja
```

Also supports Autotools:
```bash
./autogen.sh
./configure
make -j$(nproc)
```

---

## Meson

### tomlplusplus

toml++ uses Meson as its primary build system:

```bash
meson setup build
ninja -C build
ninja -C build test
```

Meson options (from `meson_options.txt`):
- Build mode (header-only vs compiled)
- Test configuration
- Example programs

Also supports CMake as an alternative:

```bash
mkdir build && cd build
cmake .. -G Ninja
ninja
ctest
```

---

## GNU Make

### cgit

cgit uses a traditional `Makefile` that first builds Git as a dependency:

```bash
# Initialize Git submodule
git submodule init
git submodule update

# Build (builds Git first, then cgit)
make

# Install (default: /var/www/htdocs/cgit)
sudo make install
```

Build options:
- `NO_LUA=1` — Build without Lua scripting support
- `LUA_PKGCONFIG=lua5.1` — Specify Lua implementation
- Custom paths via `cgit.conf`

### corebinutils

CoreBinUtils uses a `./configure` script that generates toolchain overrides,
then builds with GNU Make:

```bash
./configure
make -f GNUmakefile -j$(nproc) all
make -f GNUmakefile test
```

The `configure` script:
- Selects musl-gcc or musl-capable clang by preference
- Falls back to system gcc/clang
- Generates `config.mk` with `CC`, `AR`, `RANLIB`, `CPPFLAGS`, `CFLAGS`,
  `LDFLAGS`

Each subdirectory (e.g., `cat/`, `ls/`, `cp/`) has its own `GNUmakefile`
that the top-level `GNUmakefile` orchestrates.

---

## Autotools

### mnv

MNV supports both CMake and traditional Autotools:

```bash
# Autotools (traditional)
./configure --with-features=huge --enable-gui=auto
make -j$(nproc)
sudo make install

# CMake (alternative)
mkdir build && cd build
cmake .. -G Ninja
ninja
```

The Autotools build system supports extensive feature flags:
- `--with-features={tiny,small,normal,big,huge}`
- `--enable-gui={auto,no,gtk2,gtk3,motif,...}`
- `--enable-python3interp`
- `--enable-luainterp`
- And many more

### genqrcode

GenQRCode uses Autotools:

```bash
./autogen.sh          # Generate configure from configure.ac
./configure           # Configure
make -j$(nproc)       # Build
make check            # Run tests
sudo make install     # Install
```

### neozip

NeoZip's `./configure` is a custom script (not GNU Autoconf):

```bash
./configure
make -j$(nproc)
make test
sudo make install
```

---

## Gradle

### forgewrapper

ForgeWrapper uses Gradle for Java builds:

```bash
# Build
./gradlew build

# Test
./gradlew test

# Clean
./gradlew clean

# Generate JAR
./gradlew jar
```

Project structure:
```
forgewrapper/
├── build.gradle          # Build configuration
├── gradle.properties     # Version and settings
├── settings.gradle       # Project name and modules
├── gradlew               # Unix wrapper script
├── gradlew.bat           # Windows wrapper script
├── gradle/               # Gradle wrapper JAR
├── jigsaw/               # JPMS module configuration
└── src/
    └── main/java/        # Source code
```

The Gradle wrapper (`gradlew`) pins the Gradle version so no system-wide
Gradle installation is needed.

---

## Cargo

### tickborg

The `ofborg/` directory contains a Cargo workspace:

```toml
[workspace]
members = [
    "tickborg",
    "tickborg-simple-build"
]
resolver = "2"

[profile.release]
debug = true
```

#### Building

```bash
cd ofborg

# Build all workspace crates
cargo build

# Build in release mode
cargo build --release

# Run tests
cargo test

# Run lints
cargo clippy

# Format
cargo fmt

# Build specific crate
cargo build -p tickborg
```

#### Workspace Structure

```
ofborg/
├── Cargo.toml              # Workspace root
├── Cargo.lock              # Locked dependencies
├── tickborg/               # Main CI bot crate
│   ├── Cargo.toml
│   └── src/
└── tickborg-simple-build/  # Simplified build crate
    ├── Cargo.toml
    └── src/
```

The workspace uses `resolver = "2"` (Rust 2021 edition resolver) and enables
debug symbols in release builds for profiling.

---

## Poetry

### meta

The `meta/` component uses Poetry for Python dependency management:

```bash
cd meta

# Install dependencies
poetry install

# Run in Poetry environment
poetry run generateMojang

# Or activate shell
poetry shell
generateMojang
```

#### pyproject.toml

```toml
[tool.poetry]
name = "meta"
version = "0.0.5-1"
description = "ProjT Launcher meta generator"
license = "MS-PL"

[tool.poetry.dependencies]
python = ">=3.10,<4.0"
cachecontrol = "^0.14.0"
requests = "^2.31.0"
filelock = "^3.20.3"
packaging = "^25.0"
pydantic = "^1.10.13"

[build-system]
requires = ["poetry-core>=1.0.0"]
build-backend = "poetry.core.masonry.api"
```

#### CLI Entry Points

Poetry scripts provide named commands:

| Command | Function |
|---------|----------|
| `generateFabric` | `meta.run.generate_fabric:main` |
| `generateForge` | `meta.run.generate_forge:main` |
| `generateLiteloader` | `meta.run.generate_liteloader:main` |
| `generateMojang` | `meta.run.generate_mojang:main` |
| `generateNeoForge` | `meta.run.generate_neoforge:main` |
| `generateQuilt` | `meta.run.generate_quilt:main` |
| `generateJava` | `meta.run.generate_java:main` |
| `updateFabric` | `meta.run.update_fabric:main` |
| `updateForge` | `meta.run.update_forge:main` |
| `updateLiteloader` | `meta.run.update_liteloader:main` |
| `updateMojang` | `meta.run.update_mojang:main` |
| `updateNeoForge` | `meta.run.update_neoforge:main` |
| `updateQuilt` | `meta.run.update_quilt:main` |
| `updateJava` | `meta.run.update_java:main` |
| `index` | `meta.run.index:main` |

---

## Nix

Nix is used across the monorepo for reproducible development environments,
CI tooling, and deployment.

### Top-Level Flake (`flake.nix`)

```nix
{
  description = "Project Tick is a project dedicated to providing developers
    with ease of use and users with long-lasting software.";

  inputs = {
    nixpkgs.url = "https://channels.nixos.org/nixos-unstable/nixexprs.tar.xz";
  };
}
```

Provides:
- `devShells.default` — LLVM 22 toolchain with clang-tidy-diff
- `formatter` — nixfmt-rfc-style
- Systems: all `lib.systems.flakeExposed`

The dev shell automatically runs `git submodule update --init --force` on
entry.

### CI Nix (`ci/default.nix`)

The CI Nix expression provides:

1. **treefmt** — Multi-language formatter:
   - `actionlint` — GitHub Actions YAML lint
   - `biome` — JavaScript (single quotes, no semicolons)
   - `keep-sorted` — Sort annotated blocks
   - `nixfmt` — Nix formatting (RFC style)
   - `yamlfmt` — YAML (retain line breaks)
   - `zizmor` — GitHub Actions security scanning

2. **codeowners-validator** — Built from source with patches:
   - `owners-file-name.patch`
   - `permissions.patch`

3. **Pinned Nixpkgs** — `ci/pinned.json` locks the Nixpkgs revision:
   ```bash
   # Update pinned revision
   ./ci/update-pinned.sh
   ```

### Meta Nix (`meta/flake.nix`)

The meta component provides a NixOS module for deployment:

```nix
services.blockgame-meta = {
  enable = true;
  settings = {
    DEPLOY_TO_GIT = "true";
    GIT_AUTHOR_NAME = "...";
    GIT_AUTHOR_EMAIL = "...";
  };
};
```

### MeshMC Nix (`meshmc/flake.nix`)

MeshMC provides its own Nix flake for building:

```bash
cd meshmc
nix build
```

### Per-Project Nix Files

Several sub-projects include `default.nix`, `shell.nix`, or `flake.nix` for
Nix-based development:

| Project | Nix Files |
|---------|-----------|
| meshmc | `flake.nix`, `default.nix`, `shell.nix` |
| meta | `flake.nix` |
| ofborg | `flake.nix`, `default.nix`, `shell.nix`, `service.nix` |
| ci | `default.nix` |
| ci/github-script | `shell.nix` |
| cmark | `shell.nix` |

---

## Cross-Cutting Build Concerns

### Compile Commands Database

MeshMC generates `compile_commands.json` via:

```cmake
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
```

This file is used by clang-tidy, clangd, and other tools for accurate
code analysis.

### Testing Frameworks

| Project | Test Framework | Runner |
|---------|---------------|--------|
| meshmc | Qt Test + CTest | `ctest` |
| neozip | Google Test + CTest | `ctest` |
| json4cpp | Catch2 + CTest | `ctest` |
| tomlplusplus | Catch2 | `ninja test` |
| libnbtplusplus | CTest | `ctest` |
| cmark | Custom + CTest | `ctest` |
| forgewrapper | JUnit + Gradle | `./gradlew test` |
| tickborg | Rust built-in | `cargo test` |
| corebinutils | Custom shell tests | `make test` |
| mnv | Custom test framework | `make test` |

### Parallel Build Support

All build systems support parallel builds:

```bash
# CMake/Ninja
cmake --build build -j$(nproc)

# Make
make -j$(nproc)

# Cargo
cargo build -j $(nproc)

# Gradle
./gradlew build --parallel
```

### Out-of-Source Build Enforcement

MeshMC enforces out-of-source builds:

```cmake
string(COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${CMAKE_BUILD_DIR}" IS_IN_SOURCE_BUILD)
if(IS_IN_SOURCE_BUILD)
    message(FATAL_ERROR "You are building MeshMC in-source.")
endif()
```

### WSL Build Rejection

MeshMC explicitly rejects builds on WSL (Windows Subsystem for Linux):

```cmake
if(CMAKE_HOST_SYSTEM_VERSION MATCHES ".*[Mm]icrosoft.*" OR
   CMAKE_HOST_SYSTEM_VERSION MATCHES ".*WSL.*")
    message(FATAL_ERROR "Building MeshMC is not supported in Linux-on-Windows distributions.")
endif()
```

---

## Build Quick Reference

| Action | meshmc | neozip | cgit | toml++ | tickborg | meta | forgewrapper |
|--------|--------|--------|------|--------|----------|------|-------------|
| Configure | `cmake --preset linux` | `cmake -B build` | — | `meson setup build` | — | — | — |
| Build | `cmake --build --preset linux` | `ninja -C build` | `make` | `ninja -C build` | `cargo build` | `poetry install` | `./gradlew build` |
| Test | `ctest` | `ctest` | — | `ninja -C build test` | `cargo test` | — | `./gradlew test` |
| Install | `cmake --install build` | `ninja -C build install` | `make install` | `ninja -C build install` | `cargo install` | — | — |
| Clean | rm -rf build | rm -rf build | `make clean` | rm -rf build | `cargo clean` | — | `./gradlew clean` |
| Format | `clang-format -i` | — | — | — | `cargo fmt` | — | — |
| Lint | `clang-tidy` | — | — | — | `cargo clippy` | — | — |
