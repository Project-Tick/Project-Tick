# Section 14 — Utility API

> **Header:** `PluginAPI.h` (function pointer declarations) · **SDK:** `mmco_sdk.h` (struct definition) · **Implementation:** `PluginManager.cpp` (static trampolines) · **Backend:** `BuildConfig`, Qt `QDateTime`

---

## Overview

The Utility API provides general-purpose informational and helper
functions that do not belong to any other API section. These are the
simplest functions in the entire MMCO plugin interface: they have no
side effects, require no prior setup, and can be called at any point
during the plugin lifecycle (including inside `mmco_init()` and
`mmco_unload()`).

Section 14 exposes three function pointers in `MMCOContext`:

| Function pointer | Purpose |
|------------------|---------|
| `get_app_version` | Returns MeshMC's semantic version string |
| `get_app_name` | Returns the application display name |
| `get_timestamp` | Returns the current UNIX epoch time in seconds |

These functions serve as the foundation for:

- **Version gating** — plugins can check the host version at init time
  and gracefully degrade or refuse to load on incompatible builds.
- **Branding queries** — plugins that surface the application name in
  their UI (log windows, about dialogs, exported files) can query it
  at runtime rather than hard-coding it.
- **Timestamping** — plugins that need wall-clock time for log entries,
  cache expiry, backup filenames, or cooldown tracking have a
  single-call solution that does not require platform headers.

---

## Function Pointers in `MMCOContext`

All three utility function pointers are declared at the end of the
`MMCOContext` struct in `PluginAPI.h`:

```c
/* S14 — Utility */
const char* (*get_app_version)(void* mh);
const char* (*get_app_name)(void* mh);
int64_t     (*get_timestamp)(void* mh);
```

They are wired in `PluginManager::buildContext()`:

```cpp
// S14 — Utility
ctx.get_app_version = api_get_app_version;
ctx.get_app_name    = api_get_app_name;
ctx.get_timestamp   = api_get_timestamp;
```

The static trampoline declarations live in `PluginManager.h`:

```cpp
/* Section 14: Utility */
static const char* api_get_app_version(void* mh);
static const char* api_get_app_name(void* mh);
static int64_t     api_get_timestamp(void* mh);
```

---

## Architecture Diagram

```text
┌───────────────────────────────────────────────────────────────┐
│                     Utility API (S14)                         │
├───────────────────────────────────────────────────────────────┤
│                                                               │
│  ┌─────────────── Application Info ────────────────────────┐  │
│  │  get_app_version(mh)  → "1.2.0" (BuildConfig.VERSION_STR)│ │
│  │  get_app_name(mh)     → "MeshMC" (BuildConfig.MESHMC_NAME)│ │
│  └─────────────────────────────────────────────────────────┘  │
│                                                               │
│  ┌─────────────── Time ────────────────────────────────────┐  │
│  │  get_timestamp(mh)    → 1744694400 (epoch seconds)      │  │
│  └─────────────────────────────────────────────────────────┘  │
│                                                               │
└───────────────────────────────────────────────────────────────┘

    Plugin code                        MeshMC internals
    ──────────                         ────────────────
    ctx->get_app_version(mh)  ──→  PluginManager::api_get_app_version()
                                       │
                                       ├── rt(mh)->tempString =
                                       │      BuildConfig.VERSION_STR
                                       │      .toStdString()
                                       └── return tempString.c_str()

    ctx->get_timestamp(mh)    ──→  PluginManager::api_get_timestamp()
                                       │
                                       └── return QDateTime::
                                              currentSecsSinceEpoch()
```

---

## String Ownership — `tempString` Semantics

`get_app_version()` and `get_app_name()` return `const char*` pointers
that go through the per-module `tempString` buffer inside
`ModuleRuntime`. This has a critical consequence:

> **The returned pointer is valid only until the next MMCO API call
> (on the same module) that itself writes to `tempString`.**

Any string-returning function in the API (`instance_get_name()`,
`setting_get()`, `mod_get_filename()`, etc.) may overwrite the same
buffer. If you need the value to persist across multiple API calls, copy
it immediately:

```c
/* Safe pattern */
const char* ver = ctx->get_app_version(ctx->module_handle);
char my_version[64];
strncpy(my_version, ver, sizeof(my_version) - 1);
my_version[sizeof(my_version) - 1] = '\0';

/* Now safe to call other API functions */
const char* name = ctx->get_app_name(ctx->module_handle);
```

`get_timestamp()` returns `int64_t` directly and is not affected by
`tempString` at all.

See the individual function pages for detailed ownership tables.

---

## Cross-References

| Section | Relationship |
|---------|-------------|
| **S1 (Lifecycle)** | `get_app_version()` and `get_app_name()` are safe to call inside `mmco_init()` and `mmco_unload()`. |
| **S2 (Settings)** | Combine `get_app_version()` with `setting_set()` to record which version last ran the plugin. |
| **S10 (Filesystem)** | `get_timestamp()` pairs with `fs_write()` / `fs_plugin_data_dir()` for timestamped log files or cache-expiry metadata. |
| **S12 (UI Dialogs)** | `get_app_name()` provides the correct branding string for dialog titles. |

---

## Sub-Pages

| Page | Functions documented |
|------|---------------------|
| [Application Info](app-info.md) | `get_app_version()`, `get_app_name()` |
| [Timestamps](timestamps.md) | `get_timestamp()` |

---

## Quick Reference

```c
/* ── S14 function pointer signatures ── */

/* Application info */
const char* (*get_app_version)(void* mh);
const char* (*get_app_name)(void* mh);

/* Timestamp */
int64_t (*get_timestamp)(void* mh);
```

---

## Minimal Example

```c
#include "mmco_sdk.h"
#include <stdio.h>
#include <string.h>

static MMCOContext* g_ctx;

extern "C" int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;
    void* mh = ctx->module_handle;

    /* Query application identity */
    const char* name = ctx->get_app_name(mh);
    char app_name[64];
    snprintf(app_name, sizeof(app_name), "%s", name);

    const char* ver = ctx->get_app_version(mh);
    char app_ver[32];
    snprintf(app_ver, sizeof(app_ver), "%s", ver);

    /* Timestamp the init event */
    int64_t now = ctx->get_timestamp(mh);

    char buf[256];
    snprintf(buf, sizeof(buf),
             "Plugin loaded at epoch %lld on %s %s",
             (long long)now, app_name, app_ver);
    ctx->log_info(mh, buf);

    return 0;
}
```
