# Section 15 — Launch Modifiers API

> **Header:** `PluginAPI.h` (function pointer declarations) · **SDK:** `mmco_sdk.h` (struct definition) · **Implementation:** `PluginManager.cpp` (static trampolines) · **Backend:** `QMap<QString, QString>`, `QString`, `qputenv()` / `qunsetenv()`

---

## Overview

The Launch Modifiers API gives plugins the ability to **alter the
Minecraft launch process** at the last possible moment — right before
the JVM spawns. Unlike other API sections that can be called at any
time during the plugin lifecycle, S15 functions are **only valid inside
an `INSTANCE_PRE_LAUNCH` hook callback** (hook ID `0x0200`).

Section 15 exposes two function pointers in `MMCOContext`:

| Function pointer | Purpose |
|------------------|---------|
| `launch_set_env` | Inject an environment variable into the child JVM process |
| `launch_prepend_wrapper` | Prepend a wrapper command to the launch command line |

These functions serve as the foundation for:

- **Environment injection** — plugins can set `JVM_ARGS`, custom
  variables, debug flags, or profiler configuration without modifying
  the instance settings permanently.
- **Wrapper commands** — plugins can prepend debugging tools,
  profilers (`perf`, `strace`, `valgrind`), tracing utilities, or
  custom shell wrappers to the instance launch sequence.
- **Temporary modifications** — all modifications are automatically
  cleaned up after the instance exits, ensuring the launcher state
  is never permanently altered.

### When Can These Functions Be Called?

S15 functions are **exclusively available inside `INSTANCE_PRE_LAUNCH`
hook callbacks**. Calling them outside this context results in
undefined behavior (the pending modifications may be silently
discarded or applied to the wrong launch).

The `INSTANCE_PRE_LAUNCH` hook fires after the user clicks "Launch"
but before the JVM process is actually spawned. This is the only
cancellable hook in the system — returning a non-zero value from the
hook callback signals MeshMC to abort the launch.

```text
User clicks "Launch"
       │
       ▼
┌──────────────────────────────────────┐
│ clearPendingLaunchMods()             │ ← old modifications wiped
├──────────────────────────────────────┤
│ dispatchHook(PRE_LAUNCH, &info)      │ ← plugins receive callback
│   ├── Plugin A: launch_set_env(...)  │
│   ├── Plugin B: launch_set_env(...)  │
│   └── Plugin C: launch_prepend_wrapper(...)
├──────────────────────────────────────┤
│ takePendingLaunchEnv()               │ ← env vars consumed
│ takePendingLaunchWrapper()           │ ← wrapper consumed
├──────────────────────────────────────┤
│ qputenv() for each env var           │ ← applied to process env
│ WrapperCommand setting updated       │ ← applied to instance
├──────────────────────────────────────┤
│           JVM spawns                 │
├──────────────────────────────────────┤
│        Instance exits                │
├──────────────────────────────────────┤
│ qunsetenv() for each env var         │ ← env vars removed
│ WrapperCommand restored              │ ← original wrapper restored
└──────────────────────────────────────┘
```

---

## Function Pointers in `MMCOContext`

Both launch modifier function pointers are declared in the
`MMCOContext` struct in `PluginAPI.h`:

```c
/* S15 — Launch Modifiers (only valid inside INSTANCE_PRE_LAUNCH hooks) */
int (*launch_set_env)(void* mh, const char* key, const char* value);
int (*launch_prepend_wrapper)(void* mh, const char* wrapper_cmd);
```

They are wired in `PluginManager::buildContext()`:

```cpp
// S15 — Launch Modifiers
ctx.launch_set_env = api_launch_set_env;
ctx.launch_prepend_wrapper = api_launch_prepend_wrapper;
```

The static trampoline declarations live in `PluginManager.h`:

```cpp
/* Section 15: Launch Modifiers */
static int api_launch_set_env(void* mh, const char* key, const char* value);
static int api_launch_prepend_wrapper(void* mh, const char* wrapper_cmd);
```

---

## Internal Storage

