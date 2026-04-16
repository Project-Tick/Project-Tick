# Instance Control API

> **Sub-page of [S04 — Instance Management](README.md)**
> Covers: `instance_kill()`, `instance_is_running()`, `instance_has_crashed()`,
> `instance_has_update()`, `instance_get_total_play_time()`,
> `instance_get_last_play_time()`, `instance_get_last_launch()`

---

## instance_kill()

Terminate the running Minecraft process for the specified instance.

### Signature

```c
int (*instance_kill)(void* mh, const char* id);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle — the opaque pointer from `MMCOContext.module_handle`. |
| `id` | `const char*` | Instance ID string. Must be a valid ID from `instance_get_id()`. Must not be `NULL`. |

### Return Value

| Value | Meaning |
|-------|---------|
| `0` | The kill signal was sent successfully. The process will terminate. |
| `-1` | Kill failed. Possible causes: invalid instance ID, `NULL` parameters, instance is not currently running, application state is invalid. |

### Thread Safety

**Main thread only.** The function accesses `m_instanceExtras` and calls
`LaunchController::abort()`, both of which operate on Qt objects.

---

### How Kill Works Internally

#### Step 1: Trampoline

```cpp
int PluginManager::api_instance_kill(void* mh, const char* id)
{
    auto* r = rt(mh);
    auto* app = r->manager->m_app;
    if (!app || !app->instances() || !id)
        return -1;
    auto inst = app->instances()->getInstanceById(QString::fromUtf8(id));
    if (!inst)
        return -1;
    return app->kill(inst) ? 0 : -1;
}
```

The trampoline resolves the instance and delegates to `Application::kill()`.

#### Step 2: Application::kill()

```cpp
bool Application::kill(InstancePtr instance)
{
    if (!instance->isRunning()) {
        qWarning() << "Attempted to kill instance" << instance->id()
                   << ", which isn't running.";
        return false;
    }
    auto& extras = m_instanceExtras[instance->id()];
    auto controller = extras.controller;   // shared_ptr copy keeps alive
    if (controller) {
        return controller->abort();
    }
    return true;
}
```

#### Graceful vs. Forced Termination

MeshMC's kill path uses `LaunchController::abort()`, which calls
`LaunchTask::abort()`. The `LaunchTask` signal chain results in:

1. **SIGTERM / TerminateProcess** sent to the Java child process.
2. The process is given a brief window to exit cleanly (write world data,
   flush logs).
3. If the process does not exit within the timeout, a **SIGKILL / forced
   termination** follows.

From the plugin's perspective, `instance_kill()` is a **graceful request**
that will escalate to forced termination if necessary. There is no separate
"force kill" API — a single call handles both stages.

#### What Happens After Kill

After the process terminates:

- `BaseInstance::setRunning(false)` is called.
- `Application::subRunningInstance()` decrements the running count.
- If the process exited abnormally, `instance_has_crashed()` will return `1`.
- `MMCO_HOOK_INSTANCE_POST_LAUNCH` does **not** fire for killed processes.
  POST_LAUNCH only fires on `LaunchController::onSucceeded()`.

#### Running-Instance Count

MeshMC tracks how many instances are running via `m_runningInstances`.
When this count drops to zero, application updates are re-enabled
(`updateAllowedChanged(true)`). Killing the last running instance
re-enables the update system.

---

### Error Conditions

| Condition | Return | Notes |
|-----------|--------|-------|
| `id` is `NULL` | `-1` | Caught in trampoline. |
| Instance not found | `-1` | `getInstanceById()` returns null. |
| Instance not running | `-1` | `Application::kill()` returns `false` when `isRunning()` is false. |
| Controller is null (edge case) | `0` | If the instance is marked running but has no controller, `kill()` returns `true`. This is a defensive fallback. |
| Application pointer invalid | `-1` | Should not occur after initialization. |

---

### Implementation Notes (for MeshMC contributors)

The trampoline is declared in `PluginManager.h`:

```cpp
static int api_instance_kill(void* mh, const char* id);
```

And wired in `buildContext()`:

```cpp
ctx.instance_kill = api_instance_kill;
```

The `shared_ptr` copy in `Application::kill()` is intentional — it prevents
the controller from being destroyed while `abort()` is executing, which
could happen if the abort triggers immediate cleanup.

---

## instance_is_running()

Check whether the specified instance has an active Minecraft process.

### Signature

```c
int (*instance_is_running)(void* mh, const char* id);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. |
| `id` | `const char*` | Instance ID string. Must not be `NULL`. |

