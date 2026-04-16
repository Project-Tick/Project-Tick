# Directory & Path Operations — `fs_mkdir()`, `fs_exists_abs()`, `fs_remove()`, `fs_copy_file()`, `fs_file_size()`, `fs_list_dir()` {#directory-ops}

> **Section:** S10 (Filesystem) · **Header:** `PluginAPI.h` · **Trampolines:** `PluginManager::api_fs_mkdir`, `api_fs_exists_abs`, `api_fs_remove`, `api_fs_copy_file`, `api_fs_file_size`, `api_fs_list_dir` · **Backend:** Qt `QDir`, `QFile`, `QFileInfo`

These six functions operate on **absolute paths** and perform **no sandbox
validation**. They give plugins unrestricted access to the filesystem
(limited only by OS-level permissions). Use them when you need to work
with paths outside the plugin data directory — for example, the Minecraft
instance directories, user-selected paths from file dialogs, or system
temporary directories.

> **Security note:** These functions can read, write, and recursively
> delete anything the MeshMC process can access. Plugin authors must
> validate all paths before passing them. End users should review
> third-party plugin source code before installation.

---

## `fs_mkdir()` — Create a directory (recursively) {#fs_mkdir}

### Synopsis

```c
int fs_mkdir(void* mh, const char* abs_path);
```

Creates the directory at `abs_path`, including all intermediate (parent)
directories that do not yet exist. Equivalent to `mkdir -p` on Unix.

### Parameters

| # | Name | Type | Description |
|---|------|------|-------------|
| 1 | `mh` | `void*` | Module handle. Pass `ctx->module_handle`. Currently **unused** by the implementation (cast to `void`), but must be passed for forward compatibility. |
| 2 | `abs_path` | `const char*` | Absolute path to the directory to create. UTF-8 encoded. |

### Return value

| Condition | Value |
|-----------|-------|
| Directory created (or already exists) | `0` |
| `abs_path` is `NULL` | `-1` |
| Creation failed (permissions, invalid path, etc.) | `-1` |

### Recursive behaviour

`fs_mkdir()` calls `QDir().mkpath(path)`, which creates **every
component** of the path that does not already exist. This means:

```c
/* Creates: /tmp/meshmc/backup/2026/04/15/  (and all parents) */
ctx->fs_mkdir(mh, "/tmp/meshmc/backup/2026/04/15");
```

You do **not** need to create parent directories one at a time.

If the directory already exists, `QDir::mkpath()` returns `true` (success).
This is safe to call idempotently.

### Implementation

```cpp
int PluginManager::api_fs_mkdir(void* mh, const char* path)
{
    (void)mh;
    if (!path)
        return -1;
    return QDir().mkpath(QString::fromUtf8(path)) ? 0 : -1;
}
```

### Error conditions

| Condition | Behaviour |
|-----------|-----------|
| `abs_path` is `NULL` | Returns `-1` |
| Path is empty string | `QDir::mkpath("")` returns `false` → returns `-1` |
| Intermediate component is a file | `mkpath` fails → returns `-1` |
| No write permission on parent | `mkpath` fails → returns `-1` |
| Read-only filesystem | Returns `-1` |
| Path already exists as directory | Returns `0` (idempotent) |
| Path already exists as file | Returns `-1` |

### Example

```c
void ensure_cache_dirs(MMCOContext* ctx)
{
    void* mh = ctx->module_handle;
    const char* data_dir = ctx->fs_plugin_data_dir(mh);

    char path[4096];
    snprintf(path, sizeof(path), "%s/cache/thumbnails", data_dir);
    ctx->fs_mkdir(mh, path);

    snprintf(path, sizeof(path), "%s/cache/metadata", data_dir);
    ctx->fs_mkdir(mh, path);
}
```

---

## `fs_exists_abs()` — Check existence of any path {#fs_exists_abs}

### Synopsis

```c
int fs_exists_abs(void* mh, const char* abs_path);
```

Tests whether a file or directory exists at the given absolute path.
This is the unsandboxed counterpart to `fs_exists()`.

### Parameters

| # | Name | Type | Description |
|---|------|------|-------------|
| 1 | `mh` | `void*` | Module handle. Currently **unused** by the implementation. |
| 2 | `abs_path` | `const char*` | Absolute path to test. UTF-8 encoded. |

