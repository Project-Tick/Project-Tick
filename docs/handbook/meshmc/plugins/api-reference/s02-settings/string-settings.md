# String Settings — `setting_get()` and `setting_set()`

> **Header:** `PluginAPI.h` · **SDK:** `mmco_sdk.h` · **Implementation:** `PluginManager.cpp` (lines 410–443)

---

## Overview

These two function pointers are the **core** of the MMCO Settings API. They read and write string values in a persistent, per-plugin-namespaced key-value store backed by MeshMC's `SettingsObject`.

All setting values are strings. The API provides no typed variants — if you need integers or booleans, convert to/from string on the plugin side. See [Numeric & Boolean Patterns](numeric-settings.md) for recommended helpers.

---

## Function pointer declarations

From `MMCOContext` in `PluginAPI.h`:

```cpp
struct MMCOContext {
    /* ... ABI guard, module_handle, logging, hooks ... */

    /* S3 — Settings */
    const char* (*setting_get)(void* mh, const char* key);
    int         (*setting_set)(void* mh, const char* key, const char* value);

    /* ... remaining sections ... */
};
```

And identically in `mmco_sdk.h`:

```cpp
    /* S3 — Settings */
    const char* (*setting_get)(void* mh, const char* key);
    int (*setting_set)(void* mh, const char* key, const char* value);
```

---

<a id="setting_get"></a>
## `setting_get()`

### Signature

```cpp
const char* (*setting_get)(void* mh, const char* key);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | The module handle. **Must** be `ctx->module_handle`. This identifies the calling plugin to the runtime, which uses it to determine the namespace prefix and the `tempString` buffer to write into. |
| `key` | `const char*` | A null-terminated UTF-8 string containing the short (unqualified) setting key. Must not be `nullptr`. Must not contain the module namespace prefix — the runtime adds it automatically. Examples: `"interval"`, `"last_run"`, `"enabled"`. |

### Return value

| Condition | Returns |
|-----------|---------|
| Setting exists and has a valid value | `const char*` — a pointer to a null-terminated UTF-8 string containing the setting's current value. |
| Setting does not exist | `nullptr` |
| `Application` or `SettingsObject` not available | `nullptr` |

### Ownership

The returned `const char*` points into the `ModuleRuntime::tempString` buffer owned by the runtime. The pointer is valid **only until the next API call that returns `const char*`** on the same module. This includes calls to `setting_get()`, `instance_get_name()`, `instance_get_id()`, `fs_plugin_data_dir()`, `get_app_version()`, or any other string-returning function.

**You must copy the returned string if you need it beyond the next API call.**

### Thread safety

**Main thread only.** Must not be called from background threads, HTTP callbacks, or timer callbacks running on worker threads. The underlying `SettingsObject` is not thread-safe.

### Implementation

Full source from `PluginManager.cpp`:

```cpp
const char* PluginManager::api_setting_get(void* mh, const char* key)
{
    auto* r = rt(mh);
    auto* app = r->manager->m_app;
    if (!app || !app->settings())
        return nullptr;

    auto& meta = r->manager->m_modules[r->moduleIndex];
    QString fullKey = QString("plugin.%1.%2")
        .arg(meta.moduleId(), QString::fromUtf8(key));
    QVariant val = app->settings()->get(fullKey);
    if (!val.isValid())
        return nullptr;

    r->tempString = val.toString().toStdString();
    return r->tempString.c_str();
}
```

**Step-by-step:**

1. **Resolve runtime.** `rt(mh)` casts the opaque `void*` to `ModuleRuntime*`, giving access to the `PluginManager` instance and the module's index.

2. **Guard against null app.** If the `Application` singleton or its `SettingsObject` is null (which should not happen during normal operation), the function returns `nullptr`.

3. **Construct the full key.** The module's ID (derived from the `.mmco` filename) is combined with the caller's short key using the format `plugin.<module_id>.<key>`.

4. **Query SettingsObject.** `app->settings()->get(fullKey)` returns a `QVariant`. If the key has never been set, the `QVariant` is invalid.

5. **Handle missing keys.** If the `QVariant` is not valid, `nullptr` is returned.

6. **Store in tempString.** The `QVariant` is converted to `QString`, then to `std::string`, and stored in `r->tempString`. This ensures the data remains valid (the `QVariant` and `QString` would otherwise be temporaries destroyed at function exit).

7. **Return raw pointer.** `r->tempString.c_str()` gives a `const char*` that remains valid as long as `tempString` is not modified — i.e., until the next string-returning API call.

### Trampoline wiring

In `PluginManager::buildContext()`:

```cpp
ctx.setting_get = api_setting_get;
```

The function pointer in the `MMCOContext` struct is set to the static member function `PluginManager::api_setting_get`. Because it is a static function with C-compatible signature (`const char* (*)(void*, const char*)`), it can be stored directly in the C function-pointer field.

### Error conditions

| Condition | Behavior |
|-----------|----------|
| `key` is `nullptr` | Undefined behavior. `QString::fromUtf8(nullptr)` is implementation-defined in Qt. Do not pass `nullptr`. |
| `key` is empty string `""` | Returns the value of `plugin.<module_id>.` (with trailing dot). This is technically valid but should be avoided. |
| Key was never set | Returns `nullptr`. |
| Key was set and later the settings file was edited externally to remove it | Returns `nullptr` (the `QVariant` from `get()` will be invalid). |
| Application is shutting down | May return `nullptr` if the settings subsystem has already been torn down. |

---

### Example: basic retrieval

```cpp
void* h = g_ctx->module_handle;

