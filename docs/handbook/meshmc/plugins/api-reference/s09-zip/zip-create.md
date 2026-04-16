# `zip_compress_dir()` — Create a ZIP archive from a directory {#zip_compress_dir}

> **Section:** S09 (Zip / Archive) · **Header:** `PluginAPI.h` · **Trampoline:** `PluginManager::api_zip_compress_dir` · **Backend:** `MMCZip::compressDir`

---

## Synopsis

```c
int zip_compress_dir(void* mh, const char* zip_path, const char* dir_path);
```

Compresses the entire contents of the directory at `dir_path` into a new
ZIP archive written to `zip_path`. The archive contains **all regular
files** (including hidden files) found recursively under `dir_path`, with
their paths stored **relative to `dir_path`**.

The function is **synchronous** and **blocking**: it does not return until
the archive has been fully written (or an error occurs).

---

## Parameters

| # | Name | Type | Description |
|---|------|------|-------------|
| 1 | `mh` | `void*` | The module handle. Always pass `ctx->module_handle`. |
| 2 | `zip_path` | `const char*` | Absolute path where the output `.zip` file will be created. If a file already exists at this path, it is **overwritten** silently. Parent directories must already exist. |
| 3 | `dir_path` | `const char*` | Absolute path to the source directory whose contents will be archived. Must be an existing directory. |

### Parameter constraints

- **`mh`** — Currently unused by the implementation (the function casts
  it to `void` and ignores it), but must still be passed for forward
  compatibility.
- **`zip_path`** — Must not be `NULL`. The path must be writable by the
  current process. The filename extension is irrelevant to the format —
  the file will always contain a valid ZIP stream regardless of the
  extension, but `.zip` is conventional.
- **`dir_path`** — Must not be `NULL`. Must point to an existing
  directory that is readable by the current process. If the directory
  is empty, the result is an empty (but valid) ZIP file.

---

## Return value

| Value | Meaning |
|-------|---------|
| `0` | Success. The archive was created and closed cleanly. |
| `-1` | Failure. Either a parameter was `NULL`, the source directory does not exist, a file could not be read, or the archive could not be written. |

The function does **not** provide a detailed error code or error message
through the return value. Diagnostic information is written to MeshMC's
internal log via `qWarning()` / `qDebug()`, but is not accessible to the
plugin through the API. Use `log()` at severity `MMCO_LOG_WARN` (3) to
record your own context before and after the call.

---

## Implementation walkthrough

The `MMCOContext` function pointer `zip_compress_dir` is wired to the
static trampoline `PluginManager::api_zip_compress_dir` during context
initialisation:

```cpp
// PluginManager.cpp — context setup
ctx.zip_compress_dir = api_zip_compress_dir;
```

The trampoline itself is minimal:

```cpp
int PluginManager::api_zip_compress_dir(void* mh, const char* zip,
                                        const char* dir)
{
    (void)mh;
    if (!zip || !dir)
        return -1;
    bool ok = MMCZip::compressDir(QString::fromUtf8(zip),
                                  QString::fromUtf8(dir), nullptr);
    return ok ? 0 : -1;
}
```

Key observations:

1. **Null guard** — The function returns `-1` immediately if either
   string pointer is `NULL`.
2. **UTF-8 conversion** — Both C strings are converted to `QString`
   via `QString::fromUtf8()`. This means the paths can contain any
   UTF-8 characters (including non-ASCII directory names).
3. **No exclude filter** — The third argument to `MMCZip::compressDir`
   is `nullptr`, meaning **every** file under the directory is included.
   There is no way to specify an exclude filter through the C API.
4. **`mh` is unused** — The module handle is cast to `void`. The
   function is entirely stateless.

---

## `MMCZip::compressDir` — Deep dive

The backend function that does the actual work:

```cpp
bool MMCZip::compressDir(QString zipFile, QString dir,
                         FilterFunction excludeFilter)
{
    auto aw = createZipForWriting(zipFile);
    if (!aw)
        return false;

    QDir directory(dir);
    if (!directory.exists())
        return false;

    QDirIterator it(dir, QDir::Files | QDir::Hidden,
                    QDirIterator::Subdirectories);
    bool success = true;
    while (it.hasNext()) {
        it.next();
        QString relPath = directory.relativeFilePath(it.filePath());
        if (excludeFilter && excludeFilter(relPath))
            continue;

        QFile file(it.filePath());
        if (!file.open(QIODevice::ReadOnly)) {
            success = false;
            break;
        }
        QByteArray data = file.readAll();
        file.close();

        if (!writeFileToArchive(aw.get(), relPath, data)) {
            success = false;
            break;
        }
    }

    archive_write_close(aw.get());
    return success;
}
```

