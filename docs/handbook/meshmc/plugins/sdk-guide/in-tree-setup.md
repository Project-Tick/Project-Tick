# In-Tree Plugin Setup

This guide walks you through creating a new MeshMC plugin **inside the MeshMC
source tree** — the `meshmc/plugins/` directory.  In-tree plugins are compiled
as part of the main MeshMC CMake build, ship with the launcher, and have
access to the full `MeshMC_logic` target in addition to the SDK.

> **When to use in-tree:**  You are a MeshMC contributor adding a first-party
> feature that should be included in official builds.  The plugin source lives
> in the same Git repository and follows the same release cycle.

---

## Table of Contents

1. [Prerequisites](#1-prerequisites)
2. [Directory structure](#2-directory-structure)
3. [Step 1 — Create the plugin directory](#3-step-1--create-the-plugin-directory)
4. [Step 2 — Write the CMakeLists.txt](#4-step-2--write-the-cmakeliststxt)
5. [Step 3 — Write the plugin source](#5-step-3--write-the-plugin-source)
6. [Step 4 — Register the plugin](#6-step-4--register-the-plugin)
7. [Step 5 — Build](#7-step-5--build)
8. [Step 6 — Verify](#8-step-6--verify)
9. [Adding UI with Qt Designer](#9-adding-ui-with-qt-designer)
10. [Adding multiple source files](#10-adding-multiple-source-files)
11. [Using internal MeshMC types](#11-using-internal-meshmc-types)
12. [Build output and install locations](#12-build-output-and-install-locations)
13. [Multi-config generators](#13-multi-config-generators)
14. [Debugging in-tree plugins](#14-debugging-in-tree-plugins)
15. [Complete reference: BackupSystem plugin](#15-complete-reference-backupsystem-plugin)
16. [Troubleshooting](#16-troubleshooting)

---

## 1. Prerequisites

Before you start, make sure you can build MeshMC from source:

- A working MeshMC build environment (CMake 3.21+, Qt6 dev packages, C++17
  compiler).
- The MeshMC source tree checked out and configured with a CMake preset or
  manual configuration.
- Familiarity with the `.mmco` module ABI: `mmco_module_info`,
  `mmco_init()`, `mmco_unload()`.  See [mmco-format.md](../mmco-format.md) for
  details.

If you can run the following and get a successful build, you are ready:

```bash
cd meshmc
cmake --build --preset linux --config Release
```

---

## 2. Directory Structure

All in-tree plugins live under `meshmc/plugins/`.  Each plugin has its own
subdirectory:

```
meshmc/
├── plugins/
│   ├── CMakeLists.txt          # Plugin registry — lists all in-tree plugins
│   ├── BackupSystem/           # Existing first-party plugin
│   │   ├── CMakeLists.txt
│   │   ├── BackupPlugin.cpp
│   │   ├── BackupManager.h
│   │   ├── BackupManager.cpp
│   │   ├── BackupPage.h
│   │   ├── BackupPage.cpp
│   │   └── BackupPage.ui
│   └── YourPlugin/             # ← Your new plugin goes here
│       ├── CMakeLists.txt
│       ├── YourPlugin.cpp
│       └── ...
├── launcher/
│   └── plugin/
│       └── sdk/
│           ├── CMakeLists.txt  # SDK INTERFACE library definition
│           ├── mmco_sdk.h      # Single-include header
│           └── ...
└── CMakeLists.txt              # Top-level: adds launcher, SDK, then plugins
```

The top-level `meshmc/CMakeLists.txt` includes three subdirectories in this
order:

```cmake
# 1. The launcher itself (defines the MeshMC_logic target)
add_subdirectory(launcher)

# 2. Plugin SDK (INTERFACE library — headers only, must come before plugins)
add_subdirectory(launcher/plugin/sdk)

# 3. In-tree .mmco plugins (after launcher so MeshMC_logic target is available)
add_subdirectory(plugins)
```

This ordering is important: the SDK target `MeshMC::SDK` must exist before any
plugin `CMakeLists.txt` is processed, and `MeshMC_logic` (the launcher's
compiled library) must exist so plugins can link against it for symbol
resolution.

---

## 3. Step 1 — Create the Plugin Directory

Create a new directory under `meshmc/plugins/`:

```bash
mkdir meshmc/plugins/MyPlugin
```

You will need at minimum two files:

- `CMakeLists.txt` — build rules
- `MyPlugin.cpp` — plugin entry point (contains `mmco_module_info`,
  `mmco_init`, `mmco_unload`)

---

## 4. Step 2 — Write the CMakeLists.txt

Here is a fully annotated `CMakeLists.txt` template for an in-tree plugin.
Every line is explained.

```cmake
# MyPlugin — MeshMC plugin (.mmco)
# Short description of what your plugin does.
#
# All Qt/MeshMC types are obtained through the SDK header (mmco_sdk.h).
# Plugin source files do NOT directly #include Qt or MeshMC headers.

# ── Source files ──────────────────────────────────────────────────────────
# List every .cpp, .h file your plugin needs.
# Header files are listed so IDEs can index them; only .cpp files compile.
set(MY_PLUGIN_SOURCES
    MyPlugin.cpp
)

# ── Build as a MODULE library ─────────────────────────────────────────────
# MODULE = shared library loaded at runtime via dlopen / LoadLibrary.
# This is the correct CMake library type for .mmco plugins.
add_library(MyPlugin MODULE ${MY_PLUGIN_SOURCES})

# ── Link dependencies ────────────────────────────────────────────────────
# MeshMC_logic  — the launcher's compiled library. In-tree plugins link
#                 against this so the linker can resolve all MeshMC and Qt
#                 symbols at build time. Out-of-tree plugins do NOT have
#                 this luxury.
# MeshMC::SDK   — the SDK INTERFACE target. Provides all include paths
#                 (build-tree generator expressions) and transitive Qt6
#                 dependencies (Core, Widgets, Gui).
target_link_libraries(MyPlugin PRIVATE
    MeshMC_logic
    MeshMC::SDK
)

# ── Output as .mmco ──────────────────────────────────────────────────────
# PREFIX ""   — suppress the default "lib" prefix (Linux/macOS).
# SUFFIX      — override .so / .dylib / .dll with ".mmco".
# LIBRARY_OUTPUT_DIRECTORY — place the built module in the staging dir
#   so the launcher can discover it at runtime.
set_target_properties(MyPlugin PROPERTIES
    PREFIX ""
    SUFFIX ".mmco"
    LIBRARY_OUTPUT_DIRECTORY "${MESHMC_PLUGIN_STAGING_DIR}"
)

# ── Multi-config generator support ────────────────────────────────────────
# Generators like Visual Studio and Xcode use per-config output directories
# (e.g. Release/, Debug/).  This loop ensures the .mmco always lands in
# the staging directory regardless of the active configuration.
foreach(CFG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${CFG}" CFG_UPPER)
    set_target_properties(MyPlugin PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY_${CFG_UPPER} "${MESHMC_PLUGIN_STAGING_DIR}"
    )
endforeach()

# ── Install rule ─────────────────────────────────────────────────────────
# Installs the .mmco file alongside the binary in bin/mmcmodules/.
# BINARY_DEST_DIR is set by the top-level MeshMC CMake configuration.
install(TARGETS MyPlugin
    LIBRARY DESTINATION "${BINARY_DEST_DIR}/mmcmodules"
)
```

### Key variables used

| Variable | Set by | Value |
|----------|--------|-------|
| `MESHMC_PLUGIN_STAGING_DIR` | `meshmc/plugins/CMakeLists.txt` | `${CMAKE_BINARY_DIR}/mmcmodules` |
| `BINARY_DEST_DIR` | Top-level MeshMC CMake | Platform-dependent binary install directory |
| `CMAKE_CONFIGURATION_TYPES` | CMake (multi-config generators only) | `Debug;Release;RelWithDebInfo;MinSizeRel` |

---

## 5. Step 3 — Write the Plugin Source

Below is a minimal `MyPlugin.cpp` that you can use as a starting point.  It
registers a hook for `MMCO_HOOK_APP_INITIALIZED` and logs a message.

```cpp
/* MyPlugin.cpp — Minimal in-tree plugin template */

#include "mmco_sdk.h"

/* ── Module declaration ─────────────────────────────────────────────── */
MMCO_DEFINE_MODULE(
    "My Plugin",                       /* name         */
    "1.0.0",                           /* version      */
    "Your Name",                       /* author       */
    "A short description of MyPlugin", /* description  */
    "GPL-3.0-or-later"                 /* license      */
);

/* ── State ──────────────────────────────────────────────────────────── */
static MMCOContext* g_ctx = nullptr;

/* ── Hook callbacks ─────────────────────────────────────────────────── */

static int on_app_initialized(void* /*mh*/, uint32_t /*hook_id*/,
                              void* /*payload*/, void* /*user_data*/)
{
    MMCO_LOG(g_ctx, "MyPlugin: application initialized!");

    /* Example: enumerate instances */
    int count = g_ctx->instance_count(g_ctx->module_handle);
    char buf[256];
    snprintf(buf, sizeof(buf), "MyPlugin: found %d instance(s)", count);
    MMCO_LOG(g_ctx, buf);

    return 0; /* continue hook chain */
}

/* ── Entry points ───────────────────────────────────────────────────── */

extern "C" int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;
    MMCO_LOG(ctx, "MyPlugin initializing...");

    ctx->hook_register(ctx->module_handle,
                       MMCO_HOOK_APP_INITIALIZED,
                       on_app_initialized,
                       nullptr);

    MMCO_LOG(ctx, "MyPlugin initialized.");
    return 0; /* 0 = success */
}

extern "C" void mmco_unload()
{
    if (g_ctx) {
        MMCO_LOG(g_ctx, "MyPlugin unloading.");
    }
    g_ctx = nullptr;
}
```

### Mandatory exports

Every `.mmco` module **must** export exactly three symbols:

| Symbol | Type | Purpose |
|--------|------|---------|
| `mmco_module_info` | `MMCOModuleInfo` (global variable) | Module metadata — used by the loader to identify and validate the plugin. Created by `MMCO_DEFINE_MODULE`. |
| `mmco_init` | `extern "C" int mmco_init(MMCOContext* ctx)` | Called once when the module is loaded.  Register hooks, initialise state.  Return 0 for success, non-zero to abort loading. |
| `mmco_unload` | `extern "C" void mmco_unload()` | Called when the module is unloaded (app shutdown or user request).  Clean up state.  The `MMCOContext` pointer is invalidated after this returns. |

### Include rules

- **DO** `#include "mmco_sdk.h"` — this is the only include you need.
- **DO NOT** directly `#include <QString>` or `#include "Application.h"`.
  Everything is provided through `mmco_sdk.h`.
- For in-tree plugins that need types beyond the SDK surface, see
  [Section 11](#11-using-internal-meshmc-types).

---

## 6. Step 4 — Register the Plugin

Open `meshmc/plugins/CMakeLists.txt` and add your plugin's subdirectory:

```cmake
# MeshMC In-Tree Plugin Registry
#
# Each subdirectory here builds a .mmco shared module.
# Built modules are placed in ${MESHMC_PLUGIN_STAGING_DIR} so the
# launcher can discover them at runtime from the app-relative
# mmcmodules/ directory.

set(MESHMC_PLUGIN_STAGING_DIR "${CMAKE_BINARY_DIR}/mmcmodules" CACHE PATH
    "Directory where built .mmco plugins are placed for runtime discovery")

file(MAKE_DIRECTORY "${MESHMC_PLUGIN_STAGING_DIR}")

#################################### Register plugins here ####################################

add_subdirectory(BackupSystem)
add_subdirectory(MyPlugin)         # ← Add this line
```

The order of `add_subdirectory` calls does not matter (plugins are independent
of each other), but by convention new plugins are appended to the end of the
list.

---

## 7. Step 5 — Build

Build the entire MeshMC project.  Your plugin will be compiled along with
everything else:

```bash
# From the meshmc/ root directory:
cmake --build --preset linux --config Release
```

Or, if you are using a manual build directory:

```bash
cmake --build build --config Release
```

The build system will:

1. Compile `MyPlugin.cpp` into a MODULE shared library.
2. Link against `MeshMC_logic` and the Qt6 libraries.
3. Set the output name to `MyPlugin.mmco`.
4. Place the file in `${CMAKE_BINARY_DIR}/mmcmodules/MyPlugin.mmco`.

You can build just the plugin target for faster iteration:

```bash
cmake --build --preset linux --config Release --target MyPlugin
```

---

## 8. Step 6 — Verify

After a successful build, confirm the module exists in the staging directory:

```bash
ls -la build/mmcmodules/MyPlugin.mmco
```

You can also verify the exported symbols:

```bash
# Linux
nm -D build/mmcmodules/MyPlugin.mmco | grep mmco_

# macOS
nm build/mmcmodules/MyPlugin.mmco | grep mmco_

# Expected output (at minimum):
#   T mmco_init
#   T mmco_unload
#   D mmco_module_info
```

Launch MeshMC from the build directory and check the log output:

```bash
./build/meshmc
# In the MeshMC log or console:
#   [MyPlugin] MyPlugin initializing...
#   [MyPlugin] MyPlugin initialized.
#   [MyPlugin] MyPlugin: application initialized!
#   [MyPlugin] MyPlugin: found 3 instance(s)
```

---

## 9. Adding UI with Qt Designer

Many in-tree plugins provide a UI page (a tab in the instance settings or a
global page).  Qt Designer `.ui` files are the standard way to design these
pages.

### 9.1 Create the .ui file

Use Qt Designer or Qt Creator to design your page, or create a `.ui` file
manually.  Save it as `MyPage.ui` in your plugin directory.

Example minimal `.ui` file:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<ui version="4.0">
 <class>MyPage</class>
 <widget class="QWidget" name="MyPage">
  <layout class="QVBoxLayout" name="mainLayout">
   <item>
    <widget class="QLabel" name="titleLabel">
     <property name="text">
      <string>My Plugin Page</string>
     </property>
    </widget>
   </item>
   <item>
    <widget class="QPushButton" name="actionButton">
     <property name="text">
      <string>Do Something</string>
     </property>
    </widget>
   </item>
   <item>
    <widget class="QTreeWidget" name="resultTree">
     <column>
      <property name="text">
       <string>Name</string>
      </property>
     </column>
     <column>
      <property name="text">
       <string>Value</string>
      </property>
     </column>
    </widget>
   </item>
  </layout>
 </widget>
</ui>
```

### 9.2 Update the CMakeLists.txt

Add the `.ui` file and use `qt_wrap_ui` to generate the C++ header:

```cmake
set(MY_PLUGIN_SOURCES
    MyPlugin.cpp
    MyPage.h
    MyPage.cpp
)

set(MY_PLUGIN_UIS
    MyPage.ui
)

# Generate ui_MyPage.h from MyPage.ui
qt_wrap_ui(MY_PLUGIN_UI_HEADERS ${MY_PLUGIN_UIS})

add_library(MyPlugin MODULE ${MY_PLUGIN_SOURCES} ${MY_PLUGIN_UI_HEADERS})

target_link_libraries(MyPlugin PRIVATE
    MeshMC_logic
    MeshMC::SDK
)

# The generated ui_MyPage.h is placed in CMAKE_CURRENT_BINARY_DIR.
# We need to add this to the include path so the source can find it.
target_include_directories(MyPlugin PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
)

set_target_properties(MyPlugin PROPERTIES
    PREFIX ""
    SUFFIX ".mmco"
    LIBRARY_OUTPUT_DIRECTORY "${MESHMC_PLUGIN_STAGING_DIR}"
)

foreach(CFG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${CFG}" CFG_UPPER)
    set_target_properties(MyPlugin PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY_${CFG_UPPER} "${MESHMC_PLUGIN_STAGING_DIR}"
    )
endforeach()

install(TARGETS MyPlugin
    LIBRARY DESTINATION "${BINARY_DEST_DIR}/mmcmodules"
)
```

### 9.3 Use the generated header in your page class

```cpp
/* MyPage.h */
#pragma once

#include "mmco_sdk.h"
#include "ui_MyPage.h"  /* Generated by qt_wrap_ui from MyPage.ui */

class MyPage : public QWidget {
    Q_OBJECT

public:
    explicit MyPage(QWidget* parent = nullptr);

    void refresh();

private slots:
    void onActionClicked();

private:
    Ui::MyPage ui;
    MMCOContext* m_ctx;
};
```

```cpp
/* MyPage.cpp */
#include "MyPage.h"

MyPage::MyPage(QWidget* parent)
    : QWidget(parent)
{
    ui.setupUi(this);
    connect(ui.actionButton, &QPushButton::clicked,
            this, &MyPage::onActionClicked);
}

void MyPage::refresh()
{
    /* Populate the tree widget, update labels, etc. */
}

void MyPage::onActionClicked()
{
    /* Handle button click */
}
```

### 9.4 Register the page via hooks

In your `mmco_init`, register for `MMCO_HOOK_UI_INSTANCE_PAGES` to inject
your page into the instance settings dialog.  Alternatively, use the Page
Builder API (S13 functions in `MMCOContext`) to construct pages purely through
the C API without `.ui` files.

---

## 10. Adding Multiple Source Files

As your plugin grows, split it across multiple files.  Simply add them to the
sources list:

```cmake
set(MY_PLUGIN_SOURCES
    MyPlugin.cpp           # Entry point (mmco_init, mmco_unload)
    MyManager.h
    MyManager.cpp          # Core logic
    MyPage.h
    MyPage.cpp             # UI page
    MyUtils.h
    MyUtils.cpp            # Utility functions
)
```

All `.cpp` files will be compiled and linked into the single `MyPlugin.mmco`
output.  There is no need for intermediate static libraries or object
libraries.

### Recommended file organisation

```
meshmc/plugins/MyPlugin/
├── CMakeLists.txt           # Build rules
├── MyPlugin.cpp             # mmco_init / mmco_unload — thin entry point
├── MyManager.h              # Core plugin logic (header)
├── MyManager.cpp            # Core plugin logic (implementation)
├── MyPage.h                 # UI page class (header)
├── MyPage.cpp               # UI page class (implementation)
├── MyPage.ui                # Qt Designer form (optional)
└── resources/               # Any data files (optional, not auto-installed)
    └── icon.png
```

---

## 11. Using Internal MeshMC Types

In-tree plugins have a privilege that out-of-tree plugins do not: they link
against `MeshMC_logic`, which means they can `#include` internal MeshMC
headers and use internal types directly.

However, this comes with **strong caveats**:

1. **Internal APIs are unstable.**  They can change between any two commits
   without notice.  If you use them, your plugin may break when the launcher
   code is refactored.

2. **Prefer the SDK API.**  The `MMCOContext` function pointers are the stable
   API surface.  If something you need is not available through the SDK,
   consider adding it to the SDK rather than reaching into internals.

3. **Guard internal includes.**  If you must use an internal header, document
   why and isolate the usage:

```cpp
/* INTERNAL: We need direct access to LaunchController for X reason.
 * TODO: Expose this through the SDK API instead.
 */
#include "launch/LaunchController.h"
```

The SDK header `mmco_sdk.h` already exposes a rich set of types:

- `Application.h`, `BaseInstance.h`, `InstanceList.h`
- `minecraft/MinecraftInstance.h`, `minecraft/mod/Mod.h`
- `ui/pages/BasePage.h`
- `MMCZip.h`
- All Qt widget and core types

In the majority of cases, the SDK surface is sufficient.

---

## 12. Build Output and Install Locations

### Build tree (development)

```
${CMAKE_BINARY_DIR}/mmcmodules/
└── MyPlugin.mmco
```

This directory is the `MESHMC_PLUGIN_STAGING_DIR`.  The launcher scans this
path at startup when running from the build directory.

### Install tree (packaging / deployment)

```
${CMAKE_INSTALL_PREFIX}/${BINARY_DEST_DIR}/mmcmodules/
└── MyPlugin.mmco
```

The install rule places the `.mmco` file next to the launcher binary.  On a
typical Linux install with `--prefix /usr/local`, this resolves to:

```
/usr/local/bin/mmcmodules/MyPlugin.mmco
```

### Runtime search paths

The MeshMC plugin loader searches these directories (in order):

| Path | Platform | Typical use |
|------|----------|-------------|
| `<app_dir>/mmcmodules/` | All | In-tree / portable install |
| `~/.local/lib/mmcmodules/` | Linux | User-local plugins |
| `/usr/local/lib/mmcmodules/` | Linux | System-wide (from source) |
| `/usr/lib/mmcmodules/` | Linux | Distro packages |

On macOS and Windows, equivalent platform-specific paths are used.  The
app-relative path always takes priority.

---

## 13. Multi-Config Generators

If you use a multi-config generator (Visual Studio, Xcode, Ninja Multi-Config),
the CMake template in Section 4 already handles this with the `foreach` loop
over `CMAKE_CONFIGURATION_TYPES`.  This ensures the `.mmco` file always lands
in `${MESHMC_PLUGIN_STAGING_DIR}` regardless of the active build
configuration.

Without this loop, the output would go into a per-config subdirectory like
`mmcmodules/Release/MyPlugin.mmco` — and the launcher would not find it.

---

## 14. Debugging In-Tree Plugins

### 14.1 Logging

Use the `MMCO_LOG`, `MMCO_WARN`, `MMCO_ERR`, and `MMCO_DBG` macros
throughout your plugin.  All output is routed through the MeshMC logging
system and appears in the launcher's log file and console.

```cpp
MMCO_LOG(g_ctx, "Processing started");
MMCO_DBG(g_ctx, "Debug: item count = 42");
MMCO_WARN(g_ctx, "File not found, using default");
MMCO_ERR(g_ctx, "Critical: failed to open database");
```

Log messages are prefixed with the plugin name automatically:

```
[MyPlugin] Processing started
[MyPlugin] Debug: item count = 42
```

### 14.2 GDB / LLDB

Since the plugin is a shared library loaded at runtime, you need to tell the
debugger to set pending breakpoints:

**GDB:**

```bash
gdb ./build/meshmc
(gdb) set breakpoint pending on
(gdb) break mmco_init
(gdb) run
# GDB will stop when your plugin's mmco_init is called
```

**LLDB:**

```bash
lldb ./build/meshmc
(lldb) breakpoint set --name mmco_init
(lldb) run
```

### 14.3 Build type

For debugging, use a Debug or RelWithDebInfo build:

```bash
cmake --build --preset linux --config Debug
```

This ensures debug symbols are present in the `.mmco` file.

### 14.4 Reload without restarting

MeshMC does not currently support hot-reloading plugins.  You must restart
the launcher after rebuilding your plugin.  A fast workflow:

```bash
# Terminal 1: rebuild just the plugin
cmake --build --preset linux --config Debug --target MyPlugin

# Terminal 2: relaunch
./build/meshmc
```

### 14.5 Common pitfalls

| Symptom | Cause | Fix |
|---------|-------|-----|
| Plugin not loaded | Not registered in `plugins/CMakeLists.txt` | Add `add_subdirectory(MyPlugin)` |
| Plugin not loaded | `.mmco` not in `mmcmodules/` | Check `LIBRARY_OUTPUT_DIRECTORY` |
| Undefined symbols | Missing `MeshMC_logic` link | Add `MeshMC_logic` to `target_link_libraries` |
| `ui_*.h` not found | Missing `target_include_directories` for binary dir | Add `${CMAKE_CURRENT_BINARY_DIR}` to includes |
| `mmco_module_info` not found by loader | Missing `MMCO_DEFINE_MODULE` | Add the macro to your main `.cpp` |
| Segfault in hooks | Using `g_ctx` after `mmco_unload` | Set `g_ctx = nullptr` in `mmco_unload`, check before use |

---

## 15. Complete Reference: BackupSystem Plugin

The BackupSystem plugin is the canonical example of a production in-tree
plugin.  Here is its complete `CMakeLists.txt` for reference:

```cmake
# BackupSystem — MeshMC plugin (.mmco)
# Creates instance backups (snapshots), restores, exports, imports.
#
# All Qt/MeshMC types are obtained through the SDK header (mmco_sdk.h).
# Plugin source files do NOT directly #include Qt or MeshMC headers.

set(BACKUP_SOURCES
    BackupPlugin.cpp
    BackupManager.h
    BackupManager.cpp
    BackupPage.h
    BackupPage.cpp
)

set(BACKUP_UIS
    BackupPage.ui
)

qt_wrap_ui(BACKUP_UI_HEADERS ${BACKUP_UIS})

add_library(BackupSystem MODULE ${BACKUP_SOURCES} ${BACKUP_UI_HEADERS})

target_link_libraries(BackupSystem PRIVATE
    MeshMC_logic            # in-tree: link against launcher for symbols
    MeshMC::SDK             # SDK: provides all include paths + Qt deps
)

target_include_directories(BackupSystem PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}   # for generated ui headers
)

# Output as .mmco into the staging directory
set_target_properties(BackupSystem PROPERTIES
    PREFIX ""
    SUFFIX ".mmco"
    LIBRARY_OUTPUT_DIRECTORY "${MESHMC_PLUGIN_STAGING_DIR}"
)

# Also copy to build dir on multi-config generators
foreach(CFG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${CFG}" CFG_UPPER)
    set_target_properties(BackupSystem PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY_${CFG_UPPER} "${MESHMC_PLUGIN_STAGING_DIR}"
    )
endforeach()

# Install the .mmco alongside the binary (bin/mmcmodules/)
install(TARGETS BackupSystem
    LIBRARY DESTINATION "${BINARY_DEST_DIR}/mmcmodules"
)
```

### File structure

```
meshmc/plugins/BackupSystem/
├── CMakeLists.txt       # Build rules (shown above)
├── BackupPlugin.cpp     # mmco_init / mmco_unload entry point
├── BackupManager.h      # Backup logic: create, restore, list, delete
├── BackupManager.cpp    # Backup logic implementation
├── BackupPage.h         # QWidget-based UI page (header)
├── BackupPage.cpp       # UI page implementation
└── BackupPage.ui        # Qt Designer form
```

### What makes it a good reference

- Uses `qt_wrap_ui` for `.ui` files.
- Links both `MeshMC_logic` and `MeshMC::SDK`.
- Adds `${CMAKE_CURRENT_BINARY_DIR}` to include paths for generated headers.
- Handles multi-config generators.
- Has a proper install rule.

Study this alongside the [BackupSystem Case Study](../tutorials/) for a
full code walkthrough.

---

## 16. Troubleshooting

### CMake configuration fails with "Unknown CMake command add_subdirectory"

You added the `add_subdirectory(MyPlugin)` call in the wrong `CMakeLists.txt`.
It belongs in `meshmc/plugins/CMakeLists.txt`, **not** in the top-level file
or the launcher's `CMakeLists.txt`.

### "Target MeshMC::SDK not found"

The SDK subdirectory is added *before* the plugins directory in
`meshmc/CMakeLists.txt`.  If you see this error, either:

- You are adding your plugin in the wrong location (not under `plugins/`).
- The SDK `CMakeLists.txt` has a syntax error preventing it from defining the
  target.

### "Target MeshMC_logic not found"

Same ordering issue — the launcher subdirectory must be added before plugins.
This is already the case in the standard `meshmc/CMakeLists.txt`:

```cmake
add_subdirectory(launcher)              # defines MeshMC_logic
add_subdirectory(launcher/plugin/sdk)   # defines MeshMC::SDK
add_subdirectory(plugins)               # your plugin goes here
```

### Plugin compiles but segfaults immediately

Check that your `MMCO_DEFINE_MODULE` uses the correct `MMCO_MAGIC`
(`0x4D4D434F`) and `MMCO_ABI_VERSION` (`1`).  If these do not match, the
loader may corrupt memory.  The `MMCO_DEFINE_MODULE` macro handles this for
you — only use it (never manually construct `mmco_module_info`).

### Plugin loads but hooks never fire

- Verify you called `ctx->hook_register(...)` in `mmco_init`.
- Verify the hook ID is correct (`MMCO_HOOK_APP_INITIALIZED`, etc.).
- Verify your callback returns `0` (continue chain) and not a non-zero value
  that would stop propagation in a *previous* plugin's callback.

### Build is slow when only changing the plugin

Use the `--target` flag to rebuild just your plugin:

```bash
cmake --build --preset linux --config Release --target MyPlugin
```

This skips rebuilding the entire launcher.

---

## Next Steps

- **[out-of-tree-setup.md](out-of-tree-setup.md)** — Build plugins outside
  the source tree
- **[cmake-config.md](cmake-config.md)** — CMake package config details
- **[../overview.md](../overview.md)** — Plugin system architecture
- **[../mmco-format.md](../mmco-format.md)** — The `.mmco` format
  specification