const char* raw = g_ctx->setting_get(h, "username");
if (raw) {
    char msg[256];
    snprintf(msg, sizeof(msg), "Welcome back, %s!", raw);
    g_ctx->log_info(h, msg);
} else {
    g_ctx->log_info(h, "No username set — using default.");
}
```

### Example: safe copy before next API call

```cpp
void* h = g_ctx->module_handle;

// Retrieve two settings — must copy the first before the second call
const char* raw_host = g_ctx->setting_get(h, "server_host");
std::string host = raw_host ? raw_host : "localhost";

const char* raw_port = g_ctx->setting_get(h, "server_port");
std::string port = raw_port ? raw_port : "8080";

char msg[512];
snprintf(msg, sizeof(msg), "Connecting to %s:%s", host.c_str(), port.c_str());
g_ctx->log_info(h, msg);
```

### Example: checking for first run

```cpp
void* h = g_ctx->module_handle;

const char* initialized = g_ctx->setting_get(h, "initialized");
if (!initialized) {
    g_ctx->log_info(h, "First run — performing initial setup...");
    perform_first_run_setup();
    g_ctx->setting_set(h, "initialized", "1");
} else {
    g_ctx->log_debug(h, "Already initialized, skipping setup.");
}
```

---

### String ownership and safe usage

Because every string-returning API shares the same `tempString` buffer within a module, you **must** follow this discipline:

#### Rule 1: Copy before calling another API function

```cpp
// WRONG — 'name' is invalidated by setting_get()
const char* name = g_ctx->instance_get_name(h, instance_id);
const char* val  = g_ctx->setting_get(h, "theme");
printf("Instance %s, theme %s\n", name, val);  // 'name' is STALE

// CORRECT
const char* raw_name = g_ctx->instance_get_name(h, instance_id);
std::string name = raw_name ? raw_name : "";

const char* raw_val = g_ctx->setting_get(h, "theme");
std::string val = raw_val ? raw_val : "";

char msg[512];
snprintf(msg, sizeof(msg), "Instance %s, theme %s", name.c_str(), val.c_str());
g_ctx->log_info(h, msg);
```

#### Rule 2: Do not store the raw pointer long-term

```cpp
// WRONG — storing raw pointer in a global
static const char* g_cached_theme = nullptr;

void init() {
    g_cached_theme = g_ctx->setting_get(h, "theme");
    // g_cached_theme becomes dangling as soon as any other API call happens
}

// CORRECT — store a copy
static std::string g_cached_theme;

