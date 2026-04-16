# Section 10 — Filesystem API

> **Header:** `PluginAPI.h` (function pointer declarations) · **SDK:** `mmco_sdk.h` (struct definition) · **Implementation:** `PluginManager.cpp` (static trampolines) · **Backend:** Qt `QFile`, `QDir`, `QFileInfo`

---

## Overview

The Filesystem API provides plugins with controlled access to the local
filesystem. It is the primary mechanism for persistent storage of plugin
configuration, caches, databases, and any other data that must survive
across launcher restarts.

The API is split into two tiers with fundamentally different security
properties:

1. **Sandboxed operations** — `fs_read()`, `fs_write()`, `fs_exists()` —
   accept **relative paths** that are resolved against the plugin's
   dedicated data directory. These functions perform path-traversal
   validation and refuse to escape the sandbox.

2. **Absolute-path operations** — `fs_mkdir()`, `fs_exists_abs()`,
   `fs_remove()`, `fs_copy_file()`, `fs_file_size()`, `fs_list_dir()` —
   accept **absolute paths** and operate on arbitrary filesystem locations
   (subject only to OS-level permissions). No path validation is performed.

A convenience function, `fs_plugin_data_dir()`, returns the absolute path
to the plugin's data directory so that plugins can construct absolute paths
themselves when needed.

```text
┌───────────────────────────────────────────────────────────────────────────┐
│                        Filesystem API (S10)                               │
├───────────────────────────────────────────────────────────────────────────┤
│                                                                           │
│  ┌─────────────── Sandboxed ────────────────────────────────────────┐     │
│  │  fs_plugin_data_dir()   → get the data dir path                 │     │
│  │  fs_read(rel, buf, sz)  → read file into caller's buffer        │     │
│  │  fs_write(rel, data, sz)→ write bytes to file (truncate)        │     │
│  │  fs_exists(rel)         → check if file exists in data dir      │     │
│  └─────────────────────────────────────────────────────────────────┘     │
│                                                                           │
│  ┌─────────────── Absolute ─────────────────────────────────────────┐    │
│  │  fs_mkdir(abs)           → create directory (recursive)          │    │
│  │  fs_exists_abs(abs)      → check existence of any path          │    │
│  │  fs_remove(abs)          → delete file or directory (recursive)  │    │
│  │  fs_copy_file(src, dst)  → copy a single file                   │    │
│  │  fs_file_size(abs)       → query file size in bytes             │    │
│  │  fs_list_dir(abs, type, cb, ud) → enumerate directory entries   │    │
│  └──────────────────────────────────────────────────────────────────┘    │
│                                                                           │
└───────────────────────────────────────────────────────────────────────────┘
```

---

## Function pointers in `MMCOContext`

All ten filesystem function pointers are declared in `PluginAPI.h` within
the `MMCOContext` struct:

```c
/* Sandboxed (relative to plugin data dir) */
const char* (*fs_plugin_data_dir)(void* mh);
int64_t     (*fs_read)(void* mh, const char* rel_path, void* buf, size_t sz);
int         (*fs_write)(void* mh, const char* rel_path, const void* data,
                        size_t sz);
int         (*fs_exists)(void* mh, const char* rel_path);

/* Absolute path operations */
int         (*fs_mkdir)(void* mh, const char* abs_path);
int         (*fs_exists_abs)(void* mh, const char* abs_path);
int         (*fs_remove)(void* mh, const char* abs_path);
int         (*fs_copy_file)(void* mh, const char* src, const char* dst);
int64_t     (*fs_file_size)(void* mh, const char* abs_path);
int         (*fs_list_dir)(void* mh, const char* abs_path, int type,
                           MMCODirEntryCallback callback, void* user_data);
```

The callback type used by `fs_list_dir()` is declared at the top of
`PluginAPI.h`:

```c
typedef void (*MMCODirEntryCallback)(void* user_data, const char* entry_name,
                                     int is_dir);
```

---

## The plugin data directory

Each plugin receives a unique, per-module data directory that is created
automatically when the plugin is loaded. The directory lives under the
platform-specific MeshMC data location:

| Platform | Base path |
|----------|-----------|
| Linux / macOS | `~/.local/share/meshmc/plugin-data/<module-id>/` |
| Windows | `%LOCALAPPDATA%/meshmc/plugin-data/<module-id>/` |

The `<module-id>` is the string returned by `PluginMetadata::moduleId()`,
which is derived from the plugin's `.mmco` filename (without the
extension). For a module file named `BackupSystem.mmco`, the data
directory on Linux would be:

```
~/.local/share/meshmc/plugin-data/BackupSystem/
```

The directory is created via `QDir().mkpath()` during
`PluginManager::ensurePluginDataDir()`, which is called before
`mmco_init()`. The plugin can therefore assume the directory exists from
the very first API call.

### Retrieving the path

```c
const char* (*fs_plugin_data_dir)(void* mh);
```

Returns a pointer to a null-terminated UTF-8 string containing the
**absolute path** to the plugin's data directory. The returned pointer is
valid for the lifetime of the plugin (unlike most string-returning API
functions, this one points into the `ModuleRuntime::dataDir` field, which
is not overwritten by subsequent API calls).

```c
const char* data_dir = ctx->fs_plugin_data_dir(ctx->module_handle);
ctx->log(ctx->module_handle, 0, "Data dir: %s", data_dir);
```

---

## Sandboxing model

### Sandboxed functions

The three sandboxed functions — `fs_read()`, `fs_write()`, `fs_exists()` —
share a common path-validation step implemented by the static helper
`validateRelativePath()` in `PluginManager.cpp`:

```cpp
static bool validateRelativePath(const char* rel)
{
    if (!rel || rel[0] == '\0')
        return false;
    QString p = QString::fromUtf8(rel);
    // Reject path traversal
    if (p.contains("..") || p.startsWith('/') || p.startsWith('\\'))
        return false;
    return true;
}
```

The validator rejects:

| Condition | Example | Why |
|-----------|---------|-----|
| `NULL` pointer | — | Null dereference prevention |
| Empty string | `""` | Would resolve to the data dir itself |
| Contains `..` | `../../../etc/passwd` | Path traversal |
| Starts with `/` | `/etc/passwd` | Absolute path (escape sandbox) |
| Starts with `\` | `\Windows\System32` | Windows absolute path |

After validation, the relative path is resolved against the plugin's data
directory:

```cpp
QString path = QDir(QString::fromStdString(r->dataDir))
                   .filePath(QString::fromUtf8(rel));