### Return value

| Condition | Value |
|-----------|-------|
| Path exists (file or directory) | `1` |
| Path does not exist | `0` |
| `abs_path` is `NULL` | `0` |

### Implementation

```cpp
int PluginManager::api_fs_exists_abs(void* mh, const char* path)
{
    (void)mh;
    if (!path)
        return 0;
    return QFileInfo::exists(QString::fromUtf8(path)) ? 1 : 0;
}
```

`QFileInfo::exists()` is a static method that checks the filesystem
directly. It returns `true` for regular files, directories, symlinks
(if the target exists), and other filesystem objects (sockets, pipes,
device nodes, etc.).

### Difference from `fs_exists()`

| | `fs_exists()` | `fs_exists_abs()` |
|-|---------------|-------------------|
| Path type | Relative (to plugin data dir) | Absolute |
| Path validation | Yes (`validateRelativePath`) | None |
| Qt backend | `QFile::exists()` | `QFileInfo::exists()` |
| Scope | Plugin data directory only | Entire filesystem |

Both `QFile::exists()` and `QFileInfo::exists()` behave identically for
normal files and directories. The functional difference is purely in path
handling.

### Example

```c
int check_instance_resources(MMCOContext* ctx, const char* game_dir)
{
    void* mh = ctx->module_handle;
    char path[4096];

    snprintf(path, sizeof(path), "%s/mods", game_dir);
    int has_mods = ctx->fs_exists_abs(mh, path);

    snprintf(path, sizeof(path), "%s/config", game_dir);
    int has_config = ctx->fs_exists_abs(mh, path);

    ctx->log(mh, 0, "Instance %s: mods=%d, config=%d",
             game_dir, has_mods, has_config);
    return 0;
}
```

---

## `fs_remove()` — Delete a file or directory {#fs_remove}

### Synopsis

```c
int fs_remove(void* mh, const char* abs_path);
```

Removes the file or directory at `abs_path`. If the path points to a
directory, the **entire directory tree is removed recursively**, including
all files and subdirectories.

> **⚠ Danger:** This function performs **recursive deletion** on
> directories. A wrong path can destroy arbitrary data. Always validate
> paths carefully. Consider confirming with the user via
> `ui_confirm_dialog()` before deleting user-visible directories.

### Parameters

| # | Name | Type | Description |
|---|------|------|-------------|
| 1 | `mh` | `void*` | Module handle. Currently **unused**. |
| 2 | `abs_path` | `const char*` | Absolute path to the file or directory to delete. UTF-8 encoded. |

### Return value

| Condition | Value |
|-----------|-------|
| Successfully removed | `0` |
| `abs_path` is `NULL` | `-1` |
| File/directory does not exist | `-1` |
| Permission denied | `-1` |
| Partial deletion of directory tree | `-1` (some files may have been removed) |

### Implementation

```cpp
int PluginManager::api_fs_remove(void* mh, const char* path)
{
    (void)mh;
    if (!path)
        return -1;

    QFileInfo fi(QString::fromUtf8(path));
    if (fi.isDir()) {
        return QDir(fi.absoluteFilePath()).removeRecursively() ? 0 : -1;
    }
    return QFile::remove(fi.absoluteFilePath()) ? 0 : -1;
}
```

### File vs. directory dispatch

The implementation uses `QFileInfo::isDir()` to determine whether the
path is a directory or a file:

- **Directory:** Calls `QDir::removeRecursively()`, which deletes the
  directory and everything inside it. This is equivalent to `rm -rf`.
  If any file within the tree cannot be removed (permissions, locked
  file, etc.), `removeRecursively()` returns `false` but may have
  already removed some entries.
- **File:** Calls `QFile::remove()`, which deletes a single file.

### Edge cases

| Path points to | Behaviour |
|----------------|-----------|
| Regular file | Deleted. Returns `0`. |
| Empty directory | Deleted. Returns `0`. |
| Non-empty directory | All contents deleted recursively. Returns `0`. |
| Symbolic link | The link itself is removed (not the target). |
| Non-existent path | `QFileInfo::isDir()` returns `false` for non-existent paths. `QFile::remove()` fails → returns `-1`. |
| Path is `/` | Would attempt `QDir("/").removeRecursively()`. **Extremely dangerous.** Always validate paths. |