void init() {
    const char* raw = g_ctx->setting_get(h, "theme");
    g_cached_theme = raw ? raw : "default";
}
```

#### Rule 3: Null-check before use

`setting_get()` returns `nullptr` for non-existent keys. Always check:

```cpp
const char* raw = g_ctx->setting_get(h, "missing_key");
// raw is nullptr here — dereferencing it is UB
std::string value = raw ? raw : "default_value";
```

#### Rule 4: The pointer is valid for exactly one call

Even calling `setting_get()` twice with the **same** key invalidates the first pointer:

```cpp
const char* a = g_ctx->setting_get(h, "key");
const char* b = g_ctx->setting_get(h, "key");
// 'a' and 'b' point to the same buffer — but 'a' was technically
// invalidated by the second call. They happen to hold the same value
// in this case, but relying on this is fragile.
```

---

<a id="setting_set"></a>
## `setting_set()`

### Signature

```cpp
int (*setting_set)(void* mh, const char* key, const char* value);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | The module handle. **Must** be `ctx->module_handle`. |
| `key` | `const char*` | A null-terminated UTF-8 string containing the short setting key. Must not be `nullptr`. The runtime prepends `plugin.<module_id>.` automatically. |
| `value` | `const char*` | A null-terminated UTF-8 string containing the value to store. Must not be `nullptr`. Any UTF-8 string is valid, including the empty string `""`. |

### Return value

| Condition | Returns |
|-----------|---------|
| Success | `0` |
| `Application` or `SettingsObject` not available | `-1` |

### Ownership

The `key` and `value` strings are **copied** into the settings backend. The plugin retains ownership of the input strings and may free or modify them immediately after `setting_set()` returns. The runtime does not hold any references to plugin-provided input strings.

### Thread safety

**Main thread only.** The same constraint as `setting_get()`. The `SettingsObject` is not thread-safe.

### Implementation

Full source from `PluginManager.cpp`:

```cpp
int PluginManager::api_setting_set(void* mh, const char* key, const char* value)
{
    auto* r = rt(mh);
    auto* app = r->manager->m_app;
    if (!app || !app->settings())
        return -1;

    auto& meta = r->manager->m_modules[r->moduleIndex];
    QString fullKey = QString("plugin.%1.%2")
        .arg(meta.moduleId(), QString::fromUtf8(key));
    app->settings()->set(fullKey, QString::fromUtf8(value));
    return 0;
}
```

**Step-by-step:**

1. **Resolve runtime.** Same as `setting_get()`: `rt(mh)` casts to `ModuleRuntime*`.

2. **Guard against null app.** Returns `-1` if the application or settings subsystem is unavailable.

3. **Construct the full key.** Identical namespace construction: `plugin.<module_id>.<key>`.

4. **Write to SettingsObject.** `app->settings()->set(fullKey, QVariant)` stores the value. The `QString` value is implicitly wrapped in a `QVariant`.

5. **Return success.** Always returns `0` when the app and settings are available. The underlying `SettingsObject::set()` may log an error if the key was not previously registered, but the API call still returns `0`.

### Trampoline wiring

In `PluginManager::buildContext()`:

```cpp
ctx.setting_set = api_setting_set;
```

### Error conditions

| Condition | Behavior |
|-----------|----------|
| `key` is `nullptr` | Undefined behavior. Do not pass `nullptr`. |
| `value` is `nullptr` | Undefined behavior. `QString::fromUtf8(nullptr)` is implementation-defined. Use `""` for empty values. |
| Key was never previously read or written | The value is stored successfully. `SettingsObject` creates the key on first write if the setting's `Setting` object was pre-registered. For dynamically created plugin keys, the `SettingsObject::set()` call may log a `qCritical` warning but the function still returns `0`. |
| Settings file is read-only on disk | The value is set in memory but may not persist to disk. No error is returned to the plugin. |
| Application shutting down | May return `-1` if the settings subsystem was already torn down. |

### Side effects