### Return Value

| Value | Meaning |
|-------|---------|
| `1` | The instance is currently running (a Minecraft process is alive). |
| `0` | The instance is not running, **or** the ID is invalid / `NULL`. |

### Thread Safety

**Main thread only.**

---

### What States Count as "Running"?

The `isRunning()` method checks `BaseInstance::m_isRunning`, a boolean flag
managed by `setRunning()`:

```cpp
bool BaseInstance::isRunning() const
{
    return m_isRunning;
}
```

The flag is set to `true` by `LaunchTask` when the Java process is forked,
and set to `false` when the process exits (regardless of exit code).

**The flag covers the entire process lifetime:**

```text
                ┌─ setRunning(true) ─────────────────── setRunning(false) ─┐
                │                                                          │
Timeline:  ─────┤  JVM starting  │  Minecraft running  │  Process exiting  ├─────
                │                                                          │
isRunning:      │◄─────────────── returns 1 ──────────────────────────────►│
                                                                           │
                                                              returns 0 ───►
```

**Important edge cases:**

| Scenario | `isRunning()` | Notes |
|----------|---------------|-------|
| Instance idle, never launched | `0` | Default state. |
| Launch in progress, JVM starting | `1` | Flag set early in launch sequence. |
| Minecraft main menu open | `1` | Process alive. |
| Minecraft in-game | `1` | Process alive. |
| Process exiting (saving worlds) | `1` | Still alive until OS reports exit. |
| Process exited cleanly | `0` | Flag cleared in exit handler. |
| Process crashed | `0` | Flag cleared; `hasCrashed()` returns `1`. |
| `instance_kill()` called, process terminating | `1` | Until the process actually exits. |

### Implementation

```cpp
int PluginManager::api_instance_is_running(void* mh, const char* id)
{
    auto* r = rt(mh);
    auto* inst = resolveInstance(r, id);
    return inst ? (inst->isRunning() ? 1 : 0) : 0;
}
```

The function does not distinguish between "not running" and "instance not
found" — both return `0`. If you need to differentiate, check that the
instance exists first (e.g., `instance_get_name()` returns non-null).

---

## instance_has_crashed()

Check whether the instance's last process exit was a crash.

### Signature

```c
int (*instance_has_crashed)(void* mh, const char* id);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. |
| `id` | `const char*` | Instance ID string. Must not be `NULL`. |

### Return Value

| Value | Meaning |
|-------|---------|
| `1` | The instance has the crashed flag set. |
| `0` | Not crashed, or invalid / `NULL` ID. |

### Thread Safety

**Main thread only.**

---

### Crash Flag Semantics

The `m_crashed` flag in `BaseInstance` is set by `setCrashed(true)` when
`LaunchController::onFailed()` detects a non-zero exit code or abnormal
termination.

**The crash flag is sticky.** Once set, it remains `true` until:
- The instance is relaunched (cleared during launch setup).
- The user manually acknowledges the crash in the UI.

This means `instance_has_crashed()` can return `1` even when the instance is
not running. It reflects the *most recent* exit status.

### Implementation

```cpp
int PluginManager::api_instance_has_crashed(void* mh, const char* id)
{
    auto* r = rt(mh);
    auto* inst = resolveInstance(r, id);
    return inst ? (inst->hasCrashed() ? 1 : 0) : 0;
}
```

Where `hasCrashed()` is:

```cpp
bool hasCrashed() const
{
    return m_crashed;
}
```

---

## instance_has_update()

Check whether an update is available for the instance.

### Signature

```c
int (*instance_has_update)(void* mh, const char* id);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. |
| `id` | `const char*` | Instance ID string. Must not be `NULL`. |

