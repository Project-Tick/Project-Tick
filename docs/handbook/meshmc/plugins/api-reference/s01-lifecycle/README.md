# Section 01 — Lifecycle, Logging & Hooks

> **API Surface:** `MMCOContext` fields from Section 1 (Logging) and Section 2 (Hooks), plus the `module_handle` field and the `MMCOModuleInfo` / `mmco_init()` / `mmco_unload()` lifecycle contract.

---

## Overview

This section documents the **foundation** of the MMCO plugin system: how a plugin is loaded, how it identifies itself to MeshMC, how it logs messages, and how it subscribes to (and unsubscribes from) lifecycle events via the hook system.

Every MMCO plugin **must** interact with these APIs. A plugin that does nothing except log a message at startup still uses `module_handle`, `log_info`, and the `mmco_init()` / `mmco_unload()` lifecycle functions. A plugin that observes any launcher event uses `hook_register()` and `hook_unregister()`.

### Architectural context

All plugin interaction with MeshMC goes through the `MMCOContext` struct — a table of C function pointers populated by `PluginManager::buildContext()` before `mmco_init()` is called. The context is the **only** sanctioned interface between plugin code and the launcher.

Every function pointer in `MMCOContext` is a **static trampoline** declared in `PluginManager`. The trampoline casts the opaque `void* mh` (module handle) back to a `PluginManager::ModuleRuntime*`, which contains a back-pointer to the owning `PluginManager` instance and the module's index. This pattern allows plain C function pointers (no captures, no `std::function`) while still reaching the full Qt/C++ state of the launcher.

```
Plugin (.mmco)                   MeshMC Launcher
─────────────────────────────────────────────────────────
ctx->log_info(ctx->module_handle, "hello")
       │
       ▼
PluginManager::api_log_info(void* mh, const char* msg)   ← static trampoline
       │
       ├─ ModuleRuntime* r = static_cast<…>(mh)
       ├─ r->manager->m_modules[r->moduleIndex]          ← resolve module metadata
       └─ qInfo() << "[Plugin:" << meta.name << "]" << msg
```

### What this section covers

| File | Contents |
|------|----------|
| [module-handle.md](module-handle.md) | The `module_handle` field, the `ModuleRuntime` struct, the opaque-handle pattern, ABI guard fields (`struct_size`, `abi_version`) |
| [logging.md](logging.md) | `log_info()`, `log_warn()`, `log_error()`, `log_debug()`, plus SDK convenience macros `MMCO_LOG`, `MMCO_WARN`, `MMCO_ERR`, `MMCO_DBG` |
| [hooks.md](hooks.md) | `hook_register()`, `hook_unregister()`, all `MMCOHookId` constants, all payload structs, dispatch semantics, cancellation |

---

## Quick-start: minimal plugin skeleton

The smallest valid MMCO plugin consists of three exports: `mmco_module_info`, `mmco_init()`, and `mmco_unload()`. The following example uses every API documented in this section.

```cpp
#include "plugin/sdk/mmco_sdk.h"

static MMCOContext* g_ctx = nullptr;

/* Hook callback — fired once when the main launcher UI is ready */
static int on_app_initialized(void* /*mh*/, uint32_t /*hook_id*/,
                               void* /*payload*/, void* /*ud*/)
{
    MMCO_LOG(g_ctx, "Application initialized — launcher is ready.");
    return 0;  /* continue hook chain */
}

MMCO_DEFINE_MODULE("HelloWorld", "1.0.0", "Your Name",
                   "A minimal MMCO plugin.", "GPL-3.0-or-later");

extern "C" {

MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;

    MMCO_LOG(ctx, "HelloWorld v1.0.0 initializing...");
    MMCO_DBG(ctx, "Debug: received module_handle.");

    int rc = ctx->hook_register(ctx->module_handle,
                                MMCO_HOOK_APP_INITIALIZED,
                                on_app_initialized, nullptr);
    if (rc != 0) {
        MMCO_ERR(ctx, "Failed to register APP_INITIALIZED hook");
        return rc;
    }

    MMCO_LOG(ctx, "HelloWorld initialized.");
    return 0;
}

MMCO_EXPORT void mmco_unload()
{
    if (g_ctx) {
        MMCO_LOG(g_ctx, "HelloWorld unloading.");
    }
    g_ctx = nullptr;
}

} /* extern "C" */
```

