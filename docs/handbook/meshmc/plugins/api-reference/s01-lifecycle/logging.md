# Logging — `log_info()`, `log_warn()`, `log_error()`, `log_debug()`

> **Header:** `PluginAPI.h` (function pointer declarations) · **SDK:** `mmco_sdk.h` (convenience macros) · **Implementation:** `PluginManager.cpp`

---

## Overview

The MMCO logging API provides four severity levels for plugin messages. All messages are automatically prefixed with the plugin's name and routed through Qt's logging infrastructure (`qInfo`, `qWarning`, `qCritical`, `qDebug`).

Plugins **must not** use `printf`, `std::cout`, `qDebug()`, or any other output mechanism directly. All output must go through the provided logging functions so that:

1. Messages are correctly attributed to the originating plugin.
2. Output is captured by the MeshMC log viewer.
3. Log filtering (by severity or by plugin) works consistently.

---

## Function pointer declarations

From `MMCOContext` in `PluginAPI.h`:

```cpp
struct MMCOContext {
    /* ... ABI guard fields ... */

    void (*log_info)(void* mh, const char* msg);
    void (*log_warn)(void* mh, const char* msg);
    void (*log_error)(void* mh, const char* msg);
    void (*log_debug)(void* mh, const char* msg);

    /* ... remaining fields ... */
};
```

All four functions share the same signature and differ only in the Qt log level used internally.

---

## `log_info()`

### Signature

```cpp
void (*log_info)(void* mh, const char* msg);
```

### Parameters

| Parameter | Type | Description |
|---|---|---|
| `mh` | `void*` | The module handle. Must be `ctx->module_handle`. |
| `msg` | `const char*` | A null-terminated UTF-8 string containing the log message. |

### Return value

None (`void`).

### Description

Logs an informational message at the `QtInfoMsg` severity level. Use this for normal operational messages: startup confirmations, feature activation, user-visible status updates.

### Output format

Messages appear in the Qt log output (stderr / log file) with the following format:

```
[Plugin: <module_name>] <msg>
```

For example, if the module name is `"BackupSystem"` and `msg` is `"Backup completed successfully."`:

```
[Plugin: BackupSystem] Backup completed successfully.
```

### Implementation (trampoline)

**Declared in `PluginManager.h`:**

```cpp
static void api_log_info(void* mh, const char* msg);
```

**Implemented in `PluginManager.cpp`:**

```cpp
void PluginManager::api_log_info(void* mh, const char* msg)
{
    auto* r = rt(mh);
    auto& meta = r->manager->m_modules[r->moduleIndex];
    qInfo().noquote() << "[Plugin:" << meta.name << "]" << msg;
}
```

**Trampoline walkthrough:**

1. `rt(mh)` — casts `void*` to `ModuleRuntime*` (see [module-handle.md](module-handle.md)).
2. `r->manager->m_modules[r->moduleIndex]` — retrieves the `PluginMetadata` for this module to get its `name`.
3. `qInfo().noquote()` — Qt log macro at info level. `.noquote()` prevents Qt from adding quotation marks around the string arguments.

### Wiring

Set in `PluginManager::buildContext()`:

```cpp
ctx.log_info = api_log_info;
```

### Example

```cpp
ctx->log_info(ctx->module_handle, "Plugin initialized successfully.");
ctx->log_info(ctx->module_handle, "Found 3 instances to monitor.");
```

---

## `log_warn()`

### Signature

```cpp
void (*log_warn)(void* mh, const char* msg);
```

### Parameters

| Parameter | Type | Description |
|---|---|---|
| `mh` | `void*` | The module handle. Must be `ctx->module_handle`. |
| `msg` | `const char*` | A null-terminated UTF-8 string containing the warning message. |

### Return value

None (`void`).

### Description

Logs a warning message at the `QtWarningMsg` severity level. Use this for recoverable problems: missing optional configuration, deprecated feature usage, retryable failures.

### Output format

```
[Plugin: <module_name>] <msg>
```

Warnings may be highlighted differently (e.g., yellow/orange) depending on the log viewer or Qt message handler configuration.

### Implementation (trampoline)

```cpp
void PluginManager::api_log_warn(void* mh, const char* msg)
{
    auto* r = rt(mh);
    auto& meta = r->manager->m_modules[r->moduleIndex];
    qWarning().noquote() << "[Plugin:" << meta.name << "]" << msg;
}
```

