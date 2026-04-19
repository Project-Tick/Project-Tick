# Section 16 — Application Settings API

> **Header:** `PluginAPI.h` (function pointer declaration) · **SDK:** `mmco_sdk.h` (struct definition) · **Implementation:** `PluginManager.cpp` (static trampoline) · **Backend:** `Application::settings()` (`SettingsObject`), Qt `QVariant`

---

## Overview

The Application Settings API provides **read-only access to global
launcher settings**. Unlike the S2 Settings API, which manages
plugin-scoped key-value pairs that each plugin owns, S16 exposes the
application-wide configuration that controls MeshMC's own behavior.

Section 16 exposes a single function pointer in `MMCOContext`:

| Function pointer | Purpose |
|------------------|---------|
| `app_setting_get` | Read a global application setting by key |

This function serves as the foundation for:

- **Adaptive behavior** — plugins can inspect launcher settings like
  theme, language, Java path, or notification preferences and adjust
  their own behavior accordingly.
- **Conditional logic** — plugins can check whether certain launcher
  features are enabled before offering related functionality.
- **Diagnostic information** — plugins can gather launcher
  configuration for bug reports, debug logs, or compatibility checks.

### Why Read-Only?

Application settings are read-only for plugins by design. Allowing
plugins to modify global launcher settings would create serious
risks:

1. **Configuration corruption** — a buggy plugin could break the
   launcher's core functionality.
2. **Setting conflicts** — multiple plugins could fight over the
   same setting value.
3. **User trust** — users expect their launcher settings to change
   only through the settings UI, not through plugin side effects.

If your plugin needs persistent configuration, use the S2 Settings
API (`setting_get()` / `setting_set()`), which provides a separate,
sandboxed namespace per plugin.

---

## Function Pointer in `MMCOContext`

The application setting function pointer is declared in the
`MMCOContext` struct in `PluginAPI.h`:

```c
/* S16 — Application Settings (read-only global settings) */
const char* (*app_setting_get)(void* mh, const char* key);
```

It is wired in `PluginManager::buildContext()`:

```cpp
// S16 — Application Settings
ctx.app_setting_get = api_app_setting_get;
```

The static trampoline declaration lives in `PluginManager.h`:

```cpp
/* Section 16: Application Settings */
static const char* api_app_setting_get(void* mh, const char* key);
```

---

## Architecture Diagram

```text
┌────────────────────────────────────────────────────────────────┐
│               Application Settings API (S16)                   │
├────────────────────────────────────────────────────────────────┤
│                                                                │
│  ┌──────────── Read-Only Query ────────────────────────────┐   │
│  │  app_setting_get(mh, "JavaPath")                        │   │
│  │  → "/usr/lib/jvm/java-17/bin/java"                      │   │
│  │                                                         │   │
│  │  app_setting_get(mh, "Language")                        │   │
│  │  → "en_US"                                              │   │
│  │                                                         │   │
│  │  app_setting_get(mh, "NonExistentKey")                  │   │
│  │  → nullptr                                              │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                │
└────────────────────────────────────────────────────────────────┘

    Plugin code                        MeshMC internals
    ──────────                         ────────────────
    ctx->app_setting_get(mh, key) ──→  PluginManager::api_app_setting_get()
                                          │
                                          ├── r = rt(mh)
                                          ├── app = r->manager->m_app
                                          ├── null check: app, settings, key
                                          ├── QString qKey = fromUtf8(key)
                                          ├── contains(qKey)?
                                          │   └── No → return nullptr
                                          ├── QVariant val = get(qKey)
                                          ├── val.isValid()?
                                          │   └── No → return nullptr
                                          ├── r->tempString = val.toString()
                                          │                      .toStdString()
                                          └── return tempString.c_str()
```

---

## Distinction from S2 (Plugin Settings)

This is the most important distinction in the API. The two settings
APIs serve fundamentally different purposes:

| Aspect | S2 — Plugin Settings | S16 — Application Settings |
|--------|---------------------|---------------------------|
| **Scope** | Per-plugin namespace | Global launcher configuration |
| **Access** | Read + Write | Read-only |
| **Functions** | `setting_get()`, `setting_set()`, `setting_get_int()`, `setting_set_int()`, `setting_get_bool()`, `setting_set_bool()` | `app_setting_get()` only |
| **Storage** | Plugin-specific settings file | Global `meshmc.cfg` |
| **Isolation** | Each plugin has its own key space | All plugins see the same global settings |
| **Modification** | Plugin can freely modify its own settings | Plugins cannot modify application settings |
| **Persistence** | Survives application restarts | Managed by the launcher settings UI |
| **Key format** | Arbitrary plugin-chosen keys | MeshMC-defined setting keys |

### Example of the Distinction

```c
/* S2: Plugin's own setting (read-write, namespaced) */
const char* my_val = ctx->setting_get(mh, "my_plugin_option");
ctx->setting_set(mh, "my_plugin_option", "new_value");

/* S16: Launcher global setting (read-only) */
const char* java = ctx->app_setting_get(mh, "JavaPath");
/* ctx->app_setting_set() does NOT exist — this is intentional */
```

---

## Cross-References

| Section | Relationship |
|---------|-------------|
| **S1 (Lifecycle)** | `app_setting_get()` is safe to call inside `mmco_init()` and `mmco_unload()`. |
| **S2 (Settings)** | Plugin-scoped settings complement app settings. Use S2 for plugin configuration, S16 for reading launcher state. |
| **S14 (Utility)** | `get_app_version()` provides build-time identity; `app_setting_get()` provides runtime configuration. |
| **S15 (Launch Modifiers)** | Pre-launch hooks can read app settings (e.g. Java path) to make conditional launch modifications. |
| **S12 (UI Dialogs)** | Plugins can display app setting values in informational dialogs. |

---

## Sub-Pages

| Page | Functions documented |
|------|---------------------|
| [Application Setting Query](app-setting-get.md) | `app_setting_get()` |

---

## Quick Reference

```c
/* ── S16 function pointer signature ── */

/* Read-only global application setting query */
const char* (*app_setting_get)(void* mh, const char* key);
```

---

## Minimal Example

```c
#include "mmco_sdk.h"
#include <stdio.h>
#include <string.h>

MMCO_DEFINE_MODULE("SettingsReader", "1.0.0", "Example Author",
                   "Reads global launcher settings", "GPL-3.0-or-later");

static MMCOContext* g_ctx;

extern "C" int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;
    void* mh = ctx->module_handle;

    /* Read the configured Java path */
    const char* java = ctx->app_setting_get(mh, "JavaPath");
    if (java) {
        char java_path[512];
        snprintf(java_path, sizeof(java_path), "%s", java);
        /* java pointer is now safe from tempString clobbering */

        char buf[640];
        snprintf(buf, sizeof(buf), "Launcher Java path: %s", java_path);
        ctx->log_info(mh, buf);
    } else {
        ctx->log_info(mh, "No Java path configured in launcher settings");
    }

    /* Read the launcher language */
    const char* lang = ctx->app_setting_get(mh, "Language");
    if (lang) {
        char buf[128];
        snprintf(buf, sizeof(buf), "Launcher language: %s", lang);
        ctx->log_info(mh, buf);
    }

    return 0;
}

extern "C" void mmco_unload(MMCOContext* ctx)
{
    (void)ctx;
}
```
