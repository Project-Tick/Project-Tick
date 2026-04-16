# File I/O — `fs_plugin_data_dir()`, `fs_read()`, `fs_write()`, `fs_exists()` {#file-io}

> **Section:** S10 (Filesystem) · **Header:** `PluginAPI.h` · **Trampolines:** `PluginManager::api_fs_plugin_data_dir`, `api_fs_read`, `api_fs_write`, `api_fs_exists` · **Backend:** Qt `QFile`, `QDir`

These four functions form the **sandboxed file I/O layer**. They operate
exclusively within the plugin's dedicated data directory and refuse paths
that attempt to escape it. This page documents each function exhaustively.

---

## `fs_plugin_data_dir()` — Get the plugin data directory {#fs_plugin_data_dir}

### Synopsis

```c
const char* fs_plugin_data_dir(void* mh);
```

Returns the absolute filesystem path to this plugin's private data
directory. The directory is guaranteed to exist when `mmco_init()` is
called.

### Parameters

| # | Name | Type | Description |
|---|------|------|-------------|
| 1 | `mh` | `void*` | Module handle. Pass `ctx->module_handle`. |

### Return value

| Condition | Value |
|-----------|-------|
| Always | Non-null pointer to a null-terminated UTF-8 string containing the absolute path. |

This function **cannot fail**. The data directory path is computed and
stored during plugin loading, before `mmco_init()` is called.

### String ownership

Unlike most string-returning API functions, the pointer returned by
`fs_plugin_data_dir()` does **not** go through the per-module
`tempString` buffer. It points directly at `ModuleRuntime::dataDir`,
which is a `std::string` that lives for the entire lifetime of the
plugin. The pointer therefore remains valid until `mmco_unload()`
returns. You do **not** need to copy it before making other API calls.

### Platform-specific paths