### Wiring

```cpp
ctx.log_warn = api_log_warn;
```

### Example

```cpp
ctx->log_warn(ctx->module_handle, "Config key 'backup_interval' not set, using default.");
ctx->log_warn(ctx->module_handle, "Instance path contains spaces — may cause issues on some systems.");
```

---

## `log_error()`

### Signature

```cpp
void (*log_error)(void* mh, const char* msg);
```

### Parameters

| Parameter | Type | Description |
|---|---|---|
| `mh` | `void*` | The module handle. Must be `ctx->module_handle`. |
| `msg` | `const char*` | A null-terminated UTF-8 string containing the error message. |

### Return value

None (`void`).

### Description

Logs an error message at the `QtCriticalMsg` severity level. Use this for serious operational failures: failed hook registration, I/O errors, unrecoverable state conditions.

**Important:** logging an error does not terminate the plugin or abort the current operation. It is purely informational. If you need to abort plugin initialization, return a non-zero value from `mmco_init()`.

### Output format

```
[Plugin: <module_name>] <msg>
```

Critical-level messages may trigger different behaviour depending on the Qt message handler — for example, they might be written to a separate error log or displayed in the MeshMC UI.

### Implementation (trampoline)

```cpp
void PluginManager::api_log_error(void* mh, const char* msg)
{
    auto* r = rt(mh);
    auto& meta = r->manager->m_modules[r->moduleIndex];
    qCritical().noquote() << "[Plugin:" << meta.name << "]" << msg;
}
```

Note that `log_error()` maps to `qCritical()`, **not** `qFatal()`. It does not call `abort()`.

### Wiring

```cpp
ctx.log_error = api_log_error;
```

### Example

```cpp
int rc = ctx->hook_register(ctx->module_handle, MMCO_HOOK_UI_INSTANCE_PAGES,
                            on_instance_pages, nullptr);
if (rc != 0) {
    ctx->log_error(ctx->module_handle, "Failed to register INSTANCE_PAGES hook");
    return rc;  /* abort init */
}
```

---

## `log_debug()`

### Signature

```cpp
void (*log_debug)(void* mh, const char* msg);
```

### Parameters

| Parameter | Type | Description |
|---|---|---|
| `mh` | `void*` | The module handle. Must be `ctx->module_handle`. |
| `msg` | `const char*` | A null-terminated UTF-8 string containing the debug message. |

### Return value

None (`void`).

### Description

Logs a debug message at the `QtDebugMsg` severity level. Use this for verbose diagnostic output that is only useful during development: function entry/exit, variable values, intermediate state.

Debug messages are typically **suppressed in release builds** (depending on the `QT_NO_DEBUG_OUTPUT` compile flag and the active Qt message handler). Do not rely on debug messages being visible to end users.

### Output format

```
[Plugin: <module_name>] <msg>
```

### Implementation (trampoline)

```cpp
void PluginManager::api_log_debug(void* mh, const char* msg)
{
    auto* r = rt(mh);
    auto& meta = r->manager->m_modules[r->moduleIndex];
    qDebug().noquote() << "[Plugin:" << meta.name << "]" << msg;
}
```

### Wiring

```cpp
ctx.log_debug = api_log_debug;
```

### Example

```cpp
ctx->log_debug(ctx->module_handle, "Entering backup creation routine.");

std::string info = "Instance count: " + std::to_string(count);
ctx->log_debug(ctx->module_handle, info.c_str());
```

---

## SDK convenience macros

The `mmco_sdk.h` header defines four macros that eliminate the repetitive `ctx->module_handle` boilerplate. These are the **recommended** way to log in plugin code.

### Definitions (from `mmco_sdk.h`)

```cpp
#define MMCO_LOG(ctx, msg)  (ctx)->log_info((ctx)->module_handle, (msg))
#define MMCO_WARN(ctx, msg) (ctx)->log_warn((ctx)->module_handle, (msg))
#define MMCO_ERR(ctx, msg)  (ctx)->log_error((ctx)->module_handle, (msg))
#define MMCO_DBG(ctx, msg)  (ctx)->log_debug((ctx)->module_handle, (msg))
```

### `MMCO_LOG(ctx, msg)`

