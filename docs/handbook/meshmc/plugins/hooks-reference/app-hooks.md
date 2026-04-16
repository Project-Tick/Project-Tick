# Application Hooks

> `MMCO_HOOK_APP_INITIALIZED` · `MMCO_HOOK_APP_SHUTDOWN` · `MMCO_HOOK_SETTINGS_CHANGED`

These hooks cover the application lifecycle: initialization, shutdown, and
settings changes.  They are the broadest hooks in the system — every plugin that
needs to coordinate with the launcher's startup/teardown or react to
configuration changes will use at least one of these.

---

## `MMCO_HOOK_APP_INITIALIZED`

### Summary

| Property | Value |
|---|---|
| **Constant** | `MMCO_HOOK_APP_INITIALIZED` |
| **Hex value** | `0x0100` |
| **Payload** | `nullptr` |
| **Cancellable** | No |
| **Thread** | Main (GUI) thread |
| **Fires** | Exactly once per application run |

### When it fires

Dispatched at the end of `PluginManager::initializeAll()`, **after** every
loaded module's `mmco_init()` has returned successfully.

**Exact dispatch location** — `PluginManager.cpp`:

```cpp
void PluginManager::initializeAll()
{
    // ... for each module: call mmco_init(&ctx) ...

    // Fire app-initialized hook
    dispatchHook(MMCO_HOOK_APP_INITIALIZED);
}
```

At the point this hook fires:

- All modules listed in the plugin manifest have been loaded and initialized.
- All `MMCOContext` function pointers are fully wired and usable.
- The `PluginManager`'s `m_hooks` map contains all registrations made during
  `mmco_init()`.
- The main window has **not yet been shown** — `UI_MAIN_READY` fires later.
- Instance list has been loaded and is accessible via `instance_count()` /
  `instance_get_id()`.

### Payload

`nullptr`.  The `payload` parameter passed to your callback is always `nullptr`
for this hook.  Do not dereference it.

### Return value

The launcher ignores the return value of `dispatchHook()` for this hook.
Callbacks should return `0`.

### Dispatch ordering

Callbacks fire in registration order, which is the order modules were loaded.
Since `APP_INITIALIZED` fires after all `mmco_init()` calls, the ordering is:

1. Module A's `mmco_init()` registers callback → position 0
2. Module B's `mmco_init()` registers callback → position 1
3. `dispatchHook(MMCO_HOOK_APP_INITIALIZED)` → fires A's callback, then B's

### Use cases

- Performing one-time initialization that depends on other modules being loaded
  (e.g., checking whether a specific companion plugin is present).
- Setting up deferred hook registrations (Pattern 4 from the overview).
- Querying the instance list for initial state.
- Starting background timers or worker threads.

### Example — Deferred setup after all modules are ready

```cpp
static MMCOContext* g_ctx = nullptr;

static int on_pre_launch(void* mh, uint32_t hook_id,
                         void* payload, void* ud)
{
    auto* info = static_cast<MMCOInstanceInfo*>(payload);
    if (!info) return 0;

    g_ctx->log_info(g_ctx->module_handle,
        (std::string("Launching: ") + info->instance_name).c_str());
    return 0;
}

static int on_app_initialized(void* mh, uint32_t hook_id,
                               void* payload, void* ud)
{
    MMCO_LOG(g_ctx, "All modules loaded — registering runtime hooks.");

    /* Now safe to register hooks that depend on full launcher state */
    g_ctx->hook_register(g_ctx->module_handle,
                         MMCO_HOOK_INSTANCE_PRE_LAUNCH,
                         on_pre_launch, nullptr);

    /* Query initial instance count */
    int count = g_ctx->instance_count(g_ctx->module_handle);
    char buf[128];
    snprintf(buf, sizeof(buf), "Found %d existing instances.", count);
    MMCO_LOG(g_ctx, buf);

    return 0;
}

MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;
    ctx->hook_register(ctx->module_handle,
                       MMCO_HOOK_APP_INITIALIZED,
                       on_app_initialized, nullptr);
    return 0;
}
```

### Example — One-shot self-unregistering callback

```cpp
static int on_app_initialized_once(void* mh, uint32_t hook_id,
                                    void* payload, void* ud)
{
    MMCO_LOG(g_ctx, "One-time init complete.");

    /* Unregister self — will not fire again (it only fires once
       anyway, but this demonstrates the pattern) */
    g_ctx->hook_unregister(g_ctx->module_handle,
                           MMCO_HOOK_APP_INITIALIZED,
                           on_app_initialized_once);
    return 0;
}
```

