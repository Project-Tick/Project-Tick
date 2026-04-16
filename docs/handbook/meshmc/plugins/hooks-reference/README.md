# Hooks Reference

> **Source headers:** [`PluginHooks.h`](../../../../../meshmc/launcher/plugin/PluginHooks.h) (hook IDs, callback typedef, payload structs) · [`PluginAPI.h`](../../../../../meshmc/launcher/plugin/PluginAPI.h) (registration function pointers) · **Implementation:** [`PluginManager.cpp`](../../../../../meshmc/launcher/plugin/PluginManager.cpp)

---

## What this section covers

This is the **complete reference** for every hook ID in the MeshMC MMCO plugin
system.  It documents:

- Every `MMCOHookId` constant — its numeric value, when it fires, what payload
  it carries, whether it supports cancellation, and which thread dispatches it.
- Every payload struct — field-by-field, including types, nullability, ownership,
  and lifetime.
- The dispatch model — ordering guarantees, cancellation semantics, payload
  mutation rules.
- Real-world patterns extracted from the
  [BackupSystem](../../../../../meshmc/plugins/BackupSystem/) reference plugin.

### Sub-pages

| Page | Hook IDs covered |
|---|---|
| [Application hooks](app-hooks.md) | `MMCO_HOOK_APP_INITIALIZED` (`0x0100`), `MMCO_HOOK_APP_SHUTDOWN` (`0x0101`), `MMCO_HOOK_SETTINGS_CHANGED` (`0x0300`) |
| [Instance hooks](instance-hooks.md) | `MMCO_HOOK_INSTANCE_PRE_LAUNCH` (`0x0200`), `MMCO_HOOK_INSTANCE_POST_LAUNCH` (`0x0201`), `MMCO_HOOK_INSTANCE_CREATED` (`0x0202`), `MMCO_HOOK_INSTANCE_REMOVED` (`0x0203`) |
| [UI hooks](ui-hooks.md) | `MMCO_HOOK_UI_MAIN_READY` (`0x0600`), `MMCO_HOOK_UI_INSTANCE_PAGES` (`0x0602`), `MMCO_HOOK_UI_CONTEXT_MENU` (`0x0601`) |

> Content and network hooks (`0x04xx`, `0x05xx`) are documented in the
> [API Reference § Content](../api-reference/s05-content/) and
> [API Reference § Network](../api-reference/s06-network/) sections
> respectively.

---

## Hook dispatch model

### The observer pattern

MMCO hooks implement the **observer pattern**.  Plugins *subscribe* to named
events by registering callback functions, and the launcher *publishes* events by
iterating over all subscribers and invoking their callbacks in order.

```
┌──────────────────────────────────────────────────────────┐
│  Launcher code                                           │
│                                                          │
│  pluginManager()->dispatchHook(HOOK_ID, &payload);       │
│       │                                                  │
│       ▼                                                  │
│  ┌──────────────────────────────────────────┐            │
│  │ PluginManager::dispatchHook()            │            │
│  │   for each registration in m_hooks[id]:  │            │
│  │     rc = reg.callback(mh, id, pl, ud)    │            │
│  │     if rc != 0 → return true (cancelled) │            │
│  │   return false (all passed)              │            │
│  └──────────────────────────────────────────┘            │
└──────────────────────────────────────────────────────────┘
```

### `PluginManager::dispatchHook()` — the core loop

```cpp
bool PluginManager::dispatchHook(uint32_t hook_id, void* payload)
{
    auto range = m_hooks.equal_range(hook_id);
    for (auto it = range.first; it != range.second; ++it) {
        const auto& reg = it.value();
        int rc = reg.callback(reg.module_handle, hook_id,
                              payload, reg.user_data);
        if (rc != 0) {
            return true;   // cancelled
        }
    }
    return false;
}
```

Key properties:

1. **Storage** — `m_hooks` is a `QMultiMap<uint32_t, HookRegistration>`.
   Registrations for the same `hook_id` are stored as multiple values under that
   key.
2. **Iteration order** — `QMultiMap` preserves insertion order for equal keys.
   Callbacks fire in the order they were registered, which is the order modules
   were loaded (first module registered = first to fire).
3. **Short-circuit cancellation** — if *any* callback returns non-zero,
   iteration stops immediately and `dispatchHook()` returns `true`.
4. **Return value** — `true` means cancelled, `false` means all callbacks
   approved the event (or no callbacks were registered).

### Cancellation semantics

Not all hooks are cancellable.  The MeshMC codebase only checks the return value
of `dispatchHook()` for hooks that semantically support cancellation — the "pre"
hooks:

| Hook ID | Cancellable? | Behaviour if cancelled |
|---|---|---|
| `MMCO_HOOK_INSTANCE_PRE_LAUNCH` | **Yes** | Instance launch is aborted. |
| `MMCO_HOOK_CONTENT_PRE_DOWNLOAD` | **Yes** | Content download is skipped. |
| `MMCO_HOOK_NETWORK_PRE_REQUEST` | **Yes** | HTTP request is not sent. |
| All other hooks | No | Return value is ignored by the launcher. |

