# Hello World — Your First MeshMC Plugin

## Table of Contents

1. [What You Will Build](#what-you-will-build)
2. [Project Structure](#project-structure)
3. [Step 1: Create the Project Directory](#step-1-create-the-project-directory)
4. [Step 2: Write CMakeLists.txt](#step-2-write-cmakeliststxt)
5. [Step 3: Write the Plugin Source](#step-3-write-the-plugin-source)
6. [Step 4: Understanding the Code — Line by Line](#step-4-understanding-the-code--line-by-line)
7. [Step 5: Build the Plugin](#step-5-build-the-plugin)
8. [Step 6: Install the Plugin](#step-6-install-the-plugin)
9. [Step 7: Test the Plugin](#step-7-test-the-plugin)
10. [Expected Console Output](#expected-console-output)
11. [Troubleshooting](#troubleshooting)
12. [How It All Fits Together](#how-it-all-fits-together)
13. [Exercises](#exercises)
14. [Next Steps](#next-steps)

---

## What You Will Build

By the end of this tutorial, you will have a working MeshMC plugin that:

- Loads automatically when MeshMC starts
- Logs a greeting message to the console
- Enumerates all configured instances and prints their names
- Reacts to instance launches with a pre-launch message
- Demonstrates reading and writing plugin settings
- Queries the application name and version
- Cleans up gracefully on unload

This is the standard "Hello World" for the MMCO plugin system. Every concept you learn here — module declaration, entry points, hooks, logging, settings — is used in every real plugin.

---

## Project Structure

The finished project has exactly two files:

```
hello-world-plugin/
├── CMakeLists.txt       # Build system configuration
└── src/
    └── hello_world.cpp  # Plugin source code
```

That is all you need. The MMCO SDK provides everything else through a single `#include`.

---

## Step 1: Create the Project Directory

```bash
mkdir -p hello-world-plugin/src
cd hello-world-plugin
```

---

## Step 2: Write CMakeLists.txt

Create `CMakeLists.txt` in the project root:

```cmake
# CMakeLists.txt — Hello World MeshMC plugin (.mmco)
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
#   cp build/HelloWorld.mmco ~/.local/lib/mmcmodules/

cmake_minimum_required(VERSION 3.21)
project(HelloWorld LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui)
find_package(MeshMC_SDK REQUIRED)

add_library(HelloWorld MODULE
    src/hello_world.cpp
)

target_link_libraries(HelloWorld PRIVATE
    MeshMC::SDK
)

set_target_properties(HelloWorld PROPERTIES
    PREFIX ""
    SUFFIX ".mmco"
)
```

### CMakeLists.txt Explained

| Line | Purpose |
|---|---|
| `cmake_minimum_required(VERSION 3.21)` | Minimum CMake version. 3.21 is required by the MeshMC SDK config. |
| `project(HelloWorld LANGUAGES CXX)` | Project name. This also becomes the default output name. |
| `set(CMAKE_CXX_STANDARD 17)` | C++17 is required. The SDK uses `std::optional`, `std::shared_ptr`, structured bindings, etc. |
| `find_package(Qt6 ...)` | Locates Qt6. The SDK header includes Qt types (QString, QWidget, etc.), so Qt must be found. |
| `find_package(MeshMC_SDK REQUIRED)` | Locates the installed SDK. Provides include paths, MeshMC types, and the `MeshMC::SDK` target. |
| `add_library(HelloWorld MODULE ...)` | Creates a shared library (not static, not object — MODULE). This is important: `.mmco` files are loaded at runtime with `dlopen`/`LoadLibrary`. |
| `target_link_libraries(... MeshMC::SDK)` | Links against the SDK. This transitively provides all include paths, Qt dependencies, and MeshMC type definitions. |
| `PREFIX ""` | Removes the `lib` prefix (so the output is `HelloWorld.mmco`, not `libHelloWorld.mmco`). |
| `SUFFIX ".mmco"` | Sets the file extension to `.mmco` instead of `.so` / `.dll` / `.dylib`. |

### Why MODULE and Not SHARED?

CMake's `MODULE` library type is specifically for plugins loaded at runtime with `dlopen()`. Unlike `SHARED`, `MODULE` libraries:

- Cannot be linked against at build time (they are not import libraries)
- Are loaded explicitly by the host application
- Do not participate in RPATH resolution

This matches exactly how MeshMC loads `.mmco` files.

---

## Step 3: Write the Plugin Source

Create `src/hello_world.cpp`:

```cpp
/* SPDX-License-Identifier: CC0-1.0
 *
 * Example MeshMC plugin (.mmco module)
 */

#include "mmco_sdk.h"
#include <cstring>

/* ──────────────────────────────────────────────────────────────
 * Module declaration
 * ────────────────────────────────────────────────────────────── */

MMCO_DEFINE_MODULE(
    "Hello World",                                        /* name */
    "1.0.0",                                              /* version */
    "Project Tick",                                       /* author */
    "Example plugin that logs instance info on app init", /* description */
    "GPL-3.0-or-later"                                    /* license */
);

/* ──────────────────────────────────────────────────────────────
 * State
 * ────────────────────────────────────────────────────────────── */

static MMCOContext* g_ctx = nullptr;

/* ──────────────────────────────────────────────────────────────
 * Hook callbacks
 * ────────────────────────────────────────────────────────────── */

static int on_app_initialized(void* /*mh*/, uint32_t /*hook_id*/,
                              void* /*payload*/, void* /*user_data*/)
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
            snprintf(buf, sizeof(buf), "  [%d] %s (%s)", i, name ? name : "?",
                     id);
            MMCO_LOG(g_ctx, buf);
        }
    }

    // Demonstrate settings
    g_ctx->setting_set(g_ctx->module_handle, "last_run", "just now");
    const char* val = g_ctx->setting_get(g_ctx->module_handle, "last_run");
    if (val) {
        snprintf(buf, sizeof(buf), "Stored setting 'last_run' = '%s'", val);
        MMCO_DBG(g_ctx, buf);
    }

    // Demonstrate version query
    const char* appName = g_ctx->get_app_name(g_ctx->module_handle);
    const char* appVer = g_ctx->get_app_version(g_ctx->module_handle);
    snprintf(buf, sizeof(buf), "Running on %s %s", appName, appVer);
    MMCO_LOG(g_ctx, buf);

    return 0; // continue hook chain
}

static int on_instance_pre_launch(void* /*mh*/, uint32_t /*hook_id*/,
                                  void* payload, void* /*user_data*/)
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

/* ──────────────────────────────────────────────────────────────
 * Entry points
 * ────────────────────────────────────────────────────────────── */

extern "C" int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;

    MMCO_LOG(ctx, "Hello World plugin initializing...");

    // Register hooks
    ctx->hook_register(ctx->module_handle, MMCO_HOOK_APP_INITIALIZED,
                       on_app_initialized, nullptr);

    ctx->hook_register(ctx->module_handle, MMCO_HOOK_INSTANCE_PRE_LAUNCH,
                       on_instance_pre_launch, nullptr);

    MMCO_LOG(ctx, "Hello World plugin initialized.");
    return 0;
}

extern "C" void mmco_unload()
{
    if (g_ctx) {
        MMCO_LOG(g_ctx, "Hello World plugin unloading. Goodbye!");
    }
    g_ctx = nullptr;
}
```

---

## Step 4: Understanding the Code — Line by Line

This is the most important section of the tutorial. Every MMCO plugin uses the same structure, so understanding this code means understanding the foundation of all plugins.

### 4.1 The Include

```cpp
#include "mmco_sdk.h"
```

This single header is the **only** include you need. It provides:

- All Qt types (QString, QWidget, QTreeWidget, QFileDialog, etc.)
- All MeshMC types (Application, BaseInstance, InstanceList, BasePage, MMCZip, etc.)
- The MMCO C API types (MMCOContext, MMCOModuleInfo, hook IDs, event structs)
- Convenience macros (MMCO_LOG, MMCO_DEFINE_MODULE, etc.)

**Rule**: Never `#include` Qt or MeshMC headers directly in a plugin. Always go through the SDK header. This ensures you only depend on types that are part of the stable ABI.

### 4.2 MMCO_DEFINE_MODULE — The Module Declaration

```cpp
MMCO_DEFINE_MODULE(
    "Hello World",                                        /* name */
    "1.0.0",                                              /* version */
    "Project Tick",                                       /* author */
    "Example plugin that logs instance info on app init", /* description */
    "GPL-3.0-or-later"                                    /* license */
);
```

This macro expands to:

```cpp
extern "C" MMCO_EXPORT MMCOModuleInfo mmco_module_info = {
    MMCO_MAGIC,         // 0x4D4D434F — "MMCO" in ASCII
    MMCO_ABI_VERSION,   // 1 (current ABI version)
    "Hello World",      // name
    "1.0.0",            // version
    "Project Tick",     // author
    "Example plugin...",// description
    "GPL-3.0-or-later", // license
    MMCO_FLAG_NONE      // flags (0x00000000)
};
```

This creates an **exported C symbol** called `mmco_module_info`. When MeshMC's `PluginLoader` opens your `.mmco` file, the very first thing it does is look up this symbol with `dlsym()`. If the symbol is missing, or the `magic` field doesn't equal `0x4D4D434F`, or the `abi_version` is incompatible, the plugin is rejected immediately.

**Every field matters:**

| Field | Purpose | Validation |
|---|---|---|
| `magic` | ABI identification. Must be `0x4D4D434F`. | Checked on load — mismatch = rejected |
| `abi_version` | Protocol version. Must match the host's expected ABI. | Checked on load — mismatch = rejected |
| `name` | Human-readable plugin name. Shown in UI and logs. | Must not be null or empty |
| `version` | Plugin version string. Informational. | Must not be null |
| `author` | Plugin author. Informational. | Must not be null |
| `description` | Brief description. Informational. | Must not be null |
| `license` | SPDX license identifier. | Must not be null |
| `flags` | Reserved for future use. Set to `MMCO_FLAG_NONE`. | Currently ignored |

### 4.3 Global State

```cpp
static MMCOContext* g_ctx = nullptr;
```

The `MMCOContext` pointer is the **single interface** between your plugin and MeshMC. It is a struct full of function pointers — every API call goes through it. You receive it in `mmco_init()` and store it globally so hook callbacks can use it.

**Why a global?** Hook callbacks are plain C function pointers (they cannot capture `this` or closures). A file-scoped `static` global is the standard pattern. If you have multiple compilation units, put `g_ctx` in a shared header or a singleton.

### 4.4 Hook Callback: on_app_initialized

```cpp
static int on_app_initialized(void* /*mh*/, uint32_t /*hook_id*/,
                              void* /*payload*/, void* /*user_data*/)
```

Every hook callback has the same signature: `int(void*, uint32_t, void*, void*)`.

| Parameter | Type | Meaning |
|---|---|---|
| `mh` | `void*` | Your module handle (same as `g_ctx->module_handle`). Unused here since we use `g_ctx` directly. |
| `hook_id` | `uint32_t` | Which hook fired. Useful if you register one callback for multiple hooks. |
| `payload` | `void*` | Hook-specific data. For `MMCO_HOOK_APP_INITIALIZED`, this is `nullptr`. For other hooks, it is a pointer to an event struct. |
| `user_data` | `void*` | Arbitrary data you passed when registering the hook. We passed `nullptr`. |

**Return value**: Return `0` to allow the hook chain to continue. Return non-zero to stop further callbacks for this hook invocation.

Inside the callback, we do four things:

#### 4.4.1 Log a Message

```cpp
MMCO_LOG(g_ctx, "Hello from the Hello World plugin!");
```

`MMCO_LOG` expands to `g_ctx->log_info(g_ctx->module_handle, "...")`. The log message appears in MeshMC's console/log output, prefixed with your plugin name.

There are four log levels:

| Macro | Function | Level |
|---|---|---|
| `MMCO_LOG(ctx, msg)` | `ctx->log_info(...)` | Info |
| `MMCO_WARN(ctx, msg)` | `ctx->log_warn(...)` | Warning |
| `MMCO_ERR(ctx, msg)` | `ctx->log_error(...)` | Error |
| `MMCO_DBG(ctx, msg)` | `ctx->log_debug(...)` | Debug (may be hidden unless debug logging is enabled) |

#### 4.4.2 Enumerate Instances

```cpp
int count = g_ctx->instance_count(g_ctx->module_handle);
```

This calls the `instance_count` function pointer in `MMCOContext`. Every API call follows this pattern:

```
g_ctx->function_name(g_ctx->module_handle, ...args...)
```

The `module_handle` is always the first argument. It identifies your plugin to the host so MeshMC can namespace your settings, validate your permissions, and attribute log messages.

The loop then iterates instances by index:

```cpp
for (int i = 0; i < count; ++i) {
    const char* id = g_ctx->instance_get_id(g_ctx->module_handle, i);
    const char* name = g_ctx->instance_get_name(g_ctx->module_handle, id);
    // ...
}
```

Note the two-step pattern: get the ID by index, then query properties by ID. Instance IDs are stable strings (like `"1a2b3c4d"`); indices can change as instances are added/removed.

#### 4.4.3 Read/Write Settings

```cpp
g_ctx->setting_set(g_ctx->module_handle, "last_run", "just now");
const char* val = g_ctx->setting_get(g_ctx->module_handle, "last_run");
```

Settings are automatically namespaced to your plugin. When you call `setting_set(..., "last_run", "just now")`, MeshMC stores it under your plugin's namespace. Another plugin calling `setting_get(..., "last_run")` would get its own (different) value, or `nullptr` if unset.

Settings persist across restarts. They are stored in MeshMC's configuration file.

#### 4.4.4 Query App Info

```cpp
const char* appName = g_ctx->get_app_name(g_ctx->module_handle);
const char* appVer = g_ctx->get_app_version(g_ctx->module_handle);
```

These are utility functions from Section 14 of the API. They return the host application name ("MeshMC") and version string.

### 4.5 Hook Callback: on_instance_pre_launch

```cpp
static int on_instance_pre_launch(void* /*mh*/, uint32_t /*hook_id*/,
                                  void* payload, void* /*user_data*/)
{
    auto* info = static_cast<MMCOInstanceInfo*>(payload);
```

This hook fires just before an instance launches. Unlike `APP_INITIALIZED`, this hook carries a payload: `MMCOInstanceInfo*`.

```cpp
struct MMCOInstanceInfo {
    const char* instance_id;
    const char* instance_name;
    const char* instance_path;
    const char* minecraft_version;
};
```

**Critical rule**: Always check the payload for null before dereferencing. The host guarantees valid structs for documented hooks, but defensive coding prevents crashes from future API changes.

### 4.6 mmco_init() — The Entry Point

```cpp
extern "C" int mmco_init(MMCOContext* ctx)
```

This is the **first function** MeshMC calls after loading your plugin. It is found by `dlsym("mmco_init")` — the name is fixed and must be exactly `mmco_init`, with C linkage.

**What to do in mmco_init:**

1. Store the context pointer
2. Log an initialization message
3. Register hooks
4. Register any toolbar actions or menu items
5. Return `0` for success, non-zero for failure

```cpp
g_ctx = ctx;

MMCO_LOG(ctx, "Hello World plugin initializing...");

ctx->hook_register(ctx->module_handle, MMCO_HOOK_APP_INITIALIZED,
                   on_app_initialized, nullptr);

ctx->hook_register(ctx->module_handle, MMCO_HOOK_INSTANCE_PRE_LAUNCH,
                   on_instance_pre_launch, nullptr);

MMCO_LOG(ctx, "Hello World plugin initialized.");
return 0;
```

`hook_register` takes four arguments:

| Argument | Value | Purpose |
|---|---|---|
| `module_handle` | `ctx->module_handle` | Identifies your plugin |
| `hook_id` | `MMCO_HOOK_APP_INITIALIZED` | Which event to subscribe to |
| `callback` | `on_app_initialized` | Your function pointer |
| `user_data` | `nullptr` | Passed back to your callback (use for context) |

**If `mmco_init` returns non-zero**, the plugin is considered failed. MeshMC will log an error and skip the plugin. Already-registered hooks are automatically cleaned up.

### 4.7 mmco_unload() — The Exit Point

```cpp
extern "C" void mmco_unload()
{
    if (g_ctx) {
        MMCO_LOG(g_ctx, "Hello World plugin unloading. Goodbye!");
    }
    g_ctx = nullptr;
}
```

Called when MeshMC shuts down (or when the plugin is explicitly unloaded). Plugins are unloaded in **reverse** order of initialization.

**What to do in mmco_unload:**

1. Log a shutdown message (optional but helpful for debugging)
2. Free any resources you allocated (file handles, memory, etc.)
3. Set your global context pointer to `nullptr`

You do **not** need to unregister hooks — MeshMC unregisters them automatically for you when the module is unloaded. However, if you need to unregister a hook mid-session (e.g., in response to a setting change), use `ctx->hook_unregister()`.

---

## Step 5: Build the Plugin

```bash
cd hello-world-plugin

# Configure — point CMAKE_PREFIX_PATH at MeshMC install and Qt6
cmake -B build -DCMAKE_PREFIX_PATH="/usr/local;/usr/lib/qt6"

# Build
cmake --build build
```

If everything succeeds, you will have:

```
build/HelloWorld.mmco
```

### Common Build Errors

| Error | Cause | Fix |
|---|---|---|
| `Could not find package MeshMC_SDK` | MeshMC not installed, or wrong prefix | Check that `/usr/local/lib/cmake/MeshMC_SDK/` exists |
| `Could not find package Qt6` | Qt6 not installed or not in prefix path | Add Qt6 path to `CMAKE_PREFIX_PATH` |
| `mmco_sdk.h: No such file` | SDK headers not installed | Re-run `cmake --install` for MeshMC |
| `undefined reference to ...` | Missing link library | Ensure `target_link_libraries` includes `MeshMC::SDK` |

---

## Step 6: Install the Plugin

Copy the `.mmco` file to one of the plugin search directories:

```bash
# User-local (recommended for development)
mkdir -p ~/.local/lib/mmcmodules
cp build/HelloWorld.mmco ~/.local/lib/mmcmodules/
```

Alternatively, for system-wide installation:

```bash
sudo mkdir -p /usr/local/lib/mmcmodules
sudo cp build/HelloWorld.mmco /usr/local/lib/mmcmodules/
```

---

## Step 7: Test the Plugin

1. Launch MeshMC from the terminal so you can see log output:

   ```bash
   meshmc  # or the full path to the binary
   ```

2. Watch the console for plugin log messages.

3. If you have instances configured, you should see them enumerated.

4. Try launching an instance — the pre-launch hook should fire.

---

## Expected Console Output

When MeshMC starts with the Hello World plugin installed, you should see output similar to:

```
[Plugin:HelloWorld] Hello World plugin initializing...
[Plugin:HelloWorld] Hello World plugin initialized.
[Plugin:HelloWorld] Hello from the Hello World plugin!
[Plugin:HelloWorld] MeshMC has 3 instance(s):
[Plugin:HelloWorld]   [0] Vanilla 1.21.4 (a1b2c3d4)
[Plugin:HelloWorld]   [1] Fabric 1.21.4 (e5f6a7b8)
[Plugin:HelloWorld]   [2] Forge 1.20.1 (c9d0e1f2)
[Plugin:HelloWorld] Running on MeshMC 1.0.0
```

When you launch an instance:

```
[Plugin:HelloWorld] Instance 'Vanilla 1.21.4' is about to launch!
```

On shutdown:

```
[Plugin:HelloWorld] Hello World plugin unloading. Goodbye!
```

The `[Plugin:HelloWorld]` prefix is added by MeshMC automatically — it uses the name from your `mmco_module_info`.

---

## Troubleshooting

### Plugin doesn't appear in logs

1. **File location**: Verify the `.mmco` file is in a search directory:
   ```bash
   ls -la ~/.local/lib/mmcmodules/HelloWorld.mmco
   ```

2. **File permissions**: The file must be readable:
   ```bash
   chmod 644 ~/.local/lib/mmcmodules/HelloWorld.mmco
   ```

3. **Magic number**: Run `readelf` or `nm` to verify the `mmco_module_info` symbol is exported:
   ```bash
   nm -D build/HelloWorld.mmco | grep mmco_module_info
   ```
   You should see a `D` (data) or `B` (BSS) symbol.

### Plugin loads but hooks don't fire

- `MMCO_HOOK_APP_INITIALIZED` fires **after** all plugins are loaded. If your plugin was the last to load, the hook fires almost immediately.
- `MMCO_HOOK_INSTANCE_PRE_LAUNCH` only fires when a user actually launches an instance.
- Check the return value of `hook_register` — it returns `0` on success.

### Settings don't persist

- Settings are saved when MeshMC exits cleanly. If you `kill -9` the process, settings may be lost.
- Check that you are using `g_ctx->module_handle` (not a stale pointer) when calling `setting_set`.

---

## How It All Fits Together

Here is the complete lifecycle of the Hello World plugin:

```
1. MeshMC starts
2. PluginLoader scans mmcmodules/ directories
3. PluginLoader finds HelloWorld.mmco
4. PluginLoader calls dlopen("HelloWorld.mmco")
5. PluginLoader looks up symbol: mmco_module_info
6. PluginLoader validates magic (0x4D4D434F) and abi_version (1)
7. PluginLoader looks up symbol: mmco_init
8. PluginManager calls mmco_init(ctx)
   ├── Plugin stores g_ctx = ctx
   ├── Plugin logs "initializing..."
   ├── Plugin registers MMCO_HOOK_APP_INITIALIZED → on_app_initialized
   ├── Plugin registers MMCO_HOOK_INSTANCE_PRE_LAUNCH → on_instance_pre_launch
   └── Plugin logs "initialized." and returns 0
9. ... (other plugins init) ...
10. MeshMC fires MMCO_HOOK_APP_INITIALIZED
    ├── on_app_initialized runs
    ├── Logs greeting, enumerates instances, stores setting, queries app info
    └── Returns 0 (continue chain)
11. ... (user interacts with MeshMC) ...
12. User launches an instance
    ├── MeshMC fires MMCO_HOOK_INSTANCE_PRE_LAUNCH
    ├── on_instance_pre_launch runs
    ├── Logs "Instance 'X' is about to launch!"
    └── Returns 0 (continue chain)
13. ... (user closes MeshMC) ...
14. PluginManager calls mmco_unload() for each plugin (reverse order)
    ├── Plugin logs "unloading. Goodbye!"
    └── Plugin sets g_ctx = nullptr
15. PluginLoader calls dlclose("HelloWorld.mmco")
```

---

## Exercises

Now that you have a working plugin, try these modifications to deepen your understanding:

### Exercise 1: Persistent Launch Counter

Add a launch counter that persists across restarts:

1. In `on_instance_pre_launch`, read a setting called `"launch_count"`
2. Parse it as an integer (or default to 0 if missing)
3. Increment it
4. Write it back with `setting_set`
5. Log the count: `"Total launches tracked: 42"`

**Hint**: `setting_get` returns `const char*`. Use `atoi()` or `strtol()` to convert.

### Exercise 2: Shut Down Hook

Register for `MMCO_HOOK_APP_SHUTDOWN` in `mmco_init`. In the callback, log the total number of instances and the `launch_count` setting. This fires just before MeshMC exits.

### Exercise 3: Add a Confirmation Dialog

When an instance is about to launch (`MMCO_HOOK_INSTANCE_PRE_LAUNCH`), show a confirmation dialog:

```cpp
int proceed = g_ctx->ui_confirm_dialog(
    g_ctx->module_handle,
    "Launch Confirmation",
    "Are you sure you want to launch this instance?");
```

If the user clicks "No" (returns 0), return a non-zero value from the hook callback to block the launch.

**Warning**: This is just an exercise to understand the API. Don't ship a plugin that asks for confirmation on every launch — your users will hate you.

### Exercise 4: React to Instance Creation

Register for `MMCO_HOOK_INSTANCE_CREATED`. The payload is `MMCOInstanceInfo*`. Log the new instance's name and ID. Then use `instance_set_notes` to add a note like `"Created while HelloWorld plugin was active"`.

---

## Next Steps

Now that you understand the plugin fundamentals, you have several paths forward:

1. **Read the API Reference** — Explore all 14 sections of the `MMCOContext` struct: [API Reference](../api-reference/)
2. **Study the Hooks** — Learn every hook, its payload, and when it fires: [Hooks Reference](../hooks-reference/)
3. **Build a Real Plugin** — Follow the [BackupSystem Walkthrough](backup-system-walkthrough.md) to study a production-quality plugin with UI pages, toolbar integration, and complex business logic
4. **Understand the SDK** — Learn about in-tree vs out-of-tree builds, the CMake config system, and what the SDK header provides: [SDK Guide](../sdk-guide/)