### Notes

- This hook fires **before** the main window is created/shown.  If your plugin
  needs the UI to be ready, register for `MMCO_HOOK_UI_MAIN_READY` instead.
- Registering for `APP_INITIALIZED` inside `mmco_init()` is guaranteed to work
  because the hook fires after all `mmco_init()` calls complete.
- If `mmco_init()` for a module returns non-zero (failure), that module's hooks
  are **not** registered (since `mmco_init()` failed before it could call
  `hook_register()`).  But the hook still fires for all successfully initialized
  modules.

---

## `MMCO_HOOK_APP_SHUTDOWN`

### Summary

| Property | Value |
|---|---|
| **Constant** | `MMCO_HOOK_APP_SHUTDOWN` |
| **Hex value** | `0x0101` |
| **Payload** | `nullptr` |
| **Cancellable** | No |
| **Thread** | Main (GUI) thread |
| **Fires** | Exactly once per application run |

### When it fires

Dispatched at the beginning of `PluginManager::shutdownAll()`, **before** any
module's `mmco_unload()` is called and **before** hook registrations are cleared.

**Exact dispatch location** — `PluginManager.cpp`:

```cpp
void PluginManager::shutdownAll()
{
    // Fire shutdown hook before unloading
    dispatchHook(MMCO_HOOK_APP_SHUTDOWN);

    // Unload in reverse order
    for (int i = m_modules.size() - 1; i >= 0; --i) {
        // ... call mmco_unload() ...
    }

    // Remove all hook registrations
    m_hooks.clear();

    // Close shared libraries
    // ...
}
```

At the point this hook fires:

- All modules are still initialized — their `mmco_unload()` has not been called.
- All `MMCOContext` function pointers are still valid.
- All hook registrations are still in place — you can even register new hooks
  (though there's no point, since shutdown is imminent).
- The main window may or may not still be visible (depends on shutdown ordering).
- Instance data is still accessible.

### Payload

`nullptr`.  Do not dereference the `payload` pointer.

### Return value

The launcher ignores the return value.  Callbacks should return `0`.

### Dispatch ordering

Callbacks fire in registration order (same as all hooks).  Since `APP_SHUTDOWN`
fires before `mmco_unload()`, the shutdown sequence is:

1. `dispatchHook(MMCO_HOOK_APP_SHUTDOWN)` — all registered callbacks fire
2. Module Z's `mmco_unload()` (reverse loading order)
3. Module Y's `mmco_unload()`
4. ...
5. `m_hooks.clear()` — all registrations wiped

This two-phase design means plugins get a "heads-up" (the hook) before the
actual teardown (`mmco_unload()`), allowing them to do cross-module coordination
that wouldn't be possible during `mmco_unload()` (where some modules may already
be torn down).

### Use cases

- Persisting plugin state to disk (settings, caches, databases).
- Flushing log buffers.
- Sending analytics or telemetry (non-blocking).
- Gracefully closing network connections or file handles.
- Notifying companion plugins of imminent shutdown.

### Example — Save state on shutdown

```cpp
struct PluginState {
    int total_launches;
    char config_path[512];
};

static PluginState g_state;

static int on_shutdown(void* mh, uint32_t hook_id,
                       void* payload, void* ud)
{
    auto* state = static_cast<PluginState*>(ud);

    /* Persist launch count to a file */
    FILE* f = fopen(state->config_path, "w");
    if (f) {
        fprintf(f, "total_launches=%d\n", state->total_launches);
        fclose(f);
    }

    MMCO_LOG(g_ctx, "State saved. Goodbye.");
    return 0;
}

MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;
    snprintf(g_state.config_path, sizeof(g_state.config_path),
             "%s/my_plugin_state.txt", /* ... get data dir ... */);

    ctx->hook_register(ctx->module_handle,
                       MMCO_HOOK_APP_SHUTDOWN,
                       on_shutdown, &g_state);
    return 0;
}
```

### Example — Shutdown coordination between plugins

```cpp
static int on_shutdown(void* mh, uint32_t hook_id,
                       void* payload, void* ud)
{
    /* Check if a companion plugin's setting still exists
       (all modules still alive at this point) */
    const char* val = g_ctx->setting_get(g_ctx->module_handle,
                                         "plugin.companion.api_key");
    if (val) {
        MMCO_LOG(g_ctx, "Companion plugin still active — "
                         "sending shutdown notification.");
        /* ... */
    }
    return 0;
}
```

