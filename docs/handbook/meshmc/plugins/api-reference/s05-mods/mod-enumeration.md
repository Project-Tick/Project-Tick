# Mod Enumeration

> `mod_count()` · `mod_get_name()` · `mod_get_version()` · `mod_get_filename()` · `mod_get_description()`

This page documents the five functions that let a plugin **discover** which
mods, resource packs, texture packs, shader packs, or core mods are present
in an instance and read their metadata. These are the read-only query
surface of the Mods API — to change mod state, see
[Mod Control](mod-control.md).

---

## `mod_count()`

### Signature

```c
int (*mod_count)(void* mh, const char* instance_id, const char* type);
```

### Description

Returns the total number of mods (or resource packs, etc.) of the given
`type` in the specified instance. The count reflects the current state of
the `ModFolderModel` backing that type — it includes both enabled and
disabled mods.

The count is based on `ModFolderModel::size()`, which returns
`mods.size()` — the length of the internal `QList<Mod>`. This list
is populated by the file-system watcher and updated when the underlying
directory changes.

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. Pass `ctx->module_handle`. |
| `instance_id` | `const char*` | Instance ID string. Must be non-`NULL` and refer to an existing instance. |
| `type` | `const char*` | Resource type: `"loader"`, `"core"`, `"resourcepack"`, `"texturepack"`, or `"shaderpack"`. Case-insensitive. |

### Return value

| Condition | Value |
|-----------|-------|
| Success | Number of mods (≥ 0) |
| `instance_id` is `NULL` or unknown | `0` |
| Instance is not a `MinecraftInstance` | `0` |
| `type` is `NULL` or unrecognised | `0` |

The function never returns a negative value.

### Thread safety

**Main thread only.** `ModFolderModel` inherits `QAbstractListModel` and
is not thread-safe.

### Ownership

No pointer is returned. No ownership concerns.

### Implementation

```cpp
// PluginManager.cpp
int PluginManager::api_mod_count(void* mh, const char* inst, const char* type)
{
    auto* r = rt(mh);
    auto model = resolveModList(r, inst, type);
    return model ? static_cast<int>(model->size()) : 0;
}
```

The call chain is:

```
Plugin:  ctx->mod_count(ctx->module_handle, inst_id, "loader")
    │
    ▼
PluginManager::api_mod_count(mh, inst, type)       ← static trampoline
    │
    ├── ModuleRuntime* r = rt(mh)
    ├── resolveModList(r, inst, type)
    │       ├── resolveMC(r, inst) → MinecraftInstance*
    │       └── mc->loaderModList() → shared_ptr<ModFolderModel>
    │
    └── return model->size()
               │
               └── mods.size()   (QList<Mod>::size)
```

### Example

```c
void print_mod_counts(const MMCOContext* ctx, const char* inst_id)
{
    void* mh = ctx->module_handle;

    const char* types[] = {
        "loader", "core", "resourcepack", "texturepack", "shaderpack"
    };

    for (int t = 0; t < 5; t++) {
        int n = ctx->mod_count(mh, inst_id, types[t]);
        char buf[256];
        snprintf(buf, sizeof(buf), "  %-14s: %d item(s)", types[t], n);
        ctx->log_info(mh, buf);
    }
}
```

### Edge cases

| Scenario | Behaviour |
|----------|-----------|
| Mod directory is empty | Returns `0` |
| Mod directory doesn't exist on disk | `ModFolderModel` reports `0` |
| All mods are disabled (`.disabled` extension) | **Still counted** — disabled mods are in the list |
| Called after `mod_remove()` | Count is reduced by 1 (the model updates on `deleteMods`) |
| Called after `mod_install()` | Count may take a moment to update if the model uses async rescan |
| Called right after `mod_refresh()` | Returns the fresh count |

---

## `mod_get_name()`

### Signature

```c
const char* (*mod_get_name)(void* mh, const char* instance_id,
                            const char* type, int index);
```

### Description

Returns the **human-readable name** of the mod at the given index. The
name is resolved from the mod's internal metadata when available. The
resolution priority is:

1. **Parsed metadata** — If the `.jar`/`.zip` has been parsed and a
   `ModDetails::name` was extracted (from `mcmod.info`, `fabric.mod.json`,
   `mods.toml`, etc.), that name is returned.
2. **File name stem** — If metadata parsing has not completed, failed, or
   produced an empty name, the file name without extension is used. For
   example, `sodium-0.5.8.jar` produces `"sodium-0.5.8"`.

The returned string is the result of `Mod::name()`, which internally calls:

```cpp
QString Mod::name() const
{
    auto dominated = details();
    if (!dominated.name.isEmpty())
        return dominated.name;
    return m_name;
}
```

Where `m_name` is set during construction from the file name with the
extension stripped.

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. Pass `ctx->module_handle`. |
| `instance_id` | `const char*` | Instance ID string. |
| `type` | `const char*` | Resource type selector (case-insensitive). |
| `index` | `int` | Zero-based index. Must be in range `[0, mod_count() - 1]`. |

### Return value

| Condition | Value |
|-----------|-------|
| Success | Pointer to a UTF-8 string containing the mod name |
| `instance_id` is `NULL` or unknown | `nullptr` |
| `type` is `NULL` or unrecognised | `nullptr` |
| `index < 0` | `nullptr` |
| `index >= mod_count()` | `nullptr` |

### String ownership

The returned pointer points into `ModuleRuntime::tempString`. It is
**invalidated by the next `const char*`-returning API call**. Copy the
string immediately if you need to retain it.

### Thread safety

**Main thread only.**

### Implementation

```cpp
// PluginManager.cpp
const char* PluginManager::api_mod_get_name(void* mh, const char* inst,
                                            const char* type, int idx)
{
    auto* r = rt(mh);
    auto model = resolveModList(r, inst, type);
    if (!model || idx < 0 || idx >= static_cast<int>(model->size()))
        return nullptr;
    r->tempString = model->at(idx).name().toStdString();
    return r->tempString.c_str();
}
```

### Example

```c
/* Print the name of the first loader mod */
void show_first_mod(const MMCOContext* ctx, const char* inst_id)
{
    void* mh = ctx->module_handle;
    int n = ctx->mod_count(mh, inst_id, "loader");
    if (n <= 0) {
        ctx->log_info(mh, "No loader mods found.");
        return;
    }

    const char* name = ctx->mod_get_name(mh, inst_id, "loader", 0);
    if (name) {
        char buf[256];
        snprintf(buf, sizeof(buf), "First mod: %s", name);
        ctx->log_info(mh, buf);
    }
}
```

### Edge cases

| Scenario | Behaviour |
|----------|-----------|
| Metadata not yet resolved | Returns the file-name-based fallback name |
| Mod has empty `name` in metadata | Returns the file-name-based fallback name |
| Mod is a folder-type mod | Returns the directory name |
| Mod is disabled | Name still returned (`.disabled` is stripped by `Mod` constructor) |

---

## `mod_get_version()`

### Signature

```c
const char* (*mod_get_version)(void* mh, const char* instance_id,
                               const char* type, int index);
```

### Description

Returns the **version string** of the mod at the given index. The version
comes from the mod's parsed metadata (`ModDetails::version`). If metadata
has not been resolved or the version field is empty, an empty string `""`
is returned (not `nullptr`).

Internally, this calls `Mod::version()`:

```cpp
QString Mod::version() const
{
    auto dominated = details();
    return dominated.version;
}
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. Pass `ctx->module_handle`. |
| `instance_id` | `const char*` | Instance ID string. |
| `type` | `const char*` | Resource type selector (case-insensitive). |
| `index` | `int` | Zero-based index. Must be in range `[0, mod_count() - 1]`. |

### Return value

| Condition | Value |
|-----------|-------|
| Success | Pointer to a UTF-8 version string (may be `""`) |
| `instance_id` is `NULL` or unknown | `nullptr` |
| `type` is `NULL` or unrecognised | `nullptr` |
| `index < 0` | `nullptr` |
| `index >= mod_count()` | `nullptr` |

### String ownership

Same rules as `mod_get_name()` — the pointer lives in `tempString` and is
invalidated by the next `const char*`-returning call.

### Thread safety

**Main thread only.**

### Implementation

```cpp
// PluginManager.cpp
const char* PluginManager::api_mod_get_version(void* mh, const char* inst,
                                               const char* type, int idx)
{
    auto* r = rt(mh);
    auto model = resolveModList(r, inst, type);
    if (!model || idx < 0 || idx >= static_cast<int>(model->size()))
        return nullptr;
    r->tempString = model->at(idx).version().toStdString();
    return r->tempString.c_str();
}
```

### Example

```c
void check_mod_version(const MMCOContext* ctx, const char* inst_id,
                       const char* target_name)
{
    void* mh = ctx->module_handle;
    int n = ctx->mod_count(mh, inst_id, "loader");

    for (int i = 0; i < n; i++) {
        const char* name_ptr = ctx->mod_get_name(mh, inst_id, "loader", i);
        /* Copy immediately — mod_get_version will overwrite tempString */
        char name[256] = {0};
        if (name_ptr) snprintf(name, sizeof(name), "%s", name_ptr);

        const char* ver = ctx->mod_get_version(mh, inst_id, "loader", i);

        if (strcmp(name, target_name) == 0) {
            char buf[512];
            snprintf(buf, sizeof(buf), "Found %s version: %s",
                     name, ver ? ver : "(unknown)");
            ctx->log_info(mh, buf);
            return;
        }
    }
    ctx->log_info(mh, "Mod not found.");
}
```

### Edge cases

| Scenario | Behaviour |
|----------|-----------|
| Metadata not resolved yet | Returns `""` (empty string via `tempString`) |
| `.jar` with no `mcmod.info` | Returns `""` |
| Resource pack (no version concept) | Returns `""` |
| `MOD_FOLDER` type | Returns `""` (no archive to parse) |

---

## `mod_get_filename()`

### Signature

```c
const char* (*mod_get_filename)(void* mh, const char* instance_id,
                                const char* type, int index);
```

### Description

Returns the **file name** (base name only, not the full path) of the mod
at the given index. For enabled mods this is the actual file name on disk;
for disabled mods this includes the `.disabled` suffix.

Internally, the implementation calls `Mod::filename()` which returns a
`QFileInfo`, then extracts `fileName()`:

```cpp
r->tempString = model->at(idx).filename().fileName().toStdString();
```

This means you get just the file name component — for example
`sodium-0.5.8.jar` or `sodium-0.5.8.jar.disabled` — never a path.

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. Pass `ctx->module_handle`. |
| `instance_id` | `const char*` | Instance ID string. |
| `type` | `const char*` | Resource type selector (case-insensitive). |
| `index` | `int` | Zero-based index. Must be in range `[0, mod_count() - 1]`. |

### Return value

| Condition | Value |
|-----------|-------|
| Success | Pointer to a UTF-8 file name string |
| `instance_id` is `NULL` or unknown | `nullptr` |
| `type` is `NULL` or unrecognised | `nullptr` |
| `index < 0` | `nullptr` |
| `index >= mod_count()` | `nullptr` |

### String ownership

Same `tempString` rules. Copy immediately.

### Thread safety

**Main thread only.**

### Implementation

```cpp
// PluginManager.cpp
const char* PluginManager::api_mod_get_filename(void* mh, const char* inst,
                                                const char* type, int idx)
{
    auto* r = rt(mh);
    auto model = resolveModList(r, inst, type);
    if (!model || idx < 0 || idx >= static_cast<int>(model->size()))
        return nullptr;
    r->tempString = model->at(idx).filename().fileName().toStdString();
    return r->tempString.c_str();
}
```

### Example

```c
/* Build a full path to a mod file for external processing */
void get_mod_full_path(const MMCOContext* ctx, const char* inst_id,
                       int mod_idx, char* out_path, size_t out_size)
{
    void* mh = ctx->module_handle;

    /* Get the instance's mods root directory */
    const char* mods_dir = ctx->instance_get_mods_root(mh, inst_id);
    char dir[512] = {0};
    if (mods_dir) snprintf(dir, sizeof(dir), "%s", mods_dir);

    /* Get the file name */
    const char* fname = ctx->mod_get_filename(mh, inst_id, "loader", mod_idx);
    if (fname && dir[0]) {
        snprintf(out_path, out_size, "%s/%s", dir, fname);
    } else {
        out_path[0] = '\0';
    }
}
```

### Edge cases

| Scenario | Behaviour |
|----------|-----------|
| Mod is disabled | Returns name **with** `.disabled` suffix (e.g. `"sodium-0.5.8.jar.disabled"`) |
| Mod is a folder | Returns the directory name |
| File name contains spaces or unicode | Returned as UTF-8, faithfully |

### Relationship to `mod_get_name()`

`mod_get_filename()` returns the **raw file name on disk**, while
`mod_get_name()` returns the **human-readable name** from metadata
(or the file name stem as fallback). They serve different purposes:

| Use case | Function |
|----------|----------|
| Display to user | `mod_get_name()` |
| Build file paths | `mod_get_filename()` |
| Detect disabled state from name | `mod_get_filename()` (check `.disabled` suffix) |
| Log which JAR is loaded | `mod_get_filename()` |

---

## `mod_get_description()`

### Signature

```c
const char* (*mod_get_description)(void* mh, const char* instance_id,
                                   const char* type, int index);