For non-cancellable hooks, callbacks should still return `0` for forward
compatibility — a future launcher version may start checking return values.

### Payload mutation

Callbacks receive a raw `void*` to the payload struct.  Because this is a plain
pointer (not a copy), **mutations to payload fields are visible to**:

- Subsequent callbacks in the same dispatch chain.
- The launcher code that created the payload (after `dispatchHook()` returns).

This is intentional for "pre" hooks.  For example, a plugin could modify
`MMCOContentEvent::url` to redirect a download through a mirror.  For "post"
and notification hooks, mutation has no effect since the launcher does not read
the payload afterwards.

> **Warning:** Modifying `const char*` fields by casting away `const` is
> undefined behaviour.  If you need to redirect a URL or change a setting, use
> the appropriate MMCO API functions instead.

### Thread guarantees

All hooks are dispatched on the **main (GUI) thread** (the Qt event loop
thread).  This is guaranteed by design — all dispatch call sites are in GUI-side
code (`MainWindow`, `LaunchController`, `PluginManager::initializeAll()`,
`PluginManager::shutdownAll()`).

Consequences:

- Callbacks may safely call any `MMCOContext` function pointer (all of which
  require the main thread).
- Callbacks must **not** block for extended periods — doing so freezes the UI.
- If a callback needs to do heavy work, it should schedule it on a worker thread
  (via platform threading) and return `0` immediately.

---

## Registration API recap

Full documentation is in [API Reference § Lifecycle — Hooks](../api-reference/s01-lifecycle/hooks.md).
This is a quick recap for convenience.

### `hook_register()`

```cpp
int (*hook_register)(void* mh, uint32_t hook_id,
                     MMCOHookCallback callback, void* user_data);
```

| Parameter | Description |
|---|---|
| `mh` | Must be `ctx->module_handle`. |
| `hook_id` | Any `MMCOHookId` constant. |
| `callback` | A plain C function pointer (no captures). Must not be `nullptr`. |
| `user_data` | Arbitrary pointer passed through to the callback. May be `nullptr`. |

Returns `0` on success, `-1` if `callback` is `nullptr`.

### `hook_unregister()`

```cpp
int (*hook_unregister)(void* mh, uint32_t hook_id,
                       MMCOHookCallback callback);
```

Removes the **first** registration matching `(mh, hook_id, callback)`.  Returns
`0` on success, `-1` if no match found.  Does **not** match on `user_data`.

### Automatic cleanup

All registrations are cleared during `PluginManager::shutdownAll()` (after all
`mmco_unload()` calls).  Plugins are not required to manually unregister.

---

## The callback signature

### `MMCOHookCallback`

```cpp
typedef int (*MMCOHookCallback)(void* module_handle,
                                uint32_t hook_id,
                                void* payload,
                                void* user_data);
```

### Parameters delivered to the callback

| Parameter | Type | Description |
|---|---|---|
| `module_handle` | `void*` | The calling module's opaque handle (same as `ctx->module_handle`). |
| `hook_id` | `uint32_t` | Which hook fired.  Useful when a single callback handles multiple hooks. |
| `payload` | `void*` | Pointer to the hook-specific payload struct, or `nullptr`.  Must be cast to the correct type for the hook ID. |
| `user_data` | `void*` | The pointer passed during `hook_register()`. |

### Return value semantics

| Value | Meaning |
|---|---|
| `0` | **Continue** — allow the hook chain to proceed and the launcher to continue the operation. |
| Non-zero (typically `1`) | **Cancel** — stop the hook chain.  Only effective for cancellable hooks (see table above). |

### Requirements

- Must be a **plain C function**, `extern "C"` function, or `static` member
  function.  Lambdas **with captures**, `std::function`, and non-static member
  functions cannot be used.
- Must be **reentrant** — the launcher does not guard against the same hook
  firing recursively (though in practice no hook triggers another hook of the
  same ID).
- Must execute **quickly**.  The callback runs on the main thread.

---

## Common patterns

### Pattern 1 — One callback per hook

The simplest and most common pattern.  Each hook gets its own dedicated callback.

```cpp
static int on_pre_launch(void* mh, uint32_t hook_id,
                         void* payload, void* ud)
{
    auto* info = static_cast<MMCOInstanceInfo*>(payload);
    /* ... */
    return 0;
}

static int on_shutdown(void* mh, uint32_t hook_id,
                       void* payload, void* ud)
{
    /* ... persist state ... */
    return 0;
}

MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;
    ctx->hook_register(ctx->module_handle,
                       MMCO_HOOK_INSTANCE_PRE_LAUNCH,
                       on_pre_launch, nullptr);
    ctx->hook_register(ctx->module_handle,
                       MMCO_HOOK_APP_SHUTDOWN,
                       on_shutdown, nullptr);
    return 0;
}
```

### Pattern 2 — Multi-hook dispatcher

A single callback handles several hook IDs by switching on `hook_id`.