### Partial deletion warning

If `QDir::removeRecursively()` encounters a file it cannot delete (e.g.
due to permissions or a locked file on Windows), it returns `false` but
**does not roll back** files already removed. The directory tree may be
left in a partially-deleted state.

### Example: cleaning up old cache entries

```c
int cleanup_old_caches(MMCOContext* ctx)
{
    void* mh = ctx->module_handle;
    const char* data_dir = ctx->fs_plugin_data_dir(mh);

    char cache_path[4096];
    snprintf(cache_path, sizeof(cache_path), "%s/cache", data_dir);

    if (!ctx->fs_exists_abs(mh, cache_path))
        return 0;  /* nothing to clean */

    /* Remove entire cache directory */
    int rc = ctx->fs_remove(mh, cache_path);
    if (rc != 0) {
        ctx->log(mh, 3, "Failed to remove cache directory");
        return -1;
    }

    /* Re-create it empty */
    ctx->fs_mkdir(mh, cache_path);
    ctx->log(mh, 0, "Cache cleaned");
    return 0;
}
```

### Example: safe deletion with user confirmation

```c
int delete_backup(MMCOContext* ctx, const char* backup_path)
{
    void* mh = ctx->module_handle;

    if (!ctx->fs_exists_abs(mh, backup_path)) {
        ctx->log(mh, 3, "Backup does not exist: %s", backup_path);
        return -1;
    }

    /* Ask the user */
    char msg[512];
    snprintf(msg, sizeof(msg),
             "Permanently delete backup?\n%s", backup_path);

    if (!ctx->ui_confirm_dialog(mh, "Delete Backup", msg))
        return 0;  /* user cancelled */

    return ctx->fs_remove(mh, backup_path);
}
```

---

## `fs_copy_file()` — Copy a single file {#fs_copy_file}

### Synopsis

```c
int fs_copy_file(void* mh, const char* src, const char* dst);
```

Copies the file at absolute path `src` to absolute path `dst`. This
copies **files only** — not directories.

### Parameters

| # | Name | Type | Description |
|---|------|------|-------------|
| 1 | `mh` | `void*` | Module handle. Currently **unused**. |
| 2 | `src` | `const char*` | Absolute path to the source file. Must exist and be a regular file. |
| 3 | `dst` | `const char*` | Absolute path to the destination. Must **not** already exist (see note). |

### Return value

| Condition | Value |
|-----------|-------|
| File copied successfully | `0` |
| `src` or `dst` is `NULL` | `-1` |
| Source does not exist | `-1` |
| Destination already exists | `-1` |
| Permission denied | `-1` |
| Disk full | `-1` |

### Overwrite behaviour

`QFile::copy()` does **not** overwrite existing files. If `dst` already
exists, the function returns `false` (and `fs_copy_file` returns `-1`).
To overwrite, remove the destination first:

```c
ctx->fs_remove(mh, dst_path);
ctx->fs_copy_file(mh, src_path, dst_path);
```

### Implementation

```cpp
int PluginManager::api_fs_copy_file(void* mh, const char* src,
                                    const char* dst)
{
    (void)mh;
    if (!src || !dst)
        return -1;
    return QFile::copy(QString::fromUtf8(src),
                       QString::fromUtf8(dst)) ? 0 : -1;
}
```

### Error conditions

| Condition | Behaviour |
|-----------|-----------|
| `src` is `NULL` | Returns `-1` |
| `dst` is `NULL` | Returns `-1` |
| `src` does not exist | Returns `-1` |
| `src` is a directory | Returns `-1` (QFile::copy does not copy directories) |
| `dst` already exists | Returns `-1` (no overwrite) |
| Parent directory of `dst` does not exist | Returns `-1` |
| Insufficient disk space | Returns `-1` |
| `src` and `dst` are the same path | Returns `-1` |

### Example: creating a backup copy