| Argument | Type | Description |
|---|---|---|
| `ctx` | `MMCOContext*` | Pointer to the module's context. |
| `msg` | `const char*` | The log message string. |

Expands to: `(ctx)->log_info((ctx)->module_handle, (msg))`

### `MMCO_WARN(ctx, msg)`

| Argument | Type | Description |
|---|---|---|
| `ctx` | `MMCOContext*` | Pointer to the module's context. |
| `msg` | `const char*` | The warning message string. |

Expands to: `(ctx)->log_warn((ctx)->module_handle, (msg))`

### `MMCO_ERR(ctx, msg)`

| Argument | Type | Description |
|---|---|---|
| `ctx` | `MMCOContext*` | Pointer to the module's context. |
| `msg` | `const char*` | The error message string. |

Expands to: `(ctx)->log_error((ctx)->module_handle, (msg))`

### `MMCO_DBG(ctx, msg)`

| Argument | Type | Description |
|---|---|---|
| `ctx` | `MMCOContext*` | Pointer to the module's context. |
| `msg` | `const char*` | The debug message string. |

Expands to: `(ctx)->log_debug((ctx)->module_handle, (msg))`

### Macro usage example

```cpp
static MMCOContext* g_ctx = nullptr;

MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;

    MMCO_LOG(ctx, "BackupSystem v1.0.0 initializing...");
    MMCO_DBG(ctx, "Debug-level startup diagnostics.");

    int rc = ctx->hook_register(ctx->module_handle,
                                MMCO_HOOK_UI_INSTANCE_PAGES,
                                on_instance_pages, nullptr);
    if (rc != 0) {
        MMCO_ERR(ctx, "Failed to register INSTANCE_PAGES hook");
        return rc;
    }

    MMCO_LOG(ctx, "BackupSystem initialized successfully.");
    return 0;
}

MMCO_EXPORT void mmco_unload()
{
    if (g_ctx) {
        MMCO_LOG(g_ctx, "BackupSystem unloading.");
    }
    g_ctx = nullptr;
}
```

### Using macros inside hook callbacks

The macros require a variable named `ctx`. In hook callbacks, the context is typically a global:

```cpp
static MMCOContext* g_ctx = nullptr;

static int on_shutting_down(void* /*mh*/, uint32_t /*hook_id*/,
                            void* /*payload*/, void* /*ud*/)
{
    /* g_ctx is our context — but the macros expect "ctx" */
    MMCOContext* ctx = g_ctx;  /* local alias for macro compatibility */
    MMCO_LOG(ctx, "Received shutdown hook.");
    return 0;
}
```

Alternatively, call the function pointer directly:

```cpp
static int on_shutting_down(void* /*mh*/, uint32_t /*hook_id*/,
                            void* /*payload*/, void* /*ud*/)
{
    g_ctx->log_info(g_ctx->module_handle, "Received shutdown hook.");
    return 0;
}
```

### `MMCO_MH` shorthand

An additional macro is provided for the module handle:

```cpp
#define MMCO_MH (ctx->module_handle)
```

This can be combined with direct function pointer calls:

```cpp
ctx->hook_register(MMCO_MH, MMCO_HOOK_APP_SHUTDOWN, on_shutdown, nullptr);
ctx->setting_set(MMCO_MH, "last_run", timestamp);
```

---

## Formatting messages

The MMCO logging functions accept `const char*` — they do not support format strings, variadic arguments, or `QString`. To log formatted messages, build the string first:

### Using `std::string`

```cpp
int count = ctx->instance_count(ctx->module_handle);
std::string msg = "Monitoring " + std::to_string(count) + " instances.";
MMCO_LOG(ctx, msg.c_str());
```

### Using `snprintf`

```cpp
char buf[256];
snprintf(buf, sizeof(buf), "Backup created: %s (%.1f MB)", name.c_str(), size_mb);
MMCO_LOG(ctx, buf);
```

### Using `QString` (available via SDK)

Since `mmco_sdk.h` includes Qt headers, you can use `QString` for convenience:

```cpp
QString msg = QString("Instance %1 launched (online=%2)").arg(id).arg(online);
MMCO_LOG(ctx, msg.toUtf8().constData());
```

---

## Thread safety

