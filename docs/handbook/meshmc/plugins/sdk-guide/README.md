# MeshMC Plugin SDK — Overview

The MeshMC Plugin SDK provides everything you need to build `.mmco` plugin
modules for the MeshMC launcher.  It ships as a **header-only INTERFACE
library** (no compiled artefacts of its own) plus a set of CMake package
configuration files that let both *in-tree* and *out-of-tree* plugin projects
consume the SDK with a single `target_link_libraries` call.

This guide is split into four pages:

| Page | Description |
|------|-------------|
| [README.md](README.md) (this page) | Architecture overview, components, platforms |
| [in-tree-setup.md](in-tree-setup.md) | Creating plugins inside the MeshMC source tree |
| [out-of-tree-setup.md](out-of-tree-setup.md) | Creating standalone plugins against an installed SDK |
| [cmake-config.md](cmake-config.md) | `MeshMC_SDK` CMake package internals |

---

## 1. What the SDK Provides

The SDK is **not** a traditional compiled library.  It is an *INTERFACE* CMake
target (`MeshMC::SDK`) that bundles:

1. **Include paths** — all header directories a plugin needs.
2. **Transitive Qt6 dependencies** — `Qt6::Core`, `Qt6::Widgets`, `Qt6::Gui`.
3. **CMake package config** — `MeshMCSDKConfig.cmake` / `MeshMCSDKTargets.cmake`
   for out-of-tree `find_package(MeshMC_SDK)`.

When you link against `MeshMC::SDK`, your plugin automatically gets every
include directory and every Qt library it needs — you never have to
`find_package(Qt6)` in an in-tree plugin, and out-of-tree projects only need
a minimal Qt6 find.

---

## 2. SDK Components

### 2.1 Core Headers

| Header | Location (source tree) | Description |
|--------|------------------------|-------------|
| `mmco_sdk.h` | `launcher/plugin/sdk/mmco_sdk.h` | **Single-include header.** The *only* header your plugin source files should `#include`. Brings in all Qt types, all MeshMC types, the `MMCOContext` struct, hook IDs, macros, and convenience defines. |
| `PluginAPI.h` | `launcher/plugin/PluginAPI.h` | Defines the `MMCOContext` struct (C ABI), callback typedefs (`MMCOHookCallback`, `MMCOHttpCallback`, `MMCOButtonCallback`, etc.), and all function-pointer sections S1–S14. |
| `PluginHooks.h` | `launcher/plugin/PluginHooks.h` | Defines the `MMCOHookId` enum and payload structs (`MMCOInstanceInfo`, `MMCOSettingChange`, `MMCOContentEvent`, `MMCONetworkEvent`, `MMCOMenuEvent`, `MMCOInstancePagesEvent`). |
| `MMCOFormat.h` | `launcher/plugin/MMCOFormat.h` | Defines `MMCOModuleInfo`, the `MMCO_MAGIC` constant (`0x4D4D434F`), `MMCO_ABI_VERSION`, `MMCO_FLAG_NONE`, and the `MMCO_EXPORT` visibility macro. |

### 2.2 Transitive MeshMC Headers (33+)

The SDK's `target_include_directories` expose the following headers so that
types used inside `mmco_sdk.h` resolve correctly.  **Plugin authors should
never include these directly** — they are all pulled in through `mmco_sdk.h`.

**Launcher root headers:**

| Header | Purpose |
|--------|---------|
| `Application.h` | `APPLICATION` singleton access |
| `BaseInstance.h` | Instance base class |
| `BaseVersion.h` | Version type |
| `BaseVersionList.h` | Version list type |
| `InstanceList.h` | Instance list container |
| `MMCZip.h` | Zip / archive operations |
| `MessageLevel.h` | Log-level enum |
| `QObjectPtr.h` | Smart pointer for QObjects |
| `Usable.h` | Usable trait type |

**Minecraft sub-tree:**