```c
int backup_config(MMCOContext* ctx, const char* instance_dir)
{
    void* mh = ctx->module_handle;

    char src[4096], dst[4096];
    snprintf(src, sizeof(src), "%s/options.txt", instance_dir);
    snprintf(dst, sizeof(dst), "%s/options.txt.bak", instance_dir);

    /* Remove old backup if it exists (QFile::copy won't overwrite) */
    if (ctx->fs_exists_abs(mh, dst))
        ctx->fs_remove(mh, dst);

    if (ctx->fs_copy_file(mh, src, dst) != 0) {
        ctx->log(mh, 3, "Failed to back up options.txt");
        return -1;
    }

    return 0;
}
```

---

## `fs_file_size()` — Query file size {#fs_file_size}

### Synopsis

```c
int64_t fs_file_size(void* mh, const char* abs_path);
```

Returns the size of the file at `abs_path` in bytes.

### Parameters

| # | Name | Type | Description |
|---|------|------|-------------|
| 1 | `mh` | `void*` | Module handle. Currently **unused**. |
| 2 | `abs_path` | `const char*` | Absolute path to the file. UTF-8 encoded. |

### Return value

| Condition | Value |
|-----------|-------|
| File exists | File size in bytes (`int64_t`, ≥ 0). Empty files return `0`. |
| `abs_path` is `NULL` | `-1` |
| File does not exist | `-1` |

### Implementation

```cpp
int64_t PluginManager::api_fs_file_size(void* mh, const char* path)
{
    (void)mh;
    if (!path)
        return -1;
    QFileInfo fi(QString::fromUtf8(path));
    if (!fi.exists())
        return -1;
    return fi.size();
}
```

`QFileInfo::size()` returns `qint64`. For regular files, this is the
file size in bytes. For directories, the return value is
platform-dependent (typically 4096 on Linux ext4, 0 on Windows).

### Use with sandboxed reads

`fs_file_size()` takes an absolute path, but `fs_read()` takes a relative
path. To size a file before reading it via the sandboxed API, build the
absolute path from `fs_plugin_data_dir()`:

```c
int64_t get_data_file_size(MMCOContext* ctx, const char* rel)
{
    void* mh = ctx->module_handle;
    const char* data_dir = ctx->fs_plugin_data_dir(mh);

    char abs[4096];
    snprintf(abs, sizeof(abs), "%s/%s", data_dir, rel);

    return ctx->fs_file_size(mh, abs);
}
```

### Example

```c
void report_backup_sizes(MMCOContext* ctx, const char* backup_dir)
{
    void* mh = ctx->module_handle;

    /* This would typically be used inside a fs_list_dir callback */
    char path[4096];
    snprintf(path, sizeof(path), "%s/backup-2026-04-15.zip", backup_dir);

    int64_t sz = ctx->fs_file_size(mh, path);
    if (sz >= 0) {
        ctx->log(mh, 0, "Backup size: %lld bytes (%.2f MiB)",
                 (long long)sz, (double)sz / (1024.0 * 1024.0));
    }
}
```

---

## `fs_list_dir()` — Enumerate directory contents {#fs_list_dir}

### Synopsis

```c
int fs_list_dir(void* mh, const char* abs_path, int type,
                MMCODirEntryCallback callback, void* user_data);
```

Enumerates the entries in the directory at `abs_path`. For each entry,
the `callback` function is invoked with the entry's name and a flag
indicating whether it is a directory.

### Callback type

```c
typedef void (*MMCODirEntryCallback)(void* user_data,
                                     const char* entry_name,
                                     int is_dir);
```

| Parameter | Description |
|-----------|-------------|
| `user_data` | The same `user_data` pointer that was passed to `fs_list_dir()`. |
| `entry_name` | The name of the entry (filename only, not the full path). UTF-8 encoded. The pointer is valid only for the duration of the callback invocation. |
| `is_dir` | `1` if the entry is a directory, `0` if it is a file. |

### Parameters

| # | Name | Type | Description |
|---|------|------|-------------|
| 1 | `mh` | `void*` | Module handle. Currently **unused**. |
| 2 | `abs_path` | `const char*` | Absolute path to the directory to list. |
| 3 | `type` | `int` | Filter for entry types. See table below. |
| 4 | `callback` | `MMCODirEntryCallback` | Function called once per matching entry. |
| 5 | `user_data` | `void*` | Opaque pointer forwarded to every callback invocation. |

