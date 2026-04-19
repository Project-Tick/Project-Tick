# Application Setting Query — `app_setting_get()` {#app-setting-get}

> **Section:** S16 (Application Settings) · **Header:** `PluginAPI.h` · **Trampoline:** `PluginManager::api_app_setting_get` · **Backend:** `Application::settings()` (`SettingsObject`), Qt `QVariant`

---

## `app_setting_get()` — Read a global application setting {#app_setting_get}

### Synopsis

```c
const char* app_setting_get(void* mh, const char* key);
```

Reads a global MeshMC application setting by key and returns its
value as a string. This is a **read-only** function — there is no
corresponding `app_setting_set()`. Application settings are managed
exclusively through the launcher's Settings UI.

### Parameters

| # | Name | Type | Description |
|---|------|------|-------------|
| 1 | `mh` | `void*` | Module handle. Pass `ctx->module_handle`. |
| 2 | `key` | `const char*` | The setting key to query. Must be non-null. UTF-8 encoded, null-terminated. |

#### Parameter Constraints

- **`key`**: Must not be `nullptr`. Must be a valid MeshMC setting
  key (see [Known Setting Keys](#known-setting-keys) below). If the
  key does not exist in the settings, the function returns `nullptr`.
- The API copies the key internally (via `QString::fromUtf8()`). The
  caller retains ownership and may free or modify the string after
  the call returns.

### Return Value

| Condition | Value |
|-----------|-------|
| Setting exists and has a valid value | Non-null pointer to a null-terminated UTF-8 string |
| `key` is `nullptr` | `nullptr` |
| Application singleton is unavailable | `nullptr` |
| `SettingsObject` is null | `nullptr` |
| Key does not exist in settings | `nullptr` |
| Value is invalid (`QVariant::isValid()` returns false) | `nullptr` |

### Implementation

```cpp
const char* PluginManager::api_app_setting_get(void* mh, const char* key)
{
    auto* r = rt(mh);
    auto* app = r->manager->m_app;
    if (!app || !app->settings() || !key)
        return nullptr;

    QString qKey = QString::fromUtf8(key);
    if (!app->settings()->contains(qKey))
        return nullptr;

    QVariant val = app->settings()->get(qKey);
    if (!val.isValid())
        return nullptr;

    r->tempString = val.toString().toStdString();
    return r->tempString.c_str();
}
```

The implementation:
1. Resolves the `ModuleRuntime*` from the opaque module handle.
2. Obtains the `Application` pointer via `r->manager->m_app`.
3. Performs a three-way null check: the `Application` pointer, its
   `settings()` object, and the `key` parameter. Returns `nullptr`
   if any is null.
4. Converts the key to `QString` using `fromUtf8()`.
5. Checks if the key exists in the `SettingsObject` via `contains()`.
   Returns `nullptr` if the key is not found.
6. Retrieves the value as a `QVariant` via `get()`.
7. Validates the `QVariant` with `isValid()`. Returns `nullptr` if
   the value is invalid.
8. Converts the `QVariant` to `QString` via `toString()`, then to
   `std::string` via `toStdString()`, storing the result in
   `ModuleRuntime::tempString`.
9. Returns `tempString.c_str()`.

### QVariant to String Conversion

Since all application settings are returned as strings (regardless
of their native type), the `QVariant::toString()` conversion rules
apply:

| Native type | `toString()` result | Example |
|-------------|-------------------|---------|
| `QString` | Returned as-is | `"en_US"` |
| `int` | Decimal string | `"17"` |
| `bool` | `"true"` or `"false"` | `"true"` |
| `double` | Decimal string | `"3.14"` |
| `QStringList` | Undefined (first element or empty) | — |
| `QByteArray` | UTF-8 interpretation | — |

For boolean settings, the returned string will be `"true"` or
`"false"`. If you need a boolean value, compare the string:

```c
const char* val = ctx->app_setting_get(mh, "AutoUpdate");
if (val && strcmp(val, "true") == 0) {
    /* Auto-update is enabled */
}
```

For integer settings, parse the returned string:

```c
const char* val = ctx->app_setting_get(mh, "MinMemAlloc");
if (val) {
    int min_mem = atoi(val);
    /* min_mem now holds the integer value */
}
```

### String Ownership

The returned pointer points into `ModuleRuntime::tempString`. It is
**invalidated** by the next API call on the same module that writes
to `tempString`.

| Property | Value |
|----------|-------|
| Backing storage | `ModuleRuntime::tempString` (`std::string`) |
| Valid until | Next `tempString`-writing API call on this module |
| Caller must free? | No. Do **not** call `free()` on this pointer. |
| Thread safety | Single-threaded only (all MMCO API calls). |

**Always copy the string if you need it beyond the next API call:**

```c
const char* raw = ctx->app_setting_get(ctx->module_handle, "JavaPath");
if (raw) {
    char java_path[512];
    snprintf(java_path, sizeof(java_path), "%s", raw);
    /* 'java_path' is now safe to use after further API calls */
}
```

---

## Known Setting Keys {#known-setting-keys}

The following is a reference of commonly available application
setting keys. The exact set depends on the MeshMC version and
build configuration. Keys are case-sensitive.

### Java & Memory

| Key | Type | Description | Example value |
|-----|------|-------------|---------------|
| `JavaPath` | `QString` | Path to the Java executable | `"/usr/lib/jvm/java-17/bin/java"` |
| `MinMemAlloc` | `int` | Minimum JVM heap allocation (MB) | `"512"` |
| `MaxMemAlloc` | `int` | Maximum JVM heap allocation (MB) | `"4096"` |
| `PermGen` | `int` | PermGen size (MB, legacy JVMs) | `"256"` |
| `JvmArgs` | `QString` | Custom JVM arguments | `"-XX:+UseG1GC"` |

### Appearance

| Key | Type | Description | Example value |
|-----|------|-------------|---------------|
| `IconTheme` | `QString` | Current icon theme name | `"pe_colored"` |
| `ApplicationTheme` | `QString` | Current application theme | `"system"` |
| `Language` | `QString` | UI language code | `"en_US"` |

### Network

| Key | Type | Description | Example value |
|-----|------|-------------|---------------|
| `ProxyType` | `int` | Proxy type (0=None, 1=SOCKS5, 2=HTTP) | `"0"` |
| `ProxyAddr` | `QString` | Proxy server address | `"proxy.example.com"` |
| `ProxyPort` | `int` | Proxy server port | `"8080"` |
| `ProxyUser` | `QString` | Proxy authentication username | `"user"` |

### Instance Defaults

| Key | Type | Description | Example value |
|-----|------|-------------|---------------|
| `InstanceDir` | `QString` | Default instance directory path | `"instances"` |
| `CentralModsDir` | `QString` | Shared mods directory path | `"mods"` |
| `AutoCloseConsole` | `bool` | Close console on instance exit | `"false"` |
| `ShowConsole` | `bool` | Show console on instance launch | `"true"` |

### Updates

| Key | Type | Description | Example value |
|-----|------|-------------|---------------|
| `AutoUpdate` | `bool` | Enable automatic update checks | `"true"` |
| `UpdateChannel` | `QString` | Update channel (stable/develop) | `"stable"` |

### Miscellaneous

| Key | Type | Description | Example value |
|-----|------|-------------|---------------|
| `Analytics` | `bool` | Whether analytics are enabled | `"false"` |
| `LastUsedInstanceId` | `QString` | ID of the last used instance | `"1a2b3c"` |
| `WrapperCommand` | `QString` | Global wrapper command | `""` |

> **Note:** This list is not exhaustive. New settings may be added
> in future MeshMC versions. Use `app_setting_get()` to probe for
> keys — it safely returns `nullptr` for unknown keys.

---

## Use Cases

### Reading Java Configuration

```c
static void log_java_config(MMCOContext* ctx)
{
    void* mh = ctx->module_handle;

    const char* java = ctx->app_setting_get(mh, "JavaPath");
    if (java) {
        char path[512];
        snprintf(path, sizeof(path), "%s", java);

        const char* min_raw = ctx->app_setting_get(mh, "MinMemAlloc");
        int min_mem = min_raw ? atoi(min_raw) : 0;

        const char* max_raw = ctx->app_setting_get(mh, "MaxMemAlloc");
        int max_mem = max_raw ? atoi(max_raw) : 0;

        char buf[640];
        snprintf(buf, sizeof(buf),
                 "Java: %s | Memory: %d-%d MB",
                 path, min_mem, max_mem);
        ctx->log_info(mh, buf);
    } else {
        ctx->log_warn(mh, "No Java path configured");
    }
}
```

### Adaptive UI Based on Theme

Adjust plugin UI behavior based on the current application theme:

```c
static int is_dark_theme(MMCOContext* ctx)
{
    const char* theme = ctx->app_setting_get(ctx->module_handle,
                                              "ApplicationTheme");
    if (!theme)
        return 0;

    /* Check for known dark themes */
    if (strstr(theme, "dark") != NULL ||
        strstr(theme, "Dark") != NULL) {
        return 1;
    }
    return 0;
}
```

### Language-Aware Plugins

Detect the launcher language to provide localized plugin messages:

```c
static const char* get_greeting(MMCOContext* ctx)
{
    const char* lang = ctx->app_setting_get(ctx->module_handle,
                                             "Language");
    if (!lang)
        return "Hello";

    char lang_code[16];
    snprintf(lang_code, sizeof(lang_code), "%s", lang);

    if (strncmp(lang_code, "tr", 2) == 0)
        return "Merhaba";
    if (strncmp(lang_code, "de", 2) == 0)
        return "Hallo";
    if (strncmp(lang_code, "fr", 2) == 0)
        return "Bonjour";
    if (strncmp(lang_code, "es", 2) == 0)
        return "Hola";
    if (strncmp(lang_code, "ja", 2) == 0)
        return "こんにちは";

    return "Hello";
}
```

### Diagnostic Information Collection

Gather launcher settings for a diagnostic report:

```c
static void generate_diagnostics(MMCOContext* ctx, char* out, size_t sz)
{
    void* mh = ctx->module_handle;

    /* Application identity */
    const char* ver = ctx->get_app_version(mh);
    char app_ver[64];
    snprintf(app_ver, sizeof(app_ver), "%s", ver);

    const char* name = ctx->get_app_name(mh);
    char app_name[64];
    snprintf(app_name, sizeof(app_name), "%s", name);

    /* Java configuration */
    const char* java_raw = ctx->app_setting_get(mh, "JavaPath");
    char java[512] = "N/A";
    if (java_raw) snprintf(java, sizeof(java), "%s", java_raw);

    const char* max_raw = ctx->app_setting_get(mh, "MaxMemAlloc");
    char max_mem[16] = "N/A";
    if (max_raw) snprintf(max_mem, sizeof(max_mem), "%s", max_raw);

    /* Theme and language */
    const char* theme_raw = ctx->app_setting_get(mh, "ApplicationTheme");
    char theme[64] = "N/A";
    if (theme_raw) snprintf(theme, sizeof(theme), "%s", theme_raw);

    const char* lang_raw = ctx->app_setting_get(mh, "Language");
    char lang[16] = "N/A";
    if (lang_raw) snprintf(lang, sizeof(lang), "%s", lang_raw);

    /* Proxy configuration */
    const char* proxy_raw = ctx->app_setting_get(mh, "ProxyType");
    char proxy[16] = "N/A";
    if (proxy_raw) snprintf(proxy, sizeof(proxy), "%s", proxy_raw);

    /* Timestamp */
    int64_t ts = ctx->get_timestamp(mh);

    snprintf(out, sz,
             "=== %s %s Diagnostics ===\n"
             "Generated: %lld\n"
             "Java Path: %s\n"
             "Max Memory: %s MB\n"
             "Theme: %s\n"
             "Language: %s\n"
             "Proxy Type: %s\n",
             app_name, app_ver, (long long)ts,
             java, max_mem, theme, lang, proxy);
}
```

### Pre-Launch Conditional Logic

Using app settings in `INSTANCE_PRE_LAUNCH` to conditionally apply
launch modifications:

```c
static int on_pre_launch(void* mh, int hook_id, void* payload)
{
    (void)hook_id;
    (void)payload;

    /* Read global max memory setting */
    const char* max_raw = g_ctx->app_setting_get(mh, "MaxMemAlloc");
    int max_mem = max_raw ? atoi(max_raw) : 0;

    /* If memory is over 8 GB, enable G1GC optimizations */
    if (max_mem >= 8192) {
        g_ctx->launch_set_env(mh,
            "_JAVA_OPTIONS",
            "-XX:+UseG1GC -XX:G1HeapRegionSize=16M "
            "-XX:MaxGCPauseMillis=50");

        g_ctx->log_info(mh,
            "Applied G1GC optimizations for high-memory instance");
    }

    return 0;
}
```

### Proxy-Aware HTTP Requests

Check proxy settings before making HTTP requests:

```c
static void fetch_with_proxy_check(MMCOContext* ctx, const char* url)
{
    void* mh = ctx->module_handle;

    const char* proxy_type = ctx->app_setting_get(mh, "ProxyType");
    if (proxy_type && strcmp(proxy_type, "0") != 0) {
        /* Proxy is configured — the HTTP API (S8) handles this
           automatically, but we can log it for debugging */
        char buf[128];
        snprintf(buf, sizeof(buf), "Proxy active (type %s), fetching: %s",
                 proxy_type, url);
        ctx->log_info(mh, buf);
    }

    /* S8 HTTP API respects the launcher's proxy settings */
    char* response = NULL;
    int rc = ctx->http_get(mh, url, &response);
    if (rc == 0 && response) {
        /* Process response... */
    }
}
```

### Settings Snapshot for Debugging

Save a snapshot of relevant settings to the plugin data directory:

```c
static void save_settings_snapshot(MMCOContext* ctx)
{
    void* mh = ctx->module_handle;

    static const char* keys[] = {
        "JavaPath", "MinMemAlloc", "MaxMemAlloc", "JvmArgs",
        "Language", "ApplicationTheme", "AutoUpdate",
        "ShowConsole", "AutoCloseConsole",
        NULL
    };

    char snapshot[4096];
    int offset = 0;
    int64_t ts = ctx->get_timestamp(mh);

    offset += snprintf(snapshot + offset, sizeof(snapshot) - offset,
                       "Settings Snapshot at %lld\n"
                       "============================\n",
                       (long long)ts);

    for (const char** k = keys; *k; ++k) {
        const char* val = ctx->app_setting_get(mh, *k);
        char value[256];
        if (val) {
            snprintf(value, sizeof(value), "%s", val);
        } else {
            snprintf(value, sizeof(value), "(not set)");
        }
        offset += snprintf(snapshot + offset, sizeof(snapshot) - offset,
                           "%-24s = %s\n", *k, value);
    }

    ctx->fs_write(mh, "settings-snapshot.txt", snapshot, offset);
    ctx->log_info(mh, "Settings snapshot saved");
}
```

---

## Error Conditions

| Condition | Return value | Behavior |
|-----------|-------------|----------|
| `key` is `nullptr` | `nullptr` | No side effects. |
| `Application` pointer is null | `nullptr` | Extremely rare — only during early startup or late shutdown. |
| `SettingsObject` is null | `nullptr` | The settings system has not been initialized. |
| Key does not exist in settings | `nullptr` | The key is not recognized by MeshMC. |
| Value is invalid (`QVariant`) | `nullptr` | The setting exists but has no valid value. |
| Key exists but value is empty | `""` (empty string, non-null) | Distinction from `nullptr`: the setting exists but is explicitly empty. |

### Distinguishing "Not Found" from "Empty"

```c
const char* val = ctx->app_setting_get(mh, "SomeKey");
if (val == NULL) {
    /* Key does not exist, or settings unavailable */
} else if (val[0] == '\0') {
    /* Key exists but value is empty string */
} else {
    /* Key exists with a non-empty value */
}
```

---

## Thread Safety

`app_setting_get()` must be called from the **main (GUI) thread**.
This is consistent with all other MMCO API functions.

Internally, `Application::settings()` is not thread-safe — it
accesses a `SettingsObject` that is only safe to read from the main
thread. Calling `app_setting_get()` from a background thread may
cause data races or crashes.

---

## When Do Settings Change?

Application settings can change at any time during the launcher's
lifetime:

- The user opens the Settings dialog and modifies values.
- An automatic update changes configuration.
- The user edits the configuration file directly.

`app_setting_get()` always reads the **current** value at the time
of the call. There is no caching — each call goes through the full
`SettingsObject::get()` → `QVariant::toString()` pipeline.

If you need to detect setting changes, poll periodically or register
for relevant hooks (if available). There is currently no dedicated
"setting changed" hook in the MMCO API.

---

## Comparison: S2 `setting_get()` vs S16 `app_setting_get()`

| Aspect | S2 `setting_get()` | S16 `app_setting_get()` |
|--------|-------------------|------------------------|
| **Purpose** | Plugin's own settings | Launcher global settings |
| **Access** | Read + Write | Read-only |
| **Key space** | Per-plugin namespace | Global, shared |
| **Storage** | Plugin settings file | `meshmc.cfg` |
| **Who defines keys** | Plugin developer | MeshMC developers |
| **Companion write function** | `setting_set()` | None (intentional) |
| **Returns** | `const char*` (via `tempString`) | `const char*` (via `tempString`) |
| **Null on missing key** | Yes | Yes |
| **Type helpers** | `setting_get_int()`, `setting_get_bool()` | None (parse strings manually) |

### Why No Type Helpers for S16?

The S2 Settings API provides `setting_get_int()` and
`setting_get_bool()` convenience functions. S16 does not, because:

1. Application settings have heterogeneous types defined by MeshMC
   internals, and adding typed accessors for all of them would
   bloat the API surface.
2. The string representation via `QVariant::toString()` is
   unambiguous and parseable.
3. Plugins typically query a small number of known keys and can
   parse the strings inline.

---

## Cross-References

| Section | Relationship |
|---------|-------------|
| **S1 (Lifecycle)** | Safe to call inside `mmco_init()` and `mmco_unload()`. |
| **S2 (Settings)** | Use S2 for plugin-scoped settings, S16 for reading global launcher configuration. |
| **S7 (Accounts & Java)** | `java_path()` provides the Java executable path for a specific Java installation by index; `app_setting_get("JavaPath")` provides the globally configured default. |
| **S14 (Utility)** | `get_app_version()` provides the build version; `app_setting_get()` provides runtime configuration. |
| **S15 (Launch Modifiers)** | `app_setting_get()` can read settings like `MaxMemAlloc` to conditionally apply launch modifications. |
| **S8 (HTTP)** | The HTTP API automatically respects proxy settings; `app_setting_get("ProxyType")` lets you check if a proxy is active. |
