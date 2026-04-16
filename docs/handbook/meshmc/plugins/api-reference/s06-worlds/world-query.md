# Worlds API — Full Function Reference

> All 10 world management function pointers documented in detail.
> For an overview of the Worlds API, Minecraft directory layout, and key
> concepts, see [README.md](README.md).

---

## Table of Contents

- [Query Functions](#query-functions)
  - [`world_count()`](#world_count)
  - [`world_get_name()`](#world_get_name)
  - [`world_get_folder()`](#world_get_folder)
  - [`world_get_seed()`](#world_get_seed)
  - [`world_get_game_type()`](#world_get_game_type)
  - [`world_get_last_played()`](#world_get_last_played)
- [Mutation Functions](#mutation-functions)
  - [`world_delete()`](#world_delete)
  - [`world_rename()`](#world_rename)
  - [`world_install()`](#world_install)
  - [`world_refresh()`](#world_refresh)
- [Practical Examples](#practical-examples)
  - [Practical Example: World Lister](#practical-example-world-lister)
  - [Practical Example: World Size Reporter](#practical-example-world-size-reporter)
  - [Practical Example: World Backup Trigger](#practical-example-world-backup-trigger)

---

# Query Functions

These six functions are **read-only**. They do not modify the world list
or any files on disk. They are safe to call at any time from the main
thread.

---

## `world_count()`

Returns the number of Minecraft worlds (save directories) present in the
specified instance's `saves/` folder.

### Signature

```c
int (*world_count)(void* mh, const char* instance_id);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle passed to `mmco_init()`. Identifies the calling plugin. Must not be `NULL`. |
| `instance_id` | `const char*` | Null-terminated instance ID string (as returned by `instance_get_id()` from [S03](../s03-instances/)). Must identify a valid Minecraft instance. |

### Return Value

| Condition | Return |
|-----------|--------|
| Success | Number of worlds (≥ 0). A value of `0` means the instance exists but has no saved worlds. |
| Instance not found | `0` |
| Instance is not a MinecraftInstance | `0` |
| `instance_id` is `NULL` | `0` |
| `mh` is `NULL` | Undefined behavior (crash likely) |

### Thread Safety

Must be called from the **main (GUI) thread** only.

### Implementation

```cpp
int PluginManager::api_world_count(void* mh, const char* inst)
{
    auto* r = rt(mh);
    auto wl = resolveWorldList(r, inst);
    return wl ? static_cast<int>(wl->size()) : 0;
}
```

The call chain is:

1. `rt(mh)` recovers the `ModuleRuntime*` from the opaque handle.
2. `resolveWorldList()` locates the `MinecraftInstance` by ID, then
   calls `mc->worldList()` to get the `shared_ptr<WorldList>`.
3. If the `WorldList` has not been loaded yet, `size()` triggers a
   directory scan of the `saves/` folder.
4. Returns the count of discovered directories that contain a
   `level.dat` file (or are otherwise recognized as worlds).

### Ambiguity: 0 means "none" or "error"

There is no way to distinguish "the instance has zero worlds" from "the
instance does not exist" — both return `0`. If you need to differentiate
these cases, first verify the instance exists using
`instance_get_name(mh, ...)` or `instance_get_type(mh, ...)` from
[S03](../s03-instances/).

### Example

```c
void check_worlds(MMCOContext* ctx, void* mh, const char* inst_id)
{
    int count = ctx->world_count(mh, inst_id);
    char msg[256];
    snprintf(msg, sizeof(msg), "Instance has %d world(s).", count);
    ctx->log(mh, msg);
}
```

---

## `world_get_name()`

Returns the **human-readable display name** of a world at the given index.
This is the `LevelName` field from the world's `level.dat` NBT data — the
name the player sees in the Minecraft singleplayer menu.

### Signature

```c
const char* (*world_get_name)(void* mh, const char* instance_id, int index);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. Must not be `NULL`. |
| `instance_id` | `const char*` | Null-terminated instance ID. Must identify a valid Minecraft instance. |
| `index` | `int` | Zero-based index into the world list. Must be in range `[0, world_count() - 1]`. |

### Return Value

| Condition | Return |
|-----------|--------|
| Success | Pointer to a null-terminated UTF-8 string containing the world's display name. |
| `index` out of range | `NULL` |
| Instance not found / not MC | `NULL` |
| `instance_id` is `NULL` | `NULL` |

### String Ownership

The returned pointer is stored in the per-module `tempString` buffer.
**It is invalidated by the next call to any API function that returns
`const char*`.** You must copy the string immediately if you need it
to persist. Do not free the pointer.

```c
/* Copy pattern */
const char* raw = ctx->world_get_name(mh, inst, idx);
char name[256];
if (raw) {
    snprintf(name, sizeof(name), "%s", raw);
} else {
    snprintf(name, sizeof(name), "(unknown)");
}
/* 'name' is now safe to use across subsequent API calls */
```

### Implementation

```cpp
const char* PluginManager::api_world_get_name(void* mh, const char* inst,
                                              int idx)
{
    auto* r = rt(mh);
    auto wl = resolveWorldList(r, inst);
    if (!wl || idx < 0 || idx >= static_cast<int>(wl->size()))
        return nullptr;
    r->tempString = wl->allWorlds().at(idx).name().toStdString();
    return r->tempString.c_str();
}
```

### Notes

- The display name can contain any Unicode characters, including spaces,
  punctuation, and emoji. Always handle UTF-8 properly.
- The display name is **not unique**. Multiple worlds can share the same
  name. Use `world_get_folder()` for a filesystem-unique identifier.
- If the world's `level.dat` is corrupted or unreadable, the name may be
  empty or the world may not appear in the list at all.

### Example

```c
void print_world_names(MMCOContext* ctx, void* mh, const char* inst_id)
{
    int count = ctx->world_count(mh, inst_id);
    for (int i = 0; i < count; i++) {
        const char* raw = ctx->world_get_name(mh, inst_id, i);
        char name[256];
        snprintf(name, sizeof(name), "%s", raw ? raw : "(null)");

        char msg[512];
        snprintf(msg, sizeof(msg), "World %d: %s", i, name);
        ctx->log(mh, msg);
    }
}
```

---

## `world_get_folder()`

Returns the **on-disk directory name** of the world at the given index.
This is the folder name inside the `saves/` directory, not the display
name. For example, a world displayed as "My Survival World" might be
stored in a folder called `New World (2)`.

### Signature

```c
const char* (*world_get_folder)(void* mh, const char* instance_id, int index);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. Must not be `NULL`. |
| `instance_id` | `const char*` | Null-terminated instance ID. |
| `index` | `int` | Zero-based world index. Must be in range `[0, world_count() - 1]`. |

### Return Value

| Condition | Return |
|-----------|--------|
| Success | Null-terminated UTF-8 string: the folder name (basename only, not a full path). |
| `index` out of range | `NULL` |
| Instance not found / not MC | `NULL` |

### String Ownership

Same `tempString` rules as `world_get_name()`. Copy immediately.

### Implementation

```cpp
const char* PluginManager::api_world_get_folder(void* mh, const char* inst,
                                                int idx)
{
    auto* r = rt(mh);
    auto wl = resolveWorldList(r, inst);
    if (!wl || idx < 0 || idx >= static_cast<int>(wl->size()))
        return nullptr;
    r->tempString = wl->allWorlds().at(idx).folderName().toStdString();
    return r->tempString.c_str();
}
```

### Constructing Full Paths

The folder name is a **basename**, not a full path. To construct the
absolute path to a world directory, combine it with the saves directory:

```c
const char* saves_dir = ctx->instance_get_worlds_dir(mh, inst_id);
char saves_copy[4096];
if (saves_dir) snprintf(saves_copy, sizeof(saves_copy), "%s", saves_dir);

const char* folder = ctx->world_get_folder(mh, inst_id, idx);
char folder_copy[256];
if (folder) snprintf(folder_copy, sizeof(folder_copy), "%s", folder);

/* Now build the full path */
char world_path[4096];
snprintf(world_path, sizeof(world_path), "%s/%s", saves_copy, folder_copy);

/* world_path is now e.g. "/home/user/.minecraft/saves/New World" */
```

> **Important:** You must copy both `saves_dir` and `folder` before using
> them together, because both calls go through `tempString`.

### Example

```c
void list_world_folders(MMCOContext* ctx, void* mh, const char* inst_id)
{
    const char* saves_raw = ctx->instance_get_worlds_dir(mh, inst_id);
    if (!saves_raw) {
        ctx->log(mh, "Could not get saves dir (not a MC instance?).");
        return;
    }
    char saves[4096];
    snprintf(saves, sizeof(saves), "%s", saves_raw);

    int count = ctx->world_count(mh, inst_id);
    for (int i = 0; i < count; i++) {
        const char* f = ctx->world_get_folder(mh, inst_id, i);
        char folder[256];
        snprintf(folder, sizeof(folder), "%s", f ? f : "?");

        char full[4096];
        snprintf(full, sizeof(full), "%s/%s", saves, folder);

        char msg[4096];
        snprintf(msg, sizeof(msg), "  [%d] %s", i, full);
        ctx->log(mh, msg);
    }
}
```

---

## `world_get_seed()`

Returns the **64-bit world seed** for the world at the given index. The
seed is a signed 64-bit integer extracted from the `RandomSeed` field in
`level.dat`.

### Signature

```c
int64_t (*world_get_seed)(void* mh, const char* instance_id, int index);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. Must not be `NULL`. |
| `instance_id` | `const char*` | Null-terminated instance ID. |
| `index` | `int` | Zero-based world index. |

### Return Value

| Condition | Return |
|-----------|--------|
| Success | The 64-bit seed value (can be any value including `0` or negative). |
| `index` out of range | `0` |
| Instance not found / not MC | `0` |

### Ambiguity

A return value of `0` is ambiguous: it could mean the world genuinely has
seed `0`, or that the index/instance was invalid. If you need to
distinguish, verify the index is valid before calling:

```c
if (idx >= 0 && idx < ctx->world_count(mh, inst_id)) {
    int64_t seed = ctx->world_get_seed(mh, inst_id, idx);
    /* seed is valid — even if it's 0 */
}
```

### Implementation

```cpp
int64_t PluginManager::api_world_get_seed(void* mh, const char* inst, int idx)
{
    auto* r = rt(mh);
    auto wl = resolveWorldList(r, inst);
    if (!wl || idx < 0 || idx >= static_cast<int>(wl->size()))
        return 0;
    return wl->allWorlds().at(idx).seed();
}
```

### Example

```c
void print_seed(MMCOContext* ctx, void* mh, const char* inst, int idx)
{
    int64_t seed = ctx->world_get_seed(mh, inst, idx);
    char msg[256];
    snprintf(msg, sizeof(msg), "World %d seed: %lld", idx, (long long)seed);
    ctx->log(mh, msg);
}
```

---

## `world_get_game_type()`

Returns the **game mode** of the world at the given index, as stored in
the `GameType` field of `level.dat`.

### Signature

```c
int (*world_get_game_type)(void* mh, const char* instance_id, int index);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. Must not be `NULL`. |
| `instance_id` | `const char*` | Null-terminated instance ID. |
| `index` | `int` | Zero-based world index. |

### Return Value

| Condition | Return |
|-----------|--------|
| Success — Survival | `0` |
| Success — Creative | `1` |
| Success — Adventure | `2` |
| Success — Spectator | `3` |
| `index` out of range | `-1` |
| Instance not found / not MC | `-1` |

### Game Type Table

| Integer | Mode | Description |
|---------|------|-------------|
| `0` | **Survival** | Normal gameplay with health, hunger, and resource gathering |
| `1` | **Creative** | Unlimited resources, flight, no damage |
| `2` | **Adventure** | Survival-like but blocks cannot be placed/broken without proper tools |
| `3` | **Spectator** | Free-floating camera, no interaction with the world |

Future Minecraft versions may add new game modes; treat any value ≥ 4 as
"unknown" rather than erroring.

### Implementation

```cpp
int PluginManager::api_world_get_game_type(void* mh, const char* inst, int idx)
{
    auto* r = rt(mh);
    auto wl = resolveWorldList(r, inst);
    if (!wl || idx < 0 || idx >= static_cast<int>(wl->size()))
        return -1;
    return wl->allWorlds().at(idx).gameType().type;
}
```

### Example

```c
static const char* game_type_str(int gt) {
    switch (gt) {
        case 0:  return "Survival";
        case 1:  return "Creative";
        case 2:  return "Adventure";
        case 3:  return "Spectator";
        default: return "Unknown";
    }
}

void show_game_type(MMCOContext* ctx, void* mh, const char* inst, int idx)
{
    int gt = ctx->world_get_game_type(mh, inst, idx);
    char msg[256];
    snprintf(msg, sizeof(msg), "World %d: %s (type=%d)",
             idx, game_type_str(gt), gt);
    ctx->log(mh, msg);
}
```

---

## `world_get_last_played()`

Returns the **last-played timestamp** for the world at the given index.
The value comes from the `LastPlayed` field in `level.dat`, converted to
**milliseconds since the Unix epoch** (1970-01-01T00:00:00Z).

### Signature

```c
int64_t (*world_get_last_played)(void* mh, const char* instance_id, int index);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. Must not be `NULL`. |
| `instance_id` | `const char*` | Null-terminated instance ID. |
| `index` | `int` | Zero-based world index. |

### Return Value

| Condition | Return |
|-----------|--------|
| Success | Epoch milliseconds (`int64_t`). Typically a large positive number (e.g. `1713200000000` for mid-2024). |
| `index` out of range | `0` |
| Instance not found / not MC | `0` |
| World has never been played | `0` (indistinguishable from error) |

### Converting to human-readable time

The timestamp is in **milliseconds**, not seconds. To convert to
`time_t` (seconds), divide by 1000:

```c
int64_t ms = ctx->world_get_last_played(mh, inst, idx);
time_t t = (time_t)(ms / 1000);
struct tm* tm_info = localtime(&t);
char buf[64];
strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm_info);
/* buf is now e.g. "2025-03-15 14:30:00" */
```

### Implementation

```cpp
int64_t PluginManager::api_world_get_last_played(void* mh, const char* inst,
                                                 int idx)
{
    auto* r = rt(mh);
    auto wl = resolveWorldList(r, inst);
    if (!wl || idx < 0 || idx >= static_cast<int>(wl->size()))
        return 0;
    return wl->allWorlds().at(idx).lastPlayed().toMSecsSinceEpoch();
}
```

### Example

```c
#include <time.h>

void print_last_played(MMCOContext* ctx, void* mh,
                       const char* inst, int idx)
{
    int64_t ms = ctx->world_get_last_played(mh, inst, idx);
    if (ms == 0) {
        ctx->log(mh, "Last played: unknown / never");
        return;
    }
    time_t sec = (time_t)(ms / 1000);
    struct tm* t = localtime(&sec);
    char buf[128];
    strftime(buf, sizeof(buf), "Last played: %Y-%m-%d %H:%M:%S", t);
    ctx->log(mh, buf);
}
```

---

# Mutation Functions

These four functions **modify** the world list or files on disk. They
carry important caveats:

1. **Index invalidation.** After any mutation (`world_delete`,
   `world_install`, `world_refresh`), all previously-obtained indices
   are invalidated. Re-query `world_count()` and re-fetch names/folders.
2. **No undo.** `world_delete()` permanently removes files. There is no
   recycle bin or undo mechanism. Prompt the user with
   `ui_confirm_dialog()` from [S12](../s12-ui-dialogs/) before deleting.
3. **Thread safety.** All mutations must be on the main (GUI) thread.

---

## `world_delete()`

Permanently deletes the world at the given index. This recursively
removes the entire world folder from disk, including all region files,
player data, and the `level.dat`. **This operation cannot be undone.**

### Signature

```c
int (*world_delete)(void* mh, const char* instance_id, int index);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. Must not be `NULL`. |
| `instance_id` | `const char*` | Null-terminated instance ID. Must identify a valid Minecraft instance. |
| `index` | `int` | Zero-based world index. Must be in range `[0, world_count() - 1]`. |

### Return Value

| Condition | Return |
|-----------|--------|
| Success | `0` |
| `index` out of range | `-1` |
| Instance not found / not MC | `-1` |
| Filesystem error (permissions, etc.) | `-1` |

### Side Effects

- The world's directory and all its contents are **permanently deleted**
  from disk.
- The internal `WorldList` model is updated; the deleted world is removed
  from the list.
- **All indices are invalidated** after this call. Do not use any index
  obtained before the deletion without re-querying `world_count()`.

### Implementation

```cpp
int PluginManager::api_world_delete(void* mh, const char* inst, int idx)
{
    auto* r = rt(mh);
    auto wl = resolveWorldList(r, inst);
    if (!wl || idx < 0 || idx >= static_cast<int>(wl->size()))
        return -1;
    return wl->deleteWorld(idx) ? 0 : -1;
}
```

### Best Practice: Confirm Before Deleting

```c
void safe_delete_world(MMCOContext* ctx, void* mh,
                       const char* inst_id, int idx)
{
    const char* raw = ctx->world_get_name(mh, inst_id, idx);
    char name[256];
    snprintf(name, sizeof(name), "%s", raw ? raw : "(unnamed)");

    char prompt[512];
    snprintf(prompt, sizeof(prompt),
             "Permanently delete world \"%s\"?\n"
             "This action cannot be undone.", name);

    if (ctx->ui_confirm_dialog(mh, "Delete World", prompt)) {
        if (ctx->world_delete(mh, inst_id, idx) == 0) {
            ctx->log(mh, "World deleted.");
        } else {
            ctx->log_error(mh, "Failed to delete world.");
        }
    }
}
```

### Best Practice: Back Up Before Deleting

See [Practical Example: World Backup Trigger](#practical-example-world-backup-trigger)
for a pattern that creates a zip archive before deleting.

---

## `world_rename()`

Changes the **display name** of the world at the given index. This
modifies the `LevelName` field inside the world's `level.dat` file. It
does **not** rename the folder on disk.

### Signature

```c
int (*world_rename)(void* mh, const char* instance_id, int index,
                    const char* new_name);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. Must not be `NULL`. |
| `instance_id` | `const char*` | Null-terminated instance ID. |
| `index` | `int` | Zero-based world index. |
| `new_name` | `const char*` | The new display name (null-terminated UTF-8). Must not be `NULL`. |

### Return Value

| Condition | Return |
|-----------|--------|
| Success | `0` |
| `index` out of range | `-1` |
| `new_name` is `NULL` | `-1` |
| Instance not found / not MC | `-1` |
| Write failure (permissions, corrupted level.dat) | `-1` |

### What Changes

| Aspect | Changed? |
|--------|----------|
| `level.dat` → `Data.LevelName` | **Yes** — updated to `new_name` |
| Folder name on disk | **No** — remains unchanged |
| `world_get_name()` return value | **Yes** — reflects the new name |
| `world_get_folder()` return value | **No** — unchanged |

### Implementation

```cpp
int PluginManager::api_world_rename(void* mh, const char* inst, int idx,
                                    const char* name)
{
    auto* r = rt(mh);
    auto wl = resolveWorldList(r, inst);
    if (!wl || idx < 0 || idx >= static_cast<int>(wl->size()) || !name)
        return -1;
    auto& worlds = wl->allWorlds();
    World w = worlds.at(idx);
    return w.rename(QString::fromUtf8(name)) ? 0 : -1;
}
```

### Example

```c
void rename_world_interactive(MMCOContext* ctx, void* mh,
                              const char* inst_id, int idx)
{
    const char* old_raw = ctx->world_get_name(mh, inst_id, idx);
    char old_name[256];
    snprintf(old_name, sizeof(old_name), "%s", old_raw ? old_raw : "");

    const char* new_name = ctx->ui_input_dialog(
        mh, "Rename World", "Enter new name:", old_name);

    if (!new_name) {
        ctx->log(mh, "Rename cancelled.");
        return;
    }

    if (ctx->world_rename(mh, inst_id, idx, new_name) == 0) {
        char msg[512];
        snprintf(msg, sizeof(msg), "Renamed \"%s\" → \"%s\"",
                 old_name, new_name);
        ctx->log(mh, msg);
    } else {
        ctx->log_error(mh, "Failed to rename world.");
    }
}
```

---

## `world_install()`

Installs (imports) a world from an external file or directory into the
instance's `saves/` folder. The launcher handles extracting archives and
copying folder structures as needed.

### Signature

```c
int (*world_install)(void* mh, const char* instance_id,
                     const char* filepath);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. Must not be `NULL`. |
| `instance_id` | `const char*` | Null-terminated instance ID. |
| `filepath` | `const char*` | Absolute path to the world file or directory to import. Must not be `NULL`. The path can point to a world folder (containing `level.dat`) or a recognized archive format. |

### Return Value

| Condition | Return |
|-----------|--------|
| Success | `0` |
| `filepath` is `NULL` | `-1` |
| Instance not found / not MC | `-1` |
| Invalid or unreadable path | `-1` |

### Side Effects

- The world is copied into the instance's `saves/` directory.
- The internal `WorldList` model is updated to include the new world.
- **All indices may change** after this call.

### Implementation

```cpp
int PluginManager::api_world_install(void* mh, const char* inst,
                                     const char* path)
{
    auto* r = rt(mh);
    auto wl = resolveWorldList(r, inst);
    if (!wl || !path)
        return -1;
    wl->installWorld(QFileInfo(QString::fromUtf8(path)));
    return 0;
}
```

### Example: Import a world chosen by the user

```c
void import_world(MMCOContext* ctx, void* mh, const char* inst_id)
{
    const char* path = ctx->ui_file_open_dialog(
        mh, "Select World", "World folders and archives (*.zip)");

    if (!path) {
        ctx->log(mh, "Import cancelled.");
        return;
    }

    if (ctx->world_install(mh, inst_id, path) == 0) {
        ctx->log(mh, "World imported successfully.");
        /* Refresh to pick up the new world */
        ctx->world_refresh(mh, inst_id);
    } else {
        ctx->log_error(mh, "Failed to import world.");
    }
}
```

---

## `world_refresh()`

Forces the launcher to **rescan** the instance's `saves/` directory and
rebuild the internal `WorldList` model. Use this after making manual
file-system changes (e.g., copying a world folder via `fs_copy_file()` or
extracting an archive via `zip_extract()`), or after `world_install()` /
`world_delete()` if you want to ensure the list is fully synchronized.

### Signature

```c
int (*world_refresh)(void* mh, const char* instance_id);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. Must not be `NULL`. |
| `instance_id` | `const char*` | Null-terminated instance ID. Must identify a valid Minecraft instance. |

### Return Value

| Condition | Return |
|-----------|--------|
| Success | `0` |
| Instance not found / not MC | `-1` |
| Scan failure | `-1` |

### Side Effects

- The saves directory is re-scanned from disk.
- The world list is rebuilt; **all previously-obtained indices, names,
  and folder strings are invalidated.**
- Any worlds that were manually added to disk since the last scan will
  now appear; any worlds removed from disk will vanish from the list.

### Implementation

```cpp
int PluginManager::api_world_refresh(void* mh, const char* inst)
{
    auto* r = rt(mh);
    auto wl = resolveWorldList(r, inst);
    if (!wl)
        return -1;
    return wl->update() ? 0 : -1;
}
```

### When to call `world_refresh()`

| Scenario | Refresh needed? |
|----------|-----------------|
| After `world_delete()` | Not strictly (model updates internally), but recommended for safety |
| After `world_install()` | Recommended to synchronize the list |
| After manual FS operations (zip extract, file copy) | **Required** — the model does not auto-detect external changes |
| After `world_rename()` | Not needed — the model reflects the change immediately |
| Periodically / on timer | Not recommended — expensive I/O operation |

### Example

```c
void force_refresh(MMCOContext* ctx, void* mh, const char* inst_id)
{
    if (ctx->world_refresh(mh, inst_id) == 0) {
        int count = ctx->world_count(mh, inst_id);
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "Refresh complete. %d world(s) found.", count);
        ctx->log(mh, msg);
    } else {
        ctx->log_error(mh, "Failed to refresh world list.");
    }
}
```

---

# Practical Examples

## Practical Example: World Lister

A complete, self-contained plugin that logs every world in every instance
at startup, with all available metadata.

```c
/*
 * world_lister.c — MMCO plugin that enumerates all worlds at init time.
 * Build with the MMCO SDK (see docs/handbook/meshmc/plugins/sdk-guide/).
 */
#include <mmco_sdk.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>

static MMCOContext* ctx;
static void* mh;

static const char* game_type_to_str(int gt)
{
    switch (gt) {
        case 0:  return "Survival";
        case 1:  return "Creative";
        case 2:  return "Adventure";
        case 3:  return "Spectator";
        default: return "Unknown";
    }
}

static void format_timestamp(int64_t epoch_ms, char* buf, size_t sz)
{
    if (epoch_ms == 0) {
        snprintf(buf, sz, "never");
        return;
    }
    time_t sec = (time_t)(epoch_ms / 1000);
    struct tm* t = localtime(&sec);
    strftime(buf, sz, "%Y-%m-%d %H:%M:%S", t);
}

static void enumerate_worlds(void)
{
    int inst_count = ctx->instance_count(mh);
    char line[1024];

    snprintf(line, sizeof(line),
             "=== World Lister: scanning %d instance(s) ===", inst_count);
    ctx->log(mh, line);

    for (int i = 0; i < inst_count; i++) {
        /* Get instance ID — copy immediately */
        const char* raw_id = ctx->instance_get_id(mh, i);
        if (!raw_id) continue;
        char inst_id[256];
        snprintf(inst_id, sizeof(inst_id), "%s", raw_id);

        /* Get instance name — copy immediately */
        const char* raw_name = ctx->instance_get_name(mh, i);
        char inst_name[256];
        snprintf(inst_name, sizeof(inst_name), "%s",
                 raw_name ? raw_name : "(unnamed)");

        int wc = ctx->world_count(mh, inst_id);
        if (wc == 0) continue;  /* skip instances with no worlds */

        snprintf(line, sizeof(line),
                 "\n--- Instance: %s [%s] — %d world(s) ---",
                 inst_name, inst_id, wc);
        ctx->log(mh, line);

        for (int w = 0; w < wc; w++) {
            /* Name — copy */
            const char* rn = ctx->world_get_name(mh, inst_id, w);
            char wname[256];
            snprintf(wname, sizeof(wname), "%s", rn ? rn : "(null)");

            /* Folder — copy */
            const char* rf = ctx->world_get_folder(mh, inst_id, w);
            char wfolder[256];
            snprintf(wfolder, sizeof(wfolder), "%s", rf ? rf : "?");

            /* Non-string values — safe to use directly */
            int64_t seed = ctx->world_get_seed(mh, inst_id, w);
            int game_type = ctx->world_get_game_type(mh, inst_id, w);
            int64_t last_played = ctx->world_get_last_played(mh, inst_id, w);

            char ts[64];
            format_timestamp(last_played, ts, sizeof(ts));

            snprintf(line, sizeof(line),
                     "  [%d] \"%s\"\n"
                     "       folder:      %s\n"
                     "       seed:        %" PRId64 "\n"
                     "       game mode:   %s (%d)\n"
                     "       last played: %s",
                     w, wname, wfolder,
                     seed, game_type_to_str(game_type), game_type, ts);
            ctx->log(mh, line);
        }
    }

    ctx->log(mh, "=== World Lister: scan complete ===");
}

MMCO_EXPORT int mmco_init(MMCOContext* c, void* handle)
{
    ctx = c;
    mh  = handle;
    ctx->log(mh, "WorldLister plugin loaded.");
    enumerate_worlds();
    return 0;
}

MMCO_EXPORT void mmco_unload(MMCOContext* c, void* handle)
{
    (void)c; (void)handle;
}
```

**Expected output:**

```text
[WorldLister] WorldLister plugin loaded.
[WorldLister] === World Lister: scanning 3 instance(s) ===
[WorldLister]
--- Instance: My Modded 1.20 [abc123] — 2 world(s) ---
[WorldLister]   [0] "Survival Island"
       folder:      New World
       seed:        -4823749283749
       game mode:   Survival (0)
       last played: 2026-04-10 18:45:22
[WorldLister]   [1] "Creative Test"
       folder:      Creative Test
       seed:        12345
       game mode:   Creative (1)
       last played: 2026-03-01 09:12:05
[WorldLister] === World Lister: scan complete ===
```

---

## Practical Example: World Size Reporter

This example demonstrates how to calculate the total disk size of each
world by combining the Worlds API with the Filesystem API
([S10](../s10-filesystem/)). Since there is no built-in `world_get_size()`
function, we must recursively walk the world directory tree.

### Strategy

1. Get the saves directory via `instance_get_worlds_dir()`.
2. Get each world's folder name via `world_get_folder()`.
3. Construct the full path: `saves_dir + "/" + folder_name`.
4. Use `fs_list_dir()` to recursively list entries.
5. Use `fs_file_size()` to get the size of each file.
6. Sum the sizes.

### Code

```c
/*
 * world_size_reporter.c — Reports the disk size of each world.
 * Uses S06 (Worlds) + S10 (Filesystem) APIs.
 */
#include <mmco_sdk.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

static MMCOContext* ctx;
static void* mh;

/* ── Recursive size calculation ────────────────────────────────── */

struct DirSizeCtx {
    MMCOContext* ctx;
    void*       mh;
    char        base_path[4096];
    int64_t     total_bytes;
};

/*
 * fs_list_dir callback.
 * type flag: 0 = file, 1 = directory (from MMCODirEntryCallback).
 *
 * For each file, we stat it with fs_file_size().
 * For each directory, we recurse by calling fs_list_dir() again.
 */
static void size_walk_cb(void* user_data, const char* name, int is_dir)
{
    struct DirSizeCtx* dc = (struct DirSizeCtx*)user_data;

    /* Skip . and .. */
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
        return;

    /* Build full path */
    char full_path[4096];
    snprintf(full_path, sizeof(full_path), "%s/%s", dc->base_path, name);

    if (is_dir) {
        /* Recurse into subdirectory */
        struct DirSizeCtx sub;
        sub.ctx = dc->ctx;
        sub.mh  = dc->mh;
        snprintf(sub.base_path, sizeof(sub.base_path), "%s", full_path);
        sub.total_bytes = 0;

        dc->ctx->fs_list_dir(dc->mh, full_path, 0, size_walk_cb, &sub);
        dc->total_bytes += sub.total_bytes;
    } else {
        /* Accumulate file size */
        int64_t sz = dc->ctx->fs_file_size(dc->mh, full_path);
        if (sz > 0) {
            dc->total_bytes += sz;
        }
    }
}

static int64_t calculate_dir_size(MMCOContext* c, void* h, const char* dir)
{
    struct DirSizeCtx dc;
    dc.ctx = c;
    dc.mh  = h;
    snprintf(dc.base_path, sizeof(dc.base_path), "%s", dir);
    dc.total_bytes = 0;

    c->fs_list_dir(h, dir, 0, size_walk_cb, &dc);
    return dc.total_bytes;
}

/* ── Human-readable formatting ─────────────────────────────────── */

static void format_size(int64_t bytes, char* buf, size_t sz)
{
    if (bytes < 0) {
        snprintf(buf, sz, "error");
    } else if (bytes < 1024) {
        snprintf(buf, sz, "%" PRId64 " B", bytes);
    } else if (bytes < 1024 * 1024) {
        snprintf(buf, sz, "%.1f KiB", (double)bytes / 1024.0);
    } else if (bytes < 1024 * 1024 * 1024) {
        snprintf(buf, sz, "%.1f MiB", (double)bytes / (1024.0 * 1024.0));
    } else {
        snprintf(buf, sz, "%.2f GiB",
                 (double)bytes / (1024.0 * 1024.0 * 1024.0));
    }
}

/* ── Main report function ──────────────────────────────────────── */

static void report_world_sizes(const char* inst_id)
{
    /* Get saves dir — copy before other calls overwrite tempString */
    const char* raw_saves = ctx->instance_get_worlds_dir(mh, inst_id);
    if (!raw_saves) {
        ctx->log(mh, "Not a Minecraft instance or no saves dir.");
        return;
    }
    char saves_dir[4096];
    snprintf(saves_dir, sizeof(saves_dir), "%s", raw_saves);

    int wc = ctx->world_count(mh, inst_id);
    char line[1024];
    snprintf(line, sizeof(line), "Calculating sizes for %d world(s)...", wc);
    ctx->log(mh, line);

    int64_t grand_total = 0;

    for (int i = 0; i < wc; i++) {
        /* Get world name — copy */
        const char* rn = ctx->world_get_name(mh, inst_id, i);
        char wname[256];
        snprintf(wname, sizeof(wname), "%s", rn ? rn : "(unnamed)");

        /* Get folder — copy */
        const char* rf = ctx->world_get_folder(mh, inst_id, i);
        char wfolder[256];
        snprintf(wfolder, sizeof(wfolder), "%s", rf ? rf : "");

        /* Build full path to world directory */
        char world_path[4096];
        snprintf(world_path, sizeof(world_path), "%s/%s", saves_dir, wfolder);

        /* Calculate recursive size */
        int64_t size = calculate_dir_size(ctx, mh, world_path);
        grand_total += size;

        char size_str[64];
        format_size(size, size_str, sizeof(size_str));

        snprintf(line, sizeof(line), "  \"%s\" (%s): %s",
                 wname, wfolder, size_str);
        ctx->log(mh, line);
    }

    char total_str[64];
    format_size(grand_total, total_str, sizeof(total_str));
    snprintf(line, sizeof(line), "Total: %s across %d world(s)",
             total_str, wc);
    ctx->log(mh, line);
}

MMCO_EXPORT int mmco_init(MMCOContext* c, void* handle)
{
    ctx = c;
    mh  = handle;

    /* Report sizes for the first instance that has worlds */
    int n = ctx->instance_count(mh);
    for (int i = 0; i < n; i++) {
        const char* raw_id = ctx->instance_get_id(mh, i);
        if (!raw_id) continue;
        char id[256];
        snprintf(id, sizeof(id), "%s", raw_id);

        if (ctx->world_count(mh, id) > 0) {
            report_world_sizes(id);
            break;
        }
    }
    return 0;
}

MMCO_EXPORT void mmco_unload(MMCOContext* c, void* handle)
{
    (void)c; (void)handle;
}
```

**Expected output:**

```text
[WorldSizeReporter] Calculating sizes for 3 world(s)...
[WorldSizeReporter]   "Survival Island" (New World): 148.3 MiB
[WorldSizeReporter]   "Creative Test" (Creative Test): 22.7 MiB
[WorldSizeReporter]   "Amplified Hardcore" (Amplified Hardcore): 1.04 GiB
[WorldSizeReporter] Total: 1.21 GiB across 3 world(s)
```

### Performance notes

- **Large worlds can take noticeable time** to scan. A heavily-explored
  world might have thousands of region files totaling several GiB.
  The recursive walk blocks the main thread during execution.
- Consider limiting the report to a single world, or displaying a
  "calculating…" message via `ui_show_message()` before starting.
- For a truly non-blocking approach, you would need to perform the size
  calculation outside the main loop and report results asynchronously.
  The current MMCO API does not provide a threading primitive, so this
  is a limitation.

---

## Practical Example: World Backup Trigger

This example combines the Worlds API (S06) with the Zip API
([S09](../s09-zip/)) and Instance Query API ([S03](../s03-instances/))
to create a one-click world backup function. It:

1. Lets the user choose a world from the list.
2. Constructs a timestamped zip filename.
3. Compresses the world folder into a zip archive.
4. Stores the backup in the plugin's data directory.

```c
/*
 * world_backup.c — Backs up a selected world to a .zip archive.
 * Uses S03 (Instance Query) + S06 (Worlds) + S09 (Zip) + S10 (FS)
 * + S12 (UI Dialogs).
 */
#include <mmco_sdk.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static MMCOContext* ctx;
static void* mh;

/*
 * backup_world — compress one world into a timestamped .zip.
 *
 * Returns 0 on success, -1 on failure.
 */
static int backup_world(const char* inst_id, int world_idx)
{
    /* 1. Get saves dir */
    const char* raw_saves = ctx->instance_get_worlds_dir(mh, inst_id);
    if (!raw_saves) return -1;
    char saves[4096];
    snprintf(saves, sizeof(saves), "%s", raw_saves);

    /* 2. Get world folder name */
    const char* raw_folder = ctx->world_get_folder(mh, inst_id, world_idx);
    if (!raw_folder) return -1;
    char folder[256];
    snprintf(folder, sizeof(folder), "%s", raw_folder);

    /* 3. Get world display name (for the log message) */
    const char* raw_name = ctx->world_get_name(mh, inst_id, world_idx);
    char wname[256];
    snprintf(wname, sizeof(wname), "%s", raw_name ? raw_name : folder);

    /* 4. Build source path */
    char src_path[4096];
    snprintf(src_path, sizeof(src_path), "%s/%s", saves, folder);

    /* 5. Build destination zip filename with timestamp */
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", t);

    const char* data_dir = ctx->fs_plugin_data_dir(mh);
    char zip_path[4096];
    snprintf(zip_path, sizeof(zip_path), "%s/backups/%s_%s.zip",
             data_dir, folder, timestamp);

    /* 6. Ensure backups/ subdirectory exists */
    char backup_dir[4096];
    snprintf(backup_dir, sizeof(backup_dir), "%s/backups", data_dir);
    ctx->fs_mkdir(mh, backup_dir);

    /* 7. Compress the world folder */
    char msg[1024];
    snprintf(msg, sizeof(msg), "Backing up \"%s\" → %s ...", wname, zip_path);
    ctx->log(mh, msg);

    int rc = ctx->zip_compress_dir(mh, zip_path, src_path);

    if (rc == 0) {
        /* Report file size */
        int64_t sz = ctx->fs_file_size(mh, zip_path);
        char size_str[64];
        if (sz > 0 && sz < 1024 * 1024) {
            snprintf(size_str, sizeof(size_str), "%.1f KiB",
                     (double)sz / 1024.0);
        } else if (sz >= 1024 * 1024) {
            snprintf(size_str, sizeof(size_str), "%.1f MiB",
                     (double)sz / (1024.0 * 1024.0));
        } else {
            snprintf(size_str, sizeof(size_str), "%lld B", (long long)sz);
        }

        snprintf(msg, sizeof(msg),
                 "Backup complete: \"%s\" → %s (%s)", wname, zip_path, size_str);
        ctx->log(mh, msg);
        ctx->ui_show_message(mh, 0, "Backup Complete", msg);
        return 0;
    } else {
        snprintf(msg, sizeof(msg), "Failed to back up \"%s\".", wname);
        ctx->log_error(mh, msg);
        ctx->ui_show_message(mh, 2, "Backup Failed", msg);
        return -1;
    }
}

/*
 * backup_all_worlds — back up every world in the given instance.
 */
static void backup_all_worlds(const char* inst_id)
{
    int wc = ctx->world_count(mh, inst_id);
    char line[256];
    snprintf(line, sizeof(line), "Backing up %d world(s)...", wc);
    ctx->log(mh, line);

    int ok = 0, fail = 0;
    for (int i = 0; i < wc; i++) {
        if (backup_world(inst_id, i) == 0)
            ok++;
        else
            fail++;
    }

    snprintf(line, sizeof(line),
             "Backup batch complete: %d succeeded, %d failed.", ok, fail);
    ctx->log(mh, line);
}

/*
 * restore_world — extract a zip backup into the saves directory.
 * Uses ui_file_open_dialog to let the user pick the zip.
 */
static void restore_world(const char* inst_id)
{
    const char* zip = ctx->ui_file_open_dialog(
        mh, "Select World Backup", "Zip archives (*.zip)");
    if (!zip) {
        ctx->log(mh, "Restore cancelled.");
        return;
    }

    /* Get the saves directory */
    const char* raw_saves = ctx->instance_get_worlds_dir(mh, inst_id);
    if (!raw_saves) {
        ctx->log_error(mh, "Cannot determine saves directory.");
        return;
    }
    char saves[4096];
    snprintf(saves, sizeof(saves), "%s", raw_saves);

    /* Extract the zip into saves/ */
    char msg[1024];
    snprintf(msg, sizeof(msg), "Restoring from %s → %s ...", zip, saves);
    ctx->log(mh, msg);

    if (ctx->zip_extract(mh, zip, saves) == 0) {
        ctx->world_refresh(mh, inst_id);
        int wc = ctx->world_count(mh, inst_id);
        snprintf(msg, sizeof(msg),
                 "Restore complete. %d world(s) now available.", wc);
        ctx->log(mh, msg);
        ctx->ui_show_message(mh, 0, "Restore Complete", msg);
    } else {
        ctx->log_error(mh, "Failed to extract world backup.");
        ctx->ui_show_message(mh, 2, "Restore Failed",
                             "Could not extract the selected archive.");
    }
}

MMCO_EXPORT int mmco_init(MMCOContext* c, void* handle)
{
    ctx = c;
    mh  = handle;
    ctx->log(mh, "WorldBackup plugin loaded.");
    /* Real plugins would register a menu item or toolbar action
       to trigger backup_all_worlds() or restore_world() on demand.
       See S11 (Instance Pages) and S12 (UI Dialogs) for how to
       add UI triggers. */
    return 0;
}

MMCO_EXPORT void mmco_unload(MMCOContext* c, void* handle)
{
    (void)c; (void)handle;
}
```

### Backup directory structure

After several backups, the plugin's data directory looks like:

```text
<plugin-data-dir>/
└── backups/
    ├── New World_20260415_143000.zip
    ├── New World_20260413_091500.zip
    ├── Creative Test_20260415_143000.zip
    └── Amplified Hardcore_20260410_184500.zip
```

Each zip contains the entire world folder tree, ready to be restored by
extracting back into the `saves/` directory.

---

## Error Handling Summary

All world functions share a consistent error-reporting pattern:

| Function | Error sentinel | Common causes |
|----------|---------------|---------------|
| `world_count()` | `0` | Non-MC instance, NULL ID |
| `world_get_name()` | `NULL` | Bad index, non-MC instance |
| `world_get_folder()` | `NULL` | Bad index, non-MC instance |
| `world_get_seed()` | `0` | Bad index (ambiguous with real seed 0) |
| `world_get_game_type()` | `-1` | Bad index, non-MC instance |
| `world_get_last_played()` | `0` | Bad index (ambiguous with epoch) |
| `world_delete()` | `-1` | Bad index, permission error |
| `world_rename()` | `-1` | Bad index, NULL name, write error |
| `world_install()` | `-1` | NULL path, non-MC instance |
| `world_refresh()` | `-1` | Non-MC instance, scan failure |

### Defensive programming pattern

```c
/*
 * Safe wrapper that validates the instance and index before
 * calling any world function.
 */
static int validate_world_access(MMCOContext* ctx, void* mh,
                                 const char* inst_id, int idx)
{
    if (!inst_id) {
        ctx->log_error(mh, "world access: NULL instance_id");
        return -1;
    }

    /* Verify the instance exists and is a Minecraft instance */
    const char* mc_ver = ctx->instance_get_mc_version(mh, inst_id);
    if (!mc_ver) {
        ctx->log_error(mh,
            "world access: instance not found or not a MC instance");
        return -1;
    }

    int count = ctx->world_count(mh, inst_id);
    if (idx < 0 || idx >= count) {
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "world access: index %d out of range [0, %d)",
                 idx, count);
        ctx->log_error(mh, msg);
        return -1;
    }

    return 0; /* OK */
}
```

---

## Non-Minecraft Instances

If a plugin calls any world function on an instance that is not a
`MinecraftInstance` (e.g., a hypothetical future generic instance type),
the following happens:

1. `resolveMC()` fails the `dynamic_cast` and returns `nullptr`.
2. `resolveWorldList()` returns `nullptr`.
3. The API function returns its error sentinel (`0`, `NULL`, or `-1`).

No crash occurs, no exception is thrown. The plugin receives a clean
error indication. This is by design: plugins should handle non-MC
instances gracefully, especially if they iterate over all instances
using `instance_count()` + `instance_get_id()`.

**Tip:** Use `instance_get_mc_version()` from
[S03](../s03-instances/instance-properties.md) to test whether an instance
is a MinecraftInstance before calling world functions. If it returns
`NULL`, the instance is not Minecraft.

```c
int is_mc_instance(MMCOContext* ctx, void* mh, const char* id)
{
    return ctx->instance_get_mc_version(mh, id) != NULL;
}
```

---

## Index Stability Contract

The world list uses **positional indexing**: worlds are accessed by their
integer position in the list. This index can change whenever the underlying
list is modified.

### Guaranteed stable

- All indices remain valid between two consecutive **read-only** API calls
  on the same instance (assuming no other thread or plugin mutates the list
  in between — which cannot happen since all calls must be on the main thread).

### Invalidated by

| Operation | All indices invalidated? |
|-----------|------------------------|
| `world_delete(inst, idx)` | **Yes** — the list shrinks by one; all indices ≥ `idx` shift down |
| `world_install(inst, path)` | **Yes** — new world is added; insertion point is unknown |
| `world_refresh(inst)` | **Yes** — the entire list is rebuilt from disk |
| `world_rename(inst, idx, n)` | **No** — only the name field changes; indices remain stable |

### Safe iteration pattern

If you need to delete multiple worlds (e.g., all worlds matching a
pattern), iterate **in reverse order** so that deleting index `i` does
not affect indices less than `i`:

```c
void delete_worlds_matching(MMCOContext* ctx, void* mh,
                            const char* inst_id, const char* pattern)
{
    int count = ctx->world_count(mh, inst_id);

    /* Iterate backwards so deletions don't shift unprocessed indices */
    for (int i = count - 1; i >= 0; i--) {
        const char* name = ctx->world_get_name(mh, inst_id, i);
        if (name && strstr(name, pattern)) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Deleting world: %s", name);
            ctx->log(mh, msg);
            ctx->world_delete(mh, inst_id, i);
        }
    }
}
```

---

## Comparison with the Mods API (S05)

Both the Mods API and the Worlds API follow the
**count → get-by-index → mutate** pattern. Key differences:

| Aspect | Mods API (S05) | Worlds API (S06) |
|--------|---------------|-----------------|
| Type parameter | `const char* type` selects mod category | None — single flat list |
| Metadata | name, version, filename, description, enabled | name, folder, seed, game-type, last-played |
| Enable / disable | `mod_set_enabled()` | Not applicable (worlds are always "enabled") |
| Install source | File path (mod jar/zip) | File path or folder (world directory or zip) |
| Identity | Filename on disk | Folder name on disk |
| Size info | Not provided (use `fs_file_size` on filename) | Not provided (recursive walk required) |

---

## See Also

- [README.md](README.md) — Section overview, directory layout, key concepts
- [S03 — Instance Query](../s03-instances/) — `instance_get_worlds_dir()`,
  `instance_get_id()`, `instance_get_mc_version()`
- [S05 — Mods](../s05-mods/) — Structurally similar count/index API
- [S09 — Zip / Archive](../s09-zip/) — `zip_compress_dir()`,
  `zip_extract()` for world backups
- [S10 — Filesystem](../s10-filesystem/) — `fs_list_dir()`,
  `fs_file_size()` for world size computation
- [S12 — UI Dialogs](../s12-ui-dialogs/) — User prompts before
  destructive operations
