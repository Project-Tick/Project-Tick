# Numeric & Boolean Settings — Typed Helper Patterns

> **API functions:** `setting_get()`, `setting_set()` (see [string-settings.md](string-settings.md)) · **This page:** Recommended convenience wrappers for integer and boolean settings

---

## Overview

The MMCO Settings API stores all values as **strings**. There are no dedicated `setting_get_int()`, `setting_set_int()`, `setting_get_bool()`, or `setting_set_bool()` function pointers in the `MMCOContext` struct. This is a deliberate design choice — it keeps the C ABI surface minimal and avoids type-system complexity across the plugin boundary.

However, plugins frequently need to store numeric counters, time intervals, feature flags, and toggle states. This page documents the **recommended helper functions** that every plugin should implement (or copy) to safely convert between the string API and typed values.

These helpers are presented as static functions that can be placed in a plugin's source file or in a shared utility header within your plugin project.

---

## Integer settings

### `setting_get_int()` — Retrieve an integer setting

#### Recommended implementation

```cpp
/**
 * Retrieve an integer setting.
 *
 * @param ctx       Pointer to the MMCOContext.
 * @param key       Short setting key (unqualified, e.g. "interval").
 * @param fallback  Value returned if the key does not exist or cannot be
 *                  parsed as an integer.
 * @return          The integer value of the setting, or @p fallback.
 */
static int setting_get_int(MMCOContext* ctx, const char* key, int fallback)
{
    const char* raw = ctx->setting_get(ctx->module_handle, key);
    if (!raw || raw[0] == '\0')
        return fallback;
    char* end = nullptr;
    long val = strtol(raw, &end, 10);
    if (end == raw)         // No digits parsed
        return fallback;
    if (val > INT_MAX)      return INT_MAX;
    if (val < INT_MIN)      return INT_MIN;
    return static_cast<int>(val);
}
```

#### Signature

```cpp
static int setting_get_int(MMCOContext* ctx, const char* key, int fallback);
```

#### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `ctx` | `MMCOContext*` | The plugin's MMCO context, stored from `mmco_init()`. |
| `key` | `const char*` | The short setting key (e.g., `"max_backups"`). The runtime prepends `plugin.<module_id>.` automatically. Must not be `nullptr`. |
| `fallback` | `int` | The default value returned when the setting does not exist, is empty, or contains non-numeric text. |

#### Return value

| Condition | Returns |
|-----------|---------|
| Key exists and value is a valid integer string | The parsed integer value. |
| Key exists but value is empty (`""`) | `fallback` |
| Key exists but value cannot be parsed as integer (e.g., `"abc"`) | `fallback` |
| Key does not exist (`setting_get()` returned `nullptr`) | `fallback` |
| Parsed value exceeds `INT_MAX` | `INT_MAX` (clamped) |
| Parsed value is below `INT_MIN` | `INT_MIN` (clamped) |

#### Thread safety

Same as `setting_get()` — **main thread only**.

#### Ownership

No ownership concerns. The return value is a plain `int` copied by value. The intermediate `const char*` from `setting_get()` is consumed and not retained.

#### Why `strtol` and not `atoi`?

`atoi()` returns `0` for both the string `"0"` and for unparseable input, making error detection impossible. `strtol()` with an `end` pointer distinguishes between a valid `0` and a parse failure, and also handles overflow.

#### Example: reading a configurable interval

```cpp
static MMCOContext* g_ctx = nullptr;

static int on_app_init(void* mh, uint32_t, void*, void*)
{
    int interval = setting_get_int(g_ctx, "backup_interval_minutes", 60);

    char msg[128];
    snprintf(msg, sizeof(msg), "Backup interval: %d minute(s)", interval);
    g_ctx->log_info(g_ctx->module_handle, msg);

    return 0;
}
```

#### Example: clamping to a valid range

```cpp
int raw_val = setting_get_int(g_ctx, "max_backups", 10);
// Clamp to [1, 100]
int max_backups = raw_val < 1 ? 1 : (raw_val > 100 ? 100 : raw_val);
```

#### Example: detecting first run vs. zero