### Return Value

| Value | Meaning |
|-------|---------|
| `1` | An update is available for the instance. |
| `0` | No update available, or invalid / `NULL` ID. |

### Thread Safety

**Main thread only.**

### Implementation

```cpp
int PluginManager::api_instance_has_update(void* mh, const char* id)
{
    auto* r = rt(mh);
    auto* inst = resolveInstance(r, id);
    return inst ? (inst->hasUpdateAvailable() ? 1 : 0) : 0;
}
```

The `m_hasUpdate` flag is set by the update-check system when a newer
version of one of the instance's components is available. The flag is
cleared when the update is applied or dismissed.

---

## instance_get_total_play_time()

Get the cumulative play time for an instance across all sessions.

### Signature

```c
int64_t (*instance_get_total_play_time)(void* mh, const char* id);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. |
| `id` | `const char*` | Instance ID string. Must not be `NULL`. |

### Return Value

| Value | Meaning |
|-------|---------|
| `> 0` | Total play time in **seconds** across all sessions. |
| `0` | Never played, or invalid / `NULL` ID. |

### Thread Safety

**Main thread only.**

### Implementation

```cpp
int64_t PluginManager::api_instance_get_total_play_time(void* mh,
                                                        const char* id)
{
    auto* r = rt(mh);
    auto* inst = resolveInstance(r, id);
    return inst ? inst->totalTimePlayed() : 0;
}
```

The total is persisted in the instance's configuration file and survives
launcher restarts. It is updated when a play session ends.

---

## instance_get_last_play_time()

Get the duration of the most recent play session.

### Signature

```c
int64_t (*instance_get_last_play_time)(void* mh, const char* id);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. |
| `id` | `const char*` | Instance ID string. Must not be `NULL`. |

### Return Value

| Value | Meaning |
|-------|---------|
| `> 0` | Duration of the last session in **seconds**. |
| `0` | Never played, or invalid / `NULL` ID. |

### Thread Safety

**Main thread only.**

### Implementation

```cpp
int64_t PluginManager::api_instance_get_last_play_time(void* mh,
                                                       const char* id)
{
    auto* r = rt(mh);
    auto* inst = resolveInstance(r, id);
    return inst ? inst->lastTimePlayed() : 0;
}
```

Updated at session end. During an active session, the value reflects the
*previous* session's duration, not the current in-progress session.

---

## instance_get_last_launch()

Get the timestamp of the most recent launch.

### Signature

```c
int64_t (*instance_get_last_launch)(void* mh, const char* id);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. |
| `id` | `const char*` | Instance ID string. Must not be `NULL`. |

### Return Value

| Value | Meaning |
|-------|---------|
| `> 0` | Epoch timestamp in **milliseconds** of the last launch. |
| `0` | Never launched, or invalid / `NULL` ID. |

### Thread Safety

**Main thread only.**

### Implementation

```cpp
int64_t PluginManager::api_instance_get_last_launch(void* mh, const char* id)
{
    auto* r = rt(mh);
    auto* inst = resolveInstance(r, id);
    return inst ? inst->lastLaunch() : 0;
}
```

Set by `setLastLaunch()` at the start of each launch sequence. Uses
`QDateTime::currentMSecsSinceEpoch()` — note the **millisecond** precision,
unlike play-time functions which use seconds.

---

## Complete Example: Kill Supervision Plugin

A plugin that monitors all running instances and kills them if they exceed
a configurable time limit:

```cpp
/* kill_supervisor.cpp — kills instances that exceed a time limit */
#include <mmco_sdk.h>
#include <cstring>
#include <cstdio>
#include <ctime>

