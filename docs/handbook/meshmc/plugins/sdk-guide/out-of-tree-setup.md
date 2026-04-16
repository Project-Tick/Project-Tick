# Out-of-Tree Plugin Setup

This guide walks you through creating a MeshMC plugin **outside the MeshMC
source tree** — as a standalone project that uses the installed SDK.
Out-of-tree development is the standard workflow for third-party and community
plugins.

> **When to use out-of-tree:**  You are building a plugin that is not part of
> the official MeshMC distribution.  Your code lives in its own repository,
> has its own release cycle, and may use any license (thanks to the MMCO
> Module Exception).

---

## Table of Contents

1. [Prerequisites](#1-prerequisites)
2. [Step 1 — Install the MeshMC SDK](#2-step-1--install-the-meshmc-sdk)
3. [Step 2 — Create the project structure](#3-step-2--create-the-project-structure)
4. [Step 3 — Write the CMakeLists.txt](#4-step-3--write-the-cmakeliststxt)
5. [Step 4 — Write the plugin source](#5-step-4--write-the-plugin-source)
6. [Step 5 — Configure and build](#6-step-5--configure-and-build)
7. [Step 6 — Test locally](#7-step-6--test-locally)
8. [Step 7 — Deploy and distribute](#8-step-7--deploy-and-distribute)
9. [Full example: Hello World out-of-tree](#9-full-example-hello-world-out-of-tree)
10. [Adding Qt Designer UI files](#10-adding-qt-designer-ui-files)
11. [Using the Page Builder API instead of .ui files](#11-using-the-page-builder-api-instead-of-ui-files)
12. [Cross-platform builds](#12-cross-platform-builds)
13. [CI/CD integration](#13-cicd-integration)
14. [The MMCO Module Exception license](#14-the-mmco-module-exception-license)
15. [Differences from in-tree development](#15-differences-from-in-tree-development)
16. [Troubleshooting](#16-troubleshooting)

---

## 1. Prerequisites

Before you start, you need:

| Requirement | Details |
|-------------|---------|
| **MeshMC installed with SDK headers** | Either from a package or built from source with `cmake --install` |
| **Qt6 development packages** | `Qt6::Core`, `Qt6::Widgets`, `Qt6::Gui` — install from your distro's package manager or the Qt online installer |
| **CMake 3.21+** | Required by the SDK's `cmake_minimum_required` |
| **C++17 compiler** | GCC 11+, Clang 14+, or MSVC 2022 |

### Linux (Debian/Ubuntu)

```bash
sudo apt install cmake g++ qt6-base-dev qt6-tools-dev-tools
```

### Linux (Fedora)

```bash
sudo dnf install cmake gcc-c++ qt6-qtbase-devel
```

### Linux (Arch)

```bash
sudo pacman -S cmake gcc qt6-base
```

### macOS

```bash
brew install cmake qt@6
```

### Windows

Install Qt6 from the [Qt online installer](https://www.qt.io/download-qt-installer-oss)
and CMake from [cmake.org](https://cmake.org/download/).  Use Visual Studio
2022 or the Ninja generator with MSVC.

---

## 2. Step 1 — Install the MeshMC SDK

The SDK is installed as part of a normal MeshMC install.  If you built MeshMC
from source:

```bash
# Build MeshMC (if not already done)
cd meshmc
cmake --preset linux
cmake --build --preset linux --config Release

# Install to a prefix (e.g. /usr/local)
sudo cmake --install build --config Release --prefix /usr/local
```

This installs:

- The launcher binary → `<prefix>/bin/meshmc`
- In-tree plugins → `<prefix>/bin/mmcmodules/*.mmco`
- **SDK headers** → `<prefix>/include/meshmc-sdk/`
- **CMake package config** → `<prefix>/lib/cmake/MeshMC_SDK/`

If you are using a packaged version of MeshMC that includes SDK headers
(check for `<prefix>/include/meshmc-sdk/plugin/sdk/mmco_sdk.h`), you can skip
the build step.

### Verifying the installation

```bash
# Check that the SDK header exists
ls /usr/local/include/meshmc-sdk/plugin/sdk/mmco_sdk.h

# Check that the CMake config exists
ls /usr/local/lib/cmake/MeshMC_SDK/MeshMCSDKConfig.cmake
```

Both files must exist for `find_package(MeshMC_SDK)` to work.

### Custom install prefix

If you install to a non-standard prefix (e.g. `~/meshmc-sdk`), you will need
to tell CMake where to find it when configuring your plugin project:

```bash
cmake -B build -DCMAKE_PREFIX_PATH="$HOME/meshmc-sdk"
```

---

## 3. Step 2 — Create the Project Structure

Create a directory for your plugin project:

```bash
mkdir my-plugin
cd my-plugin
```

A typical out-of-tree plugin has this structure:

```
my-plugin/
├── CMakeLists.txt              # Build rules
├── src/
│   └── MyPlugin.cpp            # Plugin entry point + logic
├── LICENSE                     # Your plugin's license
└── README.md                   # Plugin documentation
```

For larger plugins:

```
my-plugin/
├── CMakeLists.txt
├── src/
│   ├── MyPlugin.cpp            # Entry point (mmco_init, mmco_unload)
│   ├── MyManager.h
│   ├── MyManager.cpp           # Core logic
│   ├── MyPage.h
│   ├── MyPage.cpp              # UI page (optional)
│   └── MyPage.ui               # Qt Designer form (optional)
├── LICENSE
└── README.md
```

---

## 4. Step 3 — Write the CMakeLists.txt

Here is the full, annotated `CMakeLists.txt` for an out-of-tree plugin:

```cmake
# MyPlugin — Out-of-tree MeshMC plugin (.mmco)
#
# Prerequisites:
#   1. Install MeshMC with SDK headers:
#        cmake --install <build-dir> --prefix /usr/local
#   2. Install Qt6 development packages (Core, Widgets, Gui)
#
# Build:
#   cmake -B build -DCMAKE_PREFIX_PATH="/usr/local;/usr/lib/qt6"
#   cmake --build build
#
# Install:
#   cp build/MyPlugin.mmco ~/.local/lib/mmcmodules/

cmake_minimum_required(VERSION 3.21)
project(MyPlugin LANGUAGES CXX)

# ── C++ standard ──────────────────────────────────────────────────────
# The SDK and MeshMC require C++17.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Find Qt6 ─────────────────────────────────────────────────────────
# The SDK's INTERFACE target links against Qt6::Core, Qt6::Widgets, and
# Qt6::Gui.  These targets must exist before find_package(MeshMC_SDK)
# is called, so we find Qt6 first.
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui)

# ── Find the MeshMC SDK ──────────────────────────────────────────────
# This imports the MeshMC::SDK INTERFACE target which provides:
#   - All SDK include directories
#   - Transitive link to Qt6::Core, Qt6::Widgets, Qt6::Gui
find_package(MeshMC_SDK REQUIRED)

# ── Plugin sources ───────────────────────────────────────────────────
add_library(MyPlugin MODULE
    src/MyPlugin.cpp
)

# ── Link the SDK ─────────────────────────────────────────────────────
# This single line gives you:
#   - Include paths for mmco_sdk.h and all transitive headers
#   - Link-time dependency on Qt6 Core/Widgets/Gui
# Note: out-of-tree plugins do NOT link MeshMC_logic.  All interaction
# with MeshMC goes through the MMCOContext function pointers.
target_link_libraries(MyPlugin PRIVATE
    MeshMC::SDK
)

# ── Output as .mmco ──────────────────────────────────────────────────
# Strip the platform default prefix ("lib" on Linux/macOS) and override
# the suffix to ".mmco".
set_target_properties(MyPlugin PROPERTIES
    PREFIX ""
    SUFFIX ".mmco"
)
```

### Key differences from in-tree

| Aspect | In-tree | Out-of-tree |
|--------|---------|-------------|
| `find_package(Qt6 ...)` | Not needed (inherited from parent) | **Required** — must come before `find_package(MeshMC_SDK)` |
| `find_package(MeshMC_SDK)` | Not needed (target defined in same build) | **Required** |
| `target_link_libraries` | `MeshMC_logic` + `MeshMC::SDK` | `MeshMC::SDK` only |
| `MESHMC_PLUGIN_STAGING_DIR` | Used for output directory | Not available — use default output |
| Install rules | `DESTINATION "${BINARY_DEST_DIR}/mmcmodules"` | User copies `.mmco` manually |

---

## 5. Step 4 — Write the Plugin Source

Create `src/MyPlugin.cpp`:

```cpp
/* src/MyPlugin.cpp — Out-of-tree MeshMC plugin */

#include "mmco_sdk.h"
#include <cstring>

/* ── Module declaration ─────────────────────────────────────────────── */
MMCO_DEFINE_MODULE(
    "My Plugin",                       /* name         */
    "1.0.0",                           /* version      */
    "Your Name",                       /* author       */
    "A short description of MyPlugin", /* description  */
    "MIT"                              /* license — your choice! */
);

/* ── State ──────────────────────────────────────────────────────────── */
static MMCOContext* g_ctx = nullptr;

/* ── Hook callbacks ─────────────────────────────────────────────────── */

static int on_app_initialized(void* /*mh*/, uint32_t /*hook_id*/,
                              void* /*payload*/, void* /*user_data*/)
{
    MMCO_LOG(g_ctx, "Hello from MyPlugin (out-of-tree)!");

    int count = g_ctx->instance_count(g_ctx->module_handle);
    char buf[256];
    snprintf(buf, sizeof(buf), "Found %d instance(s)", count);
    MMCO_LOG(g_ctx, buf);

    return 0;
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
    return 0;
}

extern "C" void mmco_unload()
{
    if (g_ctx) {
        MMCO_LOG(g_ctx, "MyPlugin unloading.");
    }
    g_ctx = nullptr;
}
```

### Important notes for out-of-tree plugins

- You can **only** use the `MMCOContext` API.  You cannot call MeshMC internal
  functions or use internal types (you don't link `MeshMC_logic`).
- The `#include "mmco_sdk.h"` resolves through the installed SDK headers in
  `<prefix>/include/meshmc-sdk/plugin/sdk/`.
- All Qt types included by `mmco_sdk.h` are available for use in your code (you
  can create QWidgets, use QString, etc.).

---

## 6. Step 5 — Configure and Build

### Linux

```bash
cmake -B build -DCMAKE_PREFIX_PATH="/usr/local"
cmake --build build
```

If Qt6 is installed in a non-standard location, add it to the prefix path:

```bash
cmake -B build -DCMAKE_PREFIX_PATH="/usr/local;/opt/qt6"
```

### macOS

```bash
cmake -B build -DCMAKE_PREFIX_PATH="/usr/local;$(brew --prefix qt@6)"
cmake --build build
```

### Windows (Visual Studio)

```powershell
cmake -B build -G "Visual Studio 17 2022" `
    -DCMAKE_PREFIX_PATH="C:/MeshMC;C:/Qt/6.7.0/msvc2022_64"
cmake --build build --config Release
```

### Windows (Ninja + MSVC)

```powershell
# Open a Visual Studio Developer Command Prompt first
cmake -B build -G Ninja `
    -DCMAKE_PREFIX_PATH="C:/MeshMC;C:/Qt/6.7.0/msvc2022_64"
cmake --build build
```

### Build output

After a successful build, your plugin is at:

```
build/MyPlugin.mmco
```

You can verify the exported symbols (Linux):

```bash
nm -D build/MyPlugin.mmco | grep mmco_
# Expected:
#   T mmco_init
#   T mmco_unload
#   D mmco_module_info
```

---

## 7. Step 6 — Test Locally

Copy the `.mmco` file to one of MeshMC's plugin search paths:

```bash
# User-local (recommended for development)
mkdir -p ~/.local/lib/mmcmodules
cp build/MyPlugin.mmco ~/.local/lib/mmcmodules/

# Or, app-relative (if MeshMC is portable)
cp build/MyPlugin.mmco /path/to/meshmc/mmcmodules/
```

Launch MeshMC.  Check the log output for your plugin's messages:

```
[MyPlugin] MyPlugin initializing...
[MyPlugin] MyPlugin initialized.
[MyPlugin] Hello from MyPlugin (out-of-tree)!
[MyPlugin] Found 3 instance(s)
```

### Quick iteration workflow

During development, you can set up a symlink instead of copying:

```bash
ln -sf "$(pwd)/build/MyPlugin.mmco" ~/.local/lib/mmcmodules/MyPlugin.mmco
```

Now every rebuild immediately updates the file at the search path.  Just
restart MeshMC to load the new version.

---

## 8. Step 7 — Deploy and Distribute

### Where users place .mmco files

Instruct your users to download the `.mmco` file and place it in one of these
directories:

| Path | Platform | Scope |
|------|----------|-------|
| `<app>/mmcmodules/` | All | Portable / app-local |
| `~/.local/lib/mmcmodules/` | Linux | User-local |
| `/usr/local/lib/mmcmodules/` | Linux | System-wide (manual install) |
| `/usr/lib/mmcmodules/` | Linux | Distro packages |
| `~/Library/Application Support/MeshMC/mmcmodules/` | macOS | User-local |
| `%APPDATA%/MeshMC/mmcmodules/` | Windows | User-local |

### Release checklist

1. **Test on all target platforms.**  A `.mmco` compiled on Linux will not
   work on Windows (different shared library format).  You must provide
   separate builds for each OS.

2. **Provide clear install instructions.**  Users who are not developers need
   simple "download and place here" steps.

3. **Version your releases.**  Use semantic versioning and update the version
   string in `MMCO_DEFINE_MODULE`.

4. **Include a LICENSE file.**  The MMCO Module Exception allows any license,
   but you must still declare one.

### Naming conventions

- Plugin file: `MyPlugin.mmco` (matches the `add_library` target name)
- No `lib` prefix.
- Always `.mmco` suffix — the loader only scans for this extension.

---

## 9. Full Example: Hello World Out-of-Tree

This is the reference example shipped in the MeshMC SDK source at
`launcher/plugin/sdk/examples/`.

### Directory layout

```
hello-world-plugin/
├── CMakeLists.txt
└── src/
    └── hello_world.cpp
```

### CMakeLists.txt

```cmake
# Example CMakeLists.txt for an out-of-tree MeshMC plugin (.mmco)
#
# Prerequisites:
#   1. Install MeshMC with SDK headers:
#        cmake --install <build-dir> --prefix /usr/local
#   2. Install Qt6 development packages (Core, Widgets, Gui)
#
# Build:
#   cmake -B build -DCMAKE_PREFIX_PATH="/usr/local;/usr/lib/qt6"
#   cmake --build build
#
# Install:
#   cp build/MyPlugin.mmco ~/.local/lib/mmcmodules/

cmake_minimum_required(VERSION 3.21)
project(MyPlugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui)
find_package(MeshMC_SDK REQUIRED)

add_library(MyPlugin MODULE
    src/MyPlugin.cpp
)

target_link_libraries(MyPlugin PRIVATE
    MeshMC::SDK
)

set_target_properties(MyPlugin PROPERTIES
    PREFIX ""
    SUFFIX ".mmco"
)
```

### src/hello_world.cpp

```cpp
#include "mmco_sdk.h"
#include <cstring>

MMCO_DEFINE_MODULE(
    "Hello World",
    "1.0.0",
    "Project Tick",
    "Example plugin that logs instance info on app init",
    "GPL-3.0-or-later"
);

static MMCOContext* g_ctx = nullptr;

static int on_app_initialized(void*, uint32_t, void*, void*)
{
    MMCO_LOG(g_ctx, "Hello from the Hello World plugin!");

    int count = g_ctx->instance_count(g_ctx->module_handle);
    char buf[256];
    snprintf(buf, sizeof(buf), "MeshMC has %d instance(s):", count);
    MMCO_LOG(g_ctx, buf);

    for (int i = 0; i < count; ++i) {
        const char* id = g_ctx->instance_get_id(g_ctx->module_handle, i);
        if (id) {
            const char* name =
                g_ctx->instance_get_name(g_ctx->module_handle, id);
            snprintf(buf, sizeof(buf), "  [%d] %s (%s)", i,
                     name ? name : "?", id);
            MMCO_LOG(g_ctx, buf);
        }
    }

    g_ctx->setting_set(g_ctx->module_handle, "last_run", "just now");
    const char* val = g_ctx->setting_get(g_ctx->module_handle, "last_run");
    if (val) {
        snprintf(buf, sizeof(buf), "Stored setting 'last_run' = '%s'", val);
        MMCO_DBG(g_ctx, buf);
    }

    const char* appName = g_ctx->get_app_name(g_ctx->module_handle);
    const char* appVer = g_ctx->get_app_version(g_ctx->module_handle);
    snprintf(buf, sizeof(buf), "Running on %s %s", appName, appVer);
    MMCO_LOG(g_ctx, buf);

    return 0;
}

static int on_instance_pre_launch(void*, uint32_t, void* payload, void*)
{
    auto* info = static_cast<MMCOInstanceInfo*>(payload);
    if (info && info->instance_name) {
        char buf[256];
        snprintf(buf, sizeof(buf), "Instance '%s' is about to launch!",
                 info->instance_name);
        MMCO_LOG(g_ctx, buf);
    }
    return 0;
}

extern "C" int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;
    MMCO_LOG(ctx, "Hello World plugin initializing...");

    ctx->hook_register(ctx->module_handle, MMCO_HOOK_APP_INITIALIZED,
                       on_app_initialized, nullptr);
    ctx->hook_register(ctx->module_handle, MMCO_HOOK_INSTANCE_PRE_LAUNCH,
                       on_instance_pre_launch, nullptr);

    MMCO_LOG(ctx, "Hello World plugin initialized.");
    return 0;
}

extern "C" void mmco_unload()
{
    if (g_ctx)
        MMCO_LOG(g_ctx, "Hello World plugin unloading. Goodbye!");
    g_ctx = nullptr;
}
```

---

## 10. Adding Qt Designer UI Files

Out-of-tree plugins can use `.ui` files just like in-tree plugins.  The
process is identical since `find_package(Qt6)` provides `qt_wrap_ui`.

### CMakeLists.txt with UI support

```cmake
cmake_minimum_required(VERSION 3.21)
project(MyPlugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui)
find_package(MeshMC_SDK REQUIRED)

# Process .ui files
set(MY_UIS
    src/MyPage.ui
)
qt_wrap_ui(MY_UI_HEADERS ${MY_UIS})

add_library(MyPlugin MODULE
    src/MyPlugin.cpp
    src/MyPage.h
    src/MyPage.cpp
    ${MY_UI_HEADERS}
)

target_link_libraries(MyPlugin PRIVATE
    MeshMC::SDK
)

# Generated headers (ui_MyPage.h) go into CMAKE_CURRENT_BINARY_DIR
target_include_directories(MyPlugin PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
)

set_target_properties(MyPlugin PROPERTIES
    PREFIX ""
    SUFFIX ".mmco"
)
```

### Notes on UI in out-of-tree plugins

- The `.ui` file is processed by Qt's `uic` tool at build time, generating a
  `ui_<name>.h` header in the build directory.
- You must add `${CMAKE_CURRENT_BINARY_DIR}` to the include path so the
  generated header can be found.
- The page class uses `Ui::<Name>` from the generated header just like in an
  in-tree plugin.

---

## 11. Using the Page Builder API Instead of .ui Files

If you prefer not to use Qt Designer, you can build your UI entirely through
the SDK's Page Builder API (Section S13 of `MMCOContext`).  This approach uses
only C API function pointers and does not require `qt_wrap_ui` or `.ui` files.

```cpp
static int on_instance_pages(void* /*mh*/, uint32_t /*hook_id*/,
                             void* payload, void* /*user_data*/)
{
    auto* ev = static_cast<MMCOInstancePagesEvent*>(payload);

    /* Create the page */
    void* page = g_ctx->ui_page_create(
        g_ctx->module_handle,
        "myplugin",           /* page_id   */
        "My Plugin",          /* tab title */
        "default"             /* icon      */
    );

    /* Create a vertical layout */
    void* layout = g_ctx->ui_layout_create(g_ctx->module_handle, page, 0);

    /* Add a label */
    g_ctx->ui_label_create(g_ctx->module_handle, page, "Welcome to MyPlugin");

    /* Add a button */
    g_ctx->ui_button_create(g_ctx->module_handle, page, "Do Something",
                            nullptr, my_button_callback, nullptr);

    /* Set the layout on the page */
    g_ctx->ui_page_set_layout(g_ctx->module_handle, page, layout);

    /* Add the page to the instance's page list */
    g_ctx->ui_page_add_to_list(g_ctx->module_handle, page,
                               ev->page_list_handle);

    return 0;
}
```

Register for `MMCO_HOOK_UI_INSTANCE_PAGES` in `mmco_init`:

```cpp
ctx->hook_register(ctx->module_handle,
                   MMCO_HOOK_UI_INSTANCE_PAGES,
                   on_instance_pages,
                   nullptr);
```

This approach is simpler for plugins with basic UIs and avoids the Qt
Designer dependency entirely.

---

## 12. Cross-Platform Builds

A `.mmco` file is a native shared library renamed with a `.mmco` extension.
It is **not portable** across operating systems.  You must build separate
`.mmco` files for each target platform.

### Platform matrix

| Platform | Generator | Prefix path example |
|----------|-----------|-------------------|
| Linux x86_64 | Unix Makefiles / Ninja | `-DCMAKE_PREFIX_PATH="/usr/local"` |
| Linux aarch64 | Unix Makefiles / Ninja | Same (native build) |
| macOS x86_64 | Unix Makefiles / Ninja / Xcode | `-DCMAKE_PREFIX_PATH="/usr/local;$(brew --prefix qt@6)"` |
| macOS arm64 | Same | Same |
| Windows x64 | Visual Studio 17 / Ninja | `-DCMAKE_PREFIX_PATH="C:/MeshMC;C:/Qt/6.7.0/msvc2022_64"` |

### Cross-compilation

Cross-compilation of `.mmco` plugins is technically possible with a CMake
toolchain file, but it requires that the SDK headers and Qt6 libraries are
available for the target platform.  In practice, it is easier to build
natively on each target or use CI runners (see next section).

---

## 13. CI/CD Integration

### GitHub Actions example

```yaml
name: Build Plugin

on: [push, pull_request]

jobs:
  build:
    strategy:
      matrix:
        os: [ubuntu-latest, macos-latest, windows-latest]
    runs-on: ${{ matrix.os }}

    steps:
      - uses: actions/checkout@v4

      - name: Install Qt6 (Linux)
        if: runner.os == 'Linux'
        run: sudo apt-get install -y qt6-base-dev

      - name: Install Qt6 (macOS)
        if: runner.os == 'macOS'
        run: brew install qt@6

      - name: Install Qt6 (Windows)
        if: runner.os == 'Windows'
        uses: jurplel/install-qt-action@v4
        with:
          version: '6.7.0'
          modules: 'qtbase'

      - name: Install MeshMC SDK
        run: |
          # Download pre-built SDK headers from MeshMC releases
          # or build from source — adjust for your setup
          echo "Install MeshMC SDK headers here"

      - name: Configure
        run: cmake -B build -DCMAKE_PREFIX_PATH="${{ env.PREFIX_PATH }}"

      - name: Build
        run: cmake --build build --config Release

      - name: Upload artifact
        uses: actions/upload-artifact@v4
        with:
          name: MyPlugin-${{ matrix.os }}
          path: build/MyPlugin.mmco
```

---

## 14. The MMCO Module Exception License

The MeshMC Plugin SDK headers are licensed under:

```
GPL-3.0-or-later WITH LicenseRef-MeshMC-MMCO-Module-Exception-1.0
```

The **MMCO Module Exception** is an additional permission granted on top of
the GPL that allows plugin authors to distribute their `.mmco` modules under
**any license** — including proprietary licenses — as long as:

1. The plugin only uses the **documented SDK API surface** (the functions and
   types exposed through `mmco_sdk.h` and the `MMCOContext` struct).
2. The plugin does not include or link against MeshMC internal headers or
   libraries beyond what the SDK provides.
3. The plugin does not modify MeshMC itself.

### What this means in practice

- **Your plugin code is yours.**  You choose the license.  MIT, Apache-2.0,
  proprietary, public domain — it's your decision.
- **The SDK headers are GPL + Exception.**  You can `#include "mmco_sdk.h"`
  without your code becoming GPL, because the Module Exception explicitly
  grants this permission.
- **If you bypass the SDK** — for example, by directly including MeshMC
  internals or linking `MeshMC_logic` in an out-of-tree build — the exception
  does not apply and the GPL governs your combined work.

### SPDX identifier for your plugin

If your plugin is MIT-licensed, your source files would carry:

```
/* SPDX-License-Identifier: MIT */
```

You do not need to reference the Module Exception in your own code — it is a
permission granted *to* you by the SDK headers.

---

## 15. Differences from In-Tree Development

| Aspect | In-tree | Out-of-tree |
|--------|---------|-------------|
| **Location** | `meshmc/plugins/<Name>/` | Anywhere |
| **Build system** | Part of MeshMC's CMake build | Standalone CMake project |
| **SDK target** | `MeshMC::SDK` (from same build) | `MeshMC::SDK` (via `find_package`) |
| **Launcher symbols** | Links `MeshMC_logic` — full access | No link — SDK API only |
| **Qt6 find** | Inherited from parent CMake | Must call `find_package(Qt6)` |
| **Internal headers** | Available (but discouraged) | Not available |
| **Install** | Automatic (`install(TARGETS ...)`) | Manual (user copies `.mmco`) |
| **License** | Must be GPL-3.0 (part of MeshMC) | Your choice (Module Exception) |
| **Distribution** | Ships with MeshMC | Independent release |
| **Staging dir** | `MESHMC_PLUGIN_STAGING_DIR` auto-set | Not applicable |
| **Multi-config** | Handled by foreach loop | Not needed (single `.mmco` output) |

### API access comparison

Out-of-tree plugins use the same `MMCOContext` function pointers as in-tree
plugins.  The 14 API sections (S1–S14) are fully available:

- S1: Logging
- S2: Hooks
- S3: Settings
- S4: Instance Management
- S5: Mod Management
- S6: World Management
- S7: Account Management
- S8: Java Management
- S9: Filesystem
- S10: Zip / Archive
- S11: Network
- S12: UI Dialogs
- S13: UI Page Builder
- S14: Utility

The only thing out-of-tree plugins cannot do is call internal MeshMC functions
directly — but the SDK API covers the vast majority of use cases.

---

## 16. Troubleshooting

### "Could not find a package configuration file provided by MeshMC_SDK"

The `MeshMCSDKConfig.cmake` file is not in CMake's search path.

**Fix:**  Pass the install prefix to CMake:

```bash
cmake -B build -DCMAKE_PREFIX_PATH="/usr/local"
```

Or, if you installed to a custom prefix:

```bash
cmake -B build -DCMAKE_PREFIX_PATH="/path/to/your/prefix"
```

Alternatively, set `MeshMC_SDK_DIR` directly:

```bash
cmake -B build -DMeshMC_SDK_DIR="/usr/local/lib/cmake/MeshMC_SDK"
```

### "Could not find a package configuration file provided by Qt6"

Qt6 development packages are not installed or not in the prefix path.

**Fix:**  Install Qt6 dev packages and add the Qt prefix path:

```bash
# Linux — usually automatic if installed from distro packages
cmake -B build -DCMAKE_PREFIX_PATH="/usr/local"

# macOS — Homebrew Qt
cmake -B build -DCMAKE_PREFIX_PATH="/usr/local;$(brew --prefix qt@6)"

# Windows — Qt online installer
cmake -B build -DCMAKE_PREFIX_PATH="C:/MeshMC;C:/Qt/6.7.0/msvc2022_64"
```

### "'mmco_sdk.h' file not found"

The SDK headers are not installed or the include path is wrong.

**Fix:**  Check that the header exists:

```bash
find /usr/local -name mmco_sdk.h
# Expected: /usr/local/include/meshmc-sdk/plugin/sdk/mmco_sdk.h
```

If missing, re-run `cmake --install` for MeshMC.

### Linker errors: undefined reference to Qt symbols

Your Qt6 installation version may not match the one used to build MeshMC.

**Fix:**  Ensure the Qt6 in your `CMAKE_PREFIX_PATH` matches the version used
to compile MeshMC.  The SDK links against `Qt6::Core`, `Qt6::Widgets`, and
`Qt6::Gui` — all three must be available at the same version.

### Plugin loads but no hooks fire

- Verify you called `ctx->hook_register(...)` in `mmco_init`.
- Verify the hook ID is valid (e.g. `MMCO_HOOK_APP_INITIALIZED`).
- Verify `mmco_init` returns `0` (success).

### Plugin not found by MeshMC

- Check the file extension is `.mmco` (not `.so`, `.dylib`, or `.dll`).
- Check the file is in one of the search paths listed in Section 8.
- Check file permissions (`chmod +x` may be needed on some systems).

### Segfault in mmco_init

- Check that `ctx` is not null.
- Check that you are using `ctx->module_handle` for all API calls.
- Do not store and use the context pointer after `mmco_unload` returns.

---

## Next Steps

- **[in-tree-setup.md](in-tree-setup.md)** — For contributing plugins to MeshMC itself
- **[cmake-config.md](cmake-config.md)** — Deep dive into the CMake package configuration
- **[../overview.md](../overview.md)** — Plugin system architecture
- **[../mmco-format.md](../mmco-format.md)** — The `.mmco` format specification