```cpp
// If "launch_count" has never been set, setting_get() returns nullptr,
// and setting_get_int() returns the fallback (-1 in this case).
int count = setting_get_int(g_ctx, "launch_count", -1);
if (count < 0) {
    g_ctx->log_info(g_ctx->module_handle, "First run detected.");
    count = 0;
}
```

---

### `setting_set_int()` — Store an integer setting

#### Recommended implementation

```cpp
/**
 * Store an integer setting.
 *
 * @param ctx    Pointer to the MMCOContext.
 * @param key    Short setting key.
 * @param value  The integer value to store.
 * @return       0 on success, -1 on failure (same as setting_set()).
 */
static int setting_set_int(MMCOContext* ctx, const char* key, int value)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", value);
    return ctx->setting_set(ctx->module_handle, key, buf);
}
```

#### Signature

```cpp
static int setting_set_int(MMCOContext* ctx, const char* key, int value);
```

#### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `ctx` | `MMCOContext*` | The plugin's MMCO context. |
| `key` | `const char*` | The short setting key. Must not be `nullptr`. |
| `value` | `int` | The integer value to store. Stored as its decimal string representation (e.g., `42` → `"42"`, `-7` → `"-7"`). |

#### Return value

| Condition | Returns |
|-----------|---------|
| Success | `0` |
| `Application` or `SettingsObject` not available | `-1` |

#### Thread safety

Same as `setting_set()` — **main thread only**.

#### Ownership

No ownership concerns. The `buf` array is stack-allocated and its contents are copied by `setting_set()`.

#### Implementation notes

- `snprintf(buf, sizeof(buf), "%d", value)` is safe for all `int` values. The maximum decimal representation of a 32-bit integer (`-2147483648`) is 11 characters plus null terminator, well within the 32-byte buffer.
- No heap allocation occurs.

#### Example: incrementing a counter

```cpp
void* h = g_ctx->module_handle;

int count = setting_get_int(g_ctx, "run_count", 0);
count++;
setting_set_int(g_ctx, "run_count", count);

char msg[64];
snprintf(msg, sizeof(msg), "Run count is now: %d", count);
g_ctx->log_info(h, msg);
```

#### Example: storing a timestamp as integer (epoch seconds)

```cpp
int64_t now = g_ctx->get_timestamp(g_ctx->module_handle);
// get_timestamp returns int64_t, so use snprintf with PRId64 for safety
char buf[32];
snprintf(buf, sizeof(buf), "%" PRId64, now);
g_ctx->setting_set(g_ctx->module_handle, "last_backup_time", buf);
```