static MMCOContext* g_ctx = nullptr;
static void*        g_mh  = nullptr;

/* Maximum session length in seconds (default: 2 hours) */
static int64_t g_max_session_seconds = 7200;

/*
 * Called from MMCO_HOOK_INSTANCE_PRE_LAUNCH to record the launch.
 * We use this as a periodic check trigger — in a real plugin you'd
 * use a timer mechanism.
 */
static int on_pre_launch(void* module_handle, uint32_t hook_id,
                          void* payload, void* user_data)
{
    (void)module_handle; (void)hook_id; (void)user_data;
    MMCOInstanceInfo* info = (MMCOInstanceInfo*)payload;

    char msg[512];
    snprintf(msg, sizeof(msg), "Supervisor: instance '%s' is launching. "
             "Time limit: %lld seconds.",
             info->instance_name, (long long)g_max_session_seconds);
    g_ctx->log_info(g_mh, msg);

    return 0;  /* do NOT cancel the launch */
}

/*
 * Scan all instances and kill any that have been running too long.
 * In practice, this would be called from a timer. Here we hook it
 * to APP_INITIALIZED as a demonstration.
 */
static void check_and_kill_overtime(void)
{
    int n = g_ctx->instance_count(g_mh);
    time_t now = time(nullptr);

    for (int i = 0; i < n; i++) {
        const char* id = g_ctx->instance_get_id(g_mh, i);
        if (!id) continue;

        /* Copy ID — next API call invalidates the pointer */
        char id_buf[256];
        strncpy(id_buf, id, sizeof(id_buf) - 1);
        id_buf[sizeof(id_buf) - 1] = '\0';

        if (!g_ctx->instance_is_running(g_mh, id_buf))
            continue;

        /* Get last launch time (milliseconds since epoch) */
        int64_t launch_ms = g_ctx->instance_get_last_launch(g_mh, id_buf);
        if (launch_ms <= 0)
            continue;

        int64_t elapsed_sec = (int64_t)now - (launch_ms / 1000);
        if (elapsed_sec > g_max_session_seconds) {
            const char* name = g_ctx->instance_get_name(g_mh, id_buf);
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "Supervisor: killing '%s' — exceeded %lld seconds "
                     "(running for %lld seconds).",
                     name ? name : id_buf,
                     (long long)g_max_session_seconds,
                     (long long)elapsed_sec);
            g_ctx->log_warn(g_mh, msg);

            if (g_ctx->instance_kill(g_mh, id_buf) != 0) {
                g_ctx->log_error(g_mh,
                    "Supervisor: instance_kill() failed!");
            }
        }
    }
}

static int on_app_init(void* module_handle, uint32_t hook_id,
                        void* payload, void* user_data)
{
    (void)module_handle; (void)hook_id; (void)payload; (void)user_data;

    /* Read time limit from plugin settings */
    const char* val = g_ctx->setting_get(g_mh, "supervisor_max_seconds");
    if (val) {
        int64_t parsed = strtoll(val, nullptr, 10);
        if (parsed > 0) g_max_session_seconds = parsed;
    }

    /* Initial check */
    check_and_kill_overtime();
    return 0;
}

extern "C" int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;
    g_mh  = ctx->module_handle;

    ctx->hook_register(g_mh, MMCO_HOOK_APP_INITIALIZED,
                       on_app_init, nullptr);
    ctx->hook_register(g_mh, MMCO_HOOK_INSTANCE_PRE_LAUNCH,
                       on_pre_launch, nullptr);

    g_ctx->log_info(g_mh, "Kill Supervisor plugin loaded.");
    return 0;
}

