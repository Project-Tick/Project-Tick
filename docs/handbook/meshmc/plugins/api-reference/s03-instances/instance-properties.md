# Instance Properties

> `instance_get_path()` · `instance_get_game_root()` · `instance_get_mods_root()` · `instance_get_icon_key()` · `instance_get_type()`

This page documents the five read-only property functions that retrieve
filesystem paths, the icon key, and the type string for a given instance.
All functions take an instance ID (obtained from
[`instance_get_id()`](instance-enumeration.md#instance_get_id)) as their
key parameter.

---

## `instance_get_path()`

### Signature

```c
const char* (*instance_get_path)(void* mh, const char* id);
```

### Description

Returns the **absolute filesystem path** to the instance's root directory.
This is the top-level directory that MeshMC manages for the instance. It
contains `instance.cfg`, the `.minecraft` game root, and any instance-level
metadata.

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. Pass `ctx->module_handle`. |
| `id` | `const char*` | Instance ID. Null-terminated UTF-8 string obtained from `instance_get_id()`. |

### Return value

| Condition | Value |
|-----------|-------|
| Success | `const char*` — absolute path (null-terminated UTF-8). Example: `/home/user/.local/share/MeshMC/instances/MyInstance` |
| `id` is `nullptr` | `nullptr` |
| No instance with that ID | `nullptr` |
| Application unavailable | `nullptr` |

### Thread safety

**Main thread only.**

### Ownership

Stored in `ModuleRuntime::tempString`. Valid until the next API call on the
same module. Copy immediately if you need to persist the value.

### Implementation

```cpp
// PluginManager.cpp
const char* PluginManager::api_instance_get_path(void* mh, const char* id)
{
    auto* r = rt(mh);
    auto* app = r->manager->m_app;
    if (!app || !app->instances())
        return nullptr;

    auto inst = app->instances()->getInstanceById(QString::fromUtf8(id));
    if (!inst)
        return nullptr;

    r->tempString = inst->instanceRoot().toStdString();
    return r->tempString.c_str();
}
```

`BaseInstance::instanceRoot()` returns the value of the private `m_rootDir`
member, which is the absolute path to the instance folder as set during
instance creation:

```cpp
// BaseInstance.cpp
QString BaseInstance::instanceRoot() const
{
    return m_rootDir;
}
```

### Directory structure

The path returned by `instance_get_path()` points to a directory with this
typical layout:

```
<instance_root>/                 ← returned by instance_get_path()
├── instance.cfg                 ← INI: InstanceType, name, iconKey, notes, etc.
├── mmc-pack.json                ← PackProfile component list
├── patches/                     ← version JSON overrides
└── .minecraft/                  ← game root (returned by instance_get_game_root())
    ├── mods/
    ├── config/
    ├── resourcepacks/
    ├── saves/
    ├── options.txt
    ├── logs/
    └── ...
```

### Example

```c
void print_instance_path(const MMCOContext* ctx, const char* id) {
    void* mh = ctx->module_handle;
    const char* path = ctx->instance_get_path(mh, id);
    if (path) {
        char msg[1024];
        snprintf(msg, sizeof(msg), "Instance root: %s", path);
        ctx->log_info(mh, msg);
    } else {
        ctx->log_error(mh, "Failed to get instance path");
    }
}
```

### Relationship to other path functions

| Function | Returns | Example |
|----------|---------|---------|
| `instance_get_path()` | Instance root | `/home/user/.local/share/MeshMC/instances/MyWorld` |
| `instance_get_game_root()` | Game root (`.minecraft`) | `/home/user/.local/share/MeshMC/instances/MyWorld/.minecraft` |
| `instance_get_mods_root()` | Mods directory | `/home/user/.local/share/MeshMC/instances/MyWorld/.minecraft/mods` |

---

## `instance_get_game_root()`

### Signature

```c
const char* (*instance_get_game_root)(void* mh, const char* id);
```

### Description

Returns the **absolute filesystem path** to the instance's game root
directory. For Minecraft instances, this is the `.minecraft` directory
where Minecraft stores its data: mods, saves, resource packs, configs,
logs, etc.

The game root is the directory that Minecraft itself considers its "game
directory" — the one specified by the `--gameDir` launch argument.

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. Pass `ctx->module_handle`. |
| `id` | `const char*` | Instance ID. Null-terminated UTF-8 string. |

### Return value

| Condition | Value |
|-----------|-------|
| Success | `const char*` — absolute path (null-terminated UTF-8) |
| `id` is `nullptr` | `nullptr` |
| No instance with that ID | `nullptr` |
| Application unavailable | `nullptr` |

### Thread safety

**Main thread only.**

### Ownership

Stored in `ModuleRuntime::tempString`. Valid until the next API call on the
same module.

### Implementation

```cpp
// PluginManager.cpp
const char* PluginManager::api_instance_get_game_root(void* mh, const char* id)
{
    auto* r = rt(mh);
    auto* inst = resolveInstance(r, id);
    if (!inst)
        return nullptr;
    r->tempString = inst->gameRoot().toStdString();
    return r->tempString.c_str();
}
```

Note the use of the `resolveInstance()` helper rather than inline lookup.
This is a shared helper used by all property functions:

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

`BaseInstance::gameRoot()` is a virtual method. The base implementation
returns the instance root itself, but `MinecraftInstance` overrides it to
return the `.minecraft` subdirectory:

```cpp
// BaseInstance.h (default)
virtual QString gameRoot() const {
    return instanceRoot();
}
```

For all current instances (which are `MinecraftInstance`), the override
returns the `.minecraft` subdirectory within the instance root.

### Difference from `instance_get_path()`

| | `instance_get_path()` | `instance_get_game_root()` |
|-|----------------------|---------------------------|
| **Returns** | Instance root directory | Game data directory |
| **Contains** | `instance.cfg`, `.minecraft/`, `patches/` | `mods/`, `saves/`, `config/`, `options.txt` |
| **Backed by** | `BaseInstance::instanceRoot()` | `BaseInstance::gameRoot()` (virtual) |
| **Use case** | Reading instance metadata, backing up the whole instance | Reading/writing game data files |

### Example: Reading a game config file

```c
void check_options(const MMCOContext* ctx, const char* id) {
    void* mh = ctx->module_handle;
    const char* game_root = ctx->instance_get_game_root(mh, id);
    if (!game_root) return;

    char options_path[1024];
    snprintf(options_path, sizeof(options_path), "%s/options.txt", game_root);

    if (ctx->fs_exists_abs(mh, options_path)) {
        ctx->log_info(mh, "options.txt exists in game root");
    }
}
```

---

## `instance_get_mods_root()`

### Signature

```c
const char* (*instance_get_mods_root)(void* mh, const char* id);
```

### Description

Returns the **absolute filesystem path** to the instance's mods directory.
This is the directory where mod loader frameworks (Forge, Fabric, Quilt,
NeoForge) look for mod JAR files.

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. Pass `ctx->module_handle`. |
| `id` | `const char*` | Instance ID. Null-terminated UTF-8 string. |

### Return value

| Condition | Value |
|-----------|-------|
| Success | `const char*` — absolute path. Example: `…/.minecraft/mods` |
| `id` is `nullptr` | `nullptr` |
| No instance with that ID | `nullptr` |
| Application unavailable | `nullptr` |

### Thread safety

**Main thread only.**

### Ownership

Stored in `ModuleRuntime::tempString`. Valid until the next API call on the
same module.

### Implementation

```cpp
// PluginManager.cpp
const char* PluginManager::api_instance_get_mods_root(void* mh, const char* id)
{
    auto* r = rt(mh);
    auto* inst = resolveInstance(r, id);
    if (!inst)
        return nullptr;
    r->tempString = inst->modsRoot().toStdString();
    return r->tempString.c_str();
}
```

`BaseInstance::modsRoot()` is a **pure virtual** method:

```cpp
// BaseInstance.h
virtual QString modsRoot() const = 0;
```

`MinecraftInstance` implements it to return `<game_root>/mods`.

### Example: Check if mods directory exists

```c
void check_mods_dir(const MMCOContext* ctx, const char* id) {
    void* mh = ctx->module_handle;
    const char* mods_root = ctx->instance_get_mods_root(mh, id);
    if (!mods_root) {
        ctx->log_warn(mh, "No mods root for this instance");
        return;
    }

    char mods_copy[1024];
    snprintf(mods_copy, sizeof(mods_copy), "%s", mods_root);

    if (ctx->fs_exists_abs(mh, mods_copy)) {
        ctx->log_info(mh, "Mods directory exists");
    } else {
        ctx->log_info(mh, "Mods directory does not exist yet");
    }
}
```

### Relationship to S05 directory functions

The S05 (Mods) section provides additional directory query functions for
other content types. All return absolute paths:

| Function (S05) | Returns |
|----------------|---------|
| `instance_get_jar_mods_dir(id)` | Jar mods directory |
| `instance_get_resource_packs_dir(id)` | Resource packs directory |
| `instance_get_texture_packs_dir(id)` | Texture packs directory (legacy) |
| `instance_get_shader_packs_dir(id)` | Shader packs directory |
| `instance_get_worlds_dir(id)` | Worlds / saves directory |

These are documented in [S05 — Mods](../s05-mods/).

---

## `instance_get_icon_key()`

### Signature

```c
const char* (*instance_get_icon_key)(void* mh, const char* id);
```

### Description

Returns the **icon key** for the instance. The icon key is a string
identifier that maps to an icon in MeshMC's icon system. It determines
which icon is displayed next to the instance name in the launcher's
instance list.

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. Pass `ctx->module_handle`. |
| `id` | `const char*` | Instance ID. Null-terminated UTF-8 string. |

### Return value

| Condition | Value |
|-----------|-------|
| Success | `const char*` — the icon key string (null-terminated UTF-8) |
| `id` is `nullptr` | `nullptr` |
| No instance with that ID | `nullptr` |
| Application unavailable | `nullptr` |

### Thread safety

**Main thread only.**

### Ownership

Stored in `ModuleRuntime::tempString`. Valid until the next API call on the
same module.

### Implementation

```cpp
// PluginManager.cpp
const char* PluginManager::api_instance_get_icon_key(void* mh, const char* id)
{
    auto* r = rt(mh);
    auto* inst = resolveInstance(r, id);
    if (!inst)
        return nullptr;
    r->tempString = inst->iconKey().toStdString();
    return r->tempString.c_str();
}
```

`BaseInstance::iconKey()` reads the `iconKey` setting from the instance's
`SettingsObject`:

```cpp
// BaseInstance.h
QString iconKey() const;
void setIconKey(QString val);
```

The icon key is stored in `instance.cfg` as the `iconKey` entry.

### Common icon keys

MeshMC ships with a set of built-in icons. Common keys include:

| Key | Description |
|-----|-------------|
| `"default"` | Default dirt block icon |
| `"grass"` | Grass block |
| `"stone"` | Stone block |
| `"creeper"` | Creeper face |
| `"enderman"` | Enderman |
| `"skeleton"` | Skeleton |
| `"flame"` | Fire/flame |
| `"bee"` | Bee |

Users can also set custom icons via the UI, in which case the icon key
may be a path-derived key pointing to a user-supplied image file (e.g.,
`"custom_MyCustomIcon"`). The icon key format is internal to MeshMC and
should be treated as an opaque string by plugins.

### Example

```c
void log_icon(const MMCOContext* ctx, const char* id) {
    void* mh = ctx->module_handle;
    const char* icon = ctx->instance_get_icon_key(mh, id);

    char msg[256];
    snprintf(msg, sizeof(msg), "Icon key: %s", icon ? icon : "(none)");
    ctx->log_info(mh, msg);
}
```

### Mutation

To change an instance's icon key, use `instance_set_icon_key()` from
[S04 — Instance Management](../s04-instance-management/):

```c
int (*instance_set_icon_key)(void* mh, const char* id, const char* key);
```

---

## `instance_get_type()`

### Signature

```c
const char* (*instance_get_type)(void* mh, const char* id);
```

### Description

Returns a **string identifying the type** of the instance. The type
reflects the C++ class backing the instance, which determines what
features and launch procedures are available.

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. Pass `ctx->module_handle`. |
| `id` | `const char*` | Instance ID. Null-terminated UTF-8 string. |

### Return value

| Condition | Value |
|-----------|-------|
| Success | `const char*` — type identifier string (null-terminated UTF-8) |
| `id` is `nullptr` | `nullptr` |
| No instance with that ID | `nullptr` |
| Application unavailable | `nullptr` |

### Thread safety

**Main thread only.**

### Ownership

Stored in `ModuleRuntime::tempString`. Valid until the next API call on the
same module.

### Implementation

```cpp
// PluginManager.cpp
const char* PluginManager::api_instance_get_type(void* mh, const char* id)
{
    auto* r = rt(mh);
    auto* inst = resolveInstance(r, id);
    if (!inst)
        return nullptr;
    r->tempString = inst->instanceType().toStdString();
    return r->tempString.c_str();
}
```

`BaseInstance::instanceType()` reads the `"InstanceType"` key from the
instance settings:

```cpp
// BaseInstance.cpp
QString BaseInstance::instanceType() const
{
    return m_settings->get("InstanceType").toString();
}
```

This value is written when the instance is first created and stored
persistently in `instance.cfg`.

### Possible type values

As of the current version, all MeshMC instances are `MinecraftInstance`
objects. The type string is determined by `MinecraftInstance::typeName()`:

```cpp
// MinecraftInstance.cpp
QString MinecraftInstance::typeName() const
{
    return "Minecraft";
}
```

| Type string | C++ class | Description |
|-------------|-----------|-------------|
| `"Minecraft"` | `MinecraftInstance` | Standard Minecraft instance (vanilla, Forge, Fabric, Quilt, NeoForge, LiteLoader) |

> **Note:** The `"InstanceType"` setting value is `"Minecraft"` for all
> instances in a standard MeshMC installation. Future MeshMC versions or
> forks may add additional instance types. Plugins should handle unknown
> type strings gracefully rather than hard-coding a list.

### How type affects the API

The instance type determines the behaviour of several other API functions:

| Instance type | `instance_get_game_root()` | `instance_get_mods_root()` | Minecraft-specific APIs |
|---------------|---------------------------|---------------------------|-------------------------|
| `"Minecraft"` | Returns `.minecraft` subdir | Returns `.minecraft/mods` | Full access: components, MC version, mod list, worlds |
| Unknown | Falls back to `instanceRoot()` | Behaviour depends on subclass | May return `nullptr` for Minecraft-specific queries |

For `MinecraftInstance`, the Minecraft-specific APIs in S05 and S06
(`instance_component_count`, `instance_get_mc_version`, `mod_count`,
`world_count`) are fully functional. If a future instance type does not
represent Minecraft, these Minecraft-specific calls may return zero/null.

### Example: Filtering by type

```c
void list_minecraft_only(const MMCOContext* ctx) {
    void* mh = ctx->module_handle;
    int n = ctx->instance_count(mh);

    for (int i = 0; i < n; i++) {
        const char* id = ctx->instance_get_id(mh, i);
        if (!id) continue;

        char id_buf[256];
        snprintf(id_buf, sizeof(id_buf), "%s", id);

        const char* type = ctx->instance_get_type(mh, id_buf);
        if (!type || strcmp(type, "Minecraft") != 0)
            continue;

        const char* name = ctx->instance_get_name(mh, id_buf);
        char msg[512];
        snprintf(msg, sizeof(msg), "  Minecraft instance: %s (%s)",
                 name ? name : "?", id_buf);
        ctx->log_info(mh, msg);
    }
}
```

### Example: Defensive type checking

Plugins that only support Minecraft instances should check the type early
and skip unsupported instances:

```c
int process_instance(const MMCOContext* ctx, const char* id) {
    void* mh = ctx->module_handle;

    const char* type = ctx->instance_get_type(mh, id);
    if (!type) {
        ctx->log_error(mh, "Could not determine instance type");
        return -1;
    }

    if (strcmp(type, "Minecraft") != 0) {
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "Unsupported instance type: %s (expected Minecraft)", type);
        ctx->log_warn(mh, msg);
        return -1;
    }

    /* Safe to use Minecraft-specific APIs here */
    const char* mc_ver = ctx->instance_get_mc_version(mh, id);
    /* ... */
    return 0;
}
```

---

## Comprehensive Example: Instance Inspector

This complete example combines all five property functions to build a
detailed log dump of every instance:

```c
#include <string.h>
#include <stdio.h>

void inspect_all_instances(const MMCOContext* ctx) {
    void* mh = ctx->module_handle;
    int count = ctx->instance_count(mh);

    char header[128];
    snprintf(header, sizeof(header),
             "=== Instance Inspector: %d instance(s) ===", count);
    ctx->log_info(mh, header);

    for (int i = 0; i < count; i++) {
        const char* raw_id = ctx->instance_get_id(mh, i);
        if (!raw_id) continue;

        /* Copy ID immediately */
        char id[256];
        snprintf(id, sizeof(id), "%s", raw_id);

        /* Query each property, copying before the next call */
        const char* tmp;

        tmp = ctx->instance_get_name(mh, id);
        char name[256];
        snprintf(name, sizeof(name), "%s", tmp ? tmp : "<unnamed>");

        tmp = ctx->instance_get_type(mh, id);
        char type[64];
        snprintf(type, sizeof(type), "%s", tmp ? tmp : "<unknown>");

        tmp = ctx->instance_get_path(mh, id);
        char path[1024];
        snprintf(path, sizeof(path), "%s", tmp ? tmp : "<no path>");

        tmp = ctx->instance_get_game_root(mh, id);
        char game_root[1024];
        snprintf(game_root, sizeof(game_root), "%s", tmp ? tmp : "<no game root>");

        tmp = ctx->instance_get_mods_root(mh, id);
        char mods_root[1024];
        snprintf(mods_root, sizeof(mods_root), "%s", tmp ? tmp : "<no mods root>");

        tmp = ctx->instance_get_icon_key(mh, id);
        char icon[128];
        snprintf(icon, sizeof(icon), "%s", tmp ? tmp : "<default>");

        /* Now log everything (all values are in local buffers) */
        char msg[2048];
        snprintf(msg, sizeof(msg),
            "\n--- Instance %d ---\n"
            "  ID:         %s\n"
            "  Name:       %s\n"
            "  Type:       %s\n"
            "  Icon:       %s\n"
            "  Root:       %s\n"
            "  Game root:  %s\n"
            "  Mods root:  %s",
            i, id, name, type, icon, path, game_root, mods_root);
        ctx->log_info(mh, msg);
    }
}
```

---

## Summary of All Property Functions

| Function | Returns | Backed by | Notes |
|----------|---------|-----------|-------|
| `instance_get_path(id)` | Instance root dir | `BaseInstance::instanceRoot()` | Non-virtual; returns `m_rootDir` |
| `instance_get_game_root(id)` | Game data dir | `BaseInstance::gameRoot()` | Virtual; `MinecraftInstance` returns `.minecraft` subdir |
| `instance_get_mods_root(id)` | Mods directory | `BaseInstance::modsRoot()` | Pure virtual; `MinecraftInstance` returns `<game_root>/mods` |
| `instance_get_icon_key(id)` | Icon key string | `BaseInstance::iconKey()` | Readable/writable via S04's `instance_set_icon_key()` |
| `instance_get_type(id)` | Type identifier | `BaseInstance::instanceType()` | Read from `InstanceType` setting; currently always `"Minecraft"` |

### Common traits

All five functions share these characteristics:

| Trait | Value |
|-------|-------|
| **First parameter** | `void* mh` — module handle |
| **Second parameter** | `const char* id` — instance ID |
| **Return type** | `const char*` (nullable) |
| **Null return** | Means error (invalid ID, unavailable app, null input) |
| **String storage** | `ModuleRuntime::tempString` |
| **String lifetime** | Until next API call on same module |
| **Thread safety** | Main thread only |
| **Internal resolution** | Via `resolveInstance()` → `InstanceList::getInstanceById()` |
| **Complexity** | O(n) in number of instances (linear scan by ID) |

---

## Error Reference

| Error scenario | Affected functions | Return | How to diagnose |
|----------------|-------------------|--------|-----------------|
| `id` is `NULL` | All | `nullptr` | Check that `instance_get_id()` succeeded |
| Instance deleted | All | `nullptr` | Re-enumerate; ID is stale |
| App not initialized | All | `nullptr` | Defer to `MMCO_HOOK_APP_INITIALIZED` |
| Wrong thread | All | Undefined behaviour | Ensure main thread; check with `QThread::currentThread()` |
| `tempString` overwritten | All | Dangling pointer | Copy result before next API call |

---

## Cross-References

- **[README — Section Overview](README.md)** — Instance model, index system, string ownership
- **[Instance Enumeration](instance-enumeration.md)** — `instance_count()`, `instance_get_id()`, `instance_get_name()`
- **[S04 — Instance Management](../s04-instance-management/)** — Mutation and lifecycle operations
- **[S05 — Mods](../s05-mods/)** — Component queries and content directory functions
- **[Hooks Reference](../../hooks-reference/)** — `MMCO_HOOK_INSTANCE_CREATED`, `MMCO_HOOK_INSTANCE_REMOVED`