```

**Note:** The validation does **not** canonicalize symlinks. If the data
directory itself (or a subdirectory) contains a symbolic link, the
sandboxed functions will follow it. The sandbox prevents the plugin from
specifying *relative* paths that escape the data directory, but does not
prevent escape via symlinks created outside the API.

### Absolute-path functions

The absolute-path functions (`fs_mkdir`, `fs_exists_abs`, `fs_remove`,
`fs_copy_file`, `fs_file_size`, `fs_list_dir`) perform **no path
validation whatsoever**. They accept any path that the underlying OS
permits. This gives plugins full flexibility at the cost of requiring
the plugin author to handle paths responsibly.

> **Security implication:** Plugins using absolute-path operations can
> read, write, copy, and **recursively delete** any file or directory
> that the MeshMC process has permission to access. Review plugin source
> code carefully before installing third-party plugins.

---

## Qt backend mapping

Every filesystem function is a thin wrapper around Qt I/O classes:

| API function | Qt class / method |
|-------------|-------------------|
| `fs_plugin_data_dir` | Returns `ModuleRuntime::dataDir` (pre-computed `QString::toStdString()`) |
| `fs_read` | `QFile::open(ReadOnly)` + `QFile::read(buf, sz)` |
| `fs_write` | `QFile::open(WriteOnly | Truncate)` + `QFile::write(data, sz)` |
| `fs_exists` | `QFile::exists(path)` |
| `fs_mkdir` | `QDir().mkpath(path)` — recursive |
| `fs_exists_abs` | `QFileInfo::exists(path)` |
| `fs_remove` | `QFile::remove()` (files) / `QDir::removeRecursively()` (dirs) |
| `fs_copy_file` | `QFile::copy(src, dst)` |
| `fs_file_size` | `QFileInfo::size()` |
| `fs_list_dir` | `QDir::entryInfoList(filters)` |

All operations are **synchronous** and **blocking**. They execute on the
calling thread (which must be the main GUI thread per the general API
contract). For large files or deep directory trees, callers should be
aware that the main thread will be blocked for the duration of the I/O.

---

## Relationship to other sections

### S02 — Settings API

The [Settings API](../s02-settings/) provides a key-value store backed by
`QSettings`. For simple configuration (strings, integers, booleans), the
Settings API is simpler and avoids manual file formatting. Use the
Filesystem API when you need:

- Binary data storage
- Complex or nested data structures (JSON, TOML, etc.)
- Bulk data (caches, logs, databases)
- Multiple configuration files
- Custom file formats

### S09 — Zip / Archive API

The [Zip API](../s09-zip/) builds on top of the filesystem. A typical
pattern is:

1. Use `fs_list_dir()` to enumerate files for archiving.
2. Use `zip_compress_dir()` to create a backup archive.
3. Use `zip_extract()` to restore from an archive.
4. Use `fs_remove()` to clean up old backups.

The BackupSystem plugin follows exactly this pattern.

---

## Sub-pages

| Page | Functions |
|------|-----------|
| [File I/O](file-io.md) | `fs_plugin_data_dir()`, `fs_read()`, `fs_write()`, `fs_exists()` |
| [Directory & Path Operations](directory-ops.md) | `fs_mkdir()`, `fs_exists_abs()`, `fs_remove()`, `fs_copy_file()`, `fs_file_size()`, `fs_list_dir()` |

---

## Quick example: plugin data lifecycle

```c
#include <mmco_sdk.h>
#include <string.h>
#include <stdio.h>

static MMCOContext* ctx;

/* Write a config file on first run, read it on subsequent runs. */
int mmco_init(MMCOContext* c)
{
    ctx = c;
    void* mh = ctx->module_handle;

    /* Check if config already exists */
    if (!ctx->fs_exists(mh, "config.json")) {
        const char* defaults = "{\"version\": 1, \"theme\": \"dark\"}";
        if (ctx->fs_write(mh, "config.json",
                          defaults, strlen(defaults)) != 0) {
            ctx->log_error(mh, "Failed to write default config");
            return -1;
        }
        ctx->log(mh, 0, "Created default config.json");
    }

    /* Read the config file */
    char buf[4096];
    int64_t n = ctx->fs_read(mh, "config.json", buf, sizeof(buf) - 1);
    if (n < 0) {
        ctx->log_error(mh, "Failed to read config.json");
        return -1;
    }
    buf[n] = '\0';
    ctx->log(mh, 0, "Config: %s", buf);

    /* Report data directory location */
    ctx->log(mh, 0, "Data dir: %s",
             ctx->fs_plugin_data_dir(mh));

    return 0;
}
```

---

## Thread safety

All filesystem functions must be called from the **main (GUI) thread**.
This is the same constraint as every other MMCO API function. The
underlying Qt I/O classes are not inherently thread-unsafe, but the
`ModuleRuntime` state (including `tempString` and `dataDir`) is not
protected by any mutex.

---

## Error handling summary

| Function | Success | Failure |
|----------|---------|---------|
| `fs_plugin_data_dir` | Absolute path string | Never fails (returns pre-computed value) |
| `fs_read` | Number of bytes read (≥ 0) | `-1` |
| `fs_write` | `0` | `-1` |
| `fs_exists` | `1` (exists) | `0` (does not exist or invalid path) |
| `fs_mkdir` | `0` | `-1` |
| `fs_exists_abs` | `1` (exists) | `0` |
| `fs_remove` | `0` | `-1` |
| `fs_copy_file` | `0` | `-1` |
| `fs_file_size` | File size in bytes (≥ 0) | `-1` |
| `fs_list_dir` | `0` | `-1` |