extern "C" void mmco_unload(MMCOContext* ctx)
{
    ctx->hook_unregister(ctx->module_handle, MMCO_HOOK_APP_INITIALIZED,
                         on_app_init);
    ctx->hook_unregister(ctx->module_handle, MMCO_HOOK_INSTANCE_PRE_LAUNCH,
                         on_pre_launch);
}
```

---

## Complete Example: Running Status Monitor

A plugin that logs the running status of all instances whenever the
application finishes initializing:

```cpp
/* status_monitor.cpp — logs running status of all instances */
#include <mmco_sdk.h>
#include <cstring>
#include <cstdio>
#include <cinttypes>

static MMCOContext* g_ctx = nullptr;
static void*        g_mh  = nullptr;

static void report_all_instances(void)
{
    int n = g_ctx->instance_count(g_mh);
    char msg[1024];
    snprintf(msg, sizeof(msg), "StatusMonitor: %d instance(s) found.", n);
    g_ctx->log_info(g_mh, msg);

    for (int i = 0; i < n; i++) {
        const char* id = g_ctx->instance_get_id(g_mh, i);
        if (!id) continue;

        char id_buf[256];
        strncpy(id_buf, id, sizeof(id_buf) - 1);
        id_buf[sizeof(id_buf) - 1] = '\0';

        const char* name = g_ctx->instance_get_name(g_mh, id_buf);
        char name_buf[256] = "(unknown)";
        if (name) {
            strncpy(name_buf, name, sizeof(name_buf) - 1);
            name_buf[sizeof(name_buf) - 1] = '\0';
        }

        int running  = g_ctx->instance_is_running(g_mh, id_buf);
        int can_go   = g_ctx->instance_can_launch(g_mh, id_buf);
        int crashed  = g_ctx->instance_has_crashed(g_mh, id_buf);
        int has_upd  = g_ctx->instance_has_update(g_mh, id_buf);

        int64_t total_play = g_ctx->instance_get_total_play_time(g_mh, id_buf);
        int64_t last_play  = g_ctx->instance_get_last_play_time(g_mh, id_buf);
        int64_t last_launch = g_ctx->instance_get_last_launch(g_mh, id_buf);

        snprintf(msg, sizeof(msg),
                 "  [%d] '%s' (id=%s)\n"
                 "       running=%d  can_launch=%d  crashed=%d  update=%d\n"
                 "       total_play=%" PRId64 "s  last_play=%" PRId64 "s"
                 "  last_launch=%" PRId64 "ms",
                 i, name_buf, id_buf,
                 running, can_go, crashed, has_upd,
                 total_play, last_play, last_launch);
        g_ctx->log_info(g_mh, msg);
    }
}

static int on_app_init(void* module_handle, uint32_t hook_id,
                        void* payload, void* user_data)
{
    (void)module_handle; (void)hook_id; (void)payload; (void)user_data;
    report_all_instances();
    return 0;
}

static int on_post_launch(void* module_handle, uint32_t hook_id,
                           void* payload, void* user_data)
{
    (void)module_handle; (void)hook_id; (void)user_data;
    MMCOInstanceInfo* info = (MMCOInstanceInfo*)payload;
    char msg[512];
    snprintf(msg, sizeof(msg),
             "StatusMonitor: '%s' exited. Refreshing status...",
             info->instance_name);
    g_ctx->log_info(g_mh, msg);
    report_all_instances();
    return 0;
}

extern "C" int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;
    g_mh  = ctx->module_handle;

    ctx->hook_register(g_mh, MMCO_HOOK_APP_INITIALIZED,
                       on_app_init, nullptr);
    ctx->hook_register(g_mh, MMCO_HOOK_INSTANCE_POST_LAUNCH,
                       on_post_launch, nullptr);

    g_ctx->log_info(g_mh, "StatusMonitor plugin loaded.");
    return 0;
}