---

## Lifecycle sequence diagram

The full lifecycle of a single MMCO module:

```
PluginManager::initializeAll()
    │
    ├─ discoverModules()           // scan search paths for .mmco files
    ├─ for each module i:
    │     ├─ ensurePluginDataDir() // create ~/.local/share/meshmc/plugin-data/<id>/
    │     ├─ new ModuleRuntime { manager=this, moduleIndex=i, dataDir=… }
    │     ├─ buildContext(meta)    // populate all function pointers
    │     ├─ ctx.module_handle = runtime.get()
    │     └─ meta.initFunc(&ctx)  → calls  mmco_init(ctx)
    │
    └─ dispatchHook(MMCO_HOOK_APP_INITIALIZED)
           │
           └─ for each registered callback → callback(mh, 0x0100, nullptr, ud)

    ... application runs ...

PluginManager::shutdownAll()
    │
    ├─ dispatchHook(MMCO_HOOK_APP_SHUTDOWN)
    │     └─ for each registered callback → callback(mh, 0x0101, nullptr, ud)
    │
    ├─ for i = last..first:
    │     └─ meta.unloadFunc()    → calls  mmco_unload()
    │
    ├─ m_hooks.clear()            // remove all registrations
    └─ PluginLoader::unloadModule(meta)  // dlclose / FreeLibrary
```

### Key ordering guarantees

1. **`mmco_init()` is called before any hooks fire.** The `MMCO_HOOK_APP_INITIALIZED` dispatch happens *after* all modules have been initialized.
2. **`MMCO_HOOK_APP_SHUTDOWN` fires before any `mmco_unload()`.** All modules still have a valid context when the shutdown hook fires.
3. **Modules are unloaded in reverse load order.** If module A was loaded before module B, then B's `mmco_unload()` is called before A's.
4. **After `mmco_unload()` returns, the module's context is invalidated.** All function pointers and returned strings become dangling.

---

## Header files reference

| Header | Location | Purpose |
|--------|----------|---------|
| `PluginAPI.h` | `meshmc/launcher/plugin/PluginAPI.h` | Defines `MMCOContext` struct |
| `PluginHooks.h` | `meshmc/launcher/plugin/PluginHooks.h` | Defines `MMCOHookId` enum, `MMCOHookCallback` typedef, payload structs |
| `PluginManager.h` | `meshmc/launcher/plugin/PluginManager.h` | Declares `PluginManager` class, `ModuleRuntime`, `HookRegistration`, static trampolines |
| `PluginManager.cpp` | `meshmc/launcher/plugin/PluginManager.cpp` | Implements all trampolines, lifecycle methods, context builder |
| `mmco_sdk.h` | `meshmc/launcher/plugin/sdk/mmco_sdk.h` | All-in-one SDK header for plugin authors (re-exports types, defines macros) |

---

## Type index

Types documented across this section's files:

| Type | Kind | Defined in | Doc file |
|------|------|-----------|----------|
| `MMCOContext` | struct | `PluginAPI.h` | [module-handle.md](module-handle.md) |
| `MMCOContext::module_handle` | field (`void*`) | `PluginAPI.h` | [module-handle.md](module-handle.md) |
| `MMCOContext::struct_size` | field (`uint32_t`) | `PluginAPI.h` | [module-handle.md](module-handle.md) |
| `MMCOContext::abi_version` | field (`uint32_t`) | `PluginAPI.h` | [module-handle.md](module-handle.md) |
| `PluginManager::ModuleRuntime` | struct (internal) | `PluginManager.h` | [module-handle.md](module-handle.md) |
| `MMCOContext::log_info` | function pointer | `PluginAPI.h` | [logging.md](logging.md) |
| `MMCOContext::log_warn` | function pointer | `PluginAPI.h` | [logging.md](logging.md) |
| `MMCOContext::log_error` | function pointer | `PluginAPI.h` | [logging.md](logging.md) |
| `MMCOContext::log_debug` | function pointer | `PluginAPI.h` | [logging.md](logging.md) |
| `MMCO_LOG` | macro | `mmco_sdk.h` | [logging.md](logging.md) |
| `MMCO_WARN` | macro | `mmco_sdk.h` | [logging.md](logging.md) |
| `MMCO_ERR` | macro | `mmco_sdk.h` | [logging.md](logging.md) |
| `MMCO_DBG` | macro | `mmco_sdk.h` | [logging.md](logging.md) |
| `MMCOHookCallback` | typedef | `PluginHooks.h` | [hooks.md](hooks.md) |
| `MMCOHookId` | enum | `PluginHooks.h` | [hooks.md](hooks.md) |
| `MMCOContext::hook_register` | function pointer | `PluginAPI.h` | [hooks.md](hooks.md) |
| `MMCOContext::hook_unregister` | function pointer | `PluginAPI.h` | [hooks.md](hooks.md) |
| `PluginManager::HookRegistration` | struct (internal) | `PluginManager.h` | [hooks.md](hooks.md) |
| `MMCOInstanceInfo` | payload struct | `PluginHooks.h` | [hooks.md](hooks.md) |
| `MMCOSettingChange` | payload struct | `PluginHooks.h` | [hooks.md](hooks.md) |
| `MMCOContentEvent` | payload struct | `PluginHooks.h` | [hooks.md](hooks.md) |
| `MMCONetworkEvent` | payload struct | `PluginHooks.h` | [hooks.md](hooks.md) |
| `MMCOMenuEvent` | payload struct | `PluginHooks.h` | [hooks.md](hooks.md) |
| `MMCOInstancePagesEvent` | payload struct | `PluginHooks.h` | [hooks.md](hooks.md) |

---

## Function index

| Function / Macro | Signature | Doc file |
|------------------|-----------|----------|
| `log_info()` | `void (*)(void* mh, const char* msg)` | [logging.md](logging.md) |
| `log_warn()` | `void (*)(void* mh, const char* msg)` | [logging.md](logging.md) |
| `log_error()` | `void (*)(void* mh, const char* msg)` | [logging.md](logging.md) |
| `log_debug()` | `void (*)(void* mh, const char* msg)` | [logging.md](logging.md) |
| `MMCO_LOG()` | `MMCO_LOG(ctx, msg)` | [logging.md](logging.md) |
| `MMCO_WARN()` | `MMCO_WARN(ctx, msg)` | [logging.md](logging.md) |
| `MMCO_ERR()` | `MMCO_ERR(ctx, msg)` | [logging.md](logging.md) |
| `MMCO_DBG()` | `MMCO_DBG(ctx, msg)` | [logging.md](logging.md) |
| `hook_register()` | `int (*)(void* mh, uint32_t hook_id, MMCOHookCallback cb, void* ud)` | [hooks.md](hooks.md) |
| `hook_unregister()` | `int (*)(void* mh, uint32_t hook_id, MMCOHookCallback cb)` | [hooks.md](hooks.md) |

---

## Cross-references to other sections

- **Section 02 — Settings:** `setting_get()` / `setting_set()` — namespaced per-plugin key-value store
- **Section 03 — Instances:** Instance enumeration and property access
- **Section 04 — Instance Management:** Launch, kill, delete, groups, components
- **Section 11 — Instance Pages:** `MMCO_HOOK_UI_INSTANCE_PAGES` usage demonstrated in the BackupSystem plugin
- **Section 12 — UI Dialogs:** `ui_show_message()`, `ui_confirm_dialog()`, etc.
- **Section 14 — Utility:** `get_app_version()`, `get_app_name()`, `get_timestamp()`

---

## Conventions used in this documentation

- **"The context"** or **"`ctx`"** always refers to the `MMCOContext*` received in `mmco_init()`.
- **"Module handle"** or **"`mh`"** always refers to `ctx->module_handle`.
- **Return value `0`** means success for all `int`-returning API functions unless stated otherwise.
- **Return value `-1`** means failure (invalid arguments, internal error, not found).
- **`const char*` return values** point to storage owned by `ModuleRuntime::tempString`. They are valid until the next API call from the same module. Copy immediately if you need to keep the value.
- **Thread safety:** All API calls must be made from the **main (GUI) thread**. The MMCO API is not thread-safe.

---

*Source files: `PluginAPI.h`, `PluginHooks.h`, `PluginManager.h`, `PluginManager.cpp`, `mmco_sdk.h`*
