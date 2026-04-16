# Instance Enumeration

> `instance_count()` · `instance_get_id()` · `instance_get_name()`

This page documents the three functions that let a plugin **discover** which
instances exist and obtain their fundamental identifiers. These are the
entry point into all instance-related API work — you must call these before
you can query properties, launch, or manage any instance.

---

## `instance_count()`

### Signature

```c
int (*instance_count)(void* mh);
```

### Description

Returns the total number of instances currently loaded in MeshMC. This
includes instances of all types and in all groups. The count reflects the
live state of the `InstanceList` at the moment of the call.

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. Pass `ctx->module_handle`. |

### Return value

| Condition | Value |
|-----------|-------|
| Success | Number of instances (≥ 0) |
| Application not initialized | `0` |
| Instance list unavailable | `0` |

The function never returns a negative value. If MeshMC has not finished
loading, or the InstanceList subsystem is unavailable, it returns `0`.

### Thread safety

**Main thread only.** The function reads from `InstanceList`, which is a
`QAbstractListModel` and is not thread-safe.

### Ownership

No pointer is returned. No ownership concerns.

### Implementation

```cpp
// PluginManager.cpp
int PluginManager::api_instance_count(void* mh)
{
    auto* r = rt(mh);
    auto* app = r->manager->m_app;
    if (!app || !app->instances())
        return 0;
    return app->instances()->count();
}
```

The call chain is:

```
Plugin:  ctx->instance_count(ctx->module_handle)
    │
    ▼
PluginManager::api_instance_count(void* mh)      ← static trampoline
    │
    ├── ModuleRuntime* r = static_cast<…>(mh)
    ├── Application* app = r->manager->m_app
    └── return app->instances()->count()
                    │
                    ▼
            InstanceList::count()
                    │
                    └── return m_instances.count()   (QList<InstancePtr>::count)
```

### Example

```c
void dump_count(const MMCOContext* ctx) {
    void* mh = ctx->module_handle;
    int n = ctx->instance_count(mh);

    char msg[128];
    snprintf(msg, sizeof(msg), "MeshMC has %d instance(s) loaded.", n);
    ctx->log_info(mh, msg);
}
```

### Edge cases

| Scenario | Behaviour |
|----------|-----------|
| No instances created yet | Returns `0` |
| Called during `MMCO_HOOK_APP_INITIALIZED` | Returns the count at app startup; instances loaded from disk are already present |
| Called inside `MMCO_HOOK_INSTANCE_CREATED` | Returns the count **after** the new instance has been added |
| Called inside `MMCO_HOOK_INSTANCE_REMOVED` | Returns the count **after** the instance has been removed |
| Called from a background thread | **Undefined behaviour** — do not do this |

### Usage notes

- The count may change between calls if the user creates or deletes
  instances via the UI. Always call `instance_count()` at the start of
  each enumeration loop.
- There is no "instance list changed" callback. Use
  `MMCO_HOOK_INSTANCE_CREATED` (`0x0202`) and
  `MMCO_HOOK_INSTANCE_REMOVED` (`0x0203`) to react to additions/removals.

---

## `instance_get_id()`

### Signature

```c
const char* (*instance_get_id)(void* mh, int index);
```

### Description

Returns the **unique string identifier** of the instance at the given
zero-based index. The ID is the canonical key used by all other instance
API functions (`instance_get_name()`, `instance_get_path()`,
`instance_launch()`, etc.).

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. Pass `ctx->module_handle`. |
| `index` | `int` | Zero-based position in the instance list. Valid range: `0` to `instance_count() - 1`. |

### Return value

| Condition | Value |
|-----------|-------|
| Success | `const char*` pointing to the instance ID string (null-terminated UTF-8) |
| `index` out of bounds | `nullptr` |
| Application or instance list unavailable | `nullptr` |
| Instance at the given index is null | `nullptr` |

### Thread safety

**Main thread only.**

### Ownership

The returned `const char*` is stored in `ModuleRuntime::tempString`.

> **CRITICAL:** The pointer is valid only until the **next API call on the
> same module**. If you need the ID beyond that point, copy it immediately:
>
> ```c
> const char* id = ctx->instance_get_id(mh, i);
> if (!id) continue;
>
> char id_buf[256];
> snprintf(id_buf, sizeof(id_buf), "%s", id);
>
> /* Now id_buf is safe to use across further API calls */
> ```

### Implementation

```cpp
// PluginManager.cpp
const char* PluginManager::api_instance_get_id(void* mh, int index)
{
    auto* r = rt(mh);
    auto* app = r->manager->m_app;
    if (!app || !app->instances())
        return nullptr;

    auto list = app->instances();
    if (index < 0 || index >= list->count())
        return nullptr;

    auto inst = list->at(index);
    if (!inst)
        return nullptr;

    r->tempString = inst->id().toStdString();
    return r->tempString.c_str();
}
```

