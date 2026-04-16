# Mod Control

> `mod_is_enabled()` · `mod_set_enabled()` · `mod_remove()` · `mod_install()` · `mod_refresh()`

This page documents the five functions that let a plugin **inspect and
change** the state of mods in an instance — enabling, disabling, removing,
installing, and refreshing. For read-only enumeration of mods, see
[Mod Enumeration](mod-enumeration.md).

---

## `mod_is_enabled()`

### Signature

```c
int (*mod_is_enabled)(void* mh, const char* instance_id,
                      const char* type, int index);
```

### Description

Returns whether the mod at the given index is **enabled** (active) or
**disabled** (inactive). In MeshMC, a disabled mod has the `.disabled`
extension appended to its file name on disk. This function reads the
`Mod::enabled()` flag, which is set during construction by inspecting the
file name.

The relationship between the file name and the enabled state:

```text
sodium-0.5.8.jar            → enabled = true
sodium-0.5.8.jar.disabled   → enabled = false
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
| Mod is enabled | `1` |
| Mod is disabled | `0` |
| `instance_id` is `NULL` or unknown | `0` |
| `type` is `NULL` or unrecognised | `0` |
| `index` out of bounds | `0` |

**Note:** On error, this function returns `0`, which is the same as
"disabled". There is no way to distinguish "mod is disabled" from "invalid
arguments" using the return value alone. If you need to validate parameters,
call `mod_count()` first to confirm the index is in range.

### Thread safety

**Main thread only.**

### Ownership

No pointer is returned. No ownership concerns.

### Implementation

```cpp
// PluginManager.cpp
int PluginManager::api_mod_is_enabled(void* mh, const char* inst,
                                      const char* type, int idx)
{
    auto* r = rt(mh);
    auto model = resolveModList(r, inst, type);
    if (!model || idx < 0 || idx >= static_cast<int>(model->size()))
        return 0;
    return model->at(idx).enabled() ? 1 : 0;
}
```

The call chain is:

```
Plugin:  ctx->mod_is_enabled(mh, inst_id, "loader", 2)
    │
    ▼
PluginManager::api_mod_is_enabled(mh, inst, type, idx)
    │
    ├── resolveModList(r, inst, type) → shared_ptr<ModFolderModel>
    ├── Bounds check: idx in [0, model->size())
    └── return model->at(idx).enabled() ? 1 : 0
                         │
                         └── Mod::enabled()
                                │
                                └── return m_enabled;
```

### Example

```c
/* Count how many loader mods are currently disabled */
int count_disabled_mods(const MMCOContext* ctx, const char* inst_id)
{
    void* mh = ctx->module_handle;
    int total = ctx->mod_count(mh, inst_id, "loader");
    int disabled = 0;

    for (int i = 0; i < total; i++) {
        if (!ctx->mod_is_enabled(mh, inst_id, "loader", i)) {
            disabled++;
        }
    }
    return disabled;
}
```

### Edge cases

| Scenario | Behaviour |
|----------|-----------|
| `MOD_FOLDER` type mod | Always returns `1` — folder mods cannot be disabled |
| `MOD_UNKNOWN` type mod | Always returns `1` (default `m_enabled = true`) |
| Mod was just disabled via `mod_set_enabled()` | Returns `0` |
| Mod file renamed externally (outside MeshMC) | State may be stale until `mod_refresh()` |

---

## `mod_set_enabled()`

### Signature

```c
int (*mod_set_enabled)(void* mh, const char* instance_id,
                       const char* type, int index, int enabled);
```

### Description

Enables or disables the mod at the given index. This is the primary mod
toggling function. It works by **renaming the file on disk** — appending
or removing the `.disabled` extension.

When `enabled` is non-zero (truthy), the mod is **enabled**:
- The `.disabled` suffix (9 chars) is removed from the file name
- The file is renamed: `foo.jar.disabled` → `foo.jar`

When `enabled` is zero (falsy), the mod is **disabled**:
- The `.disabled` suffix is appended to the file name
- The file is renamed: `foo.jar` → `foo.jar.disabled`

The implementation delegates to `ModFolderModel::setModStatus()`:

```cpp
model->setModStatus(indices, e ? ModFolderModel::Enable
                                : ModFolderModel::Disable)