| Header | Purpose |
|--------|---------|
| `minecraft/MinecraftInstance.h` | Minecraft-specific instance |
| `minecraft/auth/AccountData.h` | Account data structures |
| `minecraft/auth/AuthSession.h` | Auth session |
| `minecraft/auth/MinecraftAccount.h` | Account object |
| `minecraft/launch/MinecraftServerTarget.h` | Server target info |
| `minecraft/mod/Mod.h` | Mod object |
| `minecraft/mod/ModDetails.h` | Mod metadata |

**Networking:**

| Header | Purpose |
|--------|---------|
| `net/Download.h` | Download task |
| `net/HttpMetaCache.h` | HTTP cache |
| `net/Mode.h` | Download mode enum |
| `net/NetAction.h` | Base network action |
| `net/NetJob.h` | Network job container |
| `net/Sink.h` | Data sink interface |
| `net/Validator.h` | Download validator |

**Miscellaneous:**

| Header | Purpose |
|--------|---------|
| `java/JavaVersion.h` | Java version info |
| `pathmatcher/IPathMatcher.h` | Path matcher interface |
| `settings/INIFile.h` | INI file parser |
| `settings/SettingsObject.h` | Settings object |
| `tasks/Task.h` | Async task base class |
| `ui/pages/BasePage.h` | Base page widget |
| `ui/pages/BasePageContainer.h` | Page container interface |
| `updater/UpdateChecker.h` | Update checker |

**Third-party library headers:**

| Header | Purpose |
|--------|---------|
| `nonstd/optional` | C++14-compatible `optional` type (optional-bare) |
| `katabasis/Bits.h` | OAuth token types (katabasis) |

### 2.3 Convenience Macros

The SDK header defines several macros that simplify plugin code:

| Macro | Expansion |
|-------|-----------|
| `MMCO_DEFINE_MODULE(name, ver, author, desc, license)` | Declares and exports the `mmco_module_info` global variable with the correct magic, ABI version, and flags. |
| `MMCO_LOG(ctx, msg)` | `ctx->log_info(ctx->module_handle, msg)` |
| `MMCO_WARN(ctx, msg)` | `ctx->log_warn(ctx->module_handle, msg)` |
| `MMCO_ERR(ctx, msg)` | `ctx->log_error(ctx->module_handle, msg)` |
| `MMCO_DBG(ctx, msg)` | `ctx->log_debug(ctx->module_handle, msg)` |
| `MMCO_MH` | `ctx->module_handle` (shorthand for API calls) |
| `MMCO_MAGIC` | `0x4D4D434F` |
| `MMCO_ABI_VERSION` | `1` |
| `MMCO_FLAG_NONE` | `0x00000000` |
| `MMCO_EXTENSION` | `".mmco"` |
| `MMCO_EXPORT` | Platform-specific symbol-visibility attribute |

---

## 3. Directory Layout After Install

When you install MeshMC with SDK headers (see
[out-of-tree-setup.md](out-of-tree-setup.md)), the filesystem looks like:

```
<prefix>/
├── bin/
│   ├── meshmc                      # The launcher binary
│   └── mmcmodules/                 # In-tree plugin modules
│       └── BackupSystem.mmco
├── include/
│   └── meshmc-sdk/                 # SDK headers root
│       ├── plugin/
│       │   ├── sdk/
│       │   │   └── mmco_sdk.h      # Single-include header
│       │   ├── PluginAPI.h
│       │   ├── PluginHooks.h
│       │   └── MMCOFormat.h
│       ├── Application.h
│       ├── BaseInstance.h
│       ├── BaseVersion.h
│       ├── BaseVersionList.h
│       ├── InstanceList.h
│       ├── MMCZip.h
│       ├── MessageLevel.h
│       ├── QObjectPtr.h
│       ├── Usable.h
│       ├── minecraft/
│       │   ├── MinecraftInstance.h
│       │   ├── auth/
│       │   │   ├── AccountData.h
│       │   │   ├── AuthSession.h
│       │   │   └── MinecraftAccount.h
│       │   ├── launch/
│       │   │   └── MinecraftServerTarget.h
│       │   └── mod/
│       │       ├── Mod.h
│       │       └── ModDetails.h
│       ├── net/
│       │   ├── Download.h
│       │   ├── HttpMetaCache.h
│       │   ├── Mode.h
│       │   ├── NetAction.h
│       │   ├── NetJob.h
│       │   ├── Sink.h
│       │   └── Validator.h
│       ├── java/
│       │   └── JavaVersion.h
│       ├── pathmatcher/
│       │   └── IPathMatcher.h
│       ├── settings/
│       │   ├── INIFile.h
│       │   └── SettingsObject.h
│       ├── tasks/
│       │   └── Task.h
│       ├── ui/
│       │   └── pages/
│       │       ├── BasePage.h
│       │       └── BasePageContainer.h
│       ├── updater/
│       │   └── UpdateChecker.h
│       ├── nonstd/
│       │   └── optional
│       └── katabasis/
│           └── Bits.h
└── lib/
    └── cmake/
        └── MeshMC_SDK/
            ├── MeshMCSDKConfig.cmake
            ├── MeshMCSDKConfigVersion.cmake
            └── MeshMCSDKTargets.cmake
```

The two install-tree include paths registered by the SDK target are:

- `<prefix>/include/meshmc-sdk` — launcher root headers, sub-tree headers
- `<prefix>/include/meshmc-sdk/plugin/sdk` — `mmco_sdk.h` itself

These are set in the `INSTALL_INTERFACE` generator expression so they are
automatically picked up by `find_package(MeshMC_SDK)`.

---

## 4. Supported Platforms

The SDK and the `.mmco` module format work on every platform MeshMC supports:

| Platform | Shared library format | Extension | Visibility macro |
|----------|----------------------|-----------|-----------------|
| **Linux** (x86_64, aarch64) | ELF `.so` → renamed to `.mmco` | `.mmco` | `__attribute__((visibility("default")))` |
| **macOS** (x86_64, arm64) | Mach-O `.dylib` → renamed to `.mmco` | `.mmco` | `__attribute__((visibility("default")))` |
| **Windows** (x86_64) | PE `.dll` → renamed to `.mmco` | `.mmco` | `__declspec(dllexport)` |

On all platforms the build system sets `PREFIX ""` and `SUFFIX ".mmco"` on the
plugin target so the output file is always `<PluginName>.mmco` regardless of
the platform's native shared library naming conventions.

### Compiler Requirements

| Requirement | Minimum |
|-------------|---------|
| C++ standard | C++17 |
| CMake | 3.21 |
| Qt | 6.x (Core, Widgets, Gui) |
| GCC | 11+ |
| Clang | 14+ |
| MSVC | 2022 (17.x) |

---

## 5. Qt6 Dependency

The SDK links against **Qt6::Core**, **Qt6::Widgets**, and **Qt6::Gui** via
`target_link_libraries(... INTERFACE ...)`.  This means:

- **In-tree plugins** inherit Qt from the top-level MeshMC build — no extra
  work needed.
- **Out-of-tree plugins** must have Qt6 development packages installed and
  must call `find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui)` *before*
  `find_package(MeshMC_SDK)` so that the Qt targets exist when the SDK
  config is loaded.

Why Qt6 and not Qt5?  MeshMC exclusively targets Qt6.  The SDK does not
attempt to support Qt5.

The SDK header `mmco_sdk.h` includes the following Qt modules directly:

- **QtCore:** `QString`, `QStringList`, `QList`, `QDir`, `QFile`, `QFileInfo`,
  `QDirIterator`, `QDateTime`, `QDebug`, `QRegularExpression`, `QJsonObject`
