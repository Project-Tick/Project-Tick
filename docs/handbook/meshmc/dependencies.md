# Dependencies

## Overview

MeshMC depends on a mix of bundled libraries (shipped in the source tree under `libraries/`) and external libraries resolved at build time via the system package manager.

## Bundled Libraries

These libraries are included in the `libraries/` directory and built as part of the MeshMC CMake project:

| Library | Directory | Purpose | License |
|---|---|---|---|
| **ganalytics** | `libraries/ganalytics/` | Google Analytics integration for usage telemetry | MIT |
| **systeminfo** | `libraries/systeminfo/` | System information queries (OS, CPU, memory) | GPL-3.0-or-later |
| **hoedown** | `libraries/hoedown/` | Markdown rendering (changelogs, news) | ISC |
| **launcher** | `libraries/launcher/` | Java process launcher helper binary | GPL-3.0-or-later |
| **javacheck** | `libraries/javacheck/` | Java installation validator (JAR) | GPL-3.0-or-later |
| **xz-embedded** | `libraries/xz-embedded/` | XZ decompression (embedded, minimal) | Public Domain |
| **rainbow** | `libraries/rainbow/` | KDE-style color utilities | LGPL-2.1 |
| **iconfix** | `libraries/iconfix/` | Qt icon theme fixes | Apache-2.0 |
| **LocalPeer** | `libraries/LocalPeer/` | Single-instance IPC (based on QtSingleApplication) | LGPL-2.1 |
| **classparser** | `libraries/classparser/` | Java `.class` file parser (mod metadata) | GPL-3.0-or-later |
| **optional-bare** | `libraries/optional-bare/` | C++17 `std::optional` polyfill for older code | BSL-1.0 |
| **tomlc99** | `libraries/tomlc99/` | TOML parser (C99) | MIT |
| **katabasis** | `libraries/katabasis/` | OAuth2 library (MSA authentication) | BSD-2-Clause |
| **libnbtplusplus** | `libraries/libnbtplusplus/` | NBT (Named Binary Tag) parser for Minecraft data | LGPL-3.0 |
| **qdcss** | `libraries/qdcss/` | CSS-like parser for theme files | GPL-3.0-or-later |
| **murmur2** | `libraries/murmur2/` | MurmurHash2 implementation (CurseForge fingerprinting) | Public Domain |

### ganalytics

Provides opt-in usage analytics via Google Analytics Measurement Protocol. Tracks feature usage, not personal data. Can be disabled in settings.

### systeminfo

Cross-platform system info queries:
- Operating system name and version
- CPU architecture and model
- Available memory
- Used for analytics and crash reports

### launcher

A small native binary that acts as the actual Java process launcher:
- Handles process spawning on all platforms
- Supports wrapper commands
- Manages stdio piping

### javacheck

A minimal Java program (`JavaCheck.class`) that prints JVM system properties. Spawned by `JavaChecker` to validate Java installations without loading the full Minecraft runtime.

### classparser

Parses Java `.class` files to extract:
- Mod metadata (name, version, mod ID)
- Forge/Fabric/Quilt mod annotations
- Used by `LocalModParseTask` for mod discovery

### katabasis

OAuth2 implementation used for Microsoft Account authentication:
- Token storage structures (`Katabasis::Token`)
- Token validity tracking
- Refresh token management

### libnbtplusplus

Parses and writes Minecraft NBT (Named Binary Tag) format:
- Level.dat parsing for world metadata
- Server.dat parsing for server list
- Used by InstanceImportTask and world management

## External Dependencies

These are resolved at build time and must be installed on the system:

| Library | CMake Target | Purpose | Required |
|---|---|---|---|
| **Qt6::Core** | `Qt6::Core` | Foundation (strings, containers, I/O, events) | Yes |
| **Qt6::Widgets** | `Qt6::Widgets` | GUI toolkit | Yes |
| **Qt6::Concurrent** | `Qt6::Concurrent` | Threading utilities | Yes |
| **Qt6::Network** | `Qt6::Network` | HTTP, SSL, proxy | Yes |
| **Qt6::NetworkAuth** | `Qt6::NetworkAuth` | OAuth2 (MSA authentication) | Yes |
| **Qt6::Test** | `Qt6::Test` | Unit testing framework | Optional |
| **Qt6::Xml** | `Qt6::Xml` | XML parsing | Yes |
| **libarchive** | `LibArchive::LibArchive` | Archive extraction (zip, tar, 7z) | Yes |
| **zlib** | `ZLIB::ZLIB` | Compression (used by libarchive, QuaZip) | Yes |
| **ECM** | `extra-cmake-modules` | KDE CMake macros (install dirs, icons) | Yes |
| **cmark** | `cmark` | CommonMark rendering (changelogs) | Yes |
| **tomlplusplus** | `tomlplusplus::tomlplusplus` | TOML parsing (C++17) | Yes |
| **libqrencode** | `qrencode` | QR code generation (MSA login) | Optional |
| **QuaZip** | `QuaZip::QuaZip` | Qt-based ZIP file I/O | Yes |
| **Sparkle** | `Sparkle.framework` | macOS auto-update framework | macOS only |