- **Triggers hook dispatch.** When a setting value changes, the `MMCO_HOOK_SETTINGS_CHANGED` (0x0300) hook fires. The payload is an `MMCOSettingChange*` with the full namespaced key, old value, and new value. See [README.md](README.md#the-settings_changed-hook) for details.

- **Disk write.** The value is persisted to the settings file (INI on Linux/macOS, Registry on Windows). The write is synchronous — when `setting_set()` returns, the data is on disk.

- **No `tempString` mutation.** Unlike `setting_get()`, calling `setting_set()` does **not** invalidate the `tempString` buffer. You can safely call `setting_set()` without worrying about previously returned `const char*` pointers becoming stale:

  ```cpp
  const char* old = g_ctx->setting_get(h, "counter");
  // 'old' is valid here
  g_ctx->setting_set(h, "other_key", "some_value");
  // 'old' is STILL valid — setting_set() doesn't touch tempString
  ```

---

### Example: basic write

```cpp
void* h = g_ctx->module_handle;

int rc = g_ctx->setting_set(h, "last_run", "2026-04-15T10:30:00Z");
if (rc != 0) {
    g_ctx->log_error(h, "Failed to save last_run setting");
}
```

### Example: store and immediately read back

```cpp
void* h = g_ctx->module_handle;

g_ctx->setting_set(h, "greeting", "Hello, MeshMC!");

const char* val = g_ctx->setting_get(h, "greeting");
// val is guaranteed to be "Hello, MeshMC!" (not nullptr)

char msg[256];
snprintf(msg, sizeof(msg), "Stored and retrieved: %s", val);
g_ctx->log_info(h, msg);
```

### Example: increment a counter

```cpp
void* h = g_ctx->module_handle;

// Read current value
const char* raw = g_ctx->setting_get(h, "launch_count");
int count = raw ? atoi(raw) : 0;
count++;

// Write back
char buf[32];
snprintf(buf, sizeof(buf), "%d", count);
g_ctx->setting_set(h, "launch_count", buf);

char msg[128];
snprintf(msg, sizeof(msg), "Launch #%d", count);
g_ctx->log_info(h, msg);
```

### Example: store a comma-separated list

```cpp
void* h = g_ctx->module_handle;

// Build a comma-separated list of instance IDs
std::string list;
int n = g_ctx->instance_count(h);
for (int i = 0; i < n; ++i) {
    const char* id_raw = g_ctx->instance_get_id(h, i);
    std::string id = id_raw ? id_raw : "";
    if (!id.empty()) {
        if (!list.empty()) list += ",";
        list += id;
    }
}

g_ctx->setting_set(h, "known_instances", list.c_str());
```

### Example: configuration with defaults

A common pattern is to provide defaults in a helper function:

```cpp
static std::string get_config(MMCOContext* ctx, const char* key,
                              const char* default_value)
{
    const char* raw = ctx->setting_get(ctx->module_handle, key);
    return raw ? std::string(raw) : std::string(default_value);
}

static void set_config(MMCOContext* ctx, const char* key,
                       const char* value)
{
    ctx->setting_set(ctx->module_handle, key, value);
}

// Usage:
std::string theme   = get_config(g_ctx, "theme", "dark");
std::string lang    = get_config(g_ctx, "language", "en");
std::string timeout = get_config(g_ctx, "timeout_seconds", "30");
```

---

## Patterns and recipes

### Pattern: first-time initialisation with defaults

```cpp
struct ConfigDefaults {
    const char* key;
    const char* value;
};

static const ConfigDefaults DEFAULTS[] = {
    { "backup_interval_minutes", "60" },
    { "max_backups",             "10" },
    { "compress_backups",        "1"  },
    { "notify_on_backup",        "1"  },
    { nullptr, nullptr }
};

static void ensure_defaults(MMCOContext* ctx)
{
    void* h = ctx->module_handle;
    for (int i = 0; DEFAULTS[i].key; ++i) {
        const char* existing = ctx->setting_get(h, DEFAULTS[i].key);
        if (!existing) {
            ctx->setting_set(h, DEFAULTS[i].key, DEFAULTS[i].value);
        }
    }
}
```

Call `ensure_defaults(g_ctx)` from your `mmco_init()` or from the `MMCO_HOOK_APP_INITIALIZED` callback.

### Pattern: configuration migration

When updating your plugin to a new version, you may need to rename or transform settings:

```cpp
static void migrate_settings(MMCOContext* ctx)
{
    void* h = ctx->module_handle;

    // v1.0 used "backup_interval" (seconds), v2.0 uses "backup_interval_minutes"
    const char* old_val = ctx->setting_get(h, "backup_interval");
    if (old_val) {
        std::string old_str(old_val);  // Copy! Next API call will invalidate.
        int seconds = atoi(old_str.c_str());
        int minutes = seconds / 60;
        if (minutes < 1) minutes = 1;

        char buf[32];
        snprintf(buf, sizeof(buf), "%d", minutes);
        ctx->setting_set(h, "backup_interval_minutes", buf);

        // Clear old key by setting to empty (cannot truly delete)
        ctx->setting_set(h, "backup_interval", "");

        ctx->log_info(h, "Migrated backup_interval to backup_interval_minutes");
    }
}
```

### Pattern: JSON-serialised settings

For complex structured data, serialise to JSON and store as a single string:

```cpp
#include <QJsonDocument>
#include <QJsonObject>

static void save_plugin_config(MMCOContext* ctx,
                               const QJsonObject& config)
{
    QJsonDocument doc(config);
    std::string json = doc.toJson(QJsonDocument::Compact).toStdString();
    ctx->setting_set(ctx->module_handle, "config_json", json.c_str());
}

static QJsonObject load_plugin_config(MMCOContext* ctx)
{
    const char* raw = ctx->setting_get(ctx->module_handle, "config_json");
    if (!raw)
        return QJsonObject();

    QJsonDocument doc = QJsonDocument::fromJson(QByteArray(raw));
    return doc.isObject() ? doc.object() : QJsonObject();
}
```

> **Note:** The SDK header (`mmco_sdk.h`) includes `<QJsonObject>` and `QJsonDocument` is available through the Qt includes chain, so these types are accessible.

### Pattern: per-instance settings

The settings API is not instance-scoped — all keys live under `plugin.<module_id>.*`. To store per-instance configuration, embed the instance ID in the key:

```cpp
static void set_instance_setting(MMCOContext* ctx,
                                 const char* instance_id,
                                 const char* key,
                                 const char* value)
{
    std::string compound_key = std::string("inst.") + instance_id + "." + key;
    ctx->setting_set(ctx->module_handle, compound_key.c_str(), value);
}

static std::string get_instance_setting(MMCOContext* ctx,
                                        const char* instance_id,
                                        const char* key,
                                        const char* default_value)
{
    std::string compound_key = std::string("inst.") + instance_id + "." + key;
    const char* raw = ctx->setting_get(ctx->module_handle, compound_key.c_str());
    return raw ? std::string(raw) : std::string(default_value);
}

// Stored as: plugin.MyPlugin.inst.<instance_id>.<key>
// Example:   plugin.MyPlugin.inst.abc123.auto_backup = "1"
```

---

## Gotchas and FAQ

### Q: Can I delete a setting?

No. The MMCO Settings API does not expose a delete or reset function. You can set a key to `""` (empty string) to effectively clear it, but the key remains in the settings file. To check whether a key has been "deleted", treat both `nullptr` (never set) and `""` (cleared) as absent:

```cpp
const char* raw = g_ctx->setting_get(h, "key");
bool has_value = raw && raw[0] != '\0';
```

### Q: What happens if two plugins use the same key name?

Nothing — keys are namespaced. Plugin A's `"theme"` is stored as `plugin.PluginA.theme`, and Plugin B's `"theme"` is `plugin.PluginB.theme`. They are completely independent.

### Q: Is there a maximum key or value length?

Not enforced by the API, but the underlying `QSettings` backend imposes platform limits (typically 16 KB for values on Windows Registry, effectively unlimited on INI). Keep values reasonable.

### Q: Can I store binary data?

Not directly. The API works with `const char*` (null-terminated UTF-8 strings). To store binary data, encode it as Base64 first:

```cpp
#include <QByteArray>

// Store binary
QByteArray binary = /* ... */;
std::string encoded = binary.toBase64().toStdString();
g_ctx->setting_set(h, "binary_data", encoded.c_str());

// Retrieve binary
const char* raw = g_ctx->setting_get(h, "binary_data");
if (raw) {
    QByteArray decoded = QByteArray::fromBase64(QByteArray(raw));
    // Use decoded...
}
```

For large binary data, use the [Filesystem API (S10)](../s10-filesystem/) instead.

### Q: Do settings persist if the plugin is unloaded?

Yes. Settings are stored in MeshMC's global settings file. Unloading or removing a plugin does not delete its settings. If the plugin is reinstalled later, its settings are still there.

---

## Cross-references

- [Section 02 — Overview](README.md) — Namespacing, storage backend, `SETTINGS_CHANGED` hook
- [Numeric & Boolean Patterns](numeric-settings.md) — Convenience helpers for `int` and `bool` settings
- [Section 01 — Module Handle](../s01-lifecycle/module-handle.md) — `ModuleRuntime`, `tempString`, the opaque handle pattern
- [Section 01 — Hooks](../s01-lifecycle/hooks.md) — `hook_register()`, `MMCO_HOOK_SETTINGS_CHANGED`
- [Section 10 — Filesystem](../s10-filesystem/) — Alternative for large or binary data
- [SDK Guide](../../sdk-guide/) — Build setup, `mmco_sdk.h` include chain