```

This in turn calls `Mod::enable()`, which performs the actual file rename:

```cpp
bool Mod::enable(bool value)
{
    if (m_type == Mod::MOD_UNKNOWN || m_type == Mod::MOD_FOLDER)
        return false;           // Cannot toggle these types

    if (m_enabled == value)
        return false;           // Already in desired state

    QString path = m_file.absoluteFilePath();
    if (value) {
        // Enable: remove .disabled suffix
        QFile foo(path);
        if (!path.endsWith(".disabled"))
            return false;
        path.chop(9);
        if (!foo.rename(path))
            return false;
    } else {
        // Disable: append .disabled suffix
        QFile foo(path);
        path += ".disabled";
        if (!foo.rename(path))
            return false;
    }
    repath(QFileInfo(path));
    m_enabled = value;
    return true;
}
```

After the rename, `ModFolderModel::setModStatus()` updates the internal
index map (`modsIndex`) and emits `dataChanged` to notify the UI.

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. Pass `ctx->module_handle`. |
| `instance_id` | `const char*` | Instance ID string. |
| `type` | `const char*` | Resource type selector (case-insensitive). |
| `index` | `int` | Zero-based index into the mod list. |
| `enabled` | `int` | Desired state: non-zero = enable, zero = disable. |

### Return value

| Condition | Value |
|-----------|-------|
| Success | `0` |
| Mod was already in the requested state | `0` (treated as success internally — `setModStatus` returns `true` when state matches) |
| `instance_id` is `NULL` or unknown | `-1` |
| `type` is `NULL` or unrecognised | `-1` |
| `index` out of bounds | `-1` |
| Mod is `MOD_FOLDER` or `MOD_UNKNOWN` type (cannot toggle) | `-1` |
| File system rename failed (permissions, disk full, etc.) | `-1` |
| Model interaction is disabled (e.g. during an update) | `-1` |

### Thread safety

**Main thread only.** This function modifies the file system and the
`ModFolderModel` state. Calling from a background thread is undefined
behaviour.

### Side effects

1. **File renamed on disk** — the actual `.jar` / `.zip` / `.litemod` file
   is renamed immediately.
2. **Model index updated** — the `modsIndex` map inside `ModFolderModel`
   is updated to reflect the new file name / MMC ID.
3. **UI notification** — `dataChanged` signal is emitted, causing the
   mod list view to refresh the affected row.
4. **Index stability** — the mod's position in the list (its index) does
   **not** change after enabling/disabling. The same index is valid before
   and after the call.

### Implementation

```cpp
// PluginManager.cpp
int PluginManager::api_mod_set_enabled(void* mh, const char* inst,
                                       const char* type, int idx, int e)
{
    auto* r = rt(mh);
    auto model = resolveModList(r, inst, type);
    if (!model || idx < 0 || idx >= static_cast<int>(model->size()))
        return -1;
    QModelIndexList indices;
    indices.append(model->index(idx, 0));
    return model->setModStatus(indices, e ? ModFolderModel::Enable
                                          : ModFolderModel::Disable)
               ? 0
               : -1;
}
```

### Example: toggle a specific mod

```c
/* Toggle the enabled state of mod at a given index */
int toggle_mod(const MMCOContext* ctx, const char* inst_id,
               const char* type, int mod_idx)
{
    void* mh = ctx->module_handle;

    int current = ctx->mod_is_enabled(mh, inst_id, type, mod_idx);
    int desired = current ? 0 : 1;

    return ctx->mod_set_enabled(mh, inst_id, type, mod_idx, desired);
}
```

### Example: disable all mods in an instance

```c
/* Disable every loader mod in the instance */
void disable_all_mods(const MMCOContext* ctx, const char* inst_id)
{
    void* mh = ctx->module_handle;
    int n = ctx->mod_count(mh, inst_id, "loader");

    for (int i = 0; i < n; i++) {
        if (ctx->mod_is_enabled(mh, inst_id, "loader", i)) {
            int rc = ctx->mod_set_enabled(mh, inst_id, "loader", i, 0);
            if (rc != 0) {
                /* Copy name before logging (tempString concern) */
                const char* fn = ctx->mod_get_filename(mh, inst_id,
                                                        "loader", i);
                char name[256] = {0};
                if (fn) snprintf(name, sizeof(name), "%s", fn);

                char buf[512];
                snprintf(buf, sizeof(buf),
                         "Failed to disable mod [%d] %s", i, name);
                ctx->log_warning(mh, buf);
            }
        }
    }
    ctx->log_info(mh, "All loader mods disabled.");
}
```

### Edge cases

| Scenario | Behaviour |
|----------|-----------|
| Mod is already enabled, `enabled=1` | Returns `0` (no-op, success) |
| Mod is already disabled, `enabled=0` | Returns `0` (no-op, success) |
| Mod is a folder | Returns `-1` (folders cannot be toggled) |
| File is locked by another process | Returns `-1` (rename fails) |
| Mod directory is read-only | Returns `-1` (rename fails) |
| Index becomes invalid after `mod_remove()` | Undefined — always re-query `mod_count()` after removals |

---

## `mod_remove()`

### Signature

```c
int (*mod_remove)(void* mh, const char* instance_id,
                  const char* type, int index);