| Platform | Typical path |
|----------|-------------|
| Linux | `~/.local/share/meshmc/plugin-data/<module-id>/` |
| macOS | `~/.local/share/meshmc/plugin-data/<module-id>/` |
| Windows | `C:\Users\<user>\AppData\Local\meshmc\plugin-data\<module-id>\` |

The `<module-id>` is derived from the `.mmco` filename (without the
extension). For `BackupSystem.mmco`, the module ID is `BackupSystem`.

### Implementation

```cpp
const char* PluginManager::api_fs_plugin_data_dir(void* mh)
{
    auto* r = rt(mh);
    return r->dataDir.c_str();
}
```

The `ModuleRuntime::dataDir` field is populated during plugin loading:

```cpp
void PluginManager::ensurePluginDataDir(PluginMetadata& meta)
{
    QString baseDir;
#ifdef Q_OS_WIN
    baseDir = QStandardPaths::writableLocation(
                  QStandardPaths::AppLocalDataLocation);
#else
    baseDir = QDir::homePath() + "/.local/share/meshmc";
#endif
    meta.dataDir = QDir(baseDir).filePath(
                       "plugin-data/" + meta.moduleId());
    QDir().mkpath(meta.dataDir);
}
```

### Example

```c
void print_data_dir(MMCOContext* ctx)
{
    const char* dir = ctx->fs_plugin_data_dir(ctx->module_handle);
    ctx->log(ctx->module_handle, 0, "My data lives at: %s", dir);
}
```

---

## `fs_read()` — Read a file from the plugin data directory {#fs_read}

### Synopsis

```c
int64_t fs_read(void* mh, const char* rel_path, void* buf, size_t sz);
```

Reads up to `sz` bytes from the file at `rel_path` (relative to the
plugin's data directory) into the caller-provided buffer `buf`. The read
is **binary** — no encoding conversion, line-ending normalization, or
null-termination is performed.

### Parameters

| # | Name | Type | Description |
|---|------|------|-------------|
| 1 | `mh` | `void*` | Module handle. Pass `ctx->module_handle`. |
| 2 | `rel_path` | `const char*` | Relative path within the plugin data directory. Must pass `validateRelativePath()`. |
| 3 | `buf` | `void*` | Pointer to a caller-allocated buffer that will receive the file contents. |
| 4 | `sz` | `size_t` | Maximum number of bytes to read. Must be ≤ the size of `buf`. |

### Parameter constraints

- **`rel_path`** — Must not be `NULL`, empty, contain `..`, or start
  with `/` or `\`. See [Path validation](#path-validation) below.
- **`buf`** — Must not be `NULL`. Must point to at least `sz` bytes of
  writable memory.
- **`sz`** — May be `0`, in which case the function opens the file,
  reads zero bytes, and returns `0`. There is **no upper limit** enforced
  by the API; the limit is your available memory.

### Return value

| Condition | Value |
|-----------|-------|
| Success | Number of bytes actually read (`int64_t`, ≥ 0). May be less than `sz` if the file is smaller. |
| Path validation failure | `-1` |
| File does not exist | `-1` |
| File cannot be opened | `-1` |
| Read error | `-1` (partial reads may have occurred; `buf` contents are undefined on error) |

### Path validation {#path-validation}

Before resolving the path, the static helper `validateRelativePath()` is
called:

```cpp
static bool validateRelativePath(const char* rel)
{
    if (!rel || rel[0] == '\0')
        return false;
    QString p = QString::fromUtf8(rel);
    if (p.contains("..") || p.startsWith('/') || p.startsWith('\\'))
        return false;
    return true;
}
```

| Input | Result | Reason |
|-------|--------|--------|
| `"config.json"` | ✅ Valid | Simple filename |
| `"subdir/data.bin"` | ✅ Valid | Subdirectory path |
| `"a/b/c/deep.txt"` | ✅ Valid | Deeply nested |
| `NULL` | ❌ Rejected | Null pointer |
| `""` | ❌ Rejected | Empty string |
| `"../escape.txt"` | ❌ Rejected | Contains `..` |
| `"subdir/../../etc/passwd"` | ❌ Rejected | Contains `..` |
| `"/etc/passwd"` | ❌ Rejected | Starts with `/` |
| `"\\server\share"` | ❌ Rejected | Starts with `\` |
| `"foo..bar.txt"` | ❌ Rejected | Contains `..` (false positive — see note) |

> **Note on `..` detection:** The validator uses `QString::contains("..")`
> which matches the literal substring `..` anywhere in the path — including
> inside filenames like `foo..bar.txt`. This is a conservative false
> positive: such filenames will be rejected even though they are not path
> traversal attempts. If your plugin needs filenames containing consecutive
> dots, use the absolute-path APIs instead.

### Path resolution

After validation, the relative path is joined to the data directory:

```cpp
QString path = QDir(QString::fromStdString(r->dataDir))
                   .filePath(QString::fromUtf8(rel));
