# Section 03 — Instance Query API

> **Header:** `PluginAPI.h` (function pointer declarations) · **SDK:** `mmco_sdk.h` (struct definition) · **Implementation:** `PluginManager.cpp` (static trampolines)

---

## Overview

The Instance Query API lets plugins **discover and inspect** the instances
managed by MeshMC. It is a **read-only surface** — querying instances,
iterating over them, reading their names, paths, types, and icon keys.
For write operations (launching, killing, renaming, deleting) see
[Section 04 — Instance Management](../s04-instance-management/).

The API follows a two-step pattern used throughout MMCO: **enumerate by
index, then query by ID**.

```text
┌──────────────────────────────────────────────────────────────────┐
│  1. instance_count()        → total number of instances         │
│  2. instance_get_id(index)  → string ID for instance at index   │
│  3. instance_get_*(id)      → read any property by string ID    │
└──────────────────────────────────────────────────────────────────┘
```

All six function pointers live in `MMCOContext` and are declared in
`PluginAPI.h`:

```c
/* S03 — Instance Query  (from MMCOContext in PluginAPI.h) */

/* Enumeration */
int         (*instance_count)       (void* mh);
const char* (*instance_get_id)      (void* mh, int index);

/* Properties (by instance ID) */
const char* (*instance_get_name)    (void* mh, const char* id);
const char* (*instance_get_path)    (void* mh, const char* id);
const char* (*instance_get_game_root)(void* mh, const char* id);
const char* (*instance_get_mods_root)(void* mh, const char* id);
const char* (*instance_get_icon_key)(void* mh, const char* id);
const char* (*instance_get_type)    (void* mh, const char* id);
```

---

## Function summary

