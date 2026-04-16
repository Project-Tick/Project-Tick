# Section 05 — Mods API

> **Header:** `PluginAPI.h` (function pointer declarations) · **SDK:** `mmco_sdk.h` (struct definition) · **Implementation:** `PluginManager.cpp` (static trampolines)

---

## Overview

The Mods API gives plugins the ability to **enumerate, inspect, enable,
disable, remove, install, and refresh** the mod-like resources associated
with a Minecraft instance. Unlike the Instance Query API (Section 03),
which operates on instances themselves, the Mods API operates on the
*contents* of an instance — the individual `.jar`, `.zip`, `.litemod`,
and folder entries that live inside its various resource directories.

The API is generic: the same set of function pointers handles loader mods,
core mods, resource packs, texture packs, and shader packs. A `type`
string parameter selects which list to operate on.

```text
┌─────────────────────────────────────────────────────────────────────────┐
│  1. mod_count(inst, type)                → total mods of that type     │
│  2. mod_get_name(inst, type, index)      → human-readable name         │
│  3. mod_get_version(inst, type, index)   → version string              │
│  4. mod_get_filename(inst, type, index)  → file name on disk           │
│  5. mod_get_description(inst, type, index) → description text          │
│  6. mod_is_enabled(inst, type, index)    → 1 if active, 0 if disabled  │
│  7. mod_set_enabled(inst, type, index, enabled) → enable/disable       │
│  8. mod_remove(inst, type, index)        → delete a mod                │
│  9. mod_install(inst, type, filepath)    → install a mod from path     │
│ 10. mod_refresh(inst, type)              → rescan the directory         │
└─────────────────────────────────────────────────────────────────────────┘
```

All ten function pointers live in `MMCOContext` and are declared in
`PluginAPI.h`:

```c
/* S05 — Mod Management  (from MMCOContext in PluginAPI.h) */

/* Enumeration */
int         (*mod_count)       (void* mh, const char* instance_id,
                                const char* type);
const char* (*mod_get_name)    (void* mh, const char* instance_id,
                                const char* type, int index);
const char* (*mod_get_version) (void* mh, const char* instance_id,
                                const char* type, int index);
const char* (*mod_get_filename)(void* mh, const char* instance_id,
                                const char* type, int index);
const char* (*mod_get_description)(void* mh, const char* instance_id,
                                const char* type, int index);

/* Control */
int (*mod_is_enabled)  (void* mh, const char* instance_id,
                        const char* type, int index);
int (*mod_set_enabled) (void* mh, const char* instance_id,
                        const char* type, int index, int enabled);
int (*mod_remove)      (void* mh, const char* instance_id,
                        const char* type, int index);
int (*mod_install)     (void* mh, const char* instance_id,
                        const char* type, const char* filepath);
int (*mod_refresh)     (void* mh, const char* instance_id,
                        const char* type);
```

---

## Function summary

