# Hooks — `hook_register()`, `hook_unregister()`

> **Headers:** `PluginAPI.h` (function pointers), `PluginHooks.h` (hook IDs, callback typedef, payload structs) · **Implementation:** `PluginManager.h`, `PluginManager.cpp`

---

## Overview

The MMCO hook system is an **observer-pattern** event bus. Plugins register callback functions for specific hook IDs, and MeshMC dispatches to all registered callbacks when the corresponding event occurs.

Hooks are the primary mechanism for plugins to **react** to launcher events: instance launches, content downloads, UI construction, settings changes, application lifecycle, and network requests.

### Key concepts

- **Hook ID** — a `uint32_t` constant from the `MMCOHookId` enum identifying the event type.
- **Hook callback** — a C function matching the `MMCOHookCallback` typedef.
- **Payload** — a `void*` pointer to a hook-specific struct (or `nullptr`). The concrete type depends on the hook ID.
- **Cancellation** — for "pre" hooks, returning non-zero from a callback signals cancellation to the launcher. "Post" and notification hooks ignore the return value.
- **Registration** — a tuple of `(module_handle, hook_id, callback, user_data)` stored in `PluginManager::m_hooks`.

---

## `hook_register()`

### Signature

```cpp
int (*hook_register)(void* mh, uint32_t hook_id,
                     MMCOHookCallback callback, void* user_data);
```

### Parameters