```

`QDir::filePath()` performs simple string concatenation with a path
separator. It does **not** canonicalize the result (no `realpath()`).

### Qt backend

The implementation opens the file with `QIODevice::ReadOnly` (binary mode
is implicit — Qt does not distinguish binary/text on Unix; on Windows,
omitting `QIODevice::Text` ensures binary semantics):

```cpp
int64_t PluginManager::api_fs_read(void* mh, const char* rel, void* buf,
                                   size_t sz)
{
    if (!validateRelativePath(rel))
        return -1;

    auto* r = rt(mh);
    QString path = QDir(QString::fromStdString(r->dataDir))
                       .filePath(QString::fromUtf8(rel));

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return -1;

    qint64 bytesRead = f.read(static_cast<char*>(buf),
                              static_cast<qint64>(sz));
    return bytesRead;
}
```

`QFile::read()` returns the number of bytes read, or `-1` on error. The
file is automatically closed when `f` goes out of scope (RAII).

### Size limitations

There is no artificial size limit. `fs_read()` reads up to `sz` bytes,
where `sz` is a `size_t` (up to 2^64 − 1 on 64-bit platforms). The
practical limit is your buffer size and available memory.

To read a file whose size is not known in advance, first query its
size with `fs_file_size()`, then allocate a buffer:

```c
/* Read an entire file of unknown size */
int read_entire_file(MMCOContext* ctx, const char* rel,
                     char** out_buf, int64_t* out_len)
{
    void* mh = ctx->module_handle;

    /* Build absolute path for fs_file_size */
    const char* data_dir = ctx->fs_plugin_data_dir(mh);
    char abs_path[4096];
    snprintf(abs_path, sizeof(abs_path), "%s/%s", data_dir, rel);

    int64_t size = ctx->fs_file_size(mh, abs_path);
    if (size < 0)
        return -1;

    char* buf = (char*)malloc((size_t)size + 1);
    if (!buf)
        return -1;

    int64_t n = ctx->fs_read(mh, rel, buf, (size_t)size);
    if (n < 0) {
        free(buf);
        return -1;
    }

    buf[n] = '\0';  /* null-terminate for convenience */
    *out_buf = buf;
    *out_len = n;
    return 0;
}
```

### Partial reads

`QFile::read(buf, sz)` may return fewer bytes than requested if the file
is shorter than `sz`. This is **not** an error. Always use the return
value to determine how many bytes were placed in the buffer.

If the file is exactly `sz` bytes long, the function returns `sz`. If
the file is longer, only the first `sz` bytes are read. There is no
built-in way to seek or read from an offset — the API always reads
from the beginning of the file.

### Error conditions

| Condition | Behaviour |
|-----------|-----------|
| `rel_path` is `NULL` | Returns `-1` |
| `rel_path` is empty | Returns `-1` |
| `rel_path` contains `..` | Returns `-1` |
| `rel_path` starts with `/` or `\` | Returns `-1` |
| File does not exist | Returns `-1` |
| File exists but is not readable (permissions) | Returns `-1` |
| File is a directory | Returns `-1` (QFile cannot open directories) |
| `buf` is `NULL` | **Undefined behaviour** (no null check in implementation) |
| `sz` is 0 | Returns `0` (opens file, reads nothing) |
| Disk I/O error during read | Returns `-1` or a short read count |

### Example: reading a text config

```c
int load_config(MMCOContext* ctx)
{
    void* mh = ctx->module_handle;
    char buf[8192];

    int64_t n = ctx->fs_read(mh, "settings.ini", buf, sizeof(buf) - 1);
    if (n < 0) {
        ctx->log(mh, 3, "No settings.ini found, using defaults");
        return 0;  /* not fatal */
    }

    buf[n] = '\0';
    /* parse buf as INI... */
    ctx->log(mh, 0, "Loaded %lld bytes of config", (long long)n);
    return 0;
}
```

### Example: reading binary data

```c
int load_cached_thumbnail(MMCOContext* ctx, const char* name,
                          uint8_t* pixels, size_t max_size,
                          int64_t* out_size)
{
    void* mh = ctx->module_handle;
    char rel[256];
    snprintf(rel, sizeof(rel), "cache/thumbs/%s.png", name);

    int64_t n = ctx->fs_read(mh, rel, pixels, max_size);
    if (n < 0)
        return -1;

    *out_size = n;
    return 0;
}
```

---

## `fs_write()` — Write a file in the plugin data directory {#fs_write}

### Synopsis

```c
int fs_write(void* mh, const char* rel_path, const void* data, size_t sz);
```

Writes exactly `sz` bytes from `data` to the file at `rel_path`
(relative to the plugin's data directory). The file is opened with
**truncation semantics**: if it already exists, its previous contents
are replaced entirely. If it does not exist, it is created. Parent
directories are created automatically.

### Parameters

| # | Name | Type | Description |
|---|------|------|-------------|
| 1 | `mh` | `void*` | Module handle. Pass `ctx->module_handle`. |
| 2 | `rel_path` | `const char*` | Relative path within the plugin data directory. Must pass `validateRelativePath()`. |
| 3 | `data` | `const void*` | Pointer to the data to write. |
| 4 | `sz` | `size_t` | Number of bytes to write. |

### Parameter constraints

- **`rel_path`** — Same validation as `fs_read()`. Must not be `NULL`,
  empty, contain `..`, or start with `/` or `\`.
- **`data`** — Must not be `NULL` if `sz > 0`. If `sz` is `0`, `data`
  is not dereferenced (but may still be checked by the implementation).
- **`sz`** — Number of bytes to write. `0` is valid and creates an
  empty file (or truncates an existing file to zero length).

### Return value

| Condition | Value |
|-----------|-------|
| All `sz` bytes written successfully | `0` |
| Path validation failure | `-1` |
| Cannot create parent directories | `-1` (implicit — `QDir::mkpath` failure) |
| Cannot open file for writing | `-1` |
| Partial write (disk full, etc.) | `-1` |

### Automatic parent directory creation

Before opening the file, the implementation creates all intermediate
directories in the path:

```cpp
QDir().mkpath(QFileInfo(path).absolutePath());
```

This means you can write to `subdir/deep/nested/file.txt` without
calling `fs_mkdir()` first — the directories `subdir/`, `subdir/deep/`,
and `subdir/deep/nested/` will be created automatically.

### Write semantics

The file is opened with `QIODevice::WriteOnly | QIODevice::Truncate`:

- **WriteOnly** — The file is opened for writing. It cannot be read
  through this file handle (but there is no other handle, so this is
  irrelevant).
- **Truncate** — If the file exists, it is truncated to zero length
  before writing begins.

This means `fs_write()` is an **atomic-ish replace**: the file will
contain exactly the bytes you wrote, with no remnants of previous
content. However, the operation is **not truly atomic** — if a crash
occurs mid-write, the file may contain partial data. For crash-safe
writes, write to a temporary file and then use `fs_copy_file()` +
`fs_remove()` to swap.

### Verification

The implementation verifies that all bytes were written:

```cpp
qint64 written = f.write(static_cast<const char*>(data),
                         static_cast<qint64>(sz));
