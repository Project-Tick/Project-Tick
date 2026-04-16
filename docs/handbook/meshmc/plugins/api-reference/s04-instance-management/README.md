# Section 04 — Instance Management API

> **Header:** `PluginAPI.h` (function pointer declarations) · **SDK:** `mmco_sdk.h` (struct definition) · **Implementation:** `PluginManager.cpp` (static trampolines)

---

## Overview

The Instance Management API gives plugins **active control** over instances:
launching Minecraft, killing running processes, and querying runtime state.
Where [Section 03](../s03-instances/) is read-only discovery, Section 04 is
the **action surface** — the functions that make things happen.

This section covers three groups of function pointers inside `MMCOContext`:

| Group | Functions | Purpose |
|-------|-----------|---------|
| **Launch** | `instance_launch()`, `instance_can_launch()` | Start a Minecraft process |
| **Kill** | `instance_kill()` | Terminate a running process |
| **State** | `instance_is_running()`, `instance_has_crashed()`, `instance_has_update()` | Query process state |
| **Play time** | `instance_get_total_play_time()`, `instance_get_last_play_time()`, `instance_get_last_launch()` | Read timing statistics |

All functions accept an `instance_id` string obtained from S03's
`instance_get_id()`. Passing an unknown or `NULL` ID is safe — every
function returns a well-defined error value.

```c
/* S04 — Instance Management  (from MMCOContext in PluginAPI.h) */

/* State queries */
int     (*instance_is_running)         (void* mh, const char* id);
int     (*instance_can_launch)         (void* mh, const char* id);
int     (*instance_has_crashed)        (void* mh, const char* id);
int     (*instance_has_update)         (void* mh, const char* id);
int64_t (*instance_get_total_play_time)(void* mh, const char* id);
int64_t (*instance_get_last_play_time) (void* mh, const char* id);
int64_t (*instance_get_last_launch)    (void* mh, const char* id);

/* Actions */
int     (*instance_launch)             (void* mh, const char* id, int online);
int     (*instance_kill)               (void* mh, const char* id);
```

---

## Function Summary