| Function | Sub-page | Purpose | Returns |
|----------|----------|---------|---------|
| [`mod_count()`](mod-enumeration.md#mod_count) | Enumeration | Number of mods of a given type | `int` (≥ 0) |
| [`mod_get_name()`](mod-enumeration.md#mod_get_name) | Enumeration | Human-readable mod name | `const char*` (nullable) |
| [`mod_get_version()`](mod-enumeration.md#mod_get_version) | Enumeration | Mod version string | `const char*` (nullable) |
| [`mod_get_filename()`](mod-enumeration.md#mod_get_filename) | Enumeration | File name on disk | `const char*` (nullable) |
| [`mod_get_description()`](mod-enumeration.md#mod_get_description) | Enumeration | Mod description text | `const char*` (nullable) |
| [`mod_is_enabled()`](mod-control.md#mod_is_enabled) | Control | Whether the mod is active | `int` (0 or 1) |
| [`mod_set_enabled()`](mod-control.md#mod_set_enabled) | Control | Enable or disable a mod | `int` (0 or −1) |
| [`mod_remove()`](mod-control.md#mod_remove) | Control | Delete a mod from disk | `int` (0 or −1) |
| [`mod_install()`](mod-control.md#mod_install) | Control | Copy a mod file into the instance | `int` (0 or −1) |
| [`mod_refresh()`](mod-control.md#mod_refresh) | Control | Rescan the mod directory | `int` (0 or −1) |

---

## The Mod Model in MeshMC

### What is a "mod" in this API?

In the context of this API, a **mod** is any file-based resource placed in
one of an instance's special directories. This includes traditional Java
mods (`.jar`, `.zip`, `.litemod`), resource packs, texture packs, shader
packs, and core mods. The API treats them all uniformly through the
`ModFolderModel` class internally.

On disk, each resource type lives in a specific subdirectory of the
instance's game root:

| Type string | Directory | Underlying list accessor |
|-------------|-----------|--------------------------|
| `"loader"` | `mods/` | `MinecraftInstance::loaderModList()` |
| `"core"` | `coremods/` | `MinecraftInstance::coreModList()` |
| `"resourcepack"` | `resourcepacks/` | `MinecraftInstance::resourcePackList()` |
| `"texturepack"` | `texturepacks/` | `MinecraftInstance::texturePackList()` |
| `"shaderpack"` | `shaderpacks/` | `MinecraftInstance::shaderPackList()` |

The `type` string is **case-insensitive** — `"Loader"`, `"LOADER"`, and
`"loader"` all resolve to the same list. Any unrecognised string returns
`nullptr` internally and the API call fails gracefully.

### Mod types

Each mod on disk is classified by the `Mod::ModType` enum:

```cpp
enum ModType {
    MOD_UNKNOWN,    // Unrecognised format
    MOD_ZIPFILE,    // .jar or .zip archive
    MOD_SINGLEFILE, // single file (not archive, not folder)
    MOD_FOLDER,     // directory-based mod
    MOD_LITEMOD,    // .litemod file
};
```

The type is determined at construction from the file extension:

| Extension | ModType |
|-----------|---------|
| `.jar` | `MOD_ZIPFILE` |
| `.zip` | `MOD_ZIPFILE` |
| `.litemod` | `MOD_LITEMOD` |
| directory | `MOD_FOLDER` |
| anything else | `MOD_SINGLEFILE` |

### Mod metadata — `ModDetails`

When a mod is a `.jar` or `.zip`, MeshMC asynchronously parses its
metadata (from `mcmod.info`, `fabric.mod.json`, `mods.toml`, etc.) and
populates a `ModDetails` struct:

```cpp
struct ModDetails {
    QString mod_id;       // Machine-readable mod identifier
    QString name;         // Human-readable display name
    QString version;      // Mod version string
    QString mcversion;    // Compatible Minecraft version
    QString homeurl;      // Project homepage URL
    QString updateurl;    // Update URL
    QString description;  // Mod description text
    QStringList authors;  // List of authors
    QString credits;      // Credits / acknowledgements
};
```

The MMCO API exposes a subset of this metadata through `mod_get_name()`,
`mod_get_version()`, and `mod_get_description()`. If metadata has not been
resolved yet (or parsing fails), these functions fall back to the file name
as the mod name and return empty strings for version/description.

### The `.disabled` extension mechanism

MeshMC manages mod enabled/disabled state through a **file renaming**
convention. A disabled mod has `.disabled` appended to its file name:

```text
Enabled:   sodium-0.5.8.jar
Disabled:  sodium-0.5.8.jar.disabled
```

When `Mod::enable(false)` is called:
1. The file `sodium-0.5.8.jar` is renamed to `sodium-0.5.8.jar.disabled`
2. The `m_enabled` flag is set to `false`
3. The mod's `m_file` `QFileInfo` is repathed to the new name

When `Mod::enable(true)` is called:
1. The trailing `.disabled` (9 characters) is chopped from the file path
2. The file is renamed back to `sodium-0.5.8.jar`
3. The `m_enabled` flag is set to `true`

**Important constraints:**
- `MOD_FOLDER` type mods cannot be disabled (`.disabled` rename is not
  attempted — `enable()` returns `false`)
- `MOD_UNKNOWN` type mods cannot be disabled
- If the mod is already in the requested state, `enable()` returns `false`
  (no-op, but the MMCO wrapper treats this as success)
- The rename is atomic on most filesystems — the mod is never "half
  disabled"

### How mod lists are resolved: `resolveModList()`

Every S05 API function follows the same internal resolution path:

```text
Plugin call
    │
    ▼
PluginManager::api_mod_*(mh, inst, type, ...)
    │
    ├── rt(mh) → ModuleRuntime*
    ├── resolveModList(runtime, inst, type)
    │       │
    │       ├── resolveMC(runtime, inst) → MinecraftInstance*
    │       │       │
    │       │       ├── Application::instances()->getInstanceById(inst)
    │       │       └── dynamic_cast<MinecraftInstance*>(...)
    │       │
    │       ├── switch on type string:
    │       │     "loader"       → mc->loaderModList()
    │       │     "core"         → mc->coreModList()
    │       │     "resourcepack" → mc->resourcePackList()
    │       │     "texturepack"  → mc->texturePackList()
    │       │     "shaderpack"   → mc->shaderPackList()
    │       │
    │       └── return shared_ptr<ModFolderModel>  (or nullptr)
    │
    └── Operate on the ModFolderModel (size, at, etc.)
```

The `resolveModList` helper is a static function in `PluginManager.cpp`:

```cpp
static std::shared_ptr<ModFolderModel>
resolveModList(PluginManager::ModuleRuntime* r,
               const char* inst_id, const char* type)
{
    auto* mc = resolveMC(r, inst_id);
    if (!mc || !type)
        return nullptr;

    QString t = QString::fromUtf8(type).toLower();
    if (t == "loader")       return mc->loaderModList();
    if (t == "core")         return mc->coreModList();
    if (t == "resourcepack") return mc->resourcePackList();
    if (t == "texturepack")  return mc->texturePackList();
    if (t == "shaderpack")   return mc->shaderPackList();
    return nullptr;
}
```

### String ownership via `tempString`

All `const char*` returns from S05 functions use the **`tempString`
pattern** established throughout the MMCO API. The returned pointer points
into `ModuleRuntime::tempString` — a `std::string` member of the runtime
struct that is overwritten on every API call that returns a string.

**Rules:**
1. Copy the returned string **immediately** if you need to keep it.
2. The pointer is invalidated by the **next** API call that returns a
   `const char*`.
3. If the function fails, it returns `nullptr` — never a dangling pointer.

```c
/* CORRECT — copy before next call */
const char* name_ptr = ctx->mod_get_name(mh, inst, "loader", 0);
char name[256];
if (name_ptr) snprintf(name, sizeof(name), "%s", name_ptr);

const char* ver_ptr = ctx->mod_get_version(mh, inst, "loader", 0);
/* name_ptr is NOW INVALID — but we saved into name[] */
```

---

## Common parameters across all functions

Every S05 function shares three leading parameters:

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. Pass `ctx->module_handle`. |
| `instance_id` | `const char*` | The string ID of the target instance (from `instance_get_id()`). |
| `type` | `const char*` | Resource type selector: `"loader"`, `"core"`, `"resourcepack"`, `"texturepack"`, or `"shaderpack"`. Case-insensitive. |

Functions that address a specific mod add:

| Parameter | Type | Description |
|-----------|------|-------------|
| `index` | `int` | Zero-based index into the mod list (0 to `mod_count() - 1`). |

---

## Common error conditions

| Condition | Behaviour |
|-----------|-----------|
| `instance_id` is `NULL` | Returns `0`, `nullptr`, or `-1` depending on function |
| `instance_id` does not match any instance | Returns `0`, `nullptr`, or `-1` |
| Instance exists but is not a `MinecraftInstance` | `resolveMC()` returns `nullptr` → error result |
| `type` is `NULL` | `resolveModList()` returns `nullptr` → error result |
| `type` is not one of the five recognised strings | `resolveModList()` returns `nullptr` → error result |
| `index` is negative | Bounds check fails → error result |
| `index` ≥ `mod_count()` | Bounds check fails → error result |

---

## Thread safety

All S05 functions must be called from the **main (GUI) thread** only.
`ModFolderModel` is a `QAbstractListModel` subclass and is not thread-safe.
Calling these functions from a background thread results in **undefined
behaviour**.

---

## Sub-pages

| Page | Content |
|------|---------|
| [Mod Enumeration](mod-enumeration.md) | `mod_count()`, `mod_get_name()`, `mod_get_version()`, `mod_get_filename()`, `mod_get_description()` |
| [Mod Control](mod-control.md) | `mod_is_enabled()`, `mod_set_enabled()`, `mod_remove()`, `mod_install()`, `mod_refresh()` |

---

## Quick example: list all loader mods

```c
void list_loader_mods(const MMCOContext* ctx, const char* inst_id)
{
    void* mh = ctx->module_handle;
    int n = ctx->mod_count(mh, inst_id, "loader");

    char buf[512];
    snprintf(buf, sizeof(buf), "Instance %s has %d loader mod(s):", inst_id, n);
    ctx->log_info(mh, buf);

    for (int i = 0; i < n; i++) {
        const char* name = ctx->mod_get_name(mh, inst_id, "loader", i);
        char saved_name[256];
        snprintf(saved_name, sizeof(saved_name), "%s", name ? name : "(unknown)");

        const char* ver = ctx->mod_get_version(mh, inst_id, "loader", i);
        char saved_ver[128];
        snprintf(saved_ver, sizeof(saved_ver), "%s", ver ? ver : "");

        int enabled = ctx->mod_is_enabled(mh, inst_id, "loader", i);

        snprintf(buf, sizeof(buf), "  [%d] %s v%s  %s",
                 i, saved_name, saved_ver,
                 enabled ? "(enabled)" : "(DISABLED)");
        ctx->log_info(mh, buf);
    }
}
```

---

## See also

- [Section 03 — Instance Query API](../s03-instances/) — obtain instance IDs
- [Section 04 — Instance Management](../s04-instance-management/) — launch, kill, delete instances
- [Section 06 — Worlds API](../s06-worlds/) — enumerate and manage worlds