```cpp
static int dispatcher(void* mh, uint32_t hook_id,
                      void* payload, void* ud)
{
    switch (hook_id) {
    case MMCO_HOOK_INSTANCE_PRE_LAUNCH:
        return handle_pre_launch(payload);
    case MMCO_HOOK_INSTANCE_POST_LAUNCH:
        return handle_post_launch(payload);
    case MMCO_HOOK_APP_SHUTDOWN:
        return handle_shutdown();
    default:
        return 0;
    }
}
```

### Pattern 3 — One-shot hook (self-unregistering)

```cpp
static int on_app_init(void* mh, uint32_t hook_id,
                       void* payload, void* ud)
{
    /* One-time initialization ... */
    g_ctx->hook_unregister(g_ctx->module_handle,
                           MMCO_HOOK_APP_INITIALIZED,
                           on_app_init);
    return 0;
}
```

### Pattern 4 — Deferred registration

Register for runtime hooks only after `APP_INITIALIZED`, ensuring the launcher
is fully ready.

```cpp
static int on_app_init(void* mh, uint32_t hook_id,
                       void* payload, void* ud)
{
    g_ctx->hook_register(g_ctx->module_handle,
                         MMCO_HOOK_UI_CONTEXT_MENU,
                         on_context_menu, nullptr);
    g_ctx->hook_register(g_ctx->module_handle,
                         MMCO_HOOK_UI_INSTANCE_PAGES,
                         on_instance_pages, nullptr);
    return 0;
}

MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;
    ctx->hook_register(ctx->module_handle,
                       MMCO_HOOK_APP_INITIALIZED,
                       on_app_init, nullptr);
    return 0;
}
```

### Pattern 5 — Passing state through `user_data`

```cpp
struct PluginState {
    int launch_count;
    bool block_legacy;
};

static PluginState g_state = { 0, true };

static int on_post_launch(void* mh, uint32_t hook_id,
                          void* payload, void* ud)
{
    auto* state = static_cast<PluginState*>(ud);
    state->launch_count++;
    return 0;
}

MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;
    ctx->hook_register(ctx->module_handle,
                       MMCO_HOOK_INSTANCE_POST_LAUNCH,
                       on_post_launch, &g_state);
    return 0;
}
```

---

## Master hook ID table

For quick reference.  Click through to sub-pages for full details.

| Constant | Hex | Category | Payload | Cancellable | Sub-page |
|---|---|---|---|---|---|
| `MMCO_HOOK_APP_INITIALIZED` | `0x0100` | App | `nullptr` | No | [app-hooks.md](app-hooks.md) |
| `MMCO_HOOK_APP_SHUTDOWN` | `0x0101` | App | `nullptr` | No | [app-hooks.md](app-hooks.md) |
| `MMCO_HOOK_INSTANCE_PRE_LAUNCH` | `0x0200` | Instance | `MMCOInstanceInfo*` | **Yes** | [instance-hooks.md](instance-hooks.md) |
| `MMCO_HOOK_INSTANCE_POST_LAUNCH` | `0x0201` | Instance | `MMCOInstanceInfo*` | No | [instance-hooks.md](instance-hooks.md) |
| `MMCO_HOOK_INSTANCE_CREATED` | `0x0202` | Instance | `MMCOInstanceInfo*` | No | [instance-hooks.md](instance-hooks.md) |
| `MMCO_HOOK_INSTANCE_REMOVED` | `0x0203` | Instance | `MMCOInstanceInfo*` | No | [instance-hooks.md](instance-hooks.md) |
| `MMCO_HOOK_SETTINGS_CHANGED` | `0x0300` | App | `MMCOSettingChange*` | No | [app-hooks.md](app-hooks.md) |
| `MMCO_HOOK_CONTENT_PRE_DOWNLOAD` | `0x0400` | Content | `MMCOContentEvent*` | **Yes** | — |
| `MMCO_HOOK_CONTENT_POST_DOWNLOAD` | `0x0401` | Content | `MMCOContentEvent*` | No | — |
| `MMCO_HOOK_NETWORK_PRE_REQUEST` | `0x0500` | Network | `MMCONetworkEvent*` | **Yes** | — |
| `MMCO_HOOK_NETWORK_POST_REQUEST` | `0x0501` | Network | `MMCONetworkEvent*` | No | — |
| `MMCO_HOOK_UI_MAIN_READY` | `0x0600` | UI | `nullptr` | No | [ui-hooks.md](ui-hooks.md) |
| `MMCO_HOOK_UI_CONTEXT_MENU` | `0x0601` | UI | `MMCOMenuEvent*` | No | [ui-hooks.md](ui-hooks.md) |
| `MMCO_HOOK_UI_INSTANCE_PAGES` | `0x0602` | UI | `MMCOInstancePagesEvent*` | No | [ui-hooks.md](ui-hooks.md) |

---

## See also

- [API Reference § Lifecycle — Hooks](../api-reference/s01-lifecycle/hooks.md) — `hook_register()` / `hook_unregister()` trampoline internals
- [SDK Guide](../sdk-guide.md) — building out-of-tree plugins
- [BackupSystem Case Study](../backup-system-case-study.md) — real-world hook usage