| Function | Sub-page | Returns | Purpose |
|----------|----------|---------|---------|
| [`instance_launch()`](instance-launch.md#instance_launch) | Launch | `0` / `-1` | Start Minecraft for the given instance |
| [`instance_can_launch()`](instance-launch.md#instance_can_launch) | Launch | `1` / `0` | Check if an instance is in a launchable state |
| [`instance_kill()`](instance-control.md#instance_kill) | Control | `0` / `-1` | Terminate a running instance |
| [`instance_is_running()`](instance-control.md#instance_is_running) | Control | `1` / `0` | Check whether the instance process is alive |
| [`instance_has_crashed()`](instance-control.md#instance_has_crashed) | Control | `1` / `0` | Check crash flag |
| [`instance_has_update()`](instance-control.md#instance_has_update) | Control | `1` / `0` | Check whether an update is available |
| [`instance_get_total_play_time()`](instance-control.md#instance_get_total_play_time) | Control | `int64_t` | Cumulative play time in seconds |
| [`instance_get_last_play_time()`](instance-control.md#instance_get_last_play_time) | Control | `int64_t` | Duration of the most recent session in seconds |
| [`instance_get_last_launch()`](instance-control.md#instance_get_last_launch) | Control | `int64_t` | Epoch timestamp of last launch |

---

## The Launch / Kill Lifecycle

Understanding when a launch succeeds, what hooks fire, and how kill interacts
with the running process is critical for writing correct plugins. The
following diagram shows the full lifecycle from API call to process exit:

```text
Plugin calls instance_launch(mh, id, online)
│
├─ Resolve instance by ID  ──── fail? → return -1
│
├─ Check canLaunch()       ──── false? → show existing window / return -1
│
├─ Create LaunchController
│   ├─ setInstance()
│   ├─ setOnline(online)
│   └─ setParentWidget(window | mainWindow)
│
├─ LaunchController::executeTask()
│   ├─ Validate JVM args
│   ├─ decideAccount()            ←── selects default or prompts user
│   ├─ login()                    ←── authenticate / go offline
│   └─ launchInstance()
│       ├─ Reload instance settings
│       ├─ Create LaunchTask
│       ├─ ── HOOK: PRE_LAUNCH ── (all plugins notified)
│       ├─ LaunchTask::start()    ←── Minecraft process fork
│       └─ (async) ...
│
├─ ── Minecraft is running ──
│
├─ LaunchController::onSucceeded()
│   └─ ── HOOK: POST_LAUNCH ──   (all plugins notified)
│
└─ Plugin calls instance_kill(mh, id)
    ├─ Check isRunning()           ──── false? → return false
    └─ LaunchController::abort()   ──── signal process termination
```

### Key observations

1. **`instance_launch()` returns immediately.** The actual process launch is
   asynchronous. A return value of `0` means "the launch sequence was
   successfully queued", not "Minecraft is now running".

2. **Account selection may involve UI.** If no default account is set,
   MeshMC displays a dialog to the user. From the plugin's perspective this
   is transparent — the call blocks the main thread until the dialog closes.

3. **Only one launch per instance.** If the instance is already running,
   `Application::launch()` opens the console window instead of starting a
   second process. The API call returns `0` (success) in this case.

4. **Updates block launches.** While an application update is in progress,
   `Application::launch()` refuses to start any instance.

---

## Hook Interaction

Two hooks bracket every successful launch:

### `MMCO_HOOK_INSTANCE_PRE_LAUNCH` (0x0200)

Fired **after** authentication and settings reload, but **before**
`LaunchTask::start()`. The payload is `MMCOInstanceInfo*`:

```c
struct MMCOInstanceInfo {
    const char* instance_id;
    const char* instance_name;
    const char* instance_path;
    const char* minecraft_version;   /* may be nullptr */
};
```

**Can a plugin cancel the launch?** The hook dispatch in `PluginManager::dispatchHook()`
checks each callback's return value. If any callback returns **non-zero**,
dispatch returns `true` (cancelled). However, the current `LaunchController`
code dispatches the hook **without** checking the return value — it fires
and continues. This means:

> **In the current implementation, `MMCO_HOOK_INSTANCE_PRE_LAUNCH` is
> informational only.** A plugin cannot cancel a launch by returning
> non-zero from this hook.

This may change in a future ABI version. Plugin authors should still return
`0` from PRE_LAUNCH callbacks to remain forward-compatible.

### `MMCO_HOOK_INSTANCE_POST_LAUNCH` (0x0201)

Fired in `LaunchController::onSucceeded()` — after the Minecraft process
has exited cleanly. The payload is the same `MMCOInstanceInfo*`.

> **Note:** POST_LAUNCH fires on *success* only. If the process crashes or
> the launch is aborted, POST_LAUNCH does not fire. Use
> `instance_has_crashed()` or other state queries to detect failures.

---

## Safety Considerations

### Thread Safety

All Instance Management functions **must be called from the main (GUI)
thread**. They access Qt-managed data structures (`InstanceList`, settings
objects, widget hierarchies) that are not thread-safe.

If your plugin runs background work (e.g., an HTTP callback or a timer),
you must marshal the call back to the main thread before invoking any
S04 function. The recommended pattern:

```cpp
// WRONG — called from HTTP callback thread
ctx->instance_launch(mh, "my-instance", 1);

// RIGHT — post to main thread (see S14 utility or platform tricks)
// Use Qt's QMetaObject::invokeMethod or similar if linking against Qt,
// or schedule via your plugin's own event loop.
```

### Pointer Lifetime

All `const char*` returns from S04 functions are stored in
`ModuleRuntime::tempString`. They are valid **until the next API call on
the same module handle**. Copy them immediately if you need to keep the
value:

```cpp
const char* id = ctx->instance_get_id(mh, 0);
char* saved_id = strdup(id);   // safe to use later
// ... next API call invalidates `id` ...
```

### Destructive Actions

`instance_kill()` terminates a running Minecraft process. This is
inherently destructive — the player's world may not be saved. Document
clearly in your plugin if you call kill, and prefer notifying the user.

`instance_launch()` can trigger account dialogs — calling it
programmatically from a background timer without user intent is bad UX.

### Error Handling Pattern

All S04 action functions return `0` on success and `-1` on failure. State
query functions return `1` (true) or `0` (false), with `0` also serving as
the "error / not found" case. Always check the return value:

```cpp
if (ctx->instance_launch(mh, id, 1) != 0) {
    ctx->log_error(mh, "Failed to launch instance");
}
```

---

## Sub-pages

| File | Contents |
|------|----------|
| [instance-launch.md](instance-launch.md) | `instance_launch()`, `instance_can_launch()` — full documentation |
| [instance-control.md](instance-control.md) | `instance_kill()`, `instance_is_running()`, `instance_has_crashed()`, `instance_has_update()`, play time queries |

---

## Quick Example: Launch-and-Monitor

A minimal plugin that launches the first instance and monitors its state:

```cpp
#include <mmco_sdk.h>
#include <cstring>
#include <cstdio>

static MMCOContext* ctx;
static void*        mh;
static char         target_id[256];

static int on_post_launch(void* module_handle, uint32_t hook_id,
                          void* payload, void* user_data)
{
    MMCOInstanceInfo* info = (MMCOInstanceInfo*)payload;
    ctx->log_info(mh, "Instance finished successfully!");
    printf("[MyPlugin] %s exited cleanly.\n", info->instance_name);
    return 0;
}

extern "C" int mmco_init(MMCOContext* c)
{
    ctx = c;
    mh  = c->module_handle;

    // Find the first instance
    int count = ctx->instance_count(mh);
    if (count <= 0) {
        ctx->log_warn(mh, "No instances found.");
        return 0;
    }

    const char* id = ctx->instance_get_id(mh, 0);
    if (!id) return 0;
    strncpy(target_id, id, sizeof(target_id) - 1);

    // Register for post-launch notification
    ctx->hook_register(mh, MMCO_HOOK_INSTANCE_POST_LAUNCH,
                       on_post_launch, nullptr);

    // Check if we can launch
    if (!ctx->instance_can_launch(mh, target_id)) {
        ctx->log_warn(mh, "Instance is not in a launchable state.");
        return 0;
    }

    // Launch online
    if (ctx->instance_launch(mh, target_id, 1) != 0) {
        ctx->log_error(mh, "Failed to launch instance.");
    } else {
        ctx->log_info(mh, "Launch sequence started.");
    }

    return 0;
}

extern "C" void mmco_unload(MMCOContext* c)
{
    c->hook_unregister(c->module_handle,
                       MMCO_HOOK_INSTANCE_POST_LAUNCH,
                       on_post_launch);
}
```

---

## Related Documentation

- [S03 — Instance Query](../s03-instances/) — Enumerate and read instance properties
- [S05 — Mods](../s05-mods/) — Manage mods within an instance
- [Hooks Reference](../../hooks-reference/) — All hook IDs and payload structures
- [SDK Setup Guide](../../sdk-guide/) — Build system setup

---

*Section 04 of the [MMCO Plugin API Reference](../README.md).*