```

### Description

Returns the **description text** of the mod at the given index. The
description comes from the mod's parsed metadata (`ModDetails::description`).
If metadata has not been resolved, parsing failed, or the description field
is empty, an empty string `""` is returned.

Internally, this calls `Mod::description()`:

```cpp
QString Mod::description() const
{
    auto dominated = details();
    return dominated.description;
}
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. Pass `ctx->module_handle`. |
| `instance_id` | `const char*` | Instance ID string. |
| `type` | `const char*` | Resource type selector (case-insensitive). |
| `index` | `int` | Zero-based index. Must be in range `[0, mod_count() - 1]`. |

### Return value

| Condition | Value |
|-----------|-------|
| Success | Pointer to a UTF-8 description string (may be `""`) |
| `instance_id` is `NULL` or unknown | `nullptr` |
| `type` is `NULL` or unrecognised | `nullptr` |
| `index < 0` | `nullptr` |
| `index >= mod_count()` | `nullptr` |

### String ownership

Same `tempString` rules. Copy immediately.

### Thread safety

**Main thread only.**

### Implementation

```cpp
// PluginManager.cpp
const char* PluginManager::api_mod_get_description(void* mh, const char* inst,
                                                   const char* type, int idx)
{
    auto* r = rt(mh);
    auto model = resolveModList(r, inst, type);
    if (!model || idx < 0 || idx >= static_cast<int>(model->size()))
        return nullptr;
    r->tempString = model->at(idx).description().toStdString();
    return r->tempString.c_str();
}
```

### Example

```c
void show_mod_tooltip(const MMCOContext* ctx, const char* inst_id,
                      int mod_idx)
{
    void* mh = ctx->module_handle;

    const char* name_ptr = ctx->mod_get_name(mh, inst_id, "loader", mod_idx);
    char name[256] = {0};
    if (name_ptr) snprintf(name, sizeof(name), "%s", name_ptr);

    const char* desc = ctx->mod_get_description(mh, inst_id, "loader", mod_idx);

    char buf[1024];
    snprintf(buf, sizeof(buf), "%s: %s", name,
             (desc && desc[0]) ? desc : "(no description)");
    ctx->log_info(mh, buf);
}
```

---

## Complete example: full mod lister

The following example enumerates every mod of every type in a given
instance, printing a detailed table:

```c
#include <stdio.h>
#include <string.h>

/*
 * list_all_mods — Enumerate all mod-like resources across all five
 * resource types for a given instance. Prints a table to the MMCO log.
 */
void list_all_mods(const MMCOContext* ctx, const char* inst_id)
{
    void* mh = ctx->module_handle;

    static const char* types[] = {
        "loader", "core", "resourcepack", "texturepack", "shaderpack"
    };
    static const char* labels[] = {
        "Loader Mods", "Core Mods", "Resource Packs",
        "Texture Packs", "Shader Packs"
    };

    char buf[1024];

    for (int t = 0; t < 5; t++) {
        int count = ctx->mod_count(mh, inst_id, types[t]);

        snprintf(buf, sizeof(buf), "=== %s (%d) ===", labels[t], count);
        ctx->log_info(mh, buf);

        if (count == 0) {
            ctx->log_info(mh, "  (none)");
            continue;
        }

        snprintf(buf, sizeof(buf),
                 "  %-4s  %-30s  %-12s  %-7s  %s",
                 "#", "Name", "Version", "Enabled", "Filename");
        ctx->log_info(mh, buf);

        snprintf(buf, sizeof(buf),
                 "  %-4s  %-30s  %-12s  %-7s  %s",
                 "----", "------------------------------",
                 "------------", "-------",
                 "--------------------------------");
        ctx->log_info(mh, buf);

        for (int i = 0; i < count; i++) {
            /* Read name — copy before next call */
            const char* raw_name = ctx->mod_get_name(mh, inst_id, types[t], i);
            char name[256] = {0};
            if (raw_name)
                snprintf(name, sizeof(name), "%.30s", raw_name);

            /* Read version — copy before next call */
            const char* raw_ver = ctx->mod_get_version(mh, inst_id,
                                                        types[t], i);
            char ver[128] = {0};
            if (raw_ver)
                snprintf(ver, sizeof(ver), "%.12s", raw_ver);

            /* Read enabled state (returns int, no tempString issue) */
            int enabled = ctx->mod_is_enabled(mh, inst_id, types[t], i);

            /* Read filename — copy before next call */
            const char* raw_fn = ctx->mod_get_filename(mh, inst_id,
                                                        types[t], i);
            char fn[256] = {0};
            if (raw_fn)
                snprintf(fn, sizeof(fn), "%s", raw_fn);

            snprintf(buf, sizeof(buf),
                     "  %-4d  %-30s  %-12s  %-7s  %s",
                     i, name,
                     ver[0] ? ver : "-",
                     enabled ? "yes" : "NO",
                     fn);
            ctx->log_info(mh, buf);
        }
    }
}
```

**Sample output:**

```text
=== Loader Mods (3) ===
  #     Name                            Version       Enabled  Filename
  ----  ------------------------------  ------------  -------  --------------------------------
  0     Sodium                          0.5.8         yes      sodium-0.5.8.jar
  1     Lithium                         0.12.1        yes      lithium-fabric-0.12.1.jar
  2     Iris Shaders                    1.7.0         NO       iris-1.7.0.jar.disabled
=== Core Mods (0) ===
  (none)
=== Resource Packs (1) ===
  #     Name                            Version       Enabled  Filename
  ----  ------------------------------  ------------  -------  --------------------------------
  0     Faithful 32x                    -             yes      Faithful-32x.zip
=== Texture Packs (0) ===
  (none)
=== Shader Packs (1) ===
  #     Name                            Version       Enabled  Filename
  ----  ------------------------------  ------------  -------  --------------------------------
  0     Complementary Shaders           4.7.1         yes      ComplementaryShaders_v4.7.1.zip
```

---

## Enumeration pattern summary

All five enumeration functions follow an identical pattern:

```text
1.  resolveModList(runtime, instance_id, type) → shared_ptr<ModFolderModel>
2.  Bounds-check index against model->size()
3.  Call the appropriate Mod accessor: name(), version(), filename(), description()
4.  Convert QString → std::string → store in tempString
5.  Return tempString.c_str()  (or nullptr on failure)
```

The `tempString` is a **per-module-runtime** buffer. This means two
different plugins can safely call these functions in the same (main thread)
context without interfering — each plugin has its own `ModuleRuntime`
with its own `tempString`. But within a single plugin, the returned pointer
is always overwritten by the next string-returning call.

---

## See also

- [Section 05 — Mods API (overview)](README.md) — type strings, mod model, `.disabled` mechanism
- [Mod Control](mod-control.md) — `mod_is_enabled()`, `mod_set_enabled()`, `mod_remove()`, `mod_install()`, `mod_refresh()`
- [Section 03 — Instance Query API](../s03-instances/) — obtain instance IDs