### Type filter values

| Value | Constant | Entries returned |
|-------|----------|-----------------|
| `0` | (all) | Files **and** directories |
| `1` | (files) | Files only |
| `2` | (dirs) | Directories only |

Special directories `.` and `..` are **never** included (filtered by
`QDir::NoDotAndDotDot`).

### Return value

| Condition | Value |
|-----------|-------|
| Directory listed successfully | `0` (even if zero entries matched) |
| `abs_path` is `NULL` | `-1` |
| `callback` is `NULL` | `-1` |
| Directory does not exist | `-1` |
| Cannot open directory (permissions) | `-1` |

### Entry delivery model

Entries are delivered **synchronously** via the callback. The callback is
invoked once per entry, in the order determined by `QDir::entryInfoList()`
(typically filesystem-dependent, often alphabetical on most platforms).
All callbacks complete before `fs_list_dir()` returns.

The `entry_name` pointer passed to the callback is valid **only for the
duration of that specific callback invocation**. If you need to store
the name, copy it into your own buffer:

```c
void my_callback(void* ud, const char* name, int is_dir)
{
    /* WRONG: storing the pointer directly */
    // stored_names[count] = name;  /* dangling after callback returns */

    /* CORRECT: Copy the string */
    stored_names[count] = strdup(name);
    count++;
}
```

### Implementation

```cpp
int PluginManager::api_fs_list_dir(void* mh, const char* path, int type,
                                   MMCODirEntryCallback cb, void* ud)
{
    (void)mh;
    if (!path || !cb)
        return -1;

    QDir dir(QString::fromUtf8(path));
    if (!dir.exists())
        return -1;

    QDir::Filters filters;
    if (type == 1)
        filters = QDir::Files;
    else if (type == 2)
        filters = QDir::Dirs | QDir::NoDotAndDotDot;
    else
        filters = QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot;

    const auto entries = dir.entryInfoList(filters);
    for (const auto& entry : entries) {
        cb(ud, entry.fileName().toUtf8().constData(),
           entry.isDir() ? 1 : 0);
    }
    return 0;
}
```

### Qt filter mapping

| `type` value | `QDir::Filters` applied |
|-------------|------------------------|
| `0` (all) | `QDir::Files \| QDir::Dirs \| QDir::NoDotAndDotDot` |
| `1` (files) | `QDir::Files` |
| `2` (dirs) | `QDir::Dirs \| QDir::NoDotAndDotDot` |

Note that when `type == 1` (files only), the `QDir::NoDotAndDotDot` flag
is not added, but this is harmless because `.` and `..` are directories
and would be excluded by the `QDir::Files` filter anyway.

### Hidden files

The implementation does **not** add `QDir::Hidden` to the filter. On
Unix, files whose names start with `.` are considered hidden and **will
not be listed** by default. On Windows, files with the hidden attribute
set will similarly be excluded.

If your plugin needs to see hidden files, there is currently no way to
request this through the API. You would need to access them via known
filenames using `fs_exists_abs()` and `fs_file_size()`.

### Sorting

`QDir::entryInfoList()` is called without an explicit sort order. The
default is `QDir::NoSort`, which means the entries are returned in the
order provided by the underlying filesystem. On most Linux filesystems
(ext4, btrfs), this is roughly alphabetical but not guaranteed. On
Windows NTFS, it is typically alphabetical.

If you need sorted output, sort the results in your callback
accumulator.

### Error conditions

| Condition | Behaviour |
|-----------|-----------|
| `abs_path` is `NULL` | Returns `-1` |
| `callback` is `NULL` | Returns `-1` |
| `abs_path` does not point to a directory | Returns `-1` |
| `abs_path` is a file | Returns `-1` (`QDir::exists()` returns `false` for files) |
| Directory is empty | Returns `0`, callback is never invoked |
| `type` is an unrecognised value (e.g. 42) | Falls through to the `else` branch, lists all entries |
| `user_data` is `NULL` | Valid — passed through to callback unchanged |

### Example: collecting file names into an array