extern "C" void mmco_unload(MMCOContext* ctx)
{
    ctx->hook_unregister(ctx->module_handle, MMCO_HOOK_APP_INITIALIZED,
                         on_app_init);
    ctx->hook_unregister(ctx->module_handle, MMCO_HOOK_INSTANCE_POST_LAUNCH,
                         on_post_launch);
}
```

### Key Patterns Demonstrated

| Pattern | Why |
|---------|-----|
| Copy `id` and `name` before next call | `tempString` invalidation |
| Use `PRId64` format macro | Portable `int64_t` printing |
| Check all state queries together | Comprehensive status snapshot |
| Register for POST_LAUNCH to refresh | Captures state transitions |

---

## Summary of Boolean Returns

All state-query functions use the same convention:

| Function | `1` means | `0` means |
|----------|-----------|-----------|
| `instance_is_running()` | Process is alive | Not running or invalid ID |
| `instance_can_launch()` | Ready to launch | Cannot launch or invalid ID |
| `instance_has_crashed()` | Crash flag set | Not crashed or invalid ID |
| `instance_has_update()` | Update available | No update or invalid ID |

`0` is the "safe default" for all queries — an unresolvable instance
appears as not-running, not-crashed, not-updatable, and not-launchable.

---

## Summary of Time Returns

| Function | Unit | Epoch | Notes |
|----------|------|-------|-------|
| `instance_get_total_play_time()` | **Seconds** | N/A (duration) | Cumulative across all sessions |
| `instance_get_last_play_time()` | **Seconds** | N/A (duration) | Most recent session only |
| `instance_get_last_launch()` | **Milliseconds** | Unix epoch | When the last launch started |

Note the mismatch: play-time functions return **seconds**, but
`instance_get_last_launch()` returns **milliseconds**. This mirrors the
underlying C++ API where `totalTimePlayed()` / `lastTimePlayed()` use
seconds and `lastLaunch()` uses `QDateTime::currentMSecsSinceEpoch()`.

---

## FAQ

### Q: Does `instance_kill()` save the Minecraft world first?

Not explicitly. The kill sequence sends SIGTERM (on Unix) or
TerminateProcess (on Windows) to the Java process. Minecraft's own shutdown
handler *may* flush world data in response to SIGTERM, but this is not
guaranteed — especially if the process is hung. Always warn the user before
programmatically killing instances.

### Q: Can I distinguish "not running" from "invalid ID" in `instance_is_running()`?

No — both return `0`. To check if an ID is valid, call another function
like `instance_get_name(mh, id)` which returns `NULL` for invalid IDs
but a string for valid ones.

### Q: Why does `instance_has_crashed()` return `1` even when the instance isn't running?

The crash flag is **sticky**. It reflects the exit status of the last
session, not the current state. It remains set until the instance is
relaunched or the user acknowledges the crash.

### Q: Is `instance_get_last_play_time()` accurate during an active session?

No. During a running session, `lastTimePlayed()` returns the *previous*
session's duration. The current session's duration is only finalized when
the process exits.

### Q: What happens if I call `instance_kill()` on an instance that is already being killed?

The function checks `isRunning()`. If the process is still in the process
of terminating (flag still `true`), it will call `abort()` again, which is
a no-op if termination is already in progress. Once the process fully exits
and the flag clears, calling `instance_kill()` returns `-1`.

### Q: How do I implement a "restart instance" function?

Kill, wait for exit, then launch:

```cpp
static void restart_instance(MMCOContext* ctx, void* mh, const char* id)
{
    if (ctx->instance_is_running(mh, id)) {
        ctx->instance_kill(mh, id);
        /* In practice, you need to wait for the process to exit.
         * Poll instance_is_running() or use the POST_LAUNCH hook.
         * Do NOT busy-wait — use a timer or hook callback. */
    }

    /* After the process has exited: */
    if (ctx->instance_can_launch(mh, id)) {
        ctx->instance_launch(mh, id, 1);
    }
}
```

> **Warning:** There is no synchronous "wait for exit" API. You must use
> asynchronous techniques (hook callbacks or periodic polling) to detect
> when the process has actually terminated before relaunching.

---

*Part of [S04 — Instance Management](README.md) · [MMCO Plugin API Reference](../README.md)*