Pending launch modifications are stored as member variables in
`PluginManager`:

```cpp
QMap<QString, QString> m_pendingLaunchEnv;
QString m_pendingLaunchWrapper;
```

These are **not** per-module — they are shared across all plugins.
If multiple plugins set the same environment variable key, the last
one wins (standard `QMap::insert` behavior). Wrapper commands from
multiple plugins are chained: each `launch_prepend_wrapper()` call
inserts the new command **before** the existing wrapper chain.

### Lifecycle of Pending Modifications

| Phase | Method | Effect |
|-------|--------|--------|
| Before hook dispatch | `clearPendingLaunchMods()` | Clears `m_pendingLaunchEnv` and `m_pendingLaunchWrapper` |
| During hook dispatch | `launch_set_env()` / `launch_prepend_wrapper()` | Populates pending storage |
| After hook dispatch | `takePendingLaunchEnv()` / `takePendingLaunchWrapper()` | Moves data out (swap semantics) |
| After instance exits | Lambda on `Task::finished` | Restores env vars and wrapper command |

---

## Architecture Diagram

```text
┌────────────────────────────────────────────────────────────────┐
│                  Launch Modifiers API (S15)                     │
├────────────────────────────────────────────────────────────────┤
│                                                                │
│  ┌──────────── Environment Variables ──────────────────────┐   │
│  │  launch_set_env(mh, "KEY", "VALUE")                     │   │
│  │  → m_pendingLaunchEnv["KEY"] = "VALUE"                  │   │
│  │  → qputenv("KEY", "VALUE") before JVM spawn             │   │
│  │  → qunsetenv("KEY") after instance exit                 │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                │
│  ┌──────────── Wrapper Commands ───────────────────────────┐   │
│  │  launch_prepend_wrapper(mh, "strace -f")                │   │
│  │  → m_pendingLaunchWrapper = "strace -f"                 │   │
│  │  → WrapperCommand = "strace -f [original]"              │   │
│  │  → WrapperCommand restored after instance exit          │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                │
└────────────────────────────────────────────────────────────────┘

    Plugin code (inside PRE_LAUNCH hook)        MeshMC internals
    ────────────────────────────────────        ────────────────
    ctx->launch_set_env(mh, k, v)         ──→  PluginManager::api_launch_set_env()
                                                   │
                                                   ├── null check on key, value
                                                   └── m_pendingLaunchEnv.insert(k, v)

    ctx->launch_prepend_wrapper(mh, cmd)  ──→  PluginManager::api_launch_prepend_wrapper()
                                                   │
                                                   ├── null/empty check on cmd
                                                   └── prepend to m_pendingLaunchWrapper
```

---

## The `INSTANCE_PRE_LAUNCH` Hook

S15 functions are only meaningful inside this hook. The hook is
defined in `PluginHooks.h`:

```c
MMCO_HOOK_INSTANCE_PRE_LAUNCH = 0x0200,  /* payload: MMCOInstanceInfo* */
```

The hook payload is an `MMCOInstanceInfo` struct:

```c
typedef struct {
    const char* instance_id;
    const char* instance_name;
    const char* instance_path;
    const char* minecraft_version;
} MMCOInstanceInfo;
```

> **Note:** `minecraft_version` may be `nullptr` in the pre-launch
> payload depending on the instance type and resolution state.

This is the **only cancellable hook** in the system. If any hook
callback returns a non-zero value, the launch is aborted.

### Registering for the Hook

```c
static int on_pre_launch(void* mh, int hook_id, void* payload)
{
    (void)hook_id;
    MMCOInstanceInfo* info = (MMCOInstanceInfo*)payload;

    /* Set env vars, prepend wrappers, or cancel launch here */

    return 0; /* 0 = continue launch, non-zero = cancel */
}

extern "C" int mmco_init(MMCOContext* ctx)
{
    ctx->hook_register(ctx->module_handle,
                       0x0200, /* INSTANCE_PRE_LAUNCH */
                       on_pre_launch);
    return 0;
}
```

---

## Cross-References