- **QtWidgets:** `QApplication`, `QWidget`, `QLabel`, `QLineEdit`,
  `QPushButton`, `QTreeWidget`, `QTreeWidgetItem`, `QHeaderView`,
  `QHBoxLayout`, `QVBoxLayout`, `QMenu`, `QMessageBox`, `QInputDialog`,
  `QFileDialog`
- **QtGui:** `QIcon`

---

## 6. License: The MMCO Module Exception

Plugins (`.mmco` modules) are covered by the **MeshMC MMCO Module Exception
1.0**.  This is an additional permission on top of the GNU GPL v3 that allows
third-party plugins to use the SDK headers and link against MeshMC at runtime
*without* requiring the plugin itself to be GPL-licensed.

The SDK header carries this SPDX identifier:

```
GPL-3.0-or-later WITH LicenseRef-MeshMC-MMCO-Module-Exception-1.0
```

This means:

- The SDK *headers themselves* are GPL-3.0-or-later.
- The Module Exception grants plugins the right to be distributed under any
  license (proprietary, MIT, Apache-2.0, etc.) as long as they only use the
  documented SDK API surface.
- Plugins that directly include MeshMC internals (bypassing `mmco_sdk.h`)
  **do not** benefit from the exception and must comply with the GPL.

See `LICENSES/LicenseRef-MeshMC-MMCO-Module-Exception-1.0.txt` for the full
text.

---

## 7. Choosing In-Tree vs. Out-of-Tree

| Aspect | In-tree | Out-of-tree |
|--------|---------|-------------|
| **Source location** | `meshmc/plugins/<Name>/` | Anywhere on disk |
| **Build integration** | Built as part of the MeshMC CMake project | Standalone CMake project |
| **Linking** | Links `MeshMC_logic` (full launcher symbols) + `MeshMC::SDK` | Links `MeshMC::SDK` only (headers + Qt) |
| **Access level** | Can use internal MeshMC types beyond the SDK surface | Restricted to the public SDK API |
| **Distribution** | Ships with MeshMC itself | Distributed independently as a `.mmco` file |
| **Best for** | Core / first-party plugins maintained in the MeshMC repo | Third-party / community plugins |
| **License requirement** | Must follow MeshMC's GPL-3.0 | MMCO Module Exception applies — any license |

If you are a MeshMC contributor adding a feature that should ship with every
build, use **in-tree**.  If you are a third-party developer creating an
add-on, use **out-of-tree**.

---

## 8. Quick Start

### In-tree (30-second version)

```bash
mkdir meshmc/plugins/MyPlugin
# Create CMakeLists.txt and MyPlugin.cpp (see in-tree-setup.md)
# Register in meshmc/plugins/CMakeLists.txt:
#   add_subdirectory(MyPlugin)
cmake --build --preset linux --config Release
# Output: build/mmcmodules/MyPlugin.mmco
```

### Out-of-tree (30-second version)

```bash
# Install MeshMC with SDK:
cmake --install build --prefix /usr/local

# In a new directory:
mkdir my-plugin && cd my-plugin
# Create CMakeLists.txt with find_package(MeshMC_SDK) (see out-of-tree-setup.md)
# Create src/MyPlugin.cpp
cmake -B build -DCMAKE_PREFIX_PATH="/usr/local"
cmake --build build
# Output: build/MyPlugin.mmco
cp build/MyPlugin.mmco ~/.local/lib/mmcmodules/
```

---

## Next Steps

- **[in-tree-setup.md](in-tree-setup.md)** — Full walkthrough for in-tree plugin development
- **[out-of-tree-setup.md](out-of-tree-setup.md)** — Full walkthrough for standalone plugin development
- **[cmake-config.md](cmake-config.md)** — Deep dive into the CMake package configuration
- **[../overview.md](../overview.md)** — Plugin system architecture overview
- **[../mmco-format.md](../mmco-format.md)** — The `.mmco` binary format specification