### Qt6

Minimum version: **Qt 6.7** (for full C++23 and NetworkAuth support).

Required Qt modules:
```cmake
find_package(Qt6 6.7 REQUIRED COMPONENTS
    Core
    Widgets
    Concurrent
    Network
    NetworkAuth
    Xml
)
find_package(Qt6 6.7 COMPONENTS Test)
```

### libarchive

Used for extracting:
- Minecraft archives (`.jar`, `.zip`)
- Modpack archives (`.mrpack`, `.zip`)
- Java runtime archives (`.tar.gz`, `.zip`)

### Extra CMake Modules (ECM)

KDE's CMake module collection provides:
- `KDEInstallDirs` — standardized install paths
- `ecm_install_icons` — icon theme installation
- `ECMQueryQt` — Qt path queries

### cmark

CommonMark rendering for:
- Changelogs and release notes
- CurseForge/Modrinth mod descriptions
- News feed content

The `cmark` source is included in the repository at `/cmark/` as a subproject.

### tomlplusplus

Modern C++ TOML parser used for:
- Fabric/Quilt mod metadata (`fabric.mod.json` alternative format)
- Configuration file parsing

The `tomlplusplus` source is included in the repository at `/tomlplusplus/`.

### QuaZip

Qt wrapper around zlib/minizip for ZIP I/O:
- Instance export (creating `.zip` files)
- Instance import (reading `.zip` modpacks)
- Mod file inspection

## Build vs Runtime Dependencies

### Build-Only Dependencies

| Dependency | Purpose |
|---|---|
| CMake 3.28+ | Build system generator |
| Ninja | Build tool |
| C++23 compiler | GCC 14+, Clang 18+, MSVC 17.10+ |
| ECM | CMake macros |
| Qt6 (all modules) | Framework headers and libs |

### Runtime Dependencies

| Dependency | Purpose |
|---|---|
| Qt6 shared libraries | Core framework runtime |
| libarchive | Archive operations |
| zlib | Compression |
| OpenSSL / Schannel | TLS for network operations |
| Java 8-21 | Minecraft runtime (user-managed) |

### Optional Runtime Dependencies

| Dependency | Purpose | Platform |
|---|---|---|
| libqrencode | QR code for MSA login | All (optional feature) |
| Sparkle | Auto-updates | macOS |
| xdg-utils | Open URLs/files | Linux |

## Dependency Graph

```
meshmc (executable)
├── Qt6::Core, Qt6::Widgets, Qt6::Concurrent, Qt6::Network, Qt6::NetworkAuth, Qt6::Xml
├── LibArchive::LibArchive
├── ZLIB::ZLIB
├── QuaZip::QuaZip
├── cmark
├── tomlplusplus::tomlplusplus
├── libraries/
│   ├── ganalytics (→ Qt6::Core, Qt6::Network)
│   ├── systeminfo (→ Qt6::Core)
│   ├── hoedown (C library, no Qt dependency)
│   ├── launcher (→ Qt6::Core)
│   ├── javacheck (Java, no C++ deps)
│   ├── xz-embedded (C library, no deps)
│   ├── rainbow (→ Qt6::Core, Qt6::Widgets)
│   ├── iconfix (→ Qt6::Core, Qt6::Widgets)
│   ├── LocalPeer (→ Qt6::Core, Qt6::Network)
│   ├── classparser (→ Qt6::Core)
│   ├── tomlc99 (C library, no deps)
│   ├── katabasis (→ Qt6::Core, Qt6::Network, Qt6::NetworkAuth)
│   ├── libnbtplusplus (→ ZLIB::ZLIB)
│   ├── qdcss (→ Qt6::Core)
│   └── murmur2 (C library, no deps)
└── optional: qrencode, Sparkle.framework
```
