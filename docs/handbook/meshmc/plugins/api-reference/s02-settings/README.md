# Section 02 — Settings API

> **Header:** `PluginAPI.h` (function pointer declarations) · **SDK:** `mmco_sdk.h` (struct definition) · **Implementation:** `PluginManager.cpp` (static trampolines)

---

## Overview

The MMCO Settings API gives plugins a **persistent key-value store** that survives application restarts. Every plugin receives its own namespace — keys written by one plugin are invisible to others, and collisions between plugins are impossible by design.

The API surface is deliberately minimal: two function pointers in `MMCOContext` handle all setting operations.

```cpp
/* S3 — Settings  (from MMCOContext in PluginAPI.h) */
const char* (*setting_get)(void* mh, const char* key);
int         (*setting_set)(void* mh, const char* key, const char* value);
```

All values are stored and retrieved as **strings**. Plugins that need integer or boolean settings must convert to and from string representation. [Numeric & Boolean Patterns](numeric-settings.md) documents recommended helper functions for these conversions.

---

## Function summary

| Function | Purpose | Returns |
|----------|---------|---------|
| [`setting_get()`](string-settings.md#setting_get) | Retrieve a setting value by key | `const char*` (nullable) |
| [`setting_set()`](string-settings.md#setting_set) | Store a setting value by key | `int` (0 = success) |

---

## Key namespacing

Plugins pass **short, unqualified keys** like `"last_run"`, `"backup_count"`, or `"theme"`. The runtime automatically prepends a namespace prefix before the key reaches the underlying storage.

### Namespace format

```
plugin.<module_id>.<key>
```

Where `<module_id>` is derived from the `.mmco` filename by stripping the path and the `.mmco` extension. For example:

| `.mmco` filename | `module_id` | Full key for `"interval"` |
|------------------|-------------|---------------------------|
| `BackupSystem.mmco` | `BackupSystem` | `plugin.BackupSystem.interval` |
| `hello_world.mmco` | `hello_world` | `plugin.hello_world.interval` |
| `my-plugin.mmco` | `my-plugin` | `plugin.my-plugin.interval` |

### How namespacing is implemented

In `PluginManager.cpp`, both `api_setting_get()` and `api_setting_set()` resolve the module metadata from the opaque handle and construct the full key:

```cpp
auto& meta = r->manager->m_modules[r->moduleIndex];
QString fullKey = QString("plugin.%1.%2")
    .arg(meta.moduleId(), QString::fromUtf8(key));
```

The `PluginMetadata::moduleId()` function computes the module identifier by stripping the file path and extension:

```cpp
QString moduleId() const {
    QString base = filePath.section('/', -1);
    if (base.endsWith(MMCO_EXTENSION))
        base.chop(static_cast<int>(strlen(MMCO_EXTENSION)));
    return base;
}
```

### Consequences of namespacing

1. **Plugins cannot read each other's settings.** There is no API to query an arbitrary full key — the prefix is always injected by the runtime.

2. **Key names should be short and descriptive.** The namespace is prepended automatically, so `"interval"` is sufficient — there is no need to write `"BackupSystem.interval"`.

3. **Renaming a `.mmco` file changes the namespace.** If you rename `BackupSystem.mmco` to `Backups.mmco`, all previously stored settings under `plugin.BackupSystem.*` become inaccessible. The old values remain in the settings file but are orphaned.

4. **The `plugin.` prefix prevents collisions with MeshMC's own settings.** All launcher-internal settings live in different key spaces (e.g., `JavaPath`, `Language`, `Theme`).

---

## Storage backend

### SettingsObject

Underneath the API, settings are stored in MeshMC's `SettingsObject` subsystem. This is a Qt-based key-value store that wraps `QSettings` (INI format on Linux/macOS, Registry on Windows).

The call chain for `setting_set(mh, "key", "value")` is:

```
Plugin: ctx->setting_set(ctx->module_handle, "key", "value")
    │
    ▼
PluginManager::api_setting_set(void* mh, ...)     ← static trampoline
    │
    ├─ ModuleRuntime* r = static_cast<…>(mh)
    ├─ Application* app = r->manager->m_app
    ├─ fullKey = "plugin.<module_id>.key"
    └─ app->settings()->set(fullKey, QVariant("value"))
             │
             ▼
         SettingsObject::set(const QString& id, QVariant value)
             │
             ▼
         Setting::set(QVariant value)  → persisted to disk
```

The call chain for `setting_get(mh, "key")` is:

```
Plugin: ctx->setting_get(ctx->module_handle, "key")
    │
    ▼
PluginManager::api_setting_get(void* mh, ...)     ← static trampoline
    │
    ├─ ModuleRuntime* r = static_cast<…>(mh)
    ├─ Application* app = r->manager->m_app
    ├─ fullKey = "plugin.<module_id>.key"
    ├─ QVariant val = app->settings()->get(fullKey)
    ├─ if (!val.isValid()) return nullptr
    ├─ r->tempString = val.toString().toStdString()
    └─ return r->tempString.c_str()
```

### File location

On each platform, MeshMC's settings file is typically located at:

| Platform | Location |
|----------|----------|
| **Linux** | `~/.local/share/meshmc/meshmc.conf` (INI format) |
| **macOS** | `~/Library/Application Support/meshmc/meshmc.conf` |
| **Windows** | Registry: `HKEY_CURRENT_USER\Software\MeshMC\meshmc` or `%APPDATA%\meshmc\meshmc.conf` |

Plugin settings are stored in the same file alongside all other MeshMC settings. They are distinguished only by the `plugin.` prefix.

### Persistence guarantees

- **Writes are immediate.** `setting_set()` calls `Setting::set()` which triggers a save to disk (via `QSettings::sync()` internally). There is no explicit flush or commit step.
- **Reads reflect the latest write.** A `setting_get()` after a `setting_set()` for the same key will always return the new value.
- **Cross-session persistence.** Values persist across application restarts. Plugin settings are loaded as part of the global settings file at startup.

---

## String ownership and lifetime

The `setting_get()` function returns a `const char*` whose storage is managed by the runtime. Understanding this ownership model is **critical** to avoid use-after-free bugs.

### The `tempString` mechanism

Each loaded module has a `ModuleRuntime` struct, which includes a `std::string tempString` field:

```cpp
struct ModuleRuntime {
    PluginManager* manager;
    int            moduleIndex;
    std::string    dataDir;
    std::string    tempString;   // ← shared return buffer
};
```

When `api_setting_get()` (or any other string-returning API function) is called, the result is stored in `r->tempString` and a pointer to its internal buffer is returned:

```cpp
r->tempString = val.toString().toStdString();
return r->tempString.c_str();
```

### Validity rule

> **The returned `const char*` is valid until the next API call on the same module.**

Any subsequent call to **any** function pointer in `MMCOContext` that returns `const char*` will overwrite `tempString`. This means:

```cpp
// DANGEROUS — the first pointer is invalidated by the second call
const char* a = ctx->setting_get(MMCO_MH, "key_a");
const char* b = ctx->setting_get(MMCO_MH, "key_b");
// 'a' now points to the value of "key_b" (or garbage)!
```

```cpp
// SAFE — copy the value before the next call
const char* raw = ctx->setting_get(MMCO_MH, "key_a");
std::string a = raw ? raw : "";

raw = ctx->setting_get(MMCO_MH, "key_b");
std::string b = raw ? raw : "";
```

See [String Settings](string-settings.md#string-ownership-and-safe-usage) for detailed examples and patterns.

---

## Thread safety

All settings functions **must be called from the main (GUI) thread**. The `SettingsObject` backend is not thread-safe and is accessed without locking. Calling `setting_get()` or `setting_set()` from a background thread (e.g., from inside an HTTP callback) results in undefined behaviour.

If you need to read or write settings from a callback that may execute on a worker thread, copy the value into your own thread-safe storage from the main thread before the async operation begins.

---

## The SETTINGS_CHANGED hook

When a setting value changes (from any source — plugin, user via the UI, or another plugin), the `MMCO_HOOK_SETTINGS_CHANGED` (0x0300) hook fires. The payload is a pointer to an `MMCOSettingChange` struct:

```cpp
struct MMCOSettingChange {
    const char* key;        // The full namespaced key, e.g. "plugin.BackupSystem.interval"
    const char* old_value;  // Previous value as string (may be nullptr)
    const char* new_value;  // New value as string
};
```

Plugins can use this hook to react to configuration changes without polling. Note that the `key` field contains the **full namespaced key**, not the short key passed to `setting_set()`.

```cpp
static int on_settings_changed(void* mh, uint32_t hook_id,
                                void* payload, void* user_data)
{
    auto* change = static_cast<MMCOSettingChange*>(payload);
    // Check if this is one of our keys
    if (change->key && strstr(change->key, "plugin.BackupSystem.") == change->key) {
        char buf[256];
        snprintf(buf, sizeof(buf), "Setting changed: %s = %s",
                 change->key, change->new_value ? change->new_value : "(null)");
        g_ctx->log_info(g_ctx->module_handle, buf);
    }
    return 0; // Don't cancel
}
```

See [Section 01 — Hooks](../s01-lifecycle/hooks.md) for full hook registration details.

---

## Quick-start example

```cpp
#include "mmco_sdk.h"
#include <cstring>
#include <cstdlib>

static MMCOContext* g_ctx = nullptr;

MMCO_DEFINE_MODULE(
    "Settings Demo",
    "1.0.0",
    "Project Tick",
    "Demonstrates the Settings API",
    "GPL-3.0-or-later"
);

static int on_init(void* mh, uint32_t, void*, void*)
{
    void* h = g_ctx->module_handle;

    // Read a setting (returns nullptr if not set)
    const char* raw = g_ctx->setting_get(h, "run_count");
    int count = raw ? atoi(raw) : 0;
    count++;

    // Write it back
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", count);
    g_ctx->setting_set(h, "run_count", buf);

    // Log
    char msg[128];
    snprintf(msg, sizeof(msg), "This plugin has been initialised %d time(s)", count);
    g_ctx->log_info(h, msg);

    return 0;
}

extern "C" MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;
    g_ctx->hook_register(ctx->module_handle,
                         MMCO_HOOK_APP_INITIALIZED,
                         on_init, nullptr);
    return 0;
}

extern "C" MMCO_EXPORT void mmco_unload() {}
```

---

## Files in this section

| File | Contents |
|------|----------|
| [string-settings.md](string-settings.md) | `setting_get()` and `setting_set()` — full reference, parameter tables, examples, error handling |
| [numeric-settings.md](numeric-settings.md) | Integer and boolean helper patterns — `setting_get_int()`, `setting_set_int()`, `setting_get_bool()`, `setting_set_bool()` convenience wrappers |

---

## Cross-references

- **Section 01 — Lifecycle, Logging & Hooks:** [s01-lifecycle/](../s01-lifecycle/) — Hook registration for `MMCO_HOOK_SETTINGS_CHANGED`
- **Section 01 — Module Handle:** [module-handle.md](../s01-lifecycle/module-handle.md) — The `ModuleRuntime` struct and `tempString` mechanism
- **MMCO Format:** [mmco-format.md](../../mmco-format.md) — Module ID derivation from filename
- **SDK Guide:** [sdk-guide/](../../sdk-guide/) — Build setup and `mmco_sdk.h` include chain
