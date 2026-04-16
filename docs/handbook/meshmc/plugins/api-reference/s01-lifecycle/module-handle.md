# `module_handle` — Module Identity & ABI Guard

> **Header:** `PluginAPI.h` · **Internal implementation:** `PluginManager.h`, `PluginManager.cpp`

---

## Overview

Every `MMCOContext` begins with three fields that exist before any function pointers:

```cpp
struct MMCOContext {
    uint32_t struct_size;   /* sizeof(MMCOContext) for forward compat */
    uint32_t abi_version;   /* MMCO_ABI_VERSION */
    void*    module_handle; /* Opaque handle to identify this module */

    /* ... function pointers follow ... */
};
```

These fields serve two purposes:

1. **ABI compatibility** (`struct_size`, `abi_version`) — allows future ABI versions to detect mismatches without crashing.
2. **Module identity** (`module_handle`) — every API call requires the caller to pass its module handle so the launcher can identify which plugin is calling.

---

## `struct_size`

### Declaration

```cpp
uint32_t struct_size;
```

### Description

Set by `PluginManager::buildContext()` to `sizeof(MMCOContext)` at the time the launcher was compiled. This allows a plugin compiled against a **newer** SDK to detect that the context it received is smaller than expected and avoid reading past the end of the struct.

### Value

| ABI Version | Typical `struct_size` | Notes |
|---|---|---|
| 1 | Implementation-defined (`sizeof(MMCOContext)`) | Depends on pointer size and padding. ~800–1200 bytes on 64-bit. |

### Usage in plugins

Plugins should check `struct_size` before accessing function pointers that were added in later ABI versions:

```cpp
MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
    /* Guard against older launcher that doesn't have ui_register_instance_action */
    if (ctx->struct_size < offsetof(MMCOContext, ui_register_instance_action)
                           + sizeof(ctx->ui_register_instance_action)) {
        MMCO_ERR(ctx, "Launcher too old — missing ui_register_instance_action");
        return -1;
    }

    /* Safe to call */
    ctx->ui_register_instance_action(ctx->module_handle, "My Action", "Desc",
                                     "icon", "my-plugin-action");
    return 0;
}
```

### Implementation detail

Set in `PluginManager::buildContext()`:

```cpp
MMCOContext ctx{};
ctx.struct_size = sizeof(MMCOContext);
```

This is a simple assignment — no dynamic computation.

---

## `abi_version`

### Declaration

```cpp
uint32_t abi_version;
```

### Description

The ABI version number of the launcher's plugin interface. Currently always `1` (`MMCO_ABI_VERSION`). This field exists so plugins can hard-reject incompatible context versions rather than silently malfunctioning.

### Constants

```cpp
#define MMCO_ABI_VERSION   1
```

Defined in `mmco_sdk.h`.

### Usage in plugins

```cpp
MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
    if (ctx->abi_version != MMCO_ABI_VERSION) {
        /* Cannot safely use any function pointers — ABI broke */
        return -1;
    }
    /* ... */
}
```

### Forward-compatibility contract

MeshMC guarantees that:

- Fields present in ABI version N will remain at the **same offset** in version N+1.
- New function pointers will only be **appended** to `MMCOContext`.
- The `abi_version` field will be incremented only for **breaking** layout changes (field removal, reordering, signature changes).
- Additions (new function pointers at the end) do **not** bump `abi_version` — use `struct_size` to detect those.

### Implementation detail

Set in `PluginManager::buildContext()`:

```cpp
ctx.abi_version = MMCO_ABI_VERSION;
```

---

## `module_handle`

### Declaration

```cpp
void* module_handle;
```

### Description

An **opaque pointer** that uniquely identifies the calling module within the launcher. Plugins must pass this value as the first argument to every `MMCOContext` function pointer. The launcher uses it to:

1. **Resolve the calling module's metadata** (name, index, data directory).
2. **Route the call** to the correct `PluginManager` instance.
3. **Namespace operations** (e.g., settings keys are prefixed with the module's ID).

### Type

From the plugin's perspective, `module_handle` is `void*`. Plugins **must not** dereference, cast, or inspect this pointer. It is opaque.

### Lifetime

| Phase | `module_handle` validity |
|---|---|
| Before `mmco_init()` | Not yet assigned (undefined) |
| During `mmco_init()` | Valid — `ctx->module_handle` is set before the call |
| Between `mmco_init()` and `mmco_unload()` | Valid |
| During `mmco_unload()` | Valid — can still call logging functions |
| After `mmco_unload()` returns | **Invalid** — dangling pointer |

### Example: passing `module_handle` to API calls

```cpp
static MMCOContext* g_ctx = nullptr;

MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;

    /* Every API call takes module_handle as the first argument */
    ctx->log_info(ctx->module_handle, "Plugin starting");

    int count = ctx->instance_count(ctx->module_handle);
    ctx->log_info(ctx->module_handle, "Found instances");

    /* Register a hook — module_handle identifies who registered */
    ctx->hook_register(ctx->module_handle, MMCO_HOOK_APP_SHUTDOWN,
                       my_shutdown_cb, nullptr);

    return 0;
}
```

### Example: using `MMCO_MH` shorthand

The SDK provides a convenience macro for the common `ctx->module_handle` pattern:

```cpp
#define MMCO_MH (ctx->module_handle)
```

This is only usable when you have a local variable named `ctx`:

```cpp
MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
    ctx->log_info(MMCO_MH, "Starting");
    ctx->hook_register(MMCO_MH, MMCO_HOOK_APP_INITIALIZED,
                       on_app_init, nullptr);
    return 0;
}
```

> **Note:** `MMCO_MH` is a raw macro expansion. It does not work inside hook callbacks where the context variable may have a different name.

---

## Internal: `PluginManager::ModuleRuntime`

> **This section documents launcher internals. Plugin authors do not need this information to use the API, but it helps understand error diagnostics and threading constraints.**

### Declaration (from `PluginManager.h`)

```cpp
struct ModuleRuntime {
    PluginManager* manager;
    int            moduleIndex;
    std::string    dataDir;
    std::string    tempString;
};
```

### Fields

| Field | Type | Description |
|---|---|---|
| `manager` | `PluginManager*` | Back-pointer to the `PluginManager` instance that owns this module. Used by every trampoline to access launcher state. |
| `moduleIndex` | `int` | Index into `PluginManager::m_modules` (the `QVector<PluginMetadata>`). Used to retrieve the module's metadata (name, paths, init state). |
| `dataDir` | `std::string` | Absolute path to the module's dedicated data directory. Stored as `std::string` so C API functions can return `c_str()`. |
| `tempString` | `std::string` | Scratch buffer for returning `const char*` values. Many API functions that return strings store their result here and return `tempString.c_str()`. **This means the returned pointer is invalidated by the next API call from the same module.** |

### How `module_handle` resolves to `ModuleRuntime`

Every static trampoline in `PluginManager` begins with the same pattern:

```cpp
void PluginManager::api_log_info(void* mh, const char* msg)
{
    auto* r = rt(mh);                                     // ①
    auto& meta = r->manager->m_modules[r->moduleIndex];   // ②
    qInfo().noquote() << "[Plugin:" << meta.name << "]" << msg;  // ③
}
```

1. **`rt(mh)`** — static helper that performs `static_cast<ModuleRuntime*>(mh)`.
2. **`r->manager->m_modules[r->moduleIndex]`** — retrieves the `PluginMetadata` for the calling module.
3. The trampoline then performs the actual operation using Qt/MeshMC internals.

The `rt()` helper (from `PluginManager.cpp`):

```cpp
PluginManager::ModuleRuntime* PluginManager::rt(void* mh)
{
    return static_cast<ModuleRuntime*>(mh);
}
```

### Memory ownership

- Each `ModuleRuntime` is heap-allocated as a `std::unique_ptr<ModuleRuntime>` and stored in `PluginManager::m_runtimes` (a `std::vector`).
- The `ModuleRuntime` is created in `PluginManager::initializeAll()` and destroyed in `PluginManager::shutdownAll()`.
- The raw pointer stored in `ctx->module_handle` is a **non-owning** reference to the `unique_ptr`'s managed object.

### Lifecycle of ModuleRuntime objects

```
initializeAll()
    │
    ├─ m_runtimes.resize(m_modules.size())
    │
    └─ for each module i:
          auto runtime = std::make_unique<ModuleRuntime>();
          runtime->manager     = this;
          runtime->moduleIndex = i;
          runtime->dataDir     = meta.dataDir.toStdString();
          m_runtimes[i] = std::move(runtime);
          m_contexts[i].module_handle = m_runtimes[i].get();   ← raw ptr

shutdownAll()
    │
    ├─ (modules unloaded, hooks cleared)
    │
    └─ m_runtimes.clear()   ← unique_ptrs destroyed, module_handle becomes dangling
```

---

## The `tempString` invalidation rule

This is a critical correctness rule that applies to **all** `const char*`-returning API functions across the entire MMCO API.

### Rule

> A `const char*` returned by any `MMCOContext` function pointer is valid **only until the next call** to any `MMCOContext` function pointer from the **same module**.

### Mechanism

All string-returning trampoline functions store their result in `ModuleRuntime::tempString`:

```cpp
const char* PluginManager::api_setting_get(void* mh, const char* key)
{
    auto* r = rt(mh);
    /* ... resolve the value ... */
    r->tempString = val.toString().toStdString();  // overwrites previous value
    return r->tempString.c_str();
}
```

Since every module has **one** `tempString`, any subsequent API call that returns a string will overwrite the buffer.

### Safe pattern: copy immediately

```cpp
const char* raw_name = ctx->instance_get_name(ctx->module_handle, id);
if (!raw_name) { /* handle null */ }

/* Copy before making another API call */
std::string name(raw_name);

/* Now safe to call another API function */
const char* raw_path = ctx->instance_get_path(ctx->module_handle, id);
std::string path(raw_path ? raw_path : "");

/* Use both copied strings */
MMCO_LOG(ctx, (std::string("Instance: ") + name + " at " + path).c_str());
```

### Unsafe pattern: using stale pointers

```cpp
/* BUG: raw_name becomes dangling after the second API call */
const char* raw_name = ctx->instance_get_name(ctx->module_handle, id);
const char* raw_path = ctx->instance_get_path(ctx->module_handle, id);
/* raw_name now points to raw_path's content (or garbage) */
printf("Name: %s\n", raw_name);  // UNDEFINED BEHAVIOUR
```

### Functions that do NOT invalidate `tempString`

The following functions do not use `tempString` and therefore do not invalidate previously-returned strings:

- `log_info()`, `log_warn()`, `log_error()`, `log_debug()` (return `void`)
- `hook_register()`, `hook_unregister()` (return `int`)
- `setting_set()` (returns `int`)
- All `int`-returning state queries and actions

However, for simplicity and forward-compatibility, **treat all API calls as potentially invalidating**.

---

## `MMCOModuleInfo` — module identity

While `module_handle` identifies a module at runtime, the `MMCOModuleInfo` struct identifies it at **load time**. It is an exported symbol (`mmco_module_info`) that the loader reads before calling `mmco_init()`.

### Declaration (from `mmco_sdk.h`)

```cpp
struct MMCOModuleInfo {
    uint32_t    magic;        /* MMCO_MAGIC (0x4D4D434F) */
    uint32_t    abi_version;  /* MMCO_ABI_VERSION */
    const char* name;         /* Human-readable module name */
    const char* version;      /* Semantic version string */
    const char* author;       /* Author name */
    const char* description;  /* One-line description */
    const char* license;      /* SPDX license identifier */
    uint32_t    flags;        /* Reserved (MMCO_FLAG_NONE = 0) */
};
```

### Constants

```cpp
#define MMCO_MAGIC       0x4D4D434F   /* ASCII "MMCO" */
#define MMCO_ABI_VERSION 1
#define MMCO_FLAG_NONE   0x00000000
```

### `MMCO_DEFINE_MODULE` convenience macro

Rather than manually defining `mmco_module_info`, use the SDK macro:

```cpp
MMCO_DEFINE_MODULE("BackupSystem", "1.0.0", "Project Tick",
                   "Instance backup snapshots", "GPL-3.0-or-later");
```

This expands to:

```cpp
extern "C" MMCO_EXPORT MMCOModuleInfo mmco_module_info = {
    MMCO_MAGIC,
    MMCO_ABI_VERSION,
    "BackupSystem",
    "1.0.0",
    "Project Tick",
    "Instance backup snapshots",
    "GPL-3.0-or-later",
    MMCO_FLAG_NONE
};
```

### Validation

The loader validates `mmco_module_info` before calling `mmco_init()`:

1. `magic == MMCO_MAGIC` — rejects non-MMCO shared libraries.
2. `abi_version == MMCO_ABI_VERSION` — rejects ABI-incompatible modules.
3. `name != nullptr` — a module must have a name.

If validation fails, the module is **not loaded** and an error is emitted.

---

## Thread safety

All three fields (`struct_size`, `abi_version`, `module_handle`) are **read-only** from the plugin's perspective after `mmco_init()` is called. They are set once by `PluginManager::buildContext()` and never modified.

The `ModuleRuntime` struct behind `module_handle` is **not thread-safe**. The `tempString` field is a single buffer with no locking. All API calls must be made from the **main (GUI) thread**.

---

## Error conditions

| Condition | Behaviour |
|---|---|
| Plugin passes `nullptr` as `mh` to any API function | **Undefined behaviour** — `static_cast` of `nullptr` to `ModuleRuntime*` will dereference a null pointer. The launcher will crash. |
| Plugin passes a `module_handle` from a different module | **Undefined behaviour** — the trampoline will use the wrong module index. Operations will affect the wrong module's settings, logging prefix, etc. |
| Plugin stores `module_handle` after `mmco_unload()` and calls an API function | **Undefined behaviour** — `ModuleRuntime` has been destroyed. Dangling pointer access. |
| `struct_size` is smaller than expected | The plugin should refuse to use function pointers beyond the known size. |
| `abi_version` does not match | The plugin should return non-zero from `mmco_init()` to abort loading. |

---

## See also

- [logging.md](logging.md) — Logging functions that use `module_handle`
- [hooks.md](hooks.md) — Hook registration functions that use `module_handle`
- [README.md](README.md) — Section overview with lifecycle sequence diagram