### Notes

- Do **not** call `hook_unregister()` for hooks other than your own during
  shutdown.  Each module should only manage its own registrations.
- After `APP_SHUTDOWN` fires, the next step is `mmco_unload()`.  Any resources
  your shutdown callback depends on must still be valid at `mmco_unload()` time
  as well — `APP_SHUTDOWN` is not a replacement for `mmco_unload()`.
- If the application crashes, `APP_SHUTDOWN` may not fire.  Do not rely on it
  for critical data integrity — use periodic autosaves instead.

---

## `MMCO_HOOK_SETTINGS_CHANGED`

### Summary

| Property | Value |
|---|---|
| **Constant** | `MMCO_HOOK_SETTINGS_CHANGED` |
| **Hex value** | `0x0300` |
| **Payload** | `MMCOSettingChange*` |
| **Cancellable** | No |
| **Thread** | Main (GUI) thread |
| **Fires** | Every time a setting is modified via `setting_set()` |

### When it fires

Dispatched by `PluginManager::api_setting_set()` after a setting value is
updated in the internal settings store.  This covers both:

- **Global launcher settings** changed via the Settings dialog.
- **Plugin-scoped settings** changed via `ctx->setting_set()`.

The hook fires **after** the new value has been written — by the time your
callback executes, `setting_get()` will return the new value.

### Payload — `MMCOSettingChange`

```cpp
struct MMCOSettingChange {
    const char* key;        /* The full setting key */
    const char* old_value;  /* Previous value, or nullptr */
    const char* new_value;  /* New value */
};
```

#### Field reference

| Field | Type | Nullable | Allocator | Lifetime | Description |
|---|---|---|---|---|---|
| `key` | `const char*` | No | Launcher (stack `QByteArray`) | Callback duration only | The full, namespaced setting key. Plugin settings use the prefix `plugin.<module_id>.` (e.g., `"plugin.backup-system.interval"`). Global settings use their raw key name (e.g., `"JavaPath"`). |
| `old_value` | `const char*` | **Yes** | Launcher (stack `QByteArray`) | Callback duration only | The previous value of the setting as a UTF-8 string. `nullptr` if the setting was newly created (no previous value existed). |
| `new_value` | `const char*` | No | Launcher (stack `QByteArray`) | Callback duration only | The new value of the setting as a UTF-8 string. |

#### Memory / ownership

The `MMCOSettingChange` struct is **stack-allocated** by the launcher in the
`api_setting_set()` trampoline.  All `const char*` fields point to
`QByteArray::constData()` on the same stack frame.  **Copy immediately** if you
need the values after the callback returns.

```cpp
/* Safe — copies the string */
std::string saved_value = change->new_value;

/* UNSAFE — dangling pointer after callback returns */
const char* dangling = change->new_value;  // DO NOT DO THIS
```

### Return value

The launcher ignores the return value.  Callbacks should return `0`.

### Use cases

- Reacting to changes in your own plugin's settings (e.g., updating a timer
  interval, toggling a feature).
- Monitoring global settings for values that affect your plugin (e.g., Java
  path changes, proxy settings).
- Logging all settings changes for debugging/audit purposes.

### Example — React to own setting change

```cpp
static int backup_interval_minutes = 30;  /* default */

static int on_settings_changed(void* mh, uint32_t hook_id,
                                void* payload, void* ud)
{
    auto* change = static_cast<MMCOSettingChange*>(payload);
    if (!change || !change->key)
        return 0;

    /* Only react to our own settings */
    if (strcmp(change->key, "plugin.backup-system.interval") == 0) {
        int new_val = atoi(change->new_value);
        if (new_val > 0) {
            backup_interval_minutes = new_val;
            char buf[128];
            snprintf(buf, sizeof(buf),
                     "Backup interval changed: %d → %d minutes",
                     change->old_value ? atoi(change->old_value) : 0,
                     new_val);
            MMCO_LOG(g_ctx, buf);
        }
    }
    return 0;
}

MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;

    /* Set a default if not already present */
    const char* existing = ctx->setting_get(ctx->module_handle,
                                            "plugin.backup-system.interval");
    if (!existing) {
        ctx->setting_set(ctx->module_handle,
                         "plugin.backup-system.interval", "30");
    } else {
        backup_interval_minutes = atoi(existing);
    }

    ctx->hook_register(ctx->module_handle,
                       MMCO_HOOK_SETTINGS_CHANGED,
                       on_settings_changed, nullptr);
    return 0;
}
```