The call chain is:

```
Plugin:  ctx->instance_get_id(ctx->module_handle, index)
    │
    ▼
PluginManager::api_instance_get_id(void* mh, int index)
    │
    ├── bounds check: index ∈ [0, list->count())
    ├── InstancePtr inst = list->at(index)
    ├── r->tempString = inst->id().toStdString()
    └── return r->tempString.c_str()
```

### Where does the ID come from?

`BaseInstance::id()` reads the `InstanceType` setting's sibling data. The ID
is assigned when the instance is first created and stored on disk. It is
derived from the instance's directory name and is guaranteed unique within
a single MeshMC installation.

```cpp
// BaseInstance.h/.cpp
virtual QString id() const;
```

The ID is a filesystem-safe string. Typical examples:

| Instance name (user-visible) | Possible ID |
|------------------------------|-------------|
| "My Survival World" | `20250415_MySurvivalWorld` |
| "Fabric 1.20.4" | `Fabric_1.20.4` |
| "Modded Server" | `Modded_Server` |

> **Never parse, construct, or assume any format for instance IDs.** Treat
> them as opaque tokens. The format may change between MeshMC versions.

### Example: Safe iteration

```c
void iterate_instances(const MMCOContext* ctx) {
    void* mh = ctx->module_handle;
    int count = ctx->instance_count(mh);

    for (int i = 0; i < count; i++) {
        const char* raw_id = ctx->instance_get_id(mh, i);
        if (!raw_id)
            continue;

        /* Copy before the next API call */
        char id[256];
        snprintf(id, sizeof(id), "%s", raw_id);

        /* Now safe to call other API functions */
        const char* name = ctx->instance_get_name(mh, id);
        const char* type = ctx->instance_get_type(mh, id);

        char msg[512];
        snprintf(msg, sizeof(msg), "  [%d] id=%s  name=%s  type=%s",
                 i, id,
                 name ? name : "(null)",
                 type ? type : "(null)");
        ctx->log_info(mh, msg);
    }
}
```

### Index volatility