| Section | Relationship |
|---------|-------------|
| **S1 (Lifecycle)** | `hook_register()` and `hook_unregister()` manage the `INSTANCE_PRE_LAUNCH` callback registration. |
| **S3 (Instance Query)** | The `MMCOInstanceInfo*` payload provides `instance_id` which can be used with S3 functions to query further instance details. |
| **S4 (Instance Management)** | `instance_launch()` triggers the `INSTANCE_PRE_LAUNCH` hook internally. |
| **S2 (Settings)** | Plugin-scoped settings can store default env vars or wrapper preferences that the pre-launch hook reads. |
| **S14 (Utility)** | `get_timestamp()` can log when launch modifications were applied. |
| **S16 (Application Settings)** | `app_setting_get()` can read global launcher settings to conditionally apply launch modifiers. |

---

## Sub-Pages

| Page | Functions documented |
|------|---------------------|
| [Environment Variables](set-env.md) | `launch_set_env()` |
| [Wrapper Commands](prepend-wrapper.md) | `launch_prepend_wrapper()` |

---

## Quick Reference

```c
/* ── S15 function pointer signatures ── */

/* Environment variable injection (PRE_LAUNCH only) */
int (*launch_set_env)(void* mh, const char* key, const char* value);

/* Wrapper command prepending (PRE_LAUNCH only) */
int (*launch_prepend_wrapper)(void* mh, const char* wrapper_cmd);
```

---

## Minimal Example

```c
#include "mmco_sdk.h"
#include <string.h>
#include <stdio.h>

MMCO_DEFINE_MODULE("LaunchMod", "1.0.0", "Example Author",
                   "Demonstrates launch modification", "GPL-3.0-or-later");

static MMCOContext* g_ctx;

static int on_pre_launch(void* mh, int hook_id, void* payload)
{
    (void)hook_id;
    MMCOInstanceInfo* info = (MMCOInstanceInfo*)payload;

    char buf[256];
    snprintf(buf, sizeof(buf), "Pre-launch hook for: %s", info->instance_name);
    g_ctx->log_info(mh, buf);

    /* Inject a custom environment variable */
    int rc = g_ctx->launch_set_env(mh, "MY_PLUGIN_ACTIVE", "1");
    if (rc != 0) {
        g_ctx->log_error(mh, "Failed to set environment variable");
    }

    /* Prepend a wrapper (e.g. for profiling) */
    rc = g_ctx->launch_prepend_wrapper(mh, "/usr/bin/env");
    if (rc != 0) {
        g_ctx->log_error(mh, "Failed to prepend wrapper");
    }

    return 0; /* continue launch */
}

extern "C" int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;
    void* mh = ctx->module_handle;

    ctx->hook_register(mh, 0x0200, on_pre_launch);
    ctx->log_info(mh, "LaunchMod plugin loaded");

    return 0;
}

extern "C" void mmco_unload(MMCOContext* ctx)
{
    ctx->hook_unregister(ctx->module_handle, 0x0200);
}
```

---

## Important Caveats

### Environment Variables Are Process-Wide

`qputenv()` sets environment variables on the **launcher process
itself**, not just on the child JVM. This means:

- Other parts of MeshMC can see these variables while the instance
  is running.
- If MeshMC launches multiple instances concurrently, env vars from
  one launch may leak into another.
- Variables are cleaned up via `qunsetenv()` after the instance exits,
  but during the instance lifetime they are globally visible.

### Wrapper Commands Are Chained, Not Replaced

If the instance already has a user-configured wrapper command (set
in the instance settings), `launch_prepend_wrapper()` **prepends** to
it rather than replacing it. The original wrapper is restored after
the instance exits.

If multiple plugins call `launch_prepend_wrapper()`, they chain in
**reverse registration order**: the last plugin to call it will have
its wrapper command appear first in the final command line.

### No Validation of Wrapper Commands

The API does **not** validate that the wrapper command is a valid
executable or that it exists on `PATH`. If you prepend a non-existent
command, the launch will fail at the operating system level.
