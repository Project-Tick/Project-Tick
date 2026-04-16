# MeshMC MMCO Plugin System — Overview

## Table of Contents

1. [Introduction](#introduction)
2. [Architecture](#architecture)
3. [Licensing](#licensing)
4. [Module Format (.mmco)](#module-format-mmco)
5. [Plugin Lifecycle](#plugin-lifecycle)
6. [API Sections at a Glance](#api-sections-at-a-glance)
7. [Quick Start](#quick-start)
8. [Documentation Map](#documentation-map)

---

## Introduction

MeshMC includes a native plugin system called **MMCO** (MeshMC Module Character Object). MMCO allows third-party developers to extend the launcher with new features, pages, hooks, and automation — all without forking or modifying MeshMC itself.

Plugins are compiled as shared libraries (`.so` on Linux, `.dll` on Windows, `.dylib` on macOS) with the `.mmco` file extension. They are discovered at runtime from well-known directories, validated against a binary ABI, and initialized through a stable C-linkage interface.

The MMCO system is designed around the following principles:

- **Stable ABI**: Plugins interact with MeshMC exclusively through the `MMCOContext` struct — a table of function pointers. There are no virtual methods or C++ STL types crossing the plugin boundary (for the C API layer; the SDK layer provides Qt/C++ convenience).
- **Sandboxed access**: Plugins cannot call arbitrary Qt or MeshMC internals. All interaction is mediated through the API.
- **Hook-driven extensibility**: Plugins observe and intercept launcher events through a publish-subscribe hook system.
- **Graceful lifecycle**: Plugins are initialized in discovery order and shut down in reverse order, with clear entry/exit points.

### What Can Plugins Do?

With the MMCO API, plugins can:

| Capability | API Section |
|---|---|
| Log messages at info/warn/error/debug levels | Section 1 — Logging |
| Register and unregister event hooks | Section 2 — Hooks |
| Read and write namespaced settings | Section 3 — Settings |
| Enumerate, query, and control instances | Section 4 — Instance Management |
| List, enable/disable, install, and remove mods | Section 5 — Mod Management |
| List, rename, delete, and install worlds | Section 6 — World Management |
| Query Minecraft accounts (read-only) | Section 7 — Account Management |
| Query detected Java installations (read-only) | Section 8 — Java Management |
| Perform sandboxed and absolute filesystem operations | Section 9 — Filesystem |
| Create and extract zip archives | Section 10 — Zip / Archive |
| Make HTTP GET and POST requests | Section 11 — Network |
| Show message boxes, file dialogs, input dialogs | Section 12 — UI Dialogs |
| Build full GUI pages with layouts, buttons, labels, trees | Section 13 — UI Page Builder |
| Register toolbar actions on the main window | Section 12 — UI Dialogs (extended) |
| Query app version, name, and current timestamp | Section 14 — Utility |

---

## Architecture

The MMCO system consists of five layers:

```
┌──────────────────────────────────────────────────┐
│                 .mmco Plugins                     │
│         (shared libraries with C linkage)         │
├──────────────────────────────────────────────────┤
│               mmco_sdk.h (SDK)                    │
│   Single-include header providing Qt types,       │
│   MeshMC types, macros, and MMCOContext struct     │
├──────────────────────────────────────────────────┤
│              MMCOContext (API)                     │
│   Struct of function pointers — the ONLY          │
│   interface between plugins and the launcher      │
├──────────────────────────────────────────────────┤
│            PluginManager (Bridge)                 │
│   Implements all API functions as static          │
│   trampolines, manages lifecycle, dispatches hooks│
├──────────────────────────────────────────────────┤
│          PluginLoader (Discovery)                 │
│   Scans directories, dlopen, validates magic/ABI, │
│   resolves symbols                                │
├──────────────────────────────────────────────────┤
│           MMCOFormat (Binary ABI)                 │
│   Magic number, ABI version, module info struct,  │
│   export macros                                   │
└──────────────────────────────────────────────────┘
```

### Key Classes

| Class | File | Responsibility |
|---|---|---|
| `MMCOFormat` | `plugin/MMCOFormat.h` | Binary format constants, `MMCOModuleInfo` struct, export macros |
| `PluginLoader` | `plugin/PluginLoader.h/.cpp` | Directory scanning, dlopen/dlsym, ABI validation |
| `PluginMetadata` | `plugin/PluginMetadata.h` | Parsed module metadata (name, version, entry points) |
| `PluginManager` | `plugin/PluginManager.h/.cpp` | Lifecycle management, API trampoline implementations, hook dispatch |
| `PluginHooks` | `plugin/PluginHooks.h` | Hook ID enum, payload structs, callback typedef |
| `PluginAPI` | `plugin/PluginAPI.h` | `MMCOContext` struct definition (the API surface) |
| SDK Header | `plugin/sdk/mmco_sdk.h` | Standalone single-include for plugin development |

### Data Flow

```
  Application::main()
       │
       ▼
  PluginManager::initializeAll()
       │
       ├── PluginLoader::discoverModules()
       │       │
       │       ├── scanDirectory("/path/mmcmodules/")
       │       │       │
       │       │       ├── dlopen("Example.mmco")
       │       │       ├── dlsym("mmco_module_info") → validate magic/ABI
       │       │       ├── dlsym("mmco_init")
       │       │       └── dlsym("mmco_unload")
       │       │
       │       └── return QVector<PluginMetadata>
       │
       ├── For each module:
       │       ├── ensurePluginDataDir()
       │       ├── buildContext() → MMCOContext with function pointers
       │       └── meta.initFunc(&context) → mmco_init()
       │
       └── dispatchHook(MMCO_HOOK_APP_INITIALIZED)
```

---

## Licensing

The MMCO plugin system is licensed under **GPL-3.0-or-later WITH LicenseRef-MeshMC-MMCO-Module-Exception-1.0**.

The **MeshMC MMCO Module Exception 1.0** grants a special permission: if your plugin interacts with MeshMC exclusively through the MMCO SDK (`mmco_sdk.h`) and the `MMCOContext` API, you may license your plugin under **any license of your choosing** — proprietary, MIT, Apache, etc.

This means:
- The MeshMC launcher itself remains GPL-3.0-or-later
- The SDK header and API definitions carry the exception
- Plugins that only use the SDK are not required to be GPL
- Plugins that directly include MeshMC internal headers (bypassing the SDK) must be GPL-compatible

This is similar to the Linux kernel's approach with kernel modules and its syscall exception.

---

## Module Format (.mmco)

Every `.mmco` file is a standard shared library (ELF `.so`, PE `.dll`, or Mach-O `.dylib`) renamed to `.mmco`. It must export three symbols with C linkage:

```cpp
extern "C" MMCOModuleInfo mmco_module_info;
extern "C" int  mmco_init(MMCOContext* ctx);
extern "C" void mmco_unload(void);
```

### MMCOModuleInfo

```cpp
struct MMCOModuleInfo {
    uint32_t    magic;          // Must be 0x4D4D434F ("MMCO")
    uint32_t    abi_version;    // Must be MMCO_ABI_VERSION (currently 1)
    const char* name;           // Human-readable name
    const char* version;        // Semantic version string
    const char* author;         // Author or organization
    const char* description;    // Short description
    const char* license;        // SPDX license identifier
    uint32_t    flags;          // Reserved, set to 0
};
```

The SDK provides a convenience macro:

```cpp
MMCO_DEFINE_MODULE("MyPlugin", "1.0.0", "Author Name",
                   "Description of the plugin",
                   "MIT");
```

See [Module Format & Lifecycle](module-lifecycle.md) for complete details.

---

## Plugin Lifecycle

```
┌─────────────┐     ┌──────────────┐     ┌───────────────┐
│  Discovery   │────▶│ Validation   │────▶│ Initialization│
│  (dlopen)    │     │ (magic/ABI)  │     │ (mmco_init)   │
└─────────────┘     └──────────────┘     └───────────────┘
                                                │
                         Running ◀──────────────┘
                           │
                    (hooks dispatched,
                     API calls made)
                           │
                           ▼
                    ┌──────────────┐     ┌──────────────┐
                    │  Shutdown    │────▶│   Cleanup    │
                    │(mmco_unload) │     │  (dlclose)   │
                    └──────────────┘     └──────────────┘
```

1. **Discovery**: `PluginLoader` scans search paths for `*.mmco` files
2. **Loading**: `dlopen()` opens the shared library
3. **Validation**: `mmco_module_info` symbol is resolved and checked (magic = `0x4D4D434F`, ABI version match)
4. **Symbol Resolution**: `mmco_init` and `mmco_unload` are resolved
5. **Context Building**: `PluginManager::buildContext()` creates `MMCOContext` with all function pointers
6. **Initialization**: `mmco_init(&ctx)` is called — return 0 for success
7. **Running**: Plugin receives hooks, makes API calls
8. **Shutdown**: `mmco_unload()` is called (reverse order)
9. **Cleanup**: Library handle is closed via `dlclose()`

### Search Paths

Modules are discovered from these directories (in order):

| Path | Purpose |
|---|---|
| `<app_dir>/mmcmodules/` | In-tree / portable installations |
| `~/.local/lib/mmcmodules/` | User-local plugins (Linux) |
| `/usr/local/lib/mmcmodules/` | System-wide (manual install) |
| `/usr/lib/mmcmodules/` | Distro packages |
| `%LOCALAPPDATA%/MeshMC/mmcmodules/` | User-local (Windows) |

Duplicate modules (same `moduleId()`) are skipped — first found wins.

---

## API Sections at a Glance

The `MMCOContext` struct is organized into 14 sections:

| # | Section | Functions | Description |
|---|---|---|---|
| 1 | Logging | 4 | `log_info`, `log_warn`, `log_error`, `log_debug` |
| 2 | Hooks | 2 | `hook_register`, `hook_unregister` |
| 3 | Settings | 2 | `setting_get`, `setting_set` |
| 4 | Instance Management | 35 | Full instance CRUD, state, groups, components, directories |
| 5 | Mod Management | 10 | Per-type mod list, enable/disable, install, remove |
| 6 | World Management | 10 | World list, rename, delete, install |
| 7 | Account Management | 7 | Account enumeration (read-only) |
| 8 | Java Management | 6 | Java installation query (read-only) |
| 9 | Filesystem | 10 | Sandboxed + absolute path file operations |
| 10 | Zip / Archive | 2 | Compress directory, extract archive |
| 11 | Network | 2 | HTTP GET and POST with async callbacks |
| 12 | UI Dialogs | 8 | Message boxes, file dialogs, input dialogs, toolbar actions |
| 13 | UI Page Builder | 17 | Pages, layouts, buttons, labels, tree widgets |
| 14 | Utility | 3 | App version, app name, timestamp |

**Total: 118 API functions**

---

## Quick Start

### Minimal Plugin

```cpp
#include "mmco_sdk.h"

MMCO_DEFINE_MODULE("MyPlugin", "1.0.0", "Your Name",
                   "A minimal MeshMC plugin", "MIT");

static MMCOContext* g_ctx = nullptr;

extern "C" int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;
    MMCO_LOG(ctx, "MyPlugin initialized!");
    return 0;
}

extern "C" void mmco_unload()
{
    if (g_ctx)
        MMCO_LOG(g_ctx, "MyPlugin unloading.");
    g_ctx = nullptr;
}
```

### Building

```bash
g++ -shared -fPIC -std=c++17 \
    -I/path/to/meshmc/launcher \
    -o MyPlugin.mmco MyPlugin.cpp
```

### Installing

```bash
cp MyPlugin.mmco ~/.local/lib/mmcmodules/
```

### Verifying

Launch MeshMC and check the log output for:
```
[Plugin: MyPlugin] MyPlugin initialized!
```

---

## Documentation Map

Each API layer and section has its own detailed documentation page:

| Document | Description |
|---|---|
| [Module Format & Lifecycle](module-lifecycle.md) | Binary format, search paths, discovery, init/shutdown, error handling |
| [SDK Header Reference](sdk-reference.md) | `mmco_sdk.h` — includes, macros, convenience helpers |
| [Section 1 — Logging](api-logging.md) | `log_info`, `log_warn`, `log_error`, `log_debug` |
| [Section 2 — Hooks](api-hooks.md) | Hook registration, hook IDs, payload structs, callback chains |
| [Section 3 — Settings](api-settings.md) | Namespaced key-value settings |
| [Section 4 — Instance Management](api-instance-management.md) | Full instance API — enumeration, properties, state, groups, components |
| [Section 5 — Mod Management](api-mod-management.md) | Per-type mod operations |
| [Section 6 — World Management](api-world-management.md) | World list, rename, delete, install |
| [Section 7 — Account Management](api-account-management.md) | Read-only account enumeration |
| [Section 8 — Java Management](api-java-management.md) | Read-only Java installation query |
| [Section 9 — Filesystem](api-filesystem.md) | Sandboxed and absolute path operations |
| [Section 10 — Zip / Archive](api-zip.md) | Archive compression and extraction |
| [Section 11 — Network](api-network.md) | HTTP GET/POST with async callbacks |
| [Section 12 — UI Dialogs](api-ui-dialogs.md) | Message boxes, file dialogs, input dialogs, toolbar actions |
| [Section 13 — UI Page Builder](api-ui-page-builder.md) | Pages, layouts, buttons, labels, tree widgets |
| [Section 14 — Utility](api-utility.md) | App info and timestamps |
| [Case Study: BackupSystem](case-study-backup-system.md) | Complete walkthrough of an in-tree plugin |

---

*This documentation corresponds to MMCO ABI version 1, as shipped with MeshMC v202604151700.*
