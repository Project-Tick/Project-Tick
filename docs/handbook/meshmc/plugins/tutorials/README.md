# MeshMC MMCO Plugin Tutorials

## Table of Contents

1. [About These Tutorials](#about-these-tutorials)
2. [Prerequisites](#prerequisites)
3. [Learning Path](#learning-path)
4. [Tutorial Index](#tutorial-index)
5. [Environment Setup Checklist](#environment-setup-checklist)
6. [Quick Reference Links](#quick-reference-links)
7. [Getting Help](#getting-help)

---

## About These Tutorials

This section contains **step-by-step, practical guides** for building MeshMC plugins using the MMCO plugin system. Unlike the API reference (which documents every function) or the SDK guide (which explains the build system), these tutorials walk you through complete projects from start to finish.

Each tutorial is self-contained: you can follow it from scratch and end up with a working `.mmco` plugin. The tutorials are ordered by complexity, and each one builds on concepts introduced in the previous.

### What You Will Build

| Tutorial | What You Build | Concepts Covered |
|---|---|---|
| [Hello World](hello-world.md) | A minimal plugin that logs messages and enumerates instances | Module declaration, `mmco_init`, `mmco_unload`, hooks, logging, settings, build system |
| [BackupSystem Walkthrough](backup-system-walkthrough.md) | A deep analysis of the BackupSystem plugin (a real, shipping plugin) | UI pages, `.ui` files, `BasePage` subclassing, toolbar actions, `MMCZip`, multi-file architecture |

---

## Prerequisites

Before starting these tutorials, you should have:

### Required Knowledge

- **C++ fundamentals** — classes, pointers, `static`, `extern "C"`, shared libraries
- **CMake basics** — `add_library`, `target_link_libraries`, `find_package`, build presets
- **Command-line comfort** — navigating directories, running build commands, reading compiler output

### Recommended Knowledge

- **Qt basics** — `QWidget`, `QObject`, signals and slots, `QTreeWidget`, `QLayout`, `.ui` files
- **Git** — cloning repos, creating branches (useful if you want to contribute plugins back)

### Required Software

| Software | Minimum Version | Purpose |
|---|---|---|
| C++ compiler | C++17 support (GCC 9+, Clang 10+, MSVC 2019+) | Compiling plugin code |
| CMake | 3.21+ | Build system |
| Qt6 | 6.x (Core, Widgets, Gui) | UI toolkit (linked through the SDK) |
| MeshMC (installed with SDK) | Latest | Provides `mmco_sdk.h`, `MeshMC::SDK` CMake target, runtime host |

### Installing MeshMC with SDK Headers

If you are building plugins **out-of-tree** (recommended for third-party plugins), you need MeshMC installed with its SDK files:

```bash
# Build MeshMC from source
cmake -B build --preset linux
cmake --build build --config Release -j$(nproc)

# Install (includes SDK headers and CMake config)
sudo cmake --install build --prefix /usr/local
```

After installation, these files should exist:

```
/usr/local/include/MeshMC/plugin/sdk/mmco_sdk.h
/usr/local/lib/cmake/MeshMC_SDK/MeshMCSDKConfig.cmake
```

---

## Learning Path

We recommend following the tutorials in this order:

### Stage 1: Foundations (Start Here)

**[Hello World](hello-world.md)** — The essential first plugin. You will:

- Create a project from scratch with CMake
- Understand the `MMCO_DEFINE_MODULE` macro
- Write `mmco_init()` and `mmco_unload()`
- Register hook callbacks for `MMCO_HOOK_APP_INITIALIZED` and `MMCO_HOOK_INSTANCE_PRE_LAUNCH`
- Log messages, read/write settings, query the app version
- Build, install, and test the plugin

**Time estimate**: 30–60 minutes for a first-timer.

### Stage 2: Understanding the API

After completing Hello World, read these reference pages to deepen your understanding:

- [API Reference](../api-reference/) — All 14 sections of `MMCOContext`
- [Hooks Reference](../hooks-reference/) — Every hook ID, its payload, and when it fires
- [MMCO Format](../mmco-format.md) — Binary structure, ABI versioning, magic numbers

### Stage 3: Real-World Plugin

**[BackupSystem Walkthrough](backup-system-walkthrough.md)** — A detailed analysis of a production plugin. You will:

- Understand multi-file plugin architecture (entry point + manager + UI page)
- See how `MMCO_HOOK_UI_INSTANCE_PAGES` injects a custom page into instance settings
- Learn how toolbar actions work (`ui_register_instance_action` → MainWindow consumer)
- Study `BackupManager` for real-world use of `MMCZip::compressDir` / `extractDir`
- Analyze the `BackupPage` UI: `.ui` file + `BasePage` + signal/slot pattern
- Understand important design decisions and pitfalls (the `QDir::Hidden` bug)

**Time estimate**: 60–90 minutes for a careful reading.

### Stage 4: Build Your Own

After completing all tutorials, you should be ready to:

1. Design a plugin architecture (entry point + logic + optional UI)
2. Choose the right hooks for your use case
3. Build and test out-of-tree with CMake
4. Distribute your `.mmco` for other users to install

---

## Tutorial Index

| File | Title | Lines | Difficulty |
|---|---|---|---|
| [hello-world.md](hello-world.md) | Hello World — Your First MeshMC Plugin | ~600 | Beginner |
| [backup-system-walkthrough.md](backup-system-walkthrough.md) | BackupSystem — A Production Plugin Case Study | ~700 | Intermediate |

---

## Environment Setup Checklist

Use this checklist to verify your development environment before starting:

- [ ] C++ compiler with C++17 support is installed (`g++ --version`)
- [ ] CMake 3.21+ is installed (`cmake --version`)
- [ ] Qt6 development packages are installed (`qmake6 --version` or check pkg-config)
- [ ] MeshMC is installed from source with `cmake --install`
- [ ] `mmco_sdk.h` is available at the install prefix
- [ ] `find_package(MeshMC_SDK)` succeeds in a test CMake project
- [ ] MeshMC can run and shows the main launcher window
- [ ] The `mmcmodules/` directory exists (or will be created) in one of the search paths

### Plugin Search Paths

Plugins (`.mmco` files) are loaded from these directories, in order:

| Path | Use Case |
|---|---|
| `<app_dir>/mmcmodules/` | Portable / bundled with the app |
| `~/.local/lib/mmcmodules/` | User-local plugins (Linux) |
| `/usr/local/lib/mmcmodules/` | System-wide (manual install) |
| `/usr/lib/mmcmodules/` | Distro packages |

For development, `~/.local/lib/mmcmodules/` is the most convenient target.

---

## Quick Reference Links

| Topic | Link |
|---|---|
| Plugin system overview | [overview.md](../overview.md) |
| MMCO binary format | [mmco-format.md](../mmco-format.md) |
| Full API reference | [api-reference/](../api-reference/) |
| Hooks reference | [hooks-reference/](../hooks-reference/) |
| SDK guide | [sdk-guide/](../sdk-guide/) |
| Out-of-tree build setup | [sdk-guide/out-of-tree-setup.md](../sdk-guide/out-of-tree-setup.md) |
| CMake config reference | [sdk-guide/cmake-config.md](../sdk-guide/cmake-config.md) |

---

## Getting Help

- **Build errors**: Check that `CMAKE_PREFIX_PATH` includes both your MeshMC install prefix and your Qt6 prefix.
- **Plugin not loading**: Run MeshMC from the terminal and check log output. Common issues: wrong ABI version, missing `mmco_module_info` symbol, file permissions.
- **Runtime crashes**: Enable debug logging and check the hook callback signatures — wrong payload casts are the #1 crash cause.
- **Qt linker errors**: Ensure you link against `MeshMC::SDK` (not raw Qt targets) and that your Qt version matches the one MeshMC was built with.