### Step-by-step

| Step | Code | Effect |
|------|------|--------|
| 1 | `createZipForWriting(zipFile)` | Opens (or creates) the output file and initialises a libarchive ZIP writer. On failure (e.g. permission denied, invalid path), returns `nullptr` → function returns `false` → API returns `-1`. |
| 2 | `QDir(dir).exists()` | Validates that the source directory exists. If not, returns `false` immediately. The archive file created in step 1 may be left as a zero-byte file. |
| 3 | `QDirIterator` | Recursively enumerates all regular files (including hidden) under `dir`. **Symlinks are not followed** — only `QDir::Files` is specified, not `QDir::FollowSymlinks`. **Directories are not enumerated** — they are implied by the file paths. |
| 4 | `relativeFilePath()` | Computes the relative path of each file with respect to the source directory. This is what gets stored as the entry name inside the ZIP. Example: if `dir` = `/home/user/.minecraft` and the file is `/home/user/.minecraft/saves/World1/level.dat`, the entry name will be `saves/World1/level.dat`. |
| 5 | `excludeFilter(relPath)` | If a filter is provided and returns `true`, the file is skipped. When called from the plugin API, the filter is always `nullptr`, so this check is bypassed. |
| 6 | `QFile::readAll()` | The entire file is read into memory at once. This means peak memory usage is proportional to the size of the largest single file in the directory. |
| 7 | `writeFileToArchive()` | Writes the entry header (name, size, type `AE_IFREG`, permissions `0644`) and then the data. On failure, the loop breaks and `success` is set to `false`. |
| 8 | `archive_write_close()` | Flushes and closes the ZIP file. Always called, even on failure, because `aw` is a `unique_ptr` and would clean up anyway, but explicit close ensures the ZIP central directory is written (or at least attempted). |

### What gets archived

| Item | Included? | Notes |
|------|-----------|-------|
| Regular files | **Yes** | All regular files recursively |
| Hidden files (`.dotfiles`) | **Yes** | `QDir::Hidden` flag is set |
| Symlinks | **No** | `QDirIterator` does not follow or list symlinks when only `QDir::Files` is requested |
| Empty directories | **No** | Only files are enumerated; empty dirs have no representative entry |
| Special files (sockets, FIFOs, devices) | **No** | `QDir::Files` excludes them |
| The directory itself | **No** | Only contents, not the root directory entry |

### Archive entry structure