return (written == static_cast<qint64>(sz)) ? 0 : -1;
```

If `QFile::write()` returns fewer bytes than requested (e.g. disk full),
the function returns `-1`.

### Implementation

```cpp
int PluginManager::api_fs_write(void* mh, const char* rel,
                                const void* data, size_t sz)
{
    if (!validateRelativePath(rel))
        return -1;

    auto* r = rt(mh);
    QString path = QDir(QString::fromStdString(r->dataDir))
                       .filePath(QString::fromUtf8(rel));

    // Ensure parent directory exists
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return -1;

    qint64 written = f.write(static_cast<const char*>(data),
                             static_cast<qint64>(sz));
    return (written == static_cast<qint64>(sz)) ? 0 : -1;
}
```

### Error conditions

| Condition | Behaviour |
|-----------|-----------|
| `rel_path` is `NULL` | Returns `-1` |
| `rel_path` is empty | Returns `-1` |
| `rel_path` contains `..` | Returns `-1` |
| `rel_path` starts with `/` or `\` | Returns `-1` |
| Parent directory cannot be created | File open fails → returns `-1` |
| Disk full | Partial write → returns `-1` |
| Path points to a directory | `QFile::open` fails → returns `-1` |
| Read-only filesystem | `QFile::open` fails → returns `-1` |
| `data` is `NULL` and `sz > 0` | **Undefined behaviour** (no null check) |

### Example: writing a JSON config

```c
int save_config(MMCOContext* ctx, const char* json_str)
{
    void* mh = ctx->module_handle;

    int rc = ctx->fs_write(mh, "config.json",
                           json_str, strlen(json_str));
    if (rc != 0) {
        ctx->log_error(mh, "Failed to save config.json");
        return -1;
    }

    ctx->log(mh, 0, "Config saved (%zu bytes)", strlen(json_str));
    return 0;
}
```

### Example: writing binary data into a subdirectory

```c
int cache_thumbnail(MMCOContext* ctx, const char* name,
                    const uint8_t* png_data, size_t png_size)
{
    void* mh = ctx->module_handle;

    /* Subdirectory "cache/thumbs" is created automatically */
    char rel[256];
    snprintf(rel, sizeof(rel), "cache/thumbs/%s.png", name);

    return ctx->fs_write(mh, rel, png_data, png_size);
}
```

### Example: crash-safe write pattern

```c
int safe_write_config(MMCOContext* ctx, const char* json, size_t len)
{
    void* mh = ctx->module_handle;
    const char* data_dir = ctx->fs_plugin_data_dir(mh);

    /* 1. Write to a temporary file (sandboxed) */
    if (ctx->fs_write(mh, "config.json.tmp", json, len) != 0)
        return -1;

    /* 2. Build absolute paths for copy + remove */
    char src[4096], dst[4096], old[4096];
    snprintf(src, sizeof(src), "%s/config.json.tmp", data_dir);
    snprintf(dst, sizeof(dst), "%s/config.json", data_dir);
    snprintf(old, sizeof(old), "%s/config.json.bak", data_dir);

    /* 3. Backup old config (ignore failure — may not exist) */
    ctx->fs_copy_file(mh, dst, old);

    /* 4. Copy temp to final (overwrites if exists? No — see
     *    fs_copy_file docs. Remove old first.) */
    ctx->fs_remove(mh, dst);
    if (ctx->fs_copy_file(mh, src, dst) != 0) {
        /* Restore backup */
        ctx->fs_copy_file(mh, old, dst);
        ctx->fs_remove(mh, src);
        return -1;
    }

    /* 5. Clean up */
    ctx->fs_remove(mh, src);
    return 0;
}
```

---

## `fs_exists()` — Check file existence in the data directory {#fs_exists}

### Synopsis

```c
int fs_exists(void* mh, const char* rel_path);
```

Tests whether a file or directory exists at `rel_path` within the
plugin's data directory.

### Parameters

| # | Name | Type | Description |
|---|------|------|-------------|
| 1 | `mh` | `void*` | Module handle. Pass `ctx->module_handle`. |
| 2 | `rel_path` | `const char*` | Relative path within the plugin data directory. Must pass `validateRelativePath()`. |

### Return value

| Condition | Value |
|-----------|-------|
| File or directory exists | `1` |
| Does not exist | `0` |
| Path validation failure | `0` |

Note that `fs_exists()` returns `0` (not `-1`) on path validation
failure. This is semantically consistent — an invalid path does not
"exist" — but it means you cannot distinguish "does not exist" from
"invalid path". If you need to distinguish these cases, validate the
path yourself before calling.

### Implementation

```cpp
int PluginManager::api_fs_exists(void* mh, const char* rel)
{
    if (!validateRelativePath(rel))
        return 0;

    auto* r = rt(mh);
    QString path = QDir(QString::fromStdString(r->dataDir))
                       .filePath(QString::fromUtf8(rel));
    return QFile::exists(path) ? 1 : 0;
}
```

`QFile::exists()` is a static method that checks both files and
directories. It returns `true` if the path exists, regardless of type.

### Files vs. directories

`fs_exists()` returns `1` for both files and directories. If you need
to distinguish between them, use `fs_list_dir()` on the parent directory
and check the `is_dir` callback parameter, or use `fs_file_size()` (which
returns `-1` for non-existent paths and `0` for empty files, but works
for files only — directories will return the directory's metadata size,
which is platform-dependent).

### Error conditions

| Condition | Behaviour |
|-----------|-----------|
| `rel_path` is `NULL` | Returns `0` |
| `rel_path` is empty | Returns `0` |
| `rel_path` contains `..` | Returns `0` |
| `rel_path` starts with `/` or `\` | Returns `0` |
| Path exists but is not readable | Implementation-dependent — `QFile::exists()` typically returns `true` if the path exists in the directory listing, regardless of read permission |

### Example: conditional initialization

```c
int mmco_init(MMCOContext* ctx)
{
    void* mh = ctx->module_handle;

    if (!ctx->fs_exists(mh, "initialized.flag")) {
        /* First-time setup */
        perform_first_run_setup(ctx);

        /* Write marker file */
        ctx->fs_write(mh, "initialized.flag", "1", 1);
    }

    /* Check for optional features */
    int has_custom_theme = ctx->fs_exists(mh, "themes/custom.css");
    int has_cache = ctx->fs_exists(mh, "cache");

    ctx->log(mh, 0, "Custom theme: %s, Cache: %s",
             has_custom_theme ? "yes" : "no",
             has_cache ? "yes" : "no");

    return 0;
}
```

---

## Complete example: config file reader/writer

This complete example demonstrates a reusable pattern for loading and
saving plugin configuration as JSON text using only sandboxed file I/O:

```c
#include <mmco_sdk.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static MMCOContext* ctx;