All four logging functions must be called from the **main (GUI) thread** only. They access `PluginManager::m_modules` (a `QVector`) and Qt's logging infrastructure, neither of which is thread-safe.

If your plugin spawns worker threads (e.g., via `std::thread` for file I/O), you must marshal log calls back to the main thread or buffer messages and log them later from a main-thread callback.

---

## Error conditions

| Condition | Behaviour |
|---|---|
| `mh` is `nullptr` | **Undefined behaviour.** The trampoline will dereference a null `ModuleRuntime*`. The launcher will crash with a segmentation fault. |
| `mh` is from a different module | The message will be logged with the **wrong module name**. No crash, but misleading output. |
| `msg` is `nullptr` | Qt's logging macro will print `(null)` or an empty string depending on the platform. No crash, but produces unhelpful output. |
| `msg` is not null-terminated | **Undefined behaviour.** `qInfo() << msg` reads until a null byte is found, potentially reading out of bounds. |
| `msg` contains non-UTF-8 bytes | Qt will process it as Latin-1 or produce mojibake. Use UTF-8. |
| Logging after `mmco_unload()` | **Undefined behaviour.** The `ModuleRuntime` has been destroyed. |
| Logging from a non-main thread | **Undefined behaviour.** Qt log macros are not thread-safe in this context; `m_modules` access is unprotected. |

---

## Performance considerations

- Each log call performs: one `static_cast`, one `QVector` index, and one `qInfo()`/`qWarning()`/`qCritical()`/`qDebug()` invocation.
- Qt's logging has minimal overhead when messages are filtered out (e.g., `QT_NO_DEBUG_OUTPUT` suppresses `qDebug()` at compile time).
- Do not log in tight loops. If you need to log per-iteration diagnostics, guard with a counter or flag.
- String construction (`std::string`, `snprintf`, `QString`) is the dominant cost for log calls. The trampoline and Qt overhead are negligible.

---

## Summary table

| Function pointer | Macro | Qt log level | Severity | Typical use |
|---|---|---|---|---|
| `log_info` | `MMCO_LOG` | `qInfo()` | Info | Normal operations, status |
| `log_warn` | `MMCO_WARN` | `qWarning()` | Warning | Recoverable problems |
| `log_error` | `MMCO_ERR` | `qCritical()` | Error/Critical | Serious failures |
| `log_debug` | `MMCO_DBG` | `qDebug()` | Debug | Development diagnostics |

---

## Real-world example: BackupSystem plugin

From `meshmc/plugins/BackupSystem/BackupPlugin.cpp`:

```cpp
#include "plugin/sdk/mmco_sdk.h"
#include "BackupPage.h"

static MMCOContext* g_ctx = nullptr;

MMCO_DEFINE_MODULE("BackupSystem", "1.0.0", "Project Tick",
                   "Instance backup snapshots — create, restore, export, import",
                   "GPL-3.0-or-later");

extern "C" {

MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;

    MMCO_LOG(ctx, "BackupSystem v1.0.0 initializing...");

    int rc = ctx->hook_register(ctx->module_handle,
                                MMCO_HOOK_UI_INSTANCE_PAGES,
                                on_instance_pages, nullptr);
    if (rc != 0) {
        MMCO_ERR(ctx, "Failed to register INSTANCE_PAGES hook");
        return rc;
    }

    /* ... register toolbar action ... */

    MMCO_LOG(ctx, "BackupSystem initialized successfully.");
    return 0;
}

MMCO_EXPORT void mmco_unload()
{
    if (g_ctx) {
        MMCO_LOG(g_ctx, "BackupSystem unloading.");
    }
    g_ctx = nullptr;
}

} /* extern "C" */
```

This demonstrates the standard pattern:

1. `MMCO_LOG()` at the start of `mmco_init()` to confirm loading.
2. `MMCO_ERR()` on hook registration failure, followed by early return.
3. `MMCO_LOG()` at the end of `mmco_init()` to confirm success.
4. `MMCO_LOG()` in `mmco_unload()` to confirm clean shutdown.
5. Null-guard on `g_ctx` in `mmco_unload()` in case init failed.

---

## See also

- [module-handle.md](module-handle.md) — The `module_handle` field required by all logging functions
- [hooks.md](hooks.md) — Hook registration (commonly paired with error logging)
- [README.md](README.md) — Section overview and lifecycle diagram