As documented in the [section overview](README.md#index-validity-and-volatility),
indices are **not stable**. The mapping between an index and an instance
changes whenever:

1. A new instance is created (new entry appended or inserted)
2. An instance is deleted (entries shift down)
3. The instance list is reloaded from disk
4. Instances are reordered by the user (e.g., drag-and-drop in the UI)

**Consequence:** If you receive a hook callback (`MMCO_HOOK_INSTANCE_CREATED`,
`MMCO_HOOK_INSTANCE_REMOVED`) while in the middle of an iteration, your
current loop's count and indices are invalid. In practice, hooks are
dispatched synchronously on the main thread, so this only happens if you
trigger instance creation/deletion from within your own enumeration
(which you should avoid).

### Error handling

| Symptom | Cause | Fix |
|---------|-------|-----|
| Returns `nullptr` | Index out of range | Check `0 ≤ index < instance_count()` |
| Returns `nullptr` | App not ready | Defer to `MMCO_HOOK_APP_INITIALIZED` |
| Returns empty string `""` | Instance has empty ID (should not happen) | Skip it |

---

## `instance_get_name()`

### Signature

```c
const char* (*instance_get_name)(void* mh, const char* id);
```

### Description

Returns the **human-readable display name** of the instance identified by
`id`. This is the name shown in the MeshMC UI instance list. The name is
set by the user when creating the instance and can be changed at any time
(via the UI or via `instance_set_name()`).

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. Pass `ctx->module_handle`. |
| `id` | `const char*` | Instance ID string (obtained from `instance_get_id()`). Must be null-terminated UTF-8. |

### Return value

| Condition | Value |
|-----------|-------|
| Success | `const char*` pointing to the instance name (null-terminated UTF-8) |
| `id` is `nullptr` | `nullptr` |
| No instance with that ID exists | `nullptr` |
| Application or instance list unavailable | `nullptr` |

### Thread safety

**Main thread only.**

### Ownership

The returned `const char*` is stored in `ModuleRuntime::tempString`.
It is valid only until the **next API call on the same module**.

### Implementation

```cpp
// PluginManager.cpp
const char* PluginManager::api_instance_get_name(void* mh, const char* id)
{
    auto* r = rt(mh);
    auto* app = r->manager->m_app;
    if (!app || !app->instances())
        return nullptr;

    auto inst = app->instances()->getInstanceById(QString::fromUtf8(id));
    if (!inst)
        return nullptr;

    r->tempString = inst->name().toStdString();
    return r->tempString.c_str();
}
```

The call chain is:

```
Plugin:  ctx->instance_get_name(ctx->module_handle, "my_instance_id")
    │
    ▼
PluginManager::api_instance_get_name(void* mh, const char* id)
    │
    ├── InstancePtr inst = app->instances()->getInstanceById(id)
    ├── r->tempString = inst->name().toStdString()
    └── return r->tempString.c_str()
```

### How names work internally

Instance names are stored in the per-instance `instance.cfg` INI file under
the key `name`. The `BaseInstance::name()` method reads from the instance's
`SettingsObject`:

```cpp
// BaseInstance.h
QString name() const;
void setName(QString val);
```

Names are:
- **Not unique.** Multiple instances can have the same name.
- **User-editable.** The user can rename instances via the UI at any time.
- **UTF-8.** Names can contain Unicode characters, spaces, special characters.
- **Possibly empty.** A name can technically be an empty string, though the
  UI discourages this.

### Example: Print name of every instance

```c
void print_all_names(const MMCOContext* ctx) {
    void* mh = ctx->module_handle;
    int n = ctx->instance_count(mh);

    for (int i = 0; i < n; i++) {
        const char* id = ctx->instance_get_id(mh, i);
        if (!id) continue;

        /* MUST copy ID before calling instance_get_name */
        char id_buf[256];
        snprintf(id_buf, sizeof(id_buf), "%s", id);

        const char* name = ctx->instance_get_name(mh, id_buf);
        char msg[512];
        snprintf(msg, sizeof(msg), "Instance %d: \"%s\" (id=%s)",
                 i, name ? name : "<unnamed>", id_buf);
        ctx->log_info(mh, msg);
    }
}
```

### Example: Look up an instance by name

Names are **not unique**, so this pattern finds the **first** match. If you
need all matches, collect IDs into an array.

```c
/* Returns 1 if found (id_out is populated), 0 if not found. */
int find_by_name(const MMCOContext* ctx, const char* target,
                 char* id_out, size_t id_out_sz) {
    void* mh = ctx->module_handle;
    int n = ctx->instance_count(mh);

    for (int i = 0; i < n; i++) {
        const char* id = ctx->instance_get_id(mh, i);
        if (!id) continue;

        char id_buf[256];
        snprintf(id_buf, sizeof(id_buf), "%s", id);

        const char* name = ctx->instance_get_name(mh, id_buf);
        if (name && strcmp(name, target) == 0) {
            snprintf(id_out, id_out_sz, "%s", id_buf);
            return 1;
        }
    }
    return 0;
}
```

### Example: Building a name → ID lookup table

For plugins that need frequent name-based lookups, cache the mapping at
startup or after list-change hooks:

```c
#include <string.h>
#include <stdlib.h>

#define MAX_INSTANCES 256

struct InstanceEntry {
    char id[256];
    char name[256];
};

struct InstanceCache {
    struct InstanceEntry entries[MAX_INSTANCES];
    int count;
};

void rebuild_cache(const MMCOContext* ctx, struct InstanceCache* cache) {
    void* mh = ctx->module_handle;
    cache->count = 0;

    int n = ctx->instance_count(mh);
    if (n > MAX_INSTANCES) n = MAX_INSTANCES;

    for (int i = 0; i < n; i++) {
        const char* id = ctx->instance_get_id(mh, i);
        if (!id) continue;

        snprintf(cache->entries[cache->count].id,
                 sizeof(cache->entries[0].id), "%s", id);

        const char* name = ctx->instance_get_name(mh,
                               cache->entries[cache->count].id);
        snprintf(cache->entries[cache->count].name,
                 sizeof(cache->entries[0].name), "%s",
                 name ? name : "");

        cache->count++;
    }
}
```

### Error handling

| Symptom | Cause | Fix |
|---------|-------|-----|
| Returns `nullptr` | `id` is `NULL` | Pass a valid ID from `instance_get_id()` |
| Returns `nullptr` | Instance was deleted | Re-enumerate; the ID is no longer valid |
| Returns empty string `""` | Instance has no name set | Treat as unnamed; display the ID instead |

---

## Complete Iteration Pattern

Bringing all three functions together, the canonical pattern for iterating
over instances is:

```c
void on_app_initialized(const MMCOContext* ctx) {
    void* mh = ctx->module_handle;

    int count = ctx->instance_count(mh);
    ctx->log_info(mh, "=== Enumerating all instances ===");

    for (int i = 0; i < count; i++) {
        /* Step 1: Get the ID (index → ID) */
        const char* raw_id = ctx->instance_get_id(mh, i);
        if (!raw_id) continue;

        /* Step 2: Copy the ID (tempString will be overwritten) */
        char id[256];
        snprintf(id, sizeof(id), "%s", raw_id);

        /* Step 3: Query properties using the copied ID */
        const char* name = ctx->instance_get_name(mh, id);
        /* (copy name too if you need it after the next call) */
        char name_buf[256];
        snprintf(name_buf, sizeof(name_buf), "%s", name ? name : "<unnamed>");

        const char* type = ctx->instance_get_type(mh, id);
        const char* path = ctx->instance_get_path(mh, id);

        char msg[1024];
        snprintf(msg, sizeof(msg),
                 "  [%d] id=%-30s  name=%-20s  type=%-12s  path=%s",
                 i, id, name_buf,
                 type ? type : "?",
                 path ? path : "?");
        ctx->log_info(mh, msg);
    }
}
```

### Pattern: Reacting to instance list changes

```c
static int on_instance_created(void* module_handle, uint32_t hook_id,
                               void* payload, void* user_data) {
    const MMCOContext* ctx = (const MMCOContext*)user_data;
    const MMCOInstanceInfo* info = (const MMCOInstanceInfo*)payload;

    char msg[512];
    snprintf(msg, sizeof(msg),
             "New instance! id=%s name=%s mc=%s",
             info->instance_id   ? info->instance_id   : "?",
             info->instance_name ? info->instance_name : "?",
             info->minecraft_version ? info->minecraft_version : "?");
    ctx->log_info(ctx->module_handle, msg);

    /* All previously cached indices are now invalid.
     * Re-enumerate if needed. */
    return 0; /* continue hook chain */
}

static int on_instance_removed(void* module_handle, uint32_t hook_id,
                               void* payload, void* user_data) {
    const MMCOInstanceInfo* info = (const MMCOInstanceInfo*)payload;
    /* The instance is already gone from InstanceList.
     * Do NOT call instance_get_* with this ID — it will return nullptr. */
    return 0;
}

/* In mmco_init(): */
int mmco_init(MMCOContext* ctx) {
    void* mh = ctx->module_handle;

    ctx->hook_register(mh, MMCO_HOOK_INSTANCE_CREATED,
                       on_instance_created, (void*)ctx);
    ctx->hook_register(mh, MMCO_HOOK_INSTANCE_REMOVED,
                       on_instance_removed, (void*)ctx);
    return 0;
}
```

---

## Relationship to Other API Sections

Once you have an instance ID from enumeration, you can pass it to functions
in other sections:

| Section | Example functions | Purpose |
|---------|-------------------|---------|
| **S03** (this section) | `instance_get_path()`, `instance_get_type()` | Read-only property queries |
| **S04** | `instance_set_name()`, `instance_launch()`, `instance_kill()`, `instance_delete()` | Mutating operations |
| **S04** | `instance_is_running()`, `instance_can_launch()`, `instance_has_crashed()` | State queries |
| **S04** | `instance_get_total_play_time()`, `instance_get_last_launch()` | Play-time statistics |
| **S04** | `instance_get_group()`, `instance_set_group()` | Group management |
| **S05** | `instance_component_count()`, `mod_count()`, `mod_get_name()` | Component / mod queries |
| **S06** | `world_count()`, `world_get_name()`, `world_delete()` | World management |
| **S05** | `instance_get_jar_mods_dir()`, `instance_get_resource_packs_dir()` | Directory queries |

All of these take `const char* id` as a parameter. The ID you obtain from
`instance_get_id()` is the universal key for the entire instance API surface.

---

## FAQ

### Q: Can two instances have the same name?

**Yes.** Names are user-defined labels and are not required to be unique.
Always use IDs (from `instance_get_id()`) as keys, never names.

### Q: Is the instance count expensive to query?

**No.** `instance_count()` calls `QList::count()`, which is O(1). Call it
freely.

### Q: Is `instance_get_id()` O(1)?

**Yes.** It indexes into a `QList` by position, which is O(1). The
subsequent `inst->id()` call is a settings lookup that is effectively O(1)
(reading from an in-memory hash map).

### Q: Is `instance_get_name()` O(1)?

**No.** It calls `getInstanceById()` which performs a linear scan over the
instance list. This is O(n) where n is the number of instances. For typical
launcher usage (tens of instances), this is negligible. For plugins that
need to query names thousands of times, build a local cache.

### Q: What if the user creates an instance while my plugin is iterating?

In normal operation, this does not happen — instance creation goes through
a task that runs on the main thread, and your hook/init code also runs on
the main thread. Since Qt is single-threaded for UI operations, you will
not be preempted mid-loop. However, if you yield to the event loop (e.g.,
by returning from `mmco_init()` and waiting for a hook), indices may change
by the time your hook fires.

### Q: How do I get notified when the instance list changes?

Register for `MMCO_HOOK_INSTANCE_CREATED` (`0x0202`) and
`MMCO_HOOK_INSTANCE_REMOVED` (`0x0203`). Both provide an
`MMCOInstanceInfo*` payload with the affected instance's ID, name, path,
and Minecraft version.