#define CONFIG_FILE   "plugin_config.json"
#define MAX_CONFIG_SZ (64 * 1024)  /* 64 KiB max config */

typedef struct {
    int   version;
    int   max_backups;
    int   auto_backup;
    char  backup_dir[512];
} PluginConfig;

static PluginConfig g_config;

static const char* DEFAULT_CONFIG =
    "{\n"
    "  \"version\": 1,\n"
    "  \"max_backups\": 10,\n"
    "  \"auto_backup\": true,\n"
    "  \"backup_dir\": \"backups\"\n"
    "}\n";

int config_load(void)
{
    void* mh = ctx->module_handle;

    /* Defaults */
    g_config.version = 1;
    g_config.max_backups = 10;
    g_config.auto_backup = 1;
    snprintf(g_config.backup_dir, sizeof(g_config.backup_dir), "backups");

    if (!ctx->fs_exists(mh, CONFIG_FILE)) {
        ctx->log(mh, 0, "No config found, writing defaults");
        return ctx->fs_write(mh, CONFIG_FILE,
                             DEFAULT_CONFIG, strlen(DEFAULT_CONFIG));
    }

    char* buf = (char*)malloc(MAX_CONFIG_SZ);
    if (!buf) return -1;

    int64_t n = ctx->fs_read(mh, CONFIG_FILE, buf, MAX_CONFIG_SZ - 1);
    if (n < 0) {
        free(buf);
        ctx->log_error(mh, "Failed to read config");
        return -1;
    }
    buf[n] = '\0';

    /* Parse JSON (simplified — real code would use a JSON library) */
    /* ... sscanf / manual parsing omitted for brevity ... */
    ctx->log(mh, 0, "Config loaded (%lld bytes)", (long long)n);

    free(buf);
    return 0;
}

int config_save(void)
{
    void* mh = ctx->module_handle;

    char buf[2048];
    int len = snprintf(buf, sizeof(buf),
        "{\n"
        "  \"version\": %d,\n"
        "  \"max_backups\": %d,\n"
        "  \"auto_backup\": %s,\n"
        "  \"backup_dir\": \"%s\"\n"
        "}\n",
        g_config.version,
        g_config.max_backups,
        g_config.auto_backup ? "true" : "false",
        g_config.backup_dir);

    if (len < 0 || len >= (int)sizeof(buf))
        return -1;

    int rc = ctx->fs_write(mh, CONFIG_FILE, buf, (size_t)len);
    if (rc != 0) {
        ctx->log_error(mh, "Failed to write config");
        return -1;
    }

    ctx->log(mh, 0, "Config saved (%d bytes)", len);
    return 0;
}

int mmco_init(MMCOContext* c)
{
    ctx = c;
    return config_load();
}
```