Entries are stored with **forward-slash separated** relative paths
(Qt's `relativeFilePath()` uses `/` on all platforms). A typical archive
produced from a Minecraft instance directory looks like:

```
options.txt
saves/
saves/MyWorld/level.dat
saves/MyWorld/region/r.0.0.mca
saves/MyWorld/region/r.0.-1.mca
mods/
mods/sodium-0.6.0.jar
config/
config/sodium-options.json
```

Note that directory entries (like `saves/`) are **not** explicitly
created — they are implicit from the file paths. Some archive tools may
show them as separate entries, but the ZIP produced by `compressDir`
does not contain explicit directory entries.

---

## Error conditions

### Null parameters

```c
int rc = ctx->zip_compress_dir(ctx->module_handle, NULL, "/some/dir");
// rc == -1 (immediate return, no side effects)

int rc = ctx->zip_compress_dir(ctx->module_handle, "/out.zip", NULL);
// rc == -1 (immediate return, no side effects)
```

### Source directory does not exist

```c
int rc = ctx->zip_compress_dir(ctx->module_handle,
                                "/tmp/out.zip",
                                "/nonexistent/path");
// rc == -1
// Side effect: /tmp/out.zip may be created as a zero-byte file
//   by createZipForWriting() before the directory check fails.
```

**Warning:** In this case a zero-byte file may be left at `zip_path`.
Always check the return value and clean up on failure:

```c
int rc = ctx->zip_compress_dir(ctx->module_handle, zip_path, dir_path);
if (rc != 0) {
    ctx->fs_remove(ctx->module_handle, zip_path);
}
```

### Output path not writable

If the output path is in a read-only directory, on a full filesystem, or
otherwise not writable, `createZipForWriting` will fail, the function
prints a `qWarning()`, and returns `-1`. No partial file is left in this
case because `archive_write_open_filename` fails before any data is
written.

### File read error during iteration

If a file within the source directory cannot be opened (e.g. permission
denied, file deleted between enumeration and read), the loop breaks
immediately with `success = false`. The archive is closed
(`archive_write_close`) but will contain only the entries that were
successfully written before the error. The **partial archive** is left
on disk.

**Recommendation:** Always check the return value and delete the archive
on failure.

### Disk full during write

If the disk fills up during `archive_write_data`, libarchive will return
an error, `writeFileToArchive` returns `false`, and the function returns
`-1`. A partial archive is left on disk.

### Very large files

Since each file is read entirely into a `QByteArray` before compression,
attempting to archive a file larger than available RAM will cause memory
allocation failure. On most systems this manifests as an `std::bad_alloc`
exception that propagates up and crashes the application. Avoid archiving
directories known to contain multi-gigabyte files unless sufficient memory
is available.

---

## Path handling

### Absolute vs. relative paths

Both `zip_path` and `dir_path` must be **absolute paths**. The API does
not resolve relative paths against any base directory. Using a relative
path will cause `QDir(dir)` to resolve against the process's current
working directory, which is unpredictable in a GUI application.

**Always construct absolute paths** using the instance root, plugin data
directory, or other known anchors:

```c
const char* root = ctx->instance_get_root_dir(ctx->module_handle, inst_id);
char zip[512], dir[512];
snprintf(zip, sizeof(zip), "%s/.backups/snap.zip", root);
snprintf(dir, sizeof(dir), "%s", root);
```

### Cross-platform path separators

Qt's `QDir` and `QDirIterator` handle both `/` and `\` transparently on
all platforms. However:

- **Inside the archive**, entry names always use `/` (forward slash),
  which is the ZIP standard.
- **On disk**, paths use the platform separator. The conversion is
  handled internally by Qt.
- **In the C API**, always use `/` in paths passed to `zip_compress_dir`
  and `zip_extract`. Qt will translate as needed.

### Special characters in paths

Paths are converted via `QString::fromUtf8()`, so any valid UTF-8
sequence is accepted. Paths containing spaces, Unicode characters, or
characters outside the ASCII printable range are fully supported. The
only characters that are problematic are those forbidden by the
underlying filesystem (e.g. `NUL`, or `<>:"|?*` on Windows).

---

## Performance characteristics

| Factor | Impact |
|--------|--------|
| **Number of files** | Each file requires a separate `QFile::open` + `readAll` + `writeFileToArchive`. I/O dominates for directories with many small files. |
| **Largest single file** | Peak memory ≈ size of largest file × 2 (raw + compressed in libarchive's buffer). |
| **Total directory size** | Wall-clock time is roughly linear in total data size. |
| **Disk I/O** | Both the source directory and the output file are accessed sequentially. SSD vs. HDD makes a large difference for random reads of many small files. |
| **Compression ratio** | Minecraft text files (JSON, NBT-text) compress well. Region files (`.mca`) and mod JARs (already compressed) compress poorly. |

### Benchmarks (indicative)

| Scenario | Files | Total size | Time (SSD) | Archive size |
|----------|-------|-----------|------------|-------------|
| Vanilla instance, 1 world | ~200 | 45 MB | ~0.3 s | ~40 MB |
| Modded instance, 50 mods | ~2 000 | 350 MB | ~2.5 s | ~320 MB |
| Large world, extensive history | ~15 000 | 2 GB | ~15 s | ~1.8 GB |

These are rough figures — actual times depend on hardware, filesystem,
and data compressibility.

---

## Examples

### Example 1: Basic directory archival

```c
static void archive_instance(MMCOContext* ctx, const char* inst_id)
{
    const char* root = ctx->instance_get_root_dir(ctx->module_handle, inst_id);
    if (!root) {
        MMCO_ERR(ctx, "Failed to get instance root");
        return;
    }

    /* Build the output path */
    char zip_path[1024];
    snprintf(zip_path, sizeof(zip_path), "%s/instance_backup.zip", root);

    /* Create the archive */
    MMCO_LOG(ctx, "Creating archive...");
    int rc = ctx->zip_compress_dir(ctx->module_handle, zip_path, root);

    if (rc == 0) {
        MMCO_LOG(ctx, "Archive created successfully.");
    } else {
        MMCO_ERR(ctx, "Archive creation failed — cleaning up.");
        ctx->fs_remove(ctx->module_handle, zip_path);
    }
}
```

### Example 2: Backup with timestamped filename

```c
#include <time.h>
#include <stdio.h>

static void create_timestamped_backup(MMCOContext* ctx, const char* inst_id)
{
    const char* root = ctx->instance_get_root_dir(ctx->module_handle, inst_id);
    if (!root)
        return;

    /* Generate a timestamped filename */
    time_t now = time(NULL);
    struct tm* tm = localtime(&now);
    char fname[256];
    strftime(fname, sizeof(fname), "%Y-%m-%d_%H-%M-%S.zip", tm);

    /* Ensure the backup directory exists (S09 Filesystem) */
    char backup_dir[1024];
    snprintf(backup_dir, sizeof(backup_dir), "%s/.backups", root);
    ctx->fs_mkdir(ctx->module_handle, backup_dir);

    /* Build full path */
    char zip_path[1024];
    snprintf(zip_path, sizeof(zip_path), "%s/%s", backup_dir, fname);

    /* Compress */
    int rc = ctx->zip_compress_dir(ctx->module_handle, zip_path, root);
    if (rc != 0) {
        MMCO_ERR(ctx, "Backup failed");
        ctx->fs_remove(ctx->module_handle, zip_path);
        return;
    }

    /* Log the result size (S09 Filesystem) */
    int64_t size = ctx->fs_file_size(ctx->module_handle, zip_path);
    char msg[512];
    snprintf(msg, sizeof(msg), "Backup created: %s (%" PRId64 " bytes)",
             fname, size);
    MMCO_LOG(ctx, msg);
}
```

### Example 3: Archive a subdirectory only

The plugin API archives the **entire** directory. To archive only a
subdirectory (e.g. just `saves/`), pass the subdirectory as `dir_path`:

```c
static void archive_saves_only(MMCOContext* ctx, const char* inst_id)
{
    const char* root = ctx->instance_get_root_dir(ctx->module_handle, inst_id);
    if (!root)
        return;

    /* Source: only the saves/ subdirectory */
    char saves_dir[1024];
    snprintf(saves_dir, sizeof(saves_dir), "%s/.minecraft/saves", root);

    /* Output path */
    char zip_path[1024];
    snprintf(zip_path, sizeof(zip_path), "%s/saves_backup.zip", root);

    int rc = ctx->zip_compress_dir(ctx->module_handle, zip_path, saves_dir);
    if (rc != 0) {
        ctx->fs_remove(ctx->module_handle, zip_path);
    }
}
```

In this case, the archive entries will be relative to `saves/`:
```
World1/level.dat
World1/region/r.0.0.mca
World2/level.dat
...
```

### Example 4: Pre-launch automatic backup (hook integration)

```c
static int on_pre_launch(void* mh, uint32_t hook_id, void* payload, void* ud)
{
    (void)hook_id;
    MMCOContext* ctx = (MMCOContext*)ud;
    MMCOLaunchEvent* evt = (MMCOLaunchEvent*)payload;

    if (!evt || !evt->instance_id)
        return 0;

    const char* root = ctx->instance_get_root_dir(mh, evt->instance_id);
    if (!root)
        return 0;

    /* Backup before every launch */
    time_t now = time(NULL);
    struct tm* tm = localtime(&now);
    char fname[256];
    strftime(fname, sizeof(fname), "pre-launch_%Y%m%d_%H%M%S.zip", tm);

    char backup_dir[1024];
    snprintf(backup_dir, sizeof(backup_dir), "%s/.backups", root);
    ctx->fs_mkdir(mh, backup_dir);

    char zip_path[1024];
    snprintf(zip_path, sizeof(zip_path), "%s/%s", backup_dir, fname);

    int rc = ctx->zip_compress_dir(mh, zip_path, root);
    if (rc != 0) {
        ctx->fs_remove(mh, zip_path);
        ctx->log(mh, 2, "Pre-launch backup failed (non-fatal).");
    } else {
        ctx->log(mh, 0, "Pre-launch backup created.");
    }

    return 0; /* allow launch to proceed */
}
```

---

## BackupManager case study

The BackupSystem plugin's `BackupManager::createBackup()` method
demonstrates the production-quality pattern for archive creation.
Although it calls `MMCZip::compressDir` directly (not through the C API),
the underlying code path is identical.

### Source code (annotated)

```cpp
BackupEntry BackupManager::createBackup(const QString& label)
{
    // [1] Ensure the backup output directory exists
    ensureBackupDir();

    // [2] Generate a unique, sortable filename
    QString fileName = generateFileName(label);
    QString zipPath = QDir(m_backupDir).filePath(fileName);

    qDebug() << "[BackupSystem] Creating backup:" << zipPath
             << "from" << m_instanceRoot;

    // [3] Compress the entire instance directory, EXCLUDING .backups/
    bool ok = MMCZip::compressDir(zipPath, m_instanceRoot,
                                  [this](const QString& entry) -> bool {
                                      return entry.startsWith(".backups/") ||
                                             entry.startsWith(".backups\\");
                                  });

    // [4] On failure, clean up the partial archive
    if (!ok) {
        qWarning() << "[BackupSystem] Failed to create backup:" << zipPath;
        QFile::remove(zipPath);
        return {};
    }

    // [5] Return metadata about the created backup
    qDebug() << "[BackupSystem] Backup created successfully:" << zipPath;
    return entryFromFile(zipPath);
}
```

### Pattern analysis

| # | Pattern | Why it matters |
|---|---------|---------------|
| 1 | **Create output dir first** | `compressDir` does not create parent directories for the zip file. If `.backups/` doesn't exist, `createZipForWriting` will fail. |
| 2 | **Timestamp-based filenames** | Ensures uniqueness and natural chronological ordering. The format `yyyy-MM-dd_HH-mm-ss[_label].zip` sorts correctly in directory listings. |
| 3 | **Exclude filter** | Prevents recursive inclusion of previous backups. The filter checks for both `/` and `\` separators for cross-platform safety. **This filter is not available through the C API** — C API plugins must use a separate directory for backups or archive only a subdirectory. |
| 4 | **Cleanup on failure** | A partial archive is useless and potentially confusing. Removing it immediately is the robust choice. |
| 5 | **Return rich metadata** | The `entryFromFile` helper reads back the file's size, modification time, and parses the label from the filename. This is a good pattern for any plugin that manages multiple archives. |

### Adapting for the C API

Since the C API does not support exclude filters, C API plugins should
use one of these strategies to avoid archiving their own backup files:

**Strategy A: Separate backup directory**
```c
/* Store backups outside the instance directory */
const char* data_dir = ctx->fs_plugin_data_dir(ctx->module_handle);
char backup_dir[1024];
snprintf(backup_dir, sizeof(backup_dir), "%s/backups/%s", data_dir, inst_id);
ctx->fs_mkdir(ctx->module_handle, backup_dir);
```

**Strategy B: Archive only the .minecraft subdirectory**
```c
/* Archive only the game data, not the instance root */
char mc_dir[1024];
snprintf(mc_dir, sizeof(mc_dir), "%s/.minecraft", root);
ctx->zip_compress_dir(ctx->module_handle, zip_path, mc_dir);
```

---

## Limitations

| Limitation | Detail |
|-----------|--------|
| **No exclude filter** | The C API always passes `nullptr` as the filter. All files are included. |
| **No progress reporting** | The function blocks until complete with no way to monitor progress or cancel. |
| **No compression level control** | libarchive's default deflate settings are used. Cannot switch to store-only or maximum compression. |
| **No password/encryption** | libarchive does not support ZIP encryption. |
| **No streaming** | Each file is loaded entirely into memory before compression. |
| **No append mode** | Calling `zip_compress_dir` with an existing `zip_path` overwrites the file — it does not add entries to an existing archive. |
| **Empty directories lost** | Only regular files are archived. Empty directories cease to exist in the archive. |
| **Symlinks skipped** | Symbolic links are silently ignored. |

---

## Cross-reference

| Topic | Section |
|-------|---------|
| Extracting archives | [zip-extract.md](zip-extract.md) — `zip_extract()` |
| Section overview | [README.md](README.md) — Zip / Archive API overview |
| Filesystem operations | S09 (Filesystem) — `fs_mkdir()`, `fs_remove()`, `fs_file_size()` |
| HTTP download → archive | S08 (HTTP) — download a remote zip, then extract |
| UI file dialogs | S12 (UI Dialogs) — `ui_file_save_dialog()` for export paths |