```

### Description

**Permanently deletes** the mod at the given index from disk. This is a
destructive operation — the mod file (or folder) is removed from the file
system and cannot be recovered through the API.

The implementation delegates to `ModFolderModel::deleteMods()`, which
calls `Mod::destroy()`:

```cpp
bool Mod::destroy()
{
    m_type = MOD_UNKNOWN;
    return FS::deletePath(m_file.filePath());
}
```

After deletion, the `ModFolderModel` removes the entry from its internal
list. **All indices at or above the deleted index shift down by one.**

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. Pass `ctx->module_handle`. |
| `instance_id` | `const char*` | Instance ID string. |
| `type` | `const char*` | Resource type selector (case-insensitive). |
| `index` | `int` | Zero-based index of the mod to remove. |

### Return value

| Condition | Value |
|-----------|-------|
| Success | `0` |
| `instance_id` is `NULL` or unknown | `-1` |
| `type` is `NULL` or unrecognised | `-1` |
| `index` out of bounds | `-1` |
| Deletion failed (permissions, file in use) | `-1` |
| Model interaction is disabled | `-1` |

### Thread safety

**Main thread only.**

### Side effects

1. **File or folder deleted from disk** permanently.
2. **Model updated** — the entry is removed from the internal `QList<Mod>`.
3. **Index invalidation** — indices of mods after the deleted one shift
   down by 1. If you are iterating and removing, iterate **backwards** to
   avoid skipping entries.

### Implementation

```cpp
// PluginManager.cpp
int PluginManager::api_mod_remove(void* mh, const char* inst, const char* type,
                                  int idx)
{
    auto* r = rt(mh);
    auto model = resolveModList(r, inst, type);
    if (!model || idx < 0 || idx >= static_cast<int>(model->size()))
        return -1;
    QModelIndexList indices;
    indices.append(model->index(idx, 0));
    return model->deleteMods(indices) ? 0 : -1;
}
```

### Example: remove all disabled mods

```c
/* Remove every disabled loader mod (iterate backwards!) */
void clean_disabled_mods(const MMCOContext* ctx, const char* inst_id)
{
    void* mh = ctx->module_handle;
    int n = ctx->mod_count(mh, inst_id, "loader");

    /* Iterate backwards to keep indices stable */
    for (int i = n - 1; i >= 0; i--) {
        if (!ctx->mod_is_enabled(mh, inst_id, "loader", i)) {
            const char* fn = ctx->mod_get_filename(mh, inst_id, "loader", i);
            char name[256] = {0};
            if (fn) snprintf(name, sizeof(name), "%s", fn);

            int rc = ctx->mod_remove(mh, inst_id, "loader", i);
            char buf[512];
            snprintf(buf, sizeof(buf), "Removed disabled mod: %s (%s)",
                     name, rc == 0 ? "ok" : "FAILED");
            ctx->log_info(mh, buf);
        }
    }
}
```

### Warning

This function is **irreversible**. There is no undo or recycle bin. If
your plugin offers mod removal functionality, consider prompting the user
via `ui_show_message()` first.

---

## `mod_install()`

### Signature

```c
int (*mod_install)(void* mh, const char* instance_id,
                   const char* type, const char* filepath);