```c
#define MAX_ENTRIES 256

typedef struct {
    char* names[MAX_ENTRIES];
    int   is_dir[MAX_ENTRIES];
    int   count;
} DirListing;

static void collect_entry(void* ud, const char* name, int is_dir)
{
    DirListing* listing = (DirListing*)ud;
    if (listing->count >= MAX_ENTRIES)
        return;  /* silently drop excess entries */

    listing->names[listing->count] = strdup(name);
    listing->is_dir[listing->count] = is_dir;
    listing->count++;
}

int list_directory(MMCOContext* ctx, const char* dir_path,
                   DirListing* out)
{
    out->count = 0;
    return ctx->fs_list_dir(ctx->module_handle, dir_path,
                            0 /* all */, collect_entry, out);
}

void free_listing(DirListing* listing)
{
    for (int i = 0; i < listing->count; i++)
        free(listing->names[i]);
    listing->count = 0;
}
```

### Example: counting files in a directory

```c
static void count_callback(void* ud, const char* name, int is_dir)
{
    (void)name;
    (void)is_dir;
    int* count = (int*)ud;
    (*count)++;
}

int count_files(MMCOContext* ctx, const char* dir_path)
{
    int count = 0;
    if (ctx->fs_list_dir(ctx->module_handle, dir_path,
                         1 /* files only */, count_callback,
                         &count) != 0) {
        return -1;
    }
    return count;
}
```

### Example: recursive directory scanner

```c
static void scan_entry(void* ud, const char* name, int is_dir);

typedef struct {
    MMCOContext* ctx;
    int   file_count;
    int64_t total_size;
} ScanState;

static void scan_dir_recursive(ScanState* state, const char* dir_path)
{
    /* List all entries */
    ctx_global = state->ctx;  /* for the callback */
    state->ctx->fs_list_dir(state->ctx->module_handle,
                            dir_path, 0, scan_entry, state);
}

/*
 * NOTE: This simplified example does not track the current
 * directory path inside the callback. A real implementation
 * would pass the full parent path through user_data.
 */
```

For a complete recursive scanner, maintain a stack of directory paths
in your `user_data` structure and call `fs_list_dir()` for each
subdirectory found.

---

## Complete example: cache cleanup utility

This complete example demonstrates using multiple filesystem functions
together to implement a cache cleanup routine that removes files older
than a certain size threshold:

```c
#include <mmco_sdk.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static MMCOContext* ctx;

typedef struct {
    char   dir_path[4096];
    int64_t total_freed;
    int64_t max_cache_bytes;
    int    files_removed;
} CleanupState;

static void cleanup_callback(void* ud, const char* name, int is_dir)
{
    CleanupState* state = (CleanupState*)ud;

    if (is_dir)
        return;  /* skip subdirectories */

    /* Build absolute path */
    char path[4096];
    snprintf(path, sizeof(path), "%s/%s", state->dir_path, name);

    /* Check file size */
    int64_t sz = ctx->fs_file_size(ctx->module_handle, path);
    if (sz < 0)
        return;

    /* Remove files larger than threshold */
    if (sz > state->max_cache_bytes) {
        if (ctx->fs_remove(ctx->module_handle, path) == 0) {
            state->total_freed += sz;
            state->files_removed++;
            ctx->log(ctx->module_handle, 0,
                     "Removed %s (%lld bytes)", name, (long long)sz);
        }
    }
}

int cleanup_cache(int64_t max_file_size)
{
    void* mh = ctx->module_handle;
    const char* data_dir = ctx->fs_plugin_data_dir(mh);

    CleanupState state;
    snprintf(state.dir_path, sizeof(state.dir_path),
             "%s/cache", data_dir);
    state.total_freed = 0;
    state.max_cache_bytes = max_file_size;
    state.files_removed = 0;

    /* Ensure cache dir exists */
    if (!ctx->fs_exists_abs(mh, state.dir_path)) {
        ctx->log(mh, 0, "No cache directory — nothing to clean");
        return 0;
    }

    /* Scan and clean */
    int rc = ctx->fs_list_dir(mh, state.dir_path,
                              1 /* files only */,
                              cleanup_callback, &state);
    if (rc != 0) {
        ctx->log_error(mh, "Failed to list cache directory");
        return -1;
    }

    ctx->log(mh, 0, "Cache cleanup: removed %d files, freed %lld bytes",
             state.files_removed, (long long)state.total_freed);
    return 0;
}
```