### Example — Monitor Java path changes

```cpp
static int on_settings_changed(void* mh, uint32_t hook_id,
                                void* payload, void* ud)
{
    auto* change = static_cast<MMCOSettingChange*>(payload);
    if (!change || !change->key)
        return 0;

    if (strcmp(change->key, "JavaPath") == 0) {
        g_ctx->log_info(g_ctx->module_handle,
            (std::string("Java path changed to: ") + change->new_value).c_str());
        /* Invalidate any cached Java version info */
        invalidate_java_cache();
    }
    return 0;
}
```

### Example — Log all setting changes (debug plugin)

```cpp
static int on_any_setting(void* mh, uint32_t hook_id,
                          void* payload, void* ud)
{
    auto* change = static_cast<MMCOSettingChange*>(payload);
    if (!change)
        return 0;

    char buf[512];
    snprintf(buf, sizeof(buf), "SETTING: [%s] '%s' → '%s'",
             change->key,
             change->old_value ? change->old_value : "(null)",
             change->new_value);
    g_ctx->log_debug(g_ctx->module_handle, buf);
    return 0;
}
```

### Notes

- `SETTINGS_CHANGED` fires for **every** call to `setting_set()`, including
  calls made by other plugins.  Filter on the `key` prefix to avoid reacting to
  irrelevant changes.
- Setting a value to its current value **still** fires the hook.  The launcher
  does not deduplicate.
- Calling `setting_set()` from within a `SETTINGS_CHANGED` callback will trigger
  another dispatch — be careful to avoid infinite recursion.  Guard with a
  comparison: only call `setting_set()` if the value actually differs.
- The `old_value` field is `nullptr` only when the key did not previously exist.
  For keys that existed with an empty string value, `old_value` will be `""`
  (an empty string), not `nullptr`.

---

## Application hook dispatch timeline

This diagram shows when each application-category hook fires relative to other
launcher lifecycle events:

```
Application::main()
  │
  ├─ PluginManager::initializeAll()
  │    ├─ Module A: mmco_init(&ctx)  →  registers hooks
  │    ├─ Module B: mmco_init(&ctx)  →  registers hooks
  │    └─ dispatchHook(MMCO_HOOK_APP_INITIALIZED)   ◄── [1]
  │
  ├─ MainWindow created and shown
  │    └─ dispatchHook(MMCO_HOOK_UI_MAIN_READY)     (UI hook, not covered here)
  │
  │  ... application running ...
  │
  ├─ User changes a setting in the Settings dialog
  │    └─ api_setting_set()
  │         └─ dispatchHook(MMCO_HOOK_SETTINGS_CHANGED, &change)  ◄── [2]
  │
  ├─ Plugin calls ctx->setting_set()
  │    └─ api_setting_set()
  │         └─ dispatchHook(MMCO_HOOK_SETTINGS_CHANGED, &change)  ◄── [2]
  │
  │  ... more runtime events ...
  │
  └─ PluginManager::shutdownAll()
       ├─ dispatchHook(MMCO_HOOK_APP_SHUTDOWN)       ◄── [3]
       ├─ Module B: mmco_unload()
       ├─ Module A: mmco_unload()
       ├─ m_hooks.clear()
       └─ Shared libraries closed
```

**Legend:**

- **[1] `APP_INITIALIZED`** — all modules loaded, contexts valid, UI not ready
- **[2] `SETTINGS_CHANGED`** — fires zero or more times during runtime
- **[3] `APP_SHUTDOWN`** — all modules still alive, about to be unloaded

---

## See also

- [Instance hooks](instance-hooks.md) — `INSTANCE_PRE_LAUNCH`, `INSTANCE_POST_LAUNCH`, `INSTANCE_CREATED`, `INSTANCE_REMOVED`
- [UI hooks](ui-hooks.md) — `UI_MAIN_READY`, `UI_INSTANCE_PAGES`, `UI_CONTEXT_MENU`
- [Hooks Reference overview](README.md) — dispatch model, registration API recap, common patterns
- [API Reference § Lifecycle — Hooks](../api-reference/s01-lifecycle/hooks.md) — `hook_register()` / `hook_unregister()` internals
- [API Reference § Settings](../api-reference/s02-settings/) — `setting_get()` / `setting_set()` functions