```

### Description

Installs a mod by **copying** a file from an arbitrary path into the
appropriate mod folder of the instance. The file is validated (must exist,
be readable, and be a recognised mod type) and then copied into the
instance's mod directory.

The implementation delegates to `ModFolderModel::installMod()`, which:

1. Normalises the source path
2. Checks the file exists and is readable
3. Constructs a `Mod` from the file to validate its type
4. Rejects `MOD_UNKNOWN` files
5. Copies the file into `m_dir` (the watched mod folder)
6. For `MOD_FOLDER` types, performs a recursive directory copy
7. Calls `update()` to rescan the directory

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. Pass `ctx->module_handle`. |
| `instance_id` | `const char*` | Instance ID string. |
| `type` | `const char*` | Resource type selector (case-insensitive). |
| `filepath` | `const char*` | Absolute path to the file to install. Must be a valid, readable file. |

### Return value

| Condition | Value |
|-----------|-------|
| Success | `0` |
| `instance_id` is `NULL` or unknown | `-1` |
| `type` is `NULL` or unrecognised | `-1` |
| `filepath` is `NULL` | `-1` |
| File does not exist or is not readable | `-1` |
| File is not a recognised mod type | `-1` |
| Copy failed (permissions, disk full) | `-1` |
| Model interaction is disabled | `-1` |

### Thread safety

**Main thread only.**

### Side effects

1. **File copied** into the instance's mod directory.
2. **Model updated** via `update()` (triggers a directory rescan).
3. If a file with the same name already exists in the target directory,
   the existing file is **overwritten** (removed first, then copied).

### Implementation

```cpp
// PluginManager.cpp
int PluginManager::api_mod_install(void* mh, const char* inst,
                                   const char* type, const char* path)
{
    auto* r = rt(mh);
    auto model = resolveModList(r, inst, type);
    if (!model || !path)
        return -1;
    return model->installMod(QString::fromUtf8(path)) ? 0 : -1;
}
```

### Example: install a mod from a file dialog

```c
void install_mod_dialog(const MMCOContext* ctx, const char* inst_id)
{
    void* mh = ctx->module_handle;

    /* Open a file picker */
    const char* path = ctx->ui_file_open_dialog(
        mh, "Select Mod File", "Mod Files (*.jar *.zip *.litemod)");

    if (!path) {
        ctx->log_info(mh, "User cancelled mod installation.");
        return;
    }

    /* Copy the path before it's overwritten */
    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "%s", path);

    int rc = ctx->mod_install(mh, inst_id, "loader", filepath);

    char buf[1024];
    snprintf(buf, sizeof(buf), "Install mod from %s: %s",
             filepath, rc == 0 ? "success" : "FAILED");
    ctx->log_info(mh, buf);

    if (rc == 0) {
        /* Optionally refresh to ensure the model is up to date */
        ctx->mod_refresh(mh, inst_id, "loader");
    }
}
```

---

## `mod_refresh()`

### Signature

```c
int (*mod_refresh)(void* mh, const char* instance_id, const char* type);
```

### Description

Forces the `ModFolderModel` to **rescan its directory** from disk. This
picks up any changes made outside of MeshMC (e.g. if another process
added, removed, or renamed mod files). It also picks up the results of
any pending asynchronous metadata resolution.

The implementation calls `ModFolderModel::update()`, which:

1. Walks the mod directory
2. Rebuilds the internal `QList<Mod>` from the files found
3. Re-triggers metadata resolution for any new/unresolved mods
4. Emits `updateFinished()` when complete

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. Pass `ctx->module_handle`. |
| `instance_id` | `const char*` | Instance ID string. |
| `type` | `const char*` | Resource type selector (case-insensitive). |

### Return value

| Condition | Value |
|-----------|-------|
| Success | `0` |
| `instance_id` is `NULL` or unknown | `-1` |
| `type` is `NULL` or unrecognised | `-1` |

### Thread safety

**Main thread only.**

### Side effects

1. **Directory rescan** — the model's file list is rebuilt from disk.
2. **Index invalidation** — mod indices may change after a refresh if
   files were added or removed externally. Always re-query `mod_count()`
   after calling `mod_refresh()`.
3. **Metadata re-resolution** — any new mods trigger async metadata parsing.

### Implementation

```cpp
// PluginManager.cpp
int PluginManager::api_mod_refresh(void* mh, const char* inst,
                                   const char* type)
{
    auto* r = rt(mh);
    auto model = resolveModList(r, inst, type);
    if (!model)
        return -1;
    return model->update() ? 0 : -1;
}
```

### Example

```c
void refresh_and_recount(const MMCOContext* ctx, const char* inst_id)
{
    void* mh = ctx->module_handle;

    ctx->mod_refresh(mh, inst_id, "loader");

    int n = ctx->mod_count(mh, inst_id, "loader");
    char buf[128];
    snprintf(buf, sizeof(buf), "After refresh: %d loader mod(s)", n);
    ctx->log_info(mh, buf);
}
```

---

## Complete example: mod toggler plugin

The following is a more complete example showing a plugin function that
finds a mod by name and toggles its enabled state:

```c
#include <stdio.h>
#include <string.h>

/*
 * find_mod_by_name — Search for a mod by name in the given type list.
 * Returns the index if found, or -1 if not found.
 *
 * IMPORTANT: This function overwrites tempString during the search,
 * so the caller must not hold any uncopied const char* from the API.
 */