---

## Complete example: directory tree copier

This example copies an entire directory tree using `fs_list_dir()`,
`fs_mkdir()`, and `fs_copy_file()`:

```c
typedef struct {
    MMCOContext* ctx;
    char src_root[4096];
    char dst_root[4096];
    int  errors;
} CopyState;

static void copy_entry(void* ud, const char* name, int is_dir)
{
    CopyState* state = (CopyState*)ud;
    void* mh = state->ctx->module_handle;

    char src_path[4096], dst_path[4096];
    snprintf(src_path, sizeof(src_path), "%s/%s",
             state->src_root, name);
    snprintf(dst_path, sizeof(dst_path), "%s/%s",
             state->dst_root, name);

    if (is_dir) {
        /* Create destination directory and recurse */
        state->ctx->fs_mkdir(mh, dst_path);

        /* Save and mutate state for recursion */
        char saved_src[4096], saved_dst[4096];
        strncpy(saved_src, state->src_root, sizeof(saved_src));
        strncpy(saved_dst, state->dst_root, sizeof(saved_dst));

        strncpy(state->src_root, src_path, sizeof(state->src_root));
        strncpy(state->dst_root, dst_path, sizeof(state->dst_root));

        state->ctx->fs_list_dir(mh, src_path, 0,
                                copy_entry, state);

        /* Restore state */
        strncpy(state->src_root, saved_src, sizeof(state->src_root));
        strncpy(state->dst_root, saved_dst, sizeof(state->dst_root));
    } else {
        /* Copy the file */
        if (state->ctx->fs_copy_file(mh, src_path, dst_path) != 0) {
            state->ctx->log(mh, 3, "Failed to copy: %s", src_path);
            state->errors++;
        }
    }
}

int copy_directory_tree(MMCOContext* ctx,
                        const char* src, const char* dst)
{
    void* mh = ctx->module_handle;

    /* Create root destination directory */
    if (ctx->fs_mkdir(mh, dst) != 0)
        return -1;

    CopyState state;
    state.ctx = ctx;
    strncpy(state.src_root, src, sizeof(state.src_root));
    strncpy(state.dst_root, dst, sizeof(state.dst_root));
    state.errors = 0;

    int rc = ctx->fs_list_dir(mh, src, 0, copy_entry, &state);
    if (rc != 0)
        return -1;

    return state.errors == 0 ? 0 : -1;
}
```

---

## Cross-reference: combining with Zip API (S09)

The filesystem and Zip APIs work together naturally. A typical
backup-and-restore workflow:

```c
/* Create a backup */
int create_backup(MMCOContext* ctx, const char* instance_dir,
                  const char* backup_path)
{
    void* mh = ctx->module_handle;

    /* Ensure backup directory exists */
    char backup_dir[4096];
    /* ... extract directory from backup_path ... */
    ctx->fs_mkdir(mh, backup_dir);

    /* Compress instance into a zip */
    return ctx->zip_compress_dir(mh, backup_path, instance_dir);
}

/* Restore from a backup */
int restore_backup(MMCOContext* ctx, const char* backup_path,
                   const char* instance_dir)
{
    void* mh = ctx->module_handle;

    /* Remove existing instance data */
    ctx->fs_remove(mh, instance_dir);

    /* Re-create directory */
    ctx->fs_mkdir(mh, instance_dir);

    /* Extract backup */
    return ctx->zip_extract(mh, backup_path, instance_dir);
}
```

See the [Zip API documentation](../s09-zip/) for details on
`zip_compress_dir()` and `zip_extract()`.

---

## Cross-reference: Settings API (S02) vs. Filesystem

| Need | Use |
|------|-----|
| Small key-value config (strings, ints, bools) | [Settings API](../s02-settings/) — simpler, persistent, no file handling |
| Complex config (JSON, TOML, XML) | Filesystem API — `fs_write()` + `fs_read()` |
| Binary data (images, caches, databases) | Filesystem API |
| Multiple config files | Filesystem API |
| Instance-specific data | Filesystem API (write into instance's game dir) |