| Function | Sub-page | Purpose | Returns |
|----------|----------|---------|---------|
| [`instance_count()`](instance-enumeration.md#instance_count) | Enumeration | Total number of loaded instances | `int` (≥ 0) |
| [`instance_get_id()`](instance-enumeration.md#instance_get_id) | Enumeration | Instance ID at a given index | `const char*` (nullable) |
| [`instance_get_name()`](instance-enumeration.md#instance_get_name) | Enumeration | Human-readable display name | `const char*` (nullable) |
| [`instance_get_path()`](instance-properties.md#instance_get_path) | Properties | Absolute path to instance root | `const char*` (nullable) |
| [`instance_get_game_root()`](instance-properties.md#instance_get_game_root) | Properties | Absolute path to game root (`.minecraft`) | `const char*` (nullable) |
| [`instance_get_mods_root()`](instance-properties.md#instance_get_mods_root) | Properties | Absolute path to mods folder | `const char*` (nullable) |
| [`instance_get_icon_key()`](instance-properties.md#instance_get_icon_key) | Properties | Icon key string used by the icon system | `const char*` (nullable) |
| [`instance_get_type()`](instance-properties.md#instance_get_type) | Properties | Instance type identifier string | `const char*` (nullable) |

---

## The Instance Model

### What is an instance?

In MeshMC, an **instance** is a self-contained Minecraft installation. Each
instance has its own game directory, configuration, mod list, resource packs,
and launch settings. Instances are created by the user through the launcher
UI (or programmatically via the API) and persist as directories on disk.

On the C++ side, every instance is represented by a `BaseInstance` subclass.
Currently, all user-created instances are `MinecraftInstance` objects, but the
inheritance hierarchy allows future extension:

```
BaseInstance                    (abstract)
  └── MinecraftInstance         (concrete — all Minecraft instances)
```

### Instance identity

Every instance has three key identifiers:

| Property | Uniqueness | Stability | Retrieved via |
|----------|-----------|-----------|---------------|
| **ID** | Unique across all instances | Stable for the instance's lifetime | `instance_get_id(index)` |
| **Name** | Not unique (user-chosen) | Changes if user renames | `instance_get_name(id)` |
| **Index** | Unique at a point in time | **Unstable** — changes when instances are added/removed | positional in `instance_count()` |

The **ID** is the canonical identifier for all property lookups. It is an
opaque string assigned by MeshMC when the instance is created. Typical format
is a filesystem-safe name like `"20250415_MyServer"` or a UUID-derived
string, but plugins **must not** parse or assume any format — treat it as an
opaque token.

### Instance storage

Each instance lives in a subdirectory under MeshMC's instance root:

```
<meshmc_data_dir>/
  instances/
    <instance_id>/
      instance.cfg        ← INI settings (InstanceType, name, icon, etc.)
      .minecraft/         ← game root (Minecraft data)
        mods/
        resourcepacks/
        saves/
        ...
```

The `instance_get_path()` function returns the full path to the instance
directory (e.g., `<meshmc_data_dir>/instances/<instance_id>`), while
`instance_get_game_root()` returns the `.minecraft` subdirectory within it.

---

## How the Index System Works

### Index-based enumeration

The Instance Query API uses **zero-based indices** to enumerate instances.
The pattern is identical to C array iteration:

```c
int n = ctx->instance_count(mh);
for (int i = 0; i < n; i++) {
    const char* id = ctx->instance_get_id(mh, i);
    /* use id ... */
}
```

### Index ↔ InstanceList mapping

Internally, `PluginManager::api_instance_count()` calls:

```cpp
app->instances()->count();
```

where `app->instances()` returns the global `InstanceList*`. The
`InstanceList` is a `QAbstractListModel` backed by a `QList<InstancePtr>`
(`m_instances`). The index you pass to `instance_get_id()` maps directly
to `InstanceList::at(index)`.

```cpp
// InstanceList.h
InstancePtr at(int i) const {
    return m_instances.at(i);
}

int count() const {
    return m_instances.count();
}
```

### Index validity and volatility

> **WARNING:** Instance indices are **volatile**. They reflect the current
> state of the internal `QList<InstancePtr>` and change whenever instances
> are added, removed, or the list is reloaded.

**Rules for safe index usage:**

1. **Always re-query `instance_count()` before iterating.** A cached count
   from a previous call may be stale.

2. **Never store an index for later use.** Indices are valid only for the
   duration of a single, uninterrupted enumeration loop.

3. **Use IDs for persistent references.** If you need to remember an instance
   across API calls (e.g., storing a "last used instance" in settings), store
   the string ID, not the index.

4. **Indices are not stable across hooks.** If you receive an
   `MMCO_HOOK_INSTANCE_CREATED` or `MMCO_HOOK_INSTANCE_REMOVED` callback,
   all previously obtained indices are invalid. Re-enumerate.

### What happens on out-of-bounds access?

`instance_get_id()` performs bounds checking:

```cpp
if (index < 0 || index >= list->count())
    return nullptr;
```

Passing an invalid index returns `nullptr`. It does **not** crash, throw, or
log an error — it silently returns null. Always check the return value.

---

## Internal Resolution: ID → Instance

All property functions (`instance_get_name`, `instance_get_path`, etc.) take
a `const char* id` and internally resolve it to a `BaseInstance*` using the
helper function in `PluginManager.cpp`:

```cpp
static BaseInstance* resolveInstance(PluginManager::ModuleRuntime* r,
                                    const char* id)
{
    auto* app = r->manager->m_app;
    if (!app || !app->instances() || !id)
        return nullptr;
    auto inst = app->instances()->getInstanceById(QString::fromUtf8(id));
    return inst.get();
}
```

This calls `InstanceList::getInstanceById()`, which performs a linear search
over the internal list. For most users this is negligible (launchers rarely
have thousands of instances), but plugins performing repeated lookups in tight
loops should cache the results of property queries rather than calling the API
repeatedly with the same ID.

For Minecraft-specific operations (components, versions), a second helper
narrows the pointer:

```cpp
static MinecraftInstance* resolveMC(PluginManager::ModuleRuntime* r,
                                   const char* id)
{
    return dynamic_cast<MinecraftInstance*>(resolveInstance(r, id));
}
```

---

## String Ownership and Lifetime

All `const char*` values returned by Instance Query functions are stored in
`ModuleRuntime::tempString`:

```cpp
struct ModuleRuntime {
    PluginManager* manager;
    int moduleIndex;
    std::string dataDir;
    std::string tempString;   // ← returned strings live here
};
```

**Lifetime rule:** The returned pointer is valid until the **next API call
on the same module**. Any subsequent `ctx->instance_get_*()` (or any other
API call) overwrites `tempString` and invalidates the previous pointer.

**Correct usage:**

```c
const char* name = ctx->instance_get_name(mh, id);
if (name) {
    /* Use immediately, or copy: */
    char buf[256];
    strncpy(buf, name, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
}
```

**Incorrect usage (dangling pointer):**

```c
const char* name1 = ctx->instance_get_name(mh, id1);
const char* name2 = ctx->instance_get_name(mh, id2);
/* BUG: name1 is now invalid — tempString was overwritten by name2 */
printf("%s vs %s\n", name1, name2);
```

---

## Thread Safety

All Instance Query functions **must be called from the main (GUI) thread**.
The `InstanceList` is a `QAbstractListModel` and is not thread-safe. Calling
these functions from a background thread leads to undefined behaviour.

Since `mmco_init()` and all hook callbacks are dispatched on the main thread,
this constraint is satisfied automatically in normal plugin code. If your
plugin spawns worker threads (e.g., via `pthread_create` or C++ `std::thread`),
those threads must **not** call any Instance Query functions directly.

---

## Cross-References

| Topic | Section | Description |
|-------|---------|-------------|
| **Launching / killing instances** | [S04 — Instance Management](../s04-instance-management/) | `instance_launch()`, `instance_kill()`, `instance_delete()` |
| **Instance state queries** | [S04 — Instance Management](../s04-instance-management/) | `instance_is_running()`, `instance_can_launch()`, `instance_has_crashed()` |
| **Instance name/notes mutation** | [S04 — Instance Management](../s04-instance-management/) | `instance_set_name()`, `instance_set_notes()`, `instance_set_icon_key()` |
| **Instance lifecycle hooks** | [Hooks Reference](../../hooks-reference/) | `MMCO_HOOK_INSTANCE_CREATED` (`0x0202`), `MMCO_HOOK_INSTANCE_REMOVED` (`0x0203`) |
| **Mod / component queries** | [S05 — Mods](../s05-mods/) | `instance_component_count()`, `instance_component_get_uid()` |
| **World queries** | [S06 — Worlds](../s06-worlds/) | `world_count()`, `world_get_name()` |
| **Instance directories** | [S05 — Mods](../s05-mods/) | `instance_get_jar_mods_dir()`, `instance_get_resource_packs_dir()`, etc. |

---

## Quick Reference: Common Patterns

### List all instances

```c
void list_all_instances(const MMCOContext* ctx) {
    void* mh = ctx->module_handle;
    int n = ctx->instance_count(mh);

    ctx->log_info(mh, "--- Instance List ---");
    for (int i = 0; i < n; i++) {
        const char* id = ctx->instance_get_id(mh, i);
        if (!id) continue;

        /* Copy ID before next API call overwrites tempString */
        char id_buf[256];
        snprintf(id_buf, sizeof(id_buf), "%s", id);

        const char* name = ctx->instance_get_name(mh, id_buf);
        char msg[512];
        snprintf(msg, sizeof(msg), "[%d] %s  (id=%s)", i, name ? name : "?", id_buf);
        ctx->log_info(mh, msg);
    }
}
```

### Find instance by name

```c
const char* find_instance_by_name(const MMCOContext* ctx,
                                  const char* target_name,
                                  char* id_out, size_t id_out_sz) {
    void* mh = ctx->module_handle;
    int n = ctx->instance_count(mh);

    for (int i = 0; i < n; i++) {
        const char* id = ctx->instance_get_id(mh, i);
        if (!id) continue;

        char id_buf[256];
        snprintf(id_buf, sizeof(id_buf), "%s", id);

        const char* name = ctx->instance_get_name(mh, id_buf);
        if (name && strcmp(name, target_name) == 0) {
            snprintf(id_out, id_out_sz, "%s", id_buf);
            return id_out;
        }
    }
    return NULL; /* not found */
}
```

### Filter by type

```c
void list_minecraft_instances(const MMCOContext* ctx) {
    void* mh = ctx->module_handle;
    int n = ctx->instance_count(mh);

    for (int i = 0; i < n; i++) {
        const char* id = ctx->instance_get_id(mh, i);
        if (!id) continue;

        char id_buf[256];
        snprintf(id_buf, sizeof(id_buf), "%s", id);

        const char* type = ctx->instance_get_type(mh, id_buf);
        if (type && strcmp(type, "Minecraft") == 0) {
            const char* name = ctx->instance_get_name(mh, id_buf);
            char msg[512];
            snprintf(msg, sizeof(msg), "MC instance: %s", name ? name : id_buf);
            ctx->log_info(mh, msg);
        }
    }
}
```

---

## Sub-pages

| Page | Contents |
|------|----------|
| [Instance Enumeration](instance-enumeration.md) | `instance_count()`, `instance_get_id()`, `instance_get_name()` — counting, indexing, and naming |
| [Instance Properties](instance-properties.md) | `instance_get_path()`, `instance_get_game_root()`, `instance_get_mods_root()`, `instance_get_icon_key()`, `instance_get_type()` — directories, icons, and types |