int find_mod_by_name(const MMCOContext* ctx, const char* inst_id,
                     const char* type, const char* target_name)
{
    void* mh = ctx->module_handle;
    int count = ctx->mod_count(mh, inst_id, type);

    for (int i = 0; i < count; i++) {
        const char* name = ctx->mod_get_name(mh, inst_id, type, i);
        if (name && strcmp(name, target_name) == 0) {
            return i;
        }
    }
    return -1;
}

/*
 * toggle_mod_by_name — Find a mod by its human-readable name and
 * toggle its enabled/disabled state.
 *
 * Returns:
 *   0  on success
 *  -1  if the mod was not found
 *  -2  if the toggle operation failed
 */
int toggle_mod_by_name(const MMCOContext* ctx, const char* inst_id,
                       const char* type, const char* mod_name)
{
    void* mh = ctx->module_handle;
    char buf[512];

    int idx = find_mod_by_name(ctx, inst_id, type, mod_name);
    if (idx < 0) {
        snprintf(buf, sizeof(buf), "Mod '%s' not found in %s list.",
                 mod_name, type);
        ctx->log_warning(mh, buf);
        return -1;
    }

    int was_enabled = ctx->mod_is_enabled(mh, inst_id, type, idx);
    int new_state = was_enabled ? 0 : 1;

    int rc = ctx->mod_set_enabled(mh, inst_id, type, idx, new_state);
    if (rc != 0) {
        snprintf(buf, sizeof(buf),
                 "Failed to %s mod '%s'.",
                 new_state ? "enable" : "disable", mod_name);
        ctx->log_error(mh, buf);
        return -2;
    }

    snprintf(buf, sizeof(buf), "Mod '%s' is now %s.",
             mod_name, new_state ? "enabled" : "disabled");
    ctx->log_info(mh, buf);
    return 0;
}

/*
 * Example hook handler — toggle Sodium when the user launches
 * the instance, as a (contrived) demonstration.
 */
void on_instance_launch(const MMCOContext* ctx, const char* inst_id)
{
    toggle_mod_by_name(ctx, inst_id, "loader", "Sodium");
}
```

---

## Interaction between control functions

Understanding how these functions interact is important for correct usage:

### `mod_set_enabled()` and indices

Enabling or disabling a mod does **not** change its index. The mod stays
at the same position in the list — only its file name and `m_enabled` flag
change. You can safely iterate and toggle without worrying about index
shifts:

```c
/* Safe: enabling/disabling does not reorder the list */
for (int i = 0; i < count; i++) {
    ctx->mod_set_enabled(mh, inst_id, "loader", i, 0);  /* disable all */
}
```

### `mod_remove()` and indices

Removing a mod **does** change indices. All mods after the removed one
shift down by 1. If you need to remove multiple mods, iterate **backwards**:

```c
/* Safe: backward iteration for removal */
for (int i = count - 1; i >= 0; i--) {
    if (!ctx->mod_is_enabled(mh, inst_id, "loader", i)) {
        ctx->mod_remove(mh, inst_id, "loader", i);
    }
}
```

### `mod_install()` and the existing list

Installing a mod triggers a directory rescan (`update()`). After
`mod_install()` returns, you should re-query `mod_count()` to get the
updated count, and **not** assume the new mod was added at any particular
index.

### `mod_refresh()` and all indices

A refresh rebuilds the entire internal list from disk. **All previously
cached indices are invalid** after a refresh. Always re-query
`mod_count()` and start from scratch.

---

## Error handling cheat sheet

| Function | Fail return | What to check |
|----------|-------------|---------------|
| `mod_is_enabled()` | `0` | Ambiguous with "disabled" — validate index with `mod_count()` first |
| `mod_set_enabled()` | `-1` | Check instance ID, type, index bounds, mod type (folders can't toggle) |
| `mod_remove()` | `-1` | Check instance ID, type, index bounds, file permissions |
| `mod_install()` | `-1` | Check instance ID, type, filepath validity, mod type recognition |
| `mod_refresh()` | `-1` | Check instance ID, type |

---

## See also

- [Section 05 — Mods API (overview)](README.md) — type strings, mod model, `.disabled` mechanism
- [Mod Enumeration](mod-enumeration.md) — `mod_count()`, `mod_get_name()`, `mod_get_version()`, `mod_get_filename()`, `mod_get_description()`
- [Section 12 — UI Dialogs](../s12-ui-dialogs/) — `ui_file_open_dialog()` for file pickers
- [Section 10 — Filesystem](../s10-filesystem/) — `fs_copy_file()`, `fs_remove()` for manual file operations