| Parameter | Type | Description |
|---|---|---|
| `mh` | `void*` | The module handle. Must be `ctx->module_handle`. |
| `hook_id` | `uint32_t` | One of the `MMCOHookId` constants (e.g., `MMCO_HOOK_INSTANCE_PRE_LAUNCH`). See [Hook ID reference](#hook-id-reference) below. |
| `callback` | `MMCOHookCallback` | A function pointer matching the hook callback signature. Must not be `nullptr`. |
| `user_data` | `void*` | An arbitrary pointer passed to the callback when the hook fires. May be `nullptr`. |

### Return value

| Value | Meaning |
|---|---|
| `0` | Success — the callback was registered. |
| `-1` | Failure — `callback` was `nullptr`. |

### Description

Registers a callback to be called whenever the specified hook fires. Multiple callbacks can be registered for the same hook ID, both from the same module and from different modules. All registered callbacks are called in insertion order.

A module can register the **same callback function** for the **same hook ID** multiple times (with different or identical `user_data`). Each registration is independent and will result in the callback being invoked once per registration.

### Ownership

- `callback` — the plugin is responsible for ensuring the function pointer remains valid for the lifetime of the registration. Since MMCO callbacks are typically static/global functions, this is usually automatic.
- `user_data` — the plugin owns this pointer. The launcher stores it but never dereferences, frees, or modifies it.

### Thread safety

Must be called from the **main (GUI) thread** only. The internal `QMultiMap::insert()` is not thread-safe.

### Implementation (trampoline)

**Declared in `PluginManager.h`:**

```cpp
static int api_hook_register(void* mh, uint32_t hook_id,
                             MMCOHookCallback cb, void* ud);
```

**Implemented in `PluginManager.cpp`:**

```cpp
int PluginManager::api_hook_register(void* mh, uint32_t hook_id,
                                     MMCOHookCallback cb, void* ud)
{
    auto* r = rt(mh);
    if (!cb)
        return -1;

    HookRegistration reg;
    reg.module_handle = mh;
    reg.callback = cb;
    reg.user_data = ud;

    r->manager->m_hooks.insert(hook_id, reg);
    return 0;
}
```

**Trampoline walkthrough:**

1. `rt(mh)` — resolve the `ModuleRuntime*`.
2. Null-check on `cb` — the only validation. Invalid `hook_id` values are silently accepted (the callback will simply never fire).
3. Construct a `HookRegistration` with the module handle, callback, and user data.
4. Insert into `m_hooks` — a `QMultiMap<uint32_t, HookRegistration>` keyed by hook ID.

### Wiring

```cpp
ctx.hook_register = api_hook_register;
```

### Internal: `HookRegistration`

```cpp
struct HookRegistration {
    void*            module_handle;
    MMCOHookCallback callback;
    void*            user_data;
};
```

This is stored in `PluginManager::m_hooks`, a `QMultiMap<uint32_t, HookRegistration>`. Multiple registrations for the same `hook_id` are stored as multiple values under that key.

### Example: registering for instance pre-launch

```cpp
static int on_pre_launch(void* mh, uint32_t hook_id,
                         void* payload, void* ud)
{
    auto* info = static_cast<MMCOInstanceInfo*>(payload);
    if (!info)
        return 0;

    /* Log the launch */
    g_ctx->log_info(g_ctx->module_handle,
                    (std::string("Pre-launch: ") + info->instance_name).c_str());

    /* Return 0 to allow launch, non-zero to cancel */
    return 0;
}

MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;

    int rc = ctx->hook_register(ctx->module_handle,
                                MMCO_HOOK_INSTANCE_PRE_LAUNCH,
                                on_pre_launch, nullptr);
    if (rc != 0) {
        MMCO_ERR(ctx, "Failed to register pre-launch hook");
        return rc;
    }
    return 0;
}
```

### Example: passing user_data

```cpp
struct MyPluginState {
    int launch_count;
    bool logging_enabled;
};

static MyPluginState g_state = { 0, true };

static int on_post_launch(void* /*mh*/, uint32_t /*hook_id*/,
                          void* payload, void* ud)
{
    auto* state = static_cast<MyPluginState*>(ud);
    state->launch_count++;
    return 0;
}

MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;

    /* Pass &g_state as user_data — will be received in on_post_launch() */
    ctx->hook_register(ctx->module_handle,
                       MMCO_HOOK_INSTANCE_POST_LAUNCH,
                       on_post_launch, &g_state);
    return 0;
}
```

---

## `hook_unregister()`

### Signature

```cpp
int (*hook_unregister)(void* mh, uint32_t hook_id,
                       MMCOHookCallback callback);
```

### Parameters

| Parameter | Type | Description |
|---|---|---|
| `mh` | `void*` | The module handle. Must be `ctx->module_handle`. |
| `hook_id` | `uint32_t` | The hook ID to unregister from. |
| `callback` | `MMCOHookCallback` | The specific callback function pointer to remove. |

### Return value

| Value | Meaning |
|---|---|
| `0` | Success — a matching registration was found and removed. |
| `-1` | Failure — no matching registration was found. |

### Description

Removes the **first** registration that matches all three criteria:

1. Same `module_handle` as the caller (`mh`).
2. Same `hook_id`.
3. Same `callback` function pointer.

If the same callback was registered multiple times for the same hook (with different `user_data`), only one registration is removed per call. To remove all, call `hook_unregister()` repeatedly until it returns `-1`.

The `user_data` value is **not** used for matching. If you registered the same callback with different `user_data` values, you cannot selectively unregister by `user_data` — only by callback pointer.

### Automatic cleanup

When `PluginManager::shutdownAll()` is called, **all** hook registrations are cleared:

```cpp
m_hooks.clear();
```

This happens after all `mmco_unload()` calls. Plugins are not required to manually unregister their hooks — it is done automatically. However, plugins **may** unregister hooks during their lifetime if they want to stop receiving certain events (e.g., after a one-shot notification).

### Thread safety

Must be called from the **main (GUI) thread** only. The internal `QMultiMap` iteration and erasure is not thread-safe.

### Implementation (trampoline)

**Declared in `PluginManager.h`:**

```cpp
static int api_hook_unregister(void* mh, uint32_t hook_id,
                               MMCOHookCallback cb);
```

**Implemented in `PluginManager.cpp`:**

```cpp
int PluginManager::api_hook_unregister(void* mh, uint32_t hook_id,
                                       MMCOHookCallback cb)
{
    auto* r = rt(mh);
    auto& hooks = r->manager->m_hooks;

    auto range = hooks.equal_range(hook_id);
    for (auto it = range.first; it != range.second; ++it) {
        if (it.value().module_handle == mh && it.value().callback == cb) {
            hooks.erase(it);
            return 0;
        }
    }
    return -1;
}
```

**Trampoline walkthrough:**

1. `rt(mh)` — resolve the `ModuleRuntime*`.
2. `hooks.equal_range(hook_id)` — get all registrations for this hook ID.
3. Linear scan for a registration matching both `module_handle` and `callback`.
4. `hooks.erase(it)` — remove the first match and return `0`.
5. If no match found, return `-1`.

### Wiring

```cpp
ctx.hook_unregister = api_hook_unregister;
```

### Example: one-shot hook

```cpp
static int on_app_initialized(void* mh, uint32_t hook_id,
                              void* /*payload*/, void* /*ud*/)
{
    MMCO_LOG(g_ctx, "App initialized — performing one-time setup.");

    /* Unregister self so this only fires once */
    g_ctx->hook_unregister(g_ctx->module_handle,
                           MMCO_HOOK_APP_INITIALIZED,
                           on_app_initialized);
    return 0;
}
```

---

## Hook dispatch mechanism

Understanding how hooks are dispatched helps predict callback ordering and cancellation behaviour.

### `PluginManager::dispatchHook()`

```cpp
bool PluginManager::dispatchHook(uint32_t hook_id, void* payload)
{
    auto range = m_hooks.equal_range(hook_id);
    for (auto it = range.first; it != range.second; ++it) {
        const auto& reg = it.value();
        int rc = reg.callback(reg.module_handle, hook_id, payload, reg.user_data);
        if (rc != 0) {
            return true;  // cancelled
        }
    }
    return false;
}
```

### Dispatch rules

1. **All registered callbacks** for the given `hook_id` are iterated in `QMultiMap` insertion order.
2. **Each callback receives:** its own `module_handle`, the `hook_id`, the `payload`, and its `user_data`.
3. **Cancellation:** if any callback returns non-zero, iteration stops immediately and `dispatchHook()` returns `true` (cancelled).
4. **Non-cancellable hooks:** for hooks where cancellation doesn't make semantic sense (post-event notifications, `APP_SHUTDOWN`), the launcher may ignore the return value of `dispatchHook()`. Callbacks should still return `0` for forward compatibility.
5. **Payload mutation:** callbacks receive a raw `void*` to the payload struct. Mutations to payload fields are visible to subsequent callbacks (and to the launcher). Modify payloads with caution — this is a feature for "pre" hooks that need to alter event data.

### Dispatch timing in the application lifecycle

```
initializeAll()
    └─ for each module: mmco_init(&ctx)
    └─ dispatchHook(MMCO_HOOK_APP_INITIALIZED)          ← all modules loaded

... application running ...
    dispatchHook(MMCO_HOOK_INSTANCE_PRE_LAUNCH, &info)   ← before launch
    dispatchHook(MMCO_HOOK_INSTANCE_POST_LAUNCH, &info)  ← after launch
    dispatchHook(MMCO_HOOK_UI_INSTANCE_PAGES, &evt)      ← dialog building
    dispatchHook(MMCO_HOOK_SETTINGS_CHANGED, &change)    ← setting updated
    ...

shutdownAll()
    └─ dispatchHook(MMCO_HOOK_APP_SHUTDOWN)              ← before unload
    └─ for each module (reverse order): mmco_unload()
```

---

## `MMCOHookCallback` — the callback typedef

### Declaration (from `PluginHooks.h`)

```cpp
typedef int (*MMCOHookCallback)(void* module_handle,
                                uint32_t hook_id,
                                void* payload,
                                void* user_data);
```

### Parameters delivered to the callback

| Parameter | Type | Description |
|---|---|---|
| `module_handle` | `void*` | The calling module's opaque handle (same value as `ctx->module_handle`). |
| `hook_id` | `uint32_t` | The hook ID that fired. Useful if a single callback handles multiple hooks. |
| `payload` | `void*` | Pointer to a hook-specific payload struct, or `nullptr`. See [Payload structs](#payload-structs) below. |
| `user_data` | `void*` | The arbitrary pointer passed during `hook_register()`. |

### Return value

| Value | Meaning |
|---|---|
| `0` | Continue — allow the hook chain to proceed, and allow the launcher to continue the operation. |
| Non-zero (typically `1`) | Cancel — stop the hook chain and signal cancellation to the launcher. Only effective for "pre" hooks. |

### Signature requirements

The callback must be a **plain C function** (or `extern "C"` C++ function, or a static member function with no captures). It cannot be a lambda with captures, a `std::function`, or a non-static member function.

### Example callback template

```cpp
static int my_hook_callback(void* mh, uint32_t hook_id,
                            void* payload, void* ud)
{
    /* Cast payload to the appropriate type for this hook_id */
    /* ... process the event ... */
    return 0;  /* or non-zero to cancel */
}
```

---

## Hook ID reference

All hook IDs are defined in the `MMCOHookId` enum in `PluginHooks.h`.

### Application lifecycle hooks

| Constant | Value | Payload | Cancellable | Description |
|---|---|---|---|---|
| `MMCO_HOOK_APP_INITIALIZED` | `0x0100` | `nullptr` | No | Fired once after all modules have been initialized (`mmco_init()` called). The launcher UI may or may not be visible yet. |
| `MMCO_HOOK_APP_SHUTDOWN` | `0x0101` | `nullptr` | No | Fired once before modules are unloaded. All modules still have valid contexts. Use this to persist state, close files, etc. |

### Instance lifecycle hooks

| Constant | Value | Payload | Cancellable | Description |
|---|---|---|---|---|
| `MMCO_HOOK_INSTANCE_PRE_LAUNCH` | `0x0200` | `MMCOInstanceInfo*` | **Yes** | Fired before an instance launches. Return non-zero to cancel the launch. |
| `MMCO_HOOK_INSTANCE_POST_LAUNCH` | `0x0201` | `MMCOInstanceInfo*` | No | Fired after an instance has been launched. |
| `MMCO_HOOK_INSTANCE_CREATED` | `0x0202` | `MMCOInstanceInfo*` | No | Fired when a new instance is created (by user or programmatically). |
| `MMCO_HOOK_INSTANCE_REMOVED` | `0x0203` | `MMCOInstanceInfo*` | No | Fired when an instance is deleted. |

### Settings hooks

| Constant | Value | Payload | Cancellable | Description |
|---|---|---|---|---|
| `MMCO_HOOK_SETTINGS_CHANGED` | `0x0300` | `MMCOSettingChange*` | No | Fired when any setting is modified (global or per-plugin). |

### Content management hooks

| Constant | Value | Payload | Cancellable | Description |
|---|---|---|---|---|
| `MMCO_HOOK_CONTENT_PRE_DOWNLOAD` | `0x0400` | `MMCOContentEvent*` | **Yes** | Fired before a mod/resource pack/etc. is downloaded. Return non-zero to cancel. |
| `MMCO_HOOK_CONTENT_POST_DOWNLOAD` | `0x0401` | `MMCOContentEvent*` | No | Fired after a content download completes. |

### Network hooks

| Constant | Value | Payload | Cancellable | Description |
|---|---|---|---|---|
| `MMCO_HOOK_NETWORK_PRE_REQUEST` | `0x0500` | `MMCONetworkEvent*` | **Yes** | Fired before any HTTP request is made. Return non-zero to block the request. |
| `MMCO_HOOK_NETWORK_POST_REQUEST` | `0x0501` | `MMCONetworkEvent*` | No | Fired after an HTTP request completes. `status_code` is populated. |

### UI extension hooks

| Constant | Value | Payload | Cancellable | Description |
|---|---|---|---|---|
| `MMCO_HOOK_UI_MAIN_READY` | `0x0600` | `nullptr` | No | Fired when the main window is fully visible and ready. |
| `MMCO_HOOK_UI_CONTEXT_MENU` | `0x0601` | `MMCOMenuEvent*` | No | Fired when a context menu is about to be shown. Plugins can add items. |
| `MMCO_HOOK_UI_INSTANCE_PAGES` | `0x0602` | `MMCOInstancePagesEvent*` | No | Fired when the instance settings dialog is building its page list. Plugins can add custom pages. |

---

## Payload structs

Each hook ID defines the concrete type of the `void* payload` parameter. Plugins must `static_cast` to the correct type.

### `MMCOInstanceInfo`

Used by: `MMCO_HOOK_INSTANCE_PRE_LAUNCH`, `MMCO_HOOK_INSTANCE_POST_LAUNCH`, `MMCO_HOOK_INSTANCE_CREATED`, `MMCO_HOOK_INSTANCE_REMOVED`

```cpp
struct MMCOInstanceInfo {
    const char* instance_id;       /* Unique instance identifier */
    const char* instance_name;     /* Human-readable instance name */
    const char* instance_path;     /* Absolute path to the instance directory */
    const char* minecraft_version; /* Minecraft version string (e.g. "1.20.4") */
};
```

| Field | Type | Nullable | Description |
|---|---|---|---|
| `instance_id` | `const char*` | No | The unique ID of the instance. Matches the value used in `instance_get_name()`, `instance_get_path()`, etc. |
| `instance_name` | `const char*` | No | Human-readable display name. |
| `instance_path` | `const char*` | No | Absolute filesystem path to the instance root directory. |
| `minecraft_version` | `const char*` | Possibly | The resolved Minecraft version. May be `nullptr` if the version cannot be determined (e.g., corrupted instance). |

**String lifetime:** all `const char*` fields point to storage owned by the launcher. They are valid only for the duration of the callback invocation. Copy immediately if needed after the callback returns.

**Example:**

```cpp
static int on_pre_launch(void* /*mh*/, uint32_t /*hook_id*/,
                         void* payload, void* /*ud*/)
{
    auto* info = static_cast<MMCOInstanceInfo*>(payload);
    if (!info)
        return 0;

    std::string msg = "Launching: " + std::string(info->instance_name)
                    + " (MC " + std::string(info->minecraft_version ? info->minecraft_version : "unknown")
                    + ")";
    g_ctx->log_info(g_ctx->module_handle, msg.c_str());

    /* Allow the launch to proceed */
    return 0;
}
```

### `MMCOSettingChange`

Used by: `MMCO_HOOK_SETTINGS_CHANGED`

```cpp
struct MMCOSettingChange {
    const char* key;       /* The full setting key (e.g., "plugin.backup.interval") */
    const char* old_value; /* Previous value as a string, or nullptr */
    const char* new_value; /* New value as a string */
};
```

| Field | Type | Nullable | Description |
|---|---|---|---|
| `key` | `const char*` | No | The full, namespaced setting key. For plugin settings, this includes the `plugin.<module_id>.` prefix. |
| `old_value` | `const char*` | Yes | The previous value of the setting. `nullptr` if the setting was newly created. |
| `new_value` | `const char*` | No | The new value of the setting. |

**Example:**

```cpp
static int on_settings_changed(void* /*mh*/, uint32_t /*hook_id*/,
                               void* payload, void* /*ud*/)
{
    auto* change = static_cast<MMCOSettingChange*>(payload);
    if (!change)
        return 0;

    std::string msg = std::string("Setting changed: ") + change->key
                    + " = " + change->new_value;
    g_ctx->log_info(g_ctx->module_handle, msg.c_str());
    return 0;
}
```

### `MMCOContentEvent`

Used by: `MMCO_HOOK_CONTENT_PRE_DOWNLOAD`, `MMCO_HOOK_CONTENT_POST_DOWNLOAD`

```cpp
struct MMCOContentEvent {
    const char* instance_id;  /* Instance receiving the content */
    const char* file_name;    /* Name of the file being downloaded */
    const char* url;          /* Source URL */
    const char* target_path;  /* Destination file path */
};
```

| Field | Type | Nullable | Description |
|---|---|---|---|
| `instance_id` | `const char*` | No | The instance the content is being downloaded for. |
| `file_name` | `const char*` | No | The filename of the content (e.g., `"sodium-0.5.8.jar"`). |
| `url` | `const char*` | No | The URL the content is being downloaded from. |
| `target_path` | `const char*` | No | The absolute filesystem path where the content will be saved. |

**Example: blocking downloads from untrusted sources**

```cpp
static int on_pre_download(void* /*mh*/, uint32_t /*hook_id*/,
                           void* payload, void* /*ud*/)
{
    auto* evt = static_cast<MMCOContentEvent*>(payload);
    if (!evt || !evt->url)
        return 0;

    /* Block downloads from non-HTTPS URLs */
    if (strncmp(evt->url, "https://", 8) != 0) {
        g_ctx->log_warn(g_ctx->module_handle,
                        (std::string("Blocking insecure download: ") + evt->url).c_str());
        return 1;  /* cancel the download */
    }
    return 0;
}
```

### `MMCONetworkEvent`

Used by: `MMCO_HOOK_NETWORK_PRE_REQUEST`, `MMCO_HOOK_NETWORK_POST_REQUEST`

```cpp
struct MMCONetworkEvent {
    const char* url;         /* Request URL */
    const char* method;      /* HTTP method: "GET", "POST", etc. */
    int         status_code; /* HTTP status code (0 for pre-request) */
};
```

| Field | Type | Nullable | Description |
|---|---|---|---|
| `url` | `const char*` | No | The request URL. |
| `method` | `const char*` | No | The HTTP method string. |
| `status_code` | `int` | N/A | For `PRE_REQUEST`: always `0`. For `POST_REQUEST`: the HTTP response status code (200, 404, etc.). |

**Example:**

```cpp
static int on_post_request(void* /*mh*/, uint32_t /*hook_id*/,
                           void* payload, void* /*ud*/)
{
    auto* evt = static_cast<MMCONetworkEvent*>(payload);
    if (!evt)
        return 0;

    if (evt->status_code >= 400) {
        std::string msg = std::string(evt->method) + " " + evt->url
                        + " failed: " + std::to_string(evt->status_code);
        g_ctx->log_warn(g_ctx->module_handle, msg.c_str());
    }
    return 0;
}
```

### `MMCOMenuEvent`

Used by: `MMCO_HOOK_UI_CONTEXT_MENU`

```cpp
struct MMCOMenuEvent {
    const char* context;     /* Menu context: "main", "instance", etc. */
    void*       menu_handle; /* Opaque handle for ui_add_menu_item() */
};
```

| Field | Type | Nullable | Description |
|---|---|---|---|
| `context` | `const char*` | No | Identifies which context menu is being shown. Values include `"main"` (main window), `"instance"` (instance right-click). |
| `menu_handle` | `void*` | No | An opaque handle that must be passed to `ctx->ui_add_menu_item()` to add items to this menu. |

**Example:**

```cpp
static void on_my_action(void* ud)
{
    MMCO_LOG(static_cast<MMCOContext*>(ud), "Custom menu item clicked!");
}

static int on_context_menu(void* /*mh*/, uint32_t /*hook_id*/,
                           void* payload, void* /*ud*/)
{
    auto* evt = static_cast<MMCOMenuEvent*>(payload);
    if (!evt || strcmp(evt->context, "instance") != 0)
        return 0;

    g_ctx->ui_add_menu_item(g_ctx->module_handle, evt->menu_handle,
                            "My Custom Action", on_my_action, g_ctx);
    return 0;
}
```

### `MMCOInstancePagesEvent`

Used by: `MMCO_HOOK_UI_INSTANCE_PAGES`

```cpp
struct MMCOInstancePagesEvent {
    const char* instance_id;       /* Instance being edited */
    const char* instance_name;     /* Instance display name */
    const char* instance_path;     /* Instance root directory */
    void*       page_list_handle;  /* Opaque: QList<BasePage*>* */
    void*       instance_handle;   /* Opaque: InstancePtr raw pointer */
};
```

| Field | Type | Nullable | Description |
|---|---|---|---|
| `instance_id` | `const char*` | No | The instance ID. |
| `instance_name` | `const char*` | No | The instance display name. |
| `instance_path` | `const char*` | No | Absolute path to the instance directory. |
| `page_list_handle` | `void*` | No | An opaque pointer to `QList<BasePage*>*`. Cast and `append()` to add pages. |
| `instance_handle` | `void*` | No | An opaque pointer to the raw `BaseInstance*`. Use to create a non-owning `InstancePtr`. |

**This is the most complex hook payload** because it involves Qt/MeshMC types. Plugin authors use `mmco_sdk.h` which provides `QList`, `BasePage`, `BaseInstance`, and `InstancePtr`.

**Example (from BackupSystem):**

```cpp
static int on_instance_pages(void* /*mh*/, uint32_t /*hook_id*/,
                             void* payload, void* /*ud*/)
{
    auto* evt = static_cast<MMCOInstancePagesEvent*>(payload);
    if (!evt || !evt->page_list_handle || !evt->instance_handle)
        return 0;

    auto* pages = static_cast<QList<BasePage*>*>(evt->page_list_handle);
    auto* instRaw = static_cast<BaseInstance*>(evt->instance_handle);

    /* Non-owning shared_ptr — the instance is managed elsewhere */
    InstancePtr inst = std::shared_ptr<BaseInstance>(
        instRaw, [](BaseInstance*) { /* no-op deleter */ });

    pages->append(new BackupPage(inst));
    return 0;
}
```

> **Important:** The `InstancePtr` created here uses a **no-op deleter**. The `BaseInstance` is owned by the launcher's `InstanceList`, not by the plugin. Deleting it would cause a crash.

---

## Advanced patterns

### Registering for multiple hooks with one callback

A single callback can handle multiple hook IDs by branching on the `hook_id` parameter:

```cpp
static int multi_handler(void* mh, uint32_t hook_id,
                         void* payload, void* ud)
{
    switch (hook_id) {
    case MMCO_HOOK_INSTANCE_PRE_LAUNCH: {
        auto* info = static_cast<MMCOInstanceInfo*>(payload);
        /* handle pre-launch */
        break;
    }
    case MMCO_HOOK_INSTANCE_POST_LAUNCH: {
        auto* info = static_cast<MMCOInstanceInfo*>(payload);
        /* handle post-launch */
        break;
    }
    case MMCO_HOOK_APP_SHUTDOWN:
        /* handle shutdown */
        break;
    }
    return 0;
}

MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;
    ctx->hook_register(ctx->module_handle, MMCO_HOOK_INSTANCE_PRE_LAUNCH,
                       multi_handler, nullptr);
    ctx->hook_register(ctx->module_handle, MMCO_HOOK_INSTANCE_POST_LAUNCH,
                       multi_handler, nullptr);
    ctx->hook_register(ctx->module_handle, MMCO_HOOK_APP_SHUTDOWN,
                       multi_handler, nullptr);
    return 0;
}
```

### Cancelling a pre-launch hook conditionally

```cpp
static int on_pre_launch(void* /*mh*/, uint32_t /*hook_id*/,
                         void* payload, void* ud)
{
    auto* info = static_cast<MMCOInstanceInfo*>(payload);
    auto* state = static_cast<MyPluginState*>(ud);

    if (!info || !info->minecraft_version)
        return 0;

    /* Block launching outdated Minecraft versions */
    if (strcmp(info->minecraft_version, "1.12.2") == 0 &&
        state->block_legacy)
    {
        g_ctx->log_warn(g_ctx->module_handle,
                        "Blocked launch of legacy MC 1.12.2");
        return 1;  /* CANCEL */
    }

    return 0;  /* allow */
}
```

### Unregistering during a callback

It is safe to call `hook_unregister()` from within a callback for the **same** hook that is currently being dispatched. The unregistered entry will be removed from the `QMultiMap`, and since `dispatchHook()` iterates with forward iterators over `equal_range()`, the removal of the current entry does not invalidate the iterator (Qt containers are designed for this pattern).

```cpp
static int one_shot_handler(void* mh, uint32_t hook_id,
                            void* payload, void* ud)
{
    /* Do one-time work... */
    MMCO_LOG(g_ctx, "One-shot hook fired — unregistering.");

    /* Safe to unregister self during dispatch */
    g_ctx->hook_unregister(g_ctx->module_handle, hook_id, one_shot_handler);
    return 0;
}
```

### Registering hooks inside `mmco_init()` vs. `APP_INITIALIZED` callback

Both patterns are valid:

**Pattern A: register in `mmco_init()`**

```cpp
MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;
    ctx->hook_register(ctx->module_handle, MMCO_HOOK_INSTANCE_PRE_LAUNCH,
                       on_pre_launch, nullptr);
    return 0;
}
```

**Pattern B: register in `APP_INITIALIZED` callback**

```cpp
static int on_app_init(void* /*mh*/, uint32_t /*hook_id*/,
                       void* /*payload*/, void* /*ud*/)
{
    g_ctx->hook_register(g_ctx->module_handle, MMCO_HOOK_INSTANCE_PRE_LAUNCH,
                         on_pre_launch, nullptr);
    return 0;
}

MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;
    ctx->hook_register(ctx->module_handle, MMCO_HOOK_APP_INITIALIZED,
                       on_app_init, nullptr);
    return 0;
}
```

Pattern A is simpler. Pattern B is useful when you need to query launcher state that may not be fully initialized during `mmco_init()` (e.g., the instance list).

---

## Error conditions

### `hook_register()` errors

| Condition | Behaviour |
|---|---|
| `mh` is `nullptr` | **Undefined behaviour.** `rt(nullptr)` will crash. |
| `callback` is `nullptr` | Returns `-1`. No registration is created. |
| `hook_id` is not a valid `MMCOHookId` value | Silent success (returns `0`). The callback is stored but will never be dispatched since the launcher never fires that hook ID. |
| `user_data` is `nullptr` | Valid. The callback will receive `nullptr` as its `ud` parameter. |
| Called after `mmco_unload()` | **Undefined behaviour.** The `ModuleRuntime` has been destroyed. |
| Called from a non-main thread | **Undefined behaviour.** `QMultiMap::insert()` is not thread-safe. |

### `hook_unregister()` errors

| Condition | Behaviour |
|---|---|
| `mh` is `nullptr` | **Undefined behaviour.** `rt(nullptr)` will crash. |
| No matching registration exists | Returns `-1`. No side effects. |
| `callback` was registered multiple times | Only the first matching entry is removed. Call again to remove more. |
| Called after `mmco_unload()` | **Undefined behaviour.** |
| Called from a non-main thread | **Undefined behaviour.** |

### Callback errors

| Condition | Behaviour |
|---|---|
| Callback throws a C++ exception | **Undefined behaviour.** Exceptions must not propagate through the C function-pointer boundary. The launcher may crash or enter an inconsistent state. |
| Callback returns non-zero for a non-cancellable hook | The return value is ignored by the launcher for non-cancellable hooks (post-events, notifications). However, `dispatchHook()` still returns `true` to its caller — some call sites may or may not check this. For safety, always return `0` for non-cancellable hooks. |
| Callback reads payload after returning | **Undefined behaviour.** Payload memory is stack-allocated by the launcher and invalidated when `dispatchHook()` returns. |
| Callback modifies payload fields | Allowed. Changes are visible to subsequent callbacks in the chain and to the launcher. |

---

## Internal: hook storage

Hooks are stored in `PluginManager::m_hooks`, declared as:

```cpp
QMultiMap<uint32_t, HookRegistration> m_hooks;
```

This is a `QMultiMap` (analogous to `std::multimap`) keyed by `hook_id` with `HookRegistration` values. The `QMultiMap` maintains insertion order for values with the same key, which defines callback execution order.

### Memory layout

```
m_hooks:
  0x0100 → [ {mh_A, cb1, ud1}, {mh_B, cb2, ud2} ]   ← APP_INITIALIZED
  0x0200 → [ {mh_A, cb3, ud3} ]                       ← INSTANCE_PRE_LAUNCH
  0x0602 → [ {mh_C, cb4, ud4}, {mh_A, cb5, ud5} ]    ← UI_INSTANCE_PAGES
```

### Cleanup

All registrations are cleared in `shutdownAll()`:

```cpp
void PluginManager::shutdownAll()
{
    dispatchHook(MMCO_HOOK_APP_SHUTDOWN);

    for (int i = m_modules.size() - 1; i >= 0; --i) {
        /* ... call mmco_unload() ... */
    }

    m_hooks.clear();            // ← removes ALL registrations
    /* ... unload shared libraries ... */
}
```

---

## Complete example: a monitoring plugin

This example demonstrates comprehensive hook usage — registering for multiple hooks, using `user_data`, cancellation, and unregistration.

```cpp
#include "plugin/sdk/mmco_sdk.h"
#include <string>
#include <vector>

static MMCOContext* g_ctx = nullptr;

struct MonitorState {
    int    total_launches;
    int    blocked_launches;
    bool   block_legacy;
    std::vector<std::string> launch_log;
};

static MonitorState g_state = { 0, 0, true, {} };

static int on_pre_launch(void* /*mh*/, uint32_t /*hook_id*/,
                         void* payload, void* ud)
{
    auto* info = static_cast<MMCOInstanceInfo*>(payload);
    auto* state = static_cast<MonitorState*>(ud);
    if (!info)
        return 0;

    state->total_launches++;

    /* Optionally block legacy versions */
    if (state->block_legacy && info->minecraft_version &&
        strcmp(info->minecraft_version, "1.12.2") == 0)
    {
        MMCO_WARN(g_ctx, "Blocked launch of legacy Minecraft 1.12.2");
        state->blocked_launches++;
        return 1;  /* cancel */
    }

    /* Log the launch */
    std::string entry = std::string(info->instance_name) + " (MC "
                      + (info->minecraft_version ? info->minecraft_version : "?")
                      + ")";
    state->launch_log.push_back(entry);

    MMCO_LOG(g_ctx, ("Launching: " + entry).c_str());
    return 0;
}

static int on_post_launch(void* /*mh*/, uint32_t /*hook_id*/,
                          void* payload, void* /*ud*/)
{
    auto* info = static_cast<MMCOInstanceInfo*>(payload);
    if (info) {
        MMCO_DBG(g_ctx, ("Post-launch: " + std::string(info->instance_name)).c_str());
    }
    return 0;
}

static int on_shutdown(void* /*mh*/, uint32_t /*hook_id*/,
                       void* /*payload*/, void* ud)
{
    auto* state = static_cast<MonitorState*>(ud);
    std::string summary = "Session summary: "
                        + std::to_string(state->total_launches) + " launches, "
                        + std::to_string(state->blocked_launches) + " blocked";
    MMCO_LOG(g_ctx, summary.c_str());
    return 0;
}

MMCO_DEFINE_MODULE("LaunchMonitor", "1.0.0", "Your Name",
                   "Monitors and optionally restricts instance launches",
                   "GPL-3.0-or-later");

extern "C" {

MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;
    MMCO_LOG(ctx, "LaunchMonitor v1.0.0 initializing...");

    ctx->hook_register(ctx->module_handle, MMCO_HOOK_INSTANCE_PRE_LAUNCH,
                       on_pre_launch, &g_state);
    ctx->hook_register(ctx->module_handle, MMCO_HOOK_INSTANCE_POST_LAUNCH,
                       on_post_launch, nullptr);
    ctx->hook_register(ctx->module_handle, MMCO_HOOK_APP_SHUTDOWN,
                       on_shutdown, &g_state);

    MMCO_LOG(ctx, "LaunchMonitor initialized.");
    return 0;
}

MMCO_EXPORT void mmco_unload()
{
    if (g_ctx) {
        MMCO_LOG(g_ctx, "LaunchMonitor unloading.");
    }
    g_ctx = nullptr;
}

} /* extern "C" */
```

---

## See also

- [module-handle.md](module-handle.md) — The `module_handle` parameter required by `hook_register()` and `hook_unregister()`
- [logging.md](logging.md) — Logging functions commonly used inside hook callbacks
- [README.md](README.md) — Section overview and lifecycle sequence diagram
- **Section 11 — Instance Pages** — Working with `MMCO_HOOK_UI_INSTANCE_PAGES` in depth
- **Section 12 — UI Dialogs** — Working with `MMCO_HOOK_UI_CONTEXT_MENU` and `ui_add_menu_item()`