> **Note:** For `int64_t` values, do not use `setting_set_int()` (which takes `int`). Use `snprintf` with the appropriate format specifier directly. See [64-bit integer pattern](#pattern-64-bit-integer-settings) below.

---

## Boolean settings

### Representation convention

The MMCO codebase uses the following convention for boolean-as-string values:

| Logical value | String representation | Rationale |
|---------------|----------------------|-----------|
| **true** | `"1"` | Consistent with C integer boolean semantics and Qt's `QVariant::toBool()` behaviour. |
| **false** | `"0"` | Zero = false, non-zero = true. |

Other representations (`"true"`, `"false"`, `"yes"`, `"no"`) are **not** used by MeshMC's internal settings and are discouraged for plugin settings. Using `"1"` / `"0"` maintains consistency and simplifies parsing.

### `setting_get_bool()` — Retrieve a boolean setting

#### Recommended implementation

```cpp
/**
 * Retrieve a boolean setting.
 *
 * Interprets the stored string as an integer: non-zero = true, zero = false.
 * If the key does not exist or the value is unparseable, returns @p fallback.
 *
 * @param ctx       Pointer to the MMCOContext.
 * @param key       Short setting key.
 * @param fallback  Value returned if the key does not exist or cannot be parsed.
 * @return          The boolean value of the setting, or @p fallback.
 */
static bool setting_get_bool(MMCOContext* ctx, const char* key, bool fallback)
{
    const char* raw = ctx->setting_get(ctx->module_handle, key);
    if (!raw || raw[0] == '\0')
        return fallback;
    // "0" is false, anything else (including "1", "true", "yes") is true
    return raw[0] != '0' || raw[1] != '\0';
}
```

#### Signature

```cpp
static bool setting_get_bool(MMCOContext* ctx, const char* key, bool fallback);
```

#### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `ctx` | `MMCOContext*` | The plugin's MMCO context. |
| `key` | `const char*` | The short setting key. Must not be `nullptr`. |
| `fallback` | `bool` | Default value when the key doesn't exist or is empty. |

#### Return value

| Stored string | Returns |
|---------------|---------|
| `"0"` | `false` |
| `"1"` | `true` |
| `""` (empty) | `fallback` |
| `nullptr` (not set) | `fallback` |
| `"true"`, `"yes"`, any other non-`"0"` string | `true` |

#### Thread safety

Same as `setting_get()` — **main thread only**.

#### Design rationale

The implementation treats **only** the exact string `"0"` as `false`. Everything else non-empty is `true`. This is consistent with C boolean semantics (`0` = false, non-zero = true) and is robust against alternative representations that a user might manually edit into the settings file.

An alternative, stricter implementation would be:

```cpp
static bool setting_get_bool_strict(MMCOContext* ctx, const char* key, bool fallback)
{
    const char* raw = ctx->setting_get(ctx->module_handle, key);
    if (!raw || raw[0] == '\0')
        return fallback;
    if (raw[0] == '1' && raw[1] == '\0')
        return true;
    if (raw[0] == '0' && raw[1] == '\0')
        return false;
    return fallback;  // Unrecognized value → use fallback
}
```

Choose the version that best fits your plugin's tolerance for unexpected input.

#### Example: feature toggle

```cpp
bool compress = setting_get_bool(g_ctx, "compress_backups", true);
if (compress) {
    g_ctx->log_info(g_ctx->module_handle, "Compression enabled.");
} else {
    g_ctx->log_info(g_ctx->module_handle, "Compression disabled.");
}
```

#### Example: guarding a feature behind a setting

```cpp
static int on_instance_post_launch(void* mh, uint32_t, void* payload, void*)
{
    if (!setting_get_bool(g_ctx, "log_launches", true))
        return 0;  // Feature disabled

    auto* info = static_cast<MMCOInstanceInfo*>(payload);
    char msg[256];
    snprintf(msg, sizeof(msg), "Instance launched: %s", info->instance_name);
    g_ctx->log_info(g_ctx->module_handle, msg);
    return 0;
}
```

---

### `setting_set_bool()` — Store a boolean setting

#### Recommended implementation

```cpp
/**
 * Store a boolean setting.
 *
 * @param ctx    Pointer to the MMCOContext.
 * @param key    Short setting key.
 * @param value  The boolean value. Stored as "1" for true, "0" for false.
 * @return       0 on success, -1 on failure.
 */
static int setting_set_bool(MMCOContext* ctx, const char* key, bool value)
{
    return ctx->setting_set(ctx->module_handle, key, value ? "1" : "0");
}
```

#### Signature

```cpp
static int setting_set_bool(MMCOContext* ctx, const char* key, bool value);
```

#### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `ctx` | `MMCOContext*` | The plugin's MMCO context. |
| `key` | `const char*` | The short setting key. Must not be `nullptr`. |
| `value` | `bool` | `true` is stored as `"1"`, `false` as `"0"`. |

#### Return value

Same as `setting_set()`:

| Condition | Returns |
|-----------|---------|
| Success | `0` |
| App/settings unavailable | `-1` |

#### Thread safety

Same as `setting_set()` — **main thread only**.

#### Example: toggling a feature

```cpp
void* h = g_ctx->module_handle;

bool current = setting_get_bool(g_ctx, "notifications", true);
bool toggled = !current;
setting_set_bool(g_ctx, "notifications", toggled);

char msg[128];
snprintf(msg, sizeof(msg), "Notifications %s", toggled ? "enabled" : "disabled");
g_ctx->log_info(h, msg);
```

#### Example: UI-driven toggle (from a button callback)

```cpp
static void on_toggle_button(void* user_data)
{
    (void)user_data;
    bool current = setting_get_bool(g_ctx, "auto_update", false);
    setting_set_bool(g_ctx, "auto_update", !current);

    // Update the button label to reflect the new state
    bool new_state = !current;
    g_ctx->ui_button_set_text(
        g_ctx->module_handle,
        g_toggle_button,
        new_state ? "Auto-Update: ON" : "Auto-Update: OFF"
    );
}
```

---

<a id="pattern-64-bit-integer-settings"></a>
## Pattern: 64-bit integer settings

For `int64_t` values (timestamps, file sizes, play-time in milliseconds), the `int`-sized helpers are insufficient. Use dedicated helpers:

```cpp
#include <cinttypes>  // for PRId64, SCNd64

/**
 * Retrieve a 64-bit integer setting.
 */
static int64_t setting_get_int64(MMCOContext* ctx, const char* key, int64_t fallback)
{
    const char* raw = ctx->setting_get(ctx->module_handle, key);
    if (!raw || raw[0] == '\0')
        return fallback;
    char* end = nullptr;
    long long val = strtoll(raw, &end, 10);
    if (end == raw)
        return fallback;
    return static_cast<int64_t>(val);
}

/**
 * Store a 64-bit integer setting.
 */
static int setting_set_int64(MMCOContext* ctx, const char* key, int64_t value)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%" PRId64, value);
    return ctx->setting_set(ctx->module_handle, key, buf);
}
```

### Example: storing a timestamp

```cpp
int64_t now = g_ctx->get_timestamp(g_ctx->module_handle);
setting_set_int64(g_ctx, "last_backup_epoch", now);

// Later...
int64_t last = setting_get_int64(g_ctx, "last_backup_epoch", 0);
int64_t elapsed = g_ctx->get_timestamp(g_ctx->module_handle) - last;

char msg[128];
snprintf(msg, sizeof(msg), "Seconds since last backup: %" PRId64, elapsed);
g_ctx->log_info(g_ctx->module_handle, msg);
```

---

## Pattern: floating-point settings

For `double` values (less common in plugin settings, but useful for thresholds, ratios, etc.):

```cpp
#include <cstdlib>  // for strtod
#include <cstdio>   // for snprintf

/**
 * Retrieve a double setting.
 */
static double setting_get_double(MMCOContext* ctx, const char* key, double fallback)
{
    const char* raw = ctx->setting_get(ctx->module_handle, key);
    if (!raw || raw[0] == '\0')
        return fallback;
    char* end = nullptr;
    double val = strtod(raw, &end);
    if (end == raw)
        return fallback;
    return val;
}

/**
 * Store a double setting.
 *
 * Uses enough decimal places to round-trip a double precisely.
 */
static int setting_set_double(MMCOContext* ctx, const char* key, double value)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%.17g", value);
    return ctx->setting_set(ctx->module_handle, key, buf);
}
```

> **Warning:** Floating-point serialisation with `"%.17g"` preserves precision but may produce unexpected string representations (e.g., `"0.10000000000000001"` instead of `"0.1"`). For user-visible values, consider a fixed format like `"%.2f"`.

---

## Complete helper header

For convenience, here is a self-contained header you can include in your plugin project. It wraps all the typed helpers documented above:

```cpp
/* mmco_settings_helpers.h — Typed setting accessors for MMCO plugins.
 *
 * Usage:
 *   #include "mmco_settings_helpers.h"
 *
 * All functions are static inline to avoid ODR issues if included
 * in multiple translation units.
 */

#pragma once

#include <climits>    // INT_MAX, INT_MIN
#include <cinttypes>  // PRId64
#include <cstdlib>    // strtol, strtoll, strtod
#include <cstdio>     // snprintf

/* Forward-declare the context struct type. If you've already included
 * mmco_sdk.h, this is harmless. */
struct MMCOContext;

/* ── Integer ───────────────────────────────────────────────────── */

static inline int setting_get_int(MMCOContext* ctx, const char* key, int fallback)
{
    const char* raw = ctx->setting_get(ctx->module_handle, key);
    if (!raw || raw[0] == '\0')
        return fallback;
    char* end = nullptr;
    long val = strtol(raw, &end, 10);
    if (end == raw) return fallback;
    if (val > INT_MAX) return INT_MAX;
    if (val < INT_MIN) return INT_MIN;
    return static_cast<int>(val);
}

static inline int setting_set_int(MMCOContext* ctx, const char* key, int value)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", value);
    return ctx->setting_set(ctx->module_handle, key, buf);
}

/* ── 64-bit Integer ────────────────────────────────────────────── */

static inline int64_t setting_get_int64(MMCOContext* ctx, const char* key, int64_t fallback)
{
    const char* raw = ctx->setting_get(ctx->module_handle, key);
    if (!raw || raw[0] == '\0')
        return fallback;
    char* end = nullptr;
    long long val = strtoll(raw, &end, 10);
    if (end == raw) return fallback;
    return static_cast<int64_t>(val);
}

static inline int setting_set_int64(MMCOContext* ctx, const char* key, int64_t value)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%" PRId64, value);
    return ctx->setting_set(ctx->module_handle, key, buf);
}

/* ── Boolean ───────────────────────────────────────────────────── */

static inline bool setting_get_bool(MMCOContext* ctx, const char* key, bool fallback)
{
    const char* raw = ctx->setting_get(ctx->module_handle, key);
    if (!raw || raw[0] == '\0')
        return fallback;
    return raw[0] != '0' || raw[1] != '\0';
}

static inline int setting_set_bool(MMCOContext* ctx, const char* key, bool value)
{
    return ctx->setting_set(ctx->module_handle, key, value ? "1" : "0");
}

/* ── Double ────────────────────────────────────────────────────── */

static inline double setting_get_double(MMCOContext* ctx, const char* key, double fallback)
{
    const char* raw = ctx->setting_get(ctx->module_handle, key);
    if (!raw || raw[0] == '\0')
        return fallback;
    char* end = nullptr;
    double val = strtod(raw, &end);
    if (end == raw) return fallback;
    return val;
}

static inline int setting_set_double(MMCOContext* ctx, const char* key, double value)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%.17g", value);
    return ctx->setting_set(ctx->module_handle, key, buf);
}
```

---

## Full worked example: plugin with typed settings

```cpp
#include "mmco_sdk.h"
#include "mmco_settings_helpers.h"
#include <cstring>

MMCO_DEFINE_MODULE(
    "Backup System",
    "2.0.0",
    "Project Tick",
    "Automatic instance backup plugin",
    "GPL-3.0-or-later"
);

static MMCOContext* g_ctx = nullptr;

/* ── Configuration defaults ─────────────────────────────────── */

struct BackupConfig {
    int  interval_minutes;
    int  max_backups;
    bool compress;
    bool notify;
};

static BackupConfig load_config(MMCOContext* ctx)
{
    BackupConfig cfg;
    cfg.interval_minutes = setting_get_int(ctx, "interval_minutes", 60);
    cfg.max_backups      = setting_get_int(ctx, "max_backups", 10);
    cfg.compress         = setting_get_bool(ctx, "compress", true);
    cfg.notify           = setting_get_bool(ctx, "notify", true);

    // Clamp interval to [5, 1440]
    if (cfg.interval_minutes < 5)    cfg.interval_minutes = 5;
    if (cfg.interval_minutes > 1440) cfg.interval_minutes = 1440;

    // Clamp max backups to [1, 100]
    if (cfg.max_backups < 1)   cfg.max_backups = 1;
    if (cfg.max_backups > 100) cfg.max_backups = 100;

    return cfg;
}

static void save_config(MMCOContext* ctx, const BackupConfig& cfg)
{
    setting_set_int(ctx, "interval_minutes", cfg.interval_minutes);
    setting_set_int(ctx, "max_backups", cfg.max_backups);
    setting_set_bool(ctx, "compress", cfg.compress);
    setting_set_bool(ctx, "notify", cfg.notify);
}

/* ── Lifecycle ────────────────────────────────────────────────── */

static int on_app_init(void*, uint32_t, void*, void*)
{
    void* h = g_ctx->module_handle;

    BackupConfig cfg = load_config(g_ctx);

    char msg[256];
    snprintf(msg, sizeof(msg),
             "BackupSystem config: interval=%d min, max=%d, compress=%s, notify=%s",
             cfg.interval_minutes, cfg.max_backups,
             cfg.compress ? "yes" : "no",
             cfg.notify ? "yes" : "no");
    g_ctx->log_info(h, msg);

    // Increment run counter
    int runs = setting_get_int(g_ctx, "run_count", 0);
    setting_set_int(g_ctx, "run_count", runs + 1);

    // Store last-run timestamp
    int64_t now = g_ctx->get_timestamp(h);
    setting_set_int64(g_ctx, "last_run_epoch", now);

    return 0;
}

static int on_settings_changed(void*, uint32_t, void* payload, void*)
{
    auto* change = static_cast<MMCOSettingChange*>(payload);

    // Reload config if one of our settings changed
    if (change->key && strstr(change->key, "plugin.Backup System.")) {
        BackupConfig cfg = load_config(g_ctx);
        // Apply new config to running backup system...
        (void)cfg;
        g_ctx->log_info(g_ctx->module_handle, "Configuration reloaded.");
    }
    return 0;
}

extern "C" MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;
    void* h = ctx->module_handle;

    ctx->hook_register(h, MMCO_HOOK_APP_INITIALIZED, on_app_init, nullptr);
    ctx->hook_register(h, MMCO_HOOK_SETTINGS_CHANGED, on_settings_changed, nullptr);

    return 0;
}

extern "C" MMCO_EXPORT void mmco_unload()
{
    if (!g_ctx) return;
    void* h = g_ctx->module_handle;
    g_ctx->hook_unregister(h, MMCO_HOOK_APP_INITIALIZED, on_app_init);
    g_ctx->hook_unregister(h, MMCO_HOOK_SETTINGS_CHANGED, on_settings_changed);
}
```

---

## Comparison table

| Helper | Storage format | Parse function | Fallback on error | Buffer size |
|--------|---------------|----------------|-------------------|-------------|
| `setting_get_int` / `setting_set_int` | `"%d"` | `strtol` | Caller-provided `int` | 32 bytes |
| `setting_get_int64` / `setting_set_int64` | `"%" PRId64` | `strtoll` | Caller-provided `int64_t` | 32 bytes |
| `setting_get_bool` / `setting_set_bool` | `"1"` / `"0"` | Character check | Caller-provided `bool` | — (literal) |
| `setting_get_double` / `setting_set_double` | `"%.17g"` | `strtod` | Caller-provided `double` | 64 bytes |

---

## Why these are not in the core API

Providing typed accessors as core `MMCOContext` function pointers would:

1. **Bloat the ABI surface.** Each new type adds two more function pointers. The C struct grows, and forward/backward compatibility becomes harder.

2. **Duplicate trivial logic.** The conversion between string and integer is a few lines of `strtol` / `snprintf`. There is no runtime state or launcher-internal knowledge required.

3. **Impose type semantics at the boundary.** By keeping the API string-only, plugins are free to use whatever serialisation they prefer (decimal, hex, JSON, Base64). The launcher does not need to know.

4. **Complicate the SettingsObject backend.** The `SettingsObject::get()` returns `QVariant`, which would need type-specific handling that doesn't exist today.

Plugin authors are expected to implement thin wrappers like the ones documented here. The complete helper header above can be copied verbatim into any plugin project.

---

## Cross-references

- [String Settings](string-settings.md) — `setting_get()` and `setting_set()` core reference
- [Section 02 — Overview](README.md) — Namespacing, storage backend, `SETTINGS_CHANGED` hook
- [Section 01 — Module Handle](../s01-lifecycle/module-handle.md) — `tempString` lifetime rules
- [Section 14 — Utility](../s14-utility/) — `get_timestamp()` for epoch-based time values
- [SDK Guide](../../sdk-guide/) — Build setup, include paths
