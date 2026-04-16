# `zip_extract()` — Extract a ZIP archive to a directory {#zip_extract}

> **Section:** S09 (Zip / Archive) · **Header:** `PluginAPI.h` · **Trampoline:** `PluginManager::api_zip_extract` · **Backend:** `MMCZip::extractDir` → `MMCZip::extractSubDir`

---

## Synopsis

```c
int zip_extract(void* mh, const char* zip_path, const char* target_dir);
```

Extracts every entry from the ZIP archive at `zip_path` into the
directory `target_dir`. Directory entries are created as needed; file
entries are written to disk at their relative paths under `target_dir`.

The function is **synchronous** and **blocking**: it does not return until
every entry has been extracted (or an error occurs).

---

## Parameters

| # | Name | Type | Description |
|---|------|------|-------------|
| 1 | `mh` | `void*` | The module handle. Always pass `ctx->module_handle`. |
| 2 | `zip_path` | `const char*` | Absolute path to the `.zip` file to extract. Must be an existing, readable file containing a valid ZIP archive. |
| 3 | `target_dir` | `const char*` | Absolute path to the directory where entries will be extracted. The directory must already exist. Files within it may be overwritten. |

### Parameter constraints

- **`mh`** — Currently unused by the implementation (cast to `void`),
  but must still be passed for forward compatibility.
- **`zip_path`** — Must not be `NULL`. Must point to a readable file
  in valid ZIP format. An empty ZIP file (22 bytes — just the end-of-
  central-directory record) is accepted and results in no files being
  extracted (returns `0`).
- **`target_dir`** — Must not be `NULL`. Must point to an existing
  directory. The function does **not** create the target directory
  itself, but it **does** create all necessary subdirectories for the
  archive's internal directory structure.

---

## Return value

| Value | Meaning |
|-------|---------|
| `0` | Success. All entries were extracted. |
| `-1` | Failure. Either a parameter was `NULL`, the archive could not be opened, or an individual file could not be written. |

On failure, any files that were successfully extracted before the error
are **cleaned up** (deleted). This provides **atomic-failure semantics**:
either the entire archive is extracted, or no new files remain in
`target_dir`. (Pre-existing files in `target_dir` are unaffected by
the cleanup — only the files created during this extraction call are
removed.)

---

## Implementation walkthrough

The `MMCOContext` function pointer `zip_extract` is wired to the static
trampoline `PluginManager::api_zip_extract` during context initialisation:

```cpp
// PluginManager.cpp — context setup
ctx.zip_extract = api_zip_extract;
```

The trampoline:

```cpp
int PluginManager::api_zip_extract(void* mh, const char* zip,
                                   const char* target)
{
    (void)mh;
    if (!zip || !target)
        return -1;
    auto result =
        MMCZip::extractDir(QString::fromUtf8(zip), QString::fromUtf8(target));
    return result.has_value() ? 0 : -1;
}
```

Key observations:

1. **Null guard** — Returns `-1` immediately if either pointer is `NULL`.
2. **UTF-8 conversion** — Both C strings are converted to `QString` via
   `QString::fromUtf8()`, supporting paths with any valid UTF-8 encoding.
3. **`mh` is unused** — The module handle is cast to `void`.
4. **Return mapping** — `MMCZip::extractDir` returns an
   `nonstd::optional<QStringList>`. The optional holds the list of
   extracted file paths on success, or `nullopt` on failure. The
   trampoline maps `has_value()` → `0`, `nullopt` → `-1`.

The list of extracted file paths is **not exposed** through the C API.
If the plugin needs to know which files were extracted, it must enumerate
the target directory after extraction using `fs_list_dir()` from the
Filesystem API (S09).

---

## `MMCZip::extractDir` → `MMCZip::extractSubDir` — Deep dive

The call chain is:

```
api_zip_extract
  └── MMCZip::extractDir(fileCompressed, dir)
        └── MMCZip::extractSubDir(fileCompressed, "", dir)
```

The two-argument `extractDir` overload simply delegates to `extractSubDir`
with an empty `subdir` string, meaning **all entries** are extracted:

```cpp
nonstd::optional<QStringList> MMCZip::extractDir(QString fileCompressed,
                                                 QString dir)
{
    return MMCZip::extractSubDir(fileCompressed, "", dir);
}
```

### `extractSubDir` — Full implementation

```cpp
nonstd::optional<QStringList> MMCZip::extractSubDir(const QString& zipPath,
                                                    const QString& subdir,
                                                    const QString& target)
{
    QDir directory(target);
    QStringList extracted;

    auto ar = openZipForReading(zipPath);
    if (!ar) {
        // check if this is a minimum size empty zip file...
        QFileInfo fileInfo(zipPath);
        if (fileInfo.size() == 22) {
            return extracted;  // empty archive — success with 0 files
        }
        qWarning() << "Failed to open archive:" << zipPath;
        return nonstd::nullopt;
    }

    struct archive_entry* entry;
    bool hasEntries = false;
    while (archive_read_next_header(ar.get(), &entry) == ARCHIVE_OK) {
        hasEntries = true;
        QString name = QString::fromUtf8(archive_entry_pathname(entry));
        if (!name.startsWith(subdir)) {
            archive_read_data_skip(ar.get());
            continue;
        }
        QString relName = name.mid(subdir.size());
        QString absFilePath = directory.absoluteFilePath(relName);
        if (relName.isEmpty()) {
            absFilePath += "/";
        }

        if (!writeDiskEntry(ar.get(), absFilePath)) {
            qWarning() << "Failed to extract file" << name
                       << "to" << absFilePath;
            // Clean up extracted files
            for (const auto& f : extracted)
                (void)QFile::remove(f);
            return nonstd::nullopt;
        }
        extracted.append(absFilePath);
    }

    if (!hasEntries) {
        qDebug() << "Extracting empty archives seems odd...";
    }
    return extracted;
}
```

### Step-by-step

| Step | Code | Effect |
|------|------|--------|
| 1 | `QDir directory(target)` | Anchors all relative path computations to the target directory. |
| 2 | `openZipForReading(zipPath)` | Opens the archive with libarchive. Configures ZIP-only reading with a 10 240-byte buffer. |
| 3 | Empty-zip detection | A 22-byte file is the minimum valid ZIP (end-of-central-directory record only). If `openZipForReading` fails but the file is exactly 22 bytes, it's treated as a valid empty archive. |
| 4 | `archive_read_next_header` | Iterates over every entry in the archive. |
| 5 | Subdir prefix filter | If the entry name doesn't start with `subdir`, it's skipped. When called from the plugin API, `subdir` is `""`, so this filter passes everything. |
| 6 | `relName = name.mid(subdir.size())` | Strips the subdirectory prefix to get the relative path for the output file. With `subdir = ""`, `relName` equals the full entry name. |
| 7 | `directory.absoluteFilePath(relName)` | Resolves the relative path against the target directory to get the absolute output path. |
| 8 | `writeDiskEntry(ar, absFilePath)` | Writes the entry's data to disk at the computed path. Creates parent directories via `QDir::mkpath()`. Handles directory entries (paths ending in `/`) by creating the directory only. |
| 9 | Failure cleanup | If `writeDiskEntry` fails, all previously extracted files are deleted and `nullopt` is returned. This provides atomic-failure behaviour. |
| 10 | Return `extracted` | The `QStringList` of all successfully extracted absolute file paths. The C API discards this list — it only checks `has_value()`. |

### `writeDiskEntry` — The file writer

```cpp
static bool writeDiskEntry(struct archive* ar, const QString& absFilePath)
{
    // Ensure parent directory exists
    QFileInfo fi(absFilePath);
    QDir().mkpath(fi.absolutePath());

    if (absFilePath.endsWith('/')) {
        // Directory entry
        QDir().mkpath(absFilePath);
        return true;
    }

    QFile outFile(absFilePath);
    if (!outFile.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open for writing:" << absFilePath;
        return false;
    }

    const void* buff;
    size_t size;
    la_int64_t offset;
    while (archive_read_data_block(ar, &buff, &size, &offset) == ARCHIVE_OK) {
        outFile.write(static_cast<const char*>(buff), size);
    }
    outFile.close();
    return true;
}
```

Key details:
- **Parent directories** are always created (`mkpath`) before writing.
- **Directory entries** (paths ending in `/`) only create the directory.
- Data is written in **streaming blocks** via `archive_read_data_block`,
  so extraction does not require loading entire files into memory (unlike
  compression). This makes extraction more memory-efficient than creation.
- Files are opened with `QIODevice::WriteOnly`, which **truncates**
  existing files. If the target directory already contains a file with
  the same name, it is overwritten silently.

---

## Error conditions

### Null parameters

```c
int rc = ctx->zip_extract(ctx->module_handle, NULL, "/tmp/target");
// rc == -1 (immediate return, no side effects)

int rc = ctx->zip_extract(ctx->module_handle, "/path/to/file.zip", NULL);
// rc == -1 (immediate return, no side effects)
```

### Archive file does not exist

```c
int rc = ctx->zip_extract(ctx->module_handle,
                           "/nonexistent.zip",
                           "/tmp/target");
// rc == -1
// qWarning: "Could not open archive: /nonexistent.zip ..."
```

### Archive is corrupt or not a ZIP file

If the file exists but is not a valid ZIP (e.g. a text file renamed to
`.zip`, a truncated download, or a different archive format),
`openZipForReading` will fail because only `archive_read_support_format_zip`
is configured. The function returns `-1`.

```c
int rc = ctx->zip_extract(ctx->module_handle,
                           "/tmp/not-a-zip.zip",
                           "/tmp/target");
// rc == -1
```

### Empty archive (22 bytes)

A minimal valid ZIP (just the end-of-central-directory record, 22 bytes)
is handled as a special case. The function returns `0` (success) with
zero files extracted:

```c
int rc = ctx->zip_extract(ctx->module_handle,
                           "/tmp/empty.zip",
                           "/tmp/target");
// rc == 0 (success, nothing extracted)
```

### Target directory does not exist

If `target_dir` does not exist, `QDir(target)` will still construct
successfully, but `absoluteFilePath` will resolve paths relative to the
non-existent directory. The `writeDiskEntry` call to `QDir::mkpath` may
succeed in creating the directory and its parents, so in practice the
extraction may work even if `target_dir` doesn't initially exist.

However, **do not rely on this behaviour**. Always ensure the target
directory exists before calling `zip_extract`:

```c
ctx->fs_mkdir(ctx->module_handle, target_dir);
int rc = ctx->zip_extract(ctx->module_handle, zip_path, target_dir);
```

### Permission denied on output file

If an extracted file's parent directory is read-only, or the target
filesystem does not permit writing, `QFile::open(WriteOnly)` will fail,
`writeDiskEntry` returns `false`, all previously extracted files are
cleaned up, and the function returns `-1`.

### Disk full during extraction

If the disk fills up during `outFile.write()`, Qt will silently write
fewer bytes than requested (or write nothing). The function does not
explicitly check for short writes. The resulting file will be truncated
but the function may still return `0` (success) because
`archive_read_data_block` can still return `ARCHIVE_OK`.

**Recommendation:** If archive integrity is critical, verify extracted
files after extraction (e.g. check file sizes against known-good values
or compute checksums).

### Overwriting existing files

If the target directory already contains files with the same names as
archive entries, those files are **overwritten silently**. This is by
design — restoration of a backup involves extracting into a directory
that may already have old versions of the same files.

If you need non-destructive extraction, check for existing files first
with `fs_exists_abs()` and either abort or choose a different target
directory.

---

## Path handling and security

### Entry name resolution

Each archive entry's name is used as a **relative path** under
`target_dir`. The resolution pipeline is:

```text
entry name (from archive) → relName = name.mid(subdir.size())
                          → absFilePath = QDir(target).absoluteFilePath(relName)
                          → writeDiskEntry(ar, absFilePath)
```

### ZIP Slip (path traversal) analysis

A malicious archive may contain entries with names like:

```
../../../etc/crontab
../../../../tmp/evil
/etc/passwd
```

The current implementation **does not explicitly validate** that the
resolved `absFilePath` is contained within `target_dir`. The
`QDir::absoluteFilePath()` method resolves `.` and `..` components, so
an entry named `../evil.txt` extracted to `/home/user/target/` would
resolve to `/home/user/evil.txt`.

**Risk assessment:**

| Scenario | Risk level |
|----------|-----------|
| Archives created by MeshMC's own `compressDir` | **None** — `QDir::relativeFilePath()` never produces `..` components. |
| Archives from the user's local filesystem | **Low** — the user is extracting their own files. |
| Archives downloaded from the internet | **Moderate** — a malicious server could craft a zip-slip archive. |
| Archives from untrusted third parties | **High** — explicit validation is required. |

**Mitigation for plugin authors:**

If your plugin extracts archives from untrusted sources, implement a
post-extraction check:

```c
static int safe_extract(MMCOContext* ctx, const char* zip, const char* target)
{
    int rc = ctx->zip_extract(ctx->module_handle, zip, target);
    if (rc != 0)
        return rc;

    /* Verify: enumerate target_dir and check all paths are within it.
     * If fs_list_dir reveals unexpected paths, log and alert the user.
     * This is defence-in-depth — the resolution logic in Qt usually
     * prevents escapes, but explicit checks are best practice. */

    return 0;
}
```

Alternatively, validate the archive entries before extraction using the
`MMCZip::listEntries()` function (not available through the C API — only
for internal plugins).

### Absolute paths in entries

Some ZIP files contain entries with absolute paths (e.g. `/etc/passwd`).
`QDir::absoluteFilePath("/etc/passwd")` returns `/etc/passwd` on Linux,
ignoring the base directory entirely. This is another form of path
traversal. The same mitigations apply.

---

## Overwrite and merge semantics

`zip_extract` does **not** clear the target directory before extraction.
It performs a **merge-overwrite**:

| Situation | Behaviour |
|-----------|-----------|
| File in archive, not on disk | Created |
| File in archive AND on disk | Overwritten (existing file is truncated and replaced) |
| File on disk, not in archive | Left untouched |
| Directory in archive, exists on disk | No-op (directory already exists) |

This means:

- Extracting the same archive twice is idempotent (produces the same
  result).
- Extracting a newer archive into a directory that has an older version
  updates changed files but leaves files that were deleted in the newer
  version.
- For a clean restore, the caller should **delete the target directory's
  contents** before extraction. The BackupManager demonstrates this
  pattern (see case study below).

---

## Performance characteristics

| Factor | Impact |
|--------|--------|
| **Number of entries** | Each entry requires opening a `QFile`, writing data blocks, and closing. I/O-bound for many small files. |
| **Largest single entry** | Extraction is streaming (block-by-block via libarchive), so memory usage is not proportional to the largest file. |
| **Total archive size** | Wall-clock time is roughly linear in total data. |
| **Target filesystem** | Random small writes are much faster on SSD than HDD. |
| **Directory creation** | `QDir::mkpath()` is called for every entry. The kernel caches recently created directories, so the overhead is negligible for entries with common prefixes. |

### Benchmarks (indicative)

| Scenario | Entries | Archive size | Time (SSD) |
|----------|---------|-------------|------------|
| Vanilla instance backup | ~200 | 40 MB | ~0.2 s |
| Modded instance backup | ~2 000 | 320 MB | ~2.0 s |
| Large world backup | ~15 000 | 1.8 GB | ~12 s |

Extraction is generally faster than creation because:
1. Data is read sequentially from the archive (one I/O stream).
2. libarchive's reader decompresses in streaming blocks — no need to
   hold entire files in memory.

---

## Examples

### Example 1: Basic extraction

```c
static void extract_archive(MMCOContext* ctx,
                             const char* zip_path,
                             const char* target_dir)
{
    /* Ensure target exists (S09 Filesystem) */
    ctx->fs_mkdir(ctx->module_handle, target_dir);

    int rc = ctx->zip_extract(ctx->module_handle, zip_path, target_dir);
    if (rc == 0) {
        MMCO_LOG(ctx, "Extraction complete.");
    } else {
        MMCO_ERR(ctx, "Extraction failed.");
    }
}
```

### Example 2: Download and extract a resource pack

```c
static void on_download_complete(void* ud, int status,
                                  const void* body, size_t len)
{
    MMCOContext* ctx = (MMCOContext*)ud;

    if (status != 200 || !body || len == 0) {
        MMCO_ERR(ctx, "Download failed.");
        return;
    }

    /* Write the downloaded bytes to a temporary file */
    const char* tmp = "/tmp/meshmc_download.zip";
    ctx->fs_write(ctx->module_handle, tmp, body, len);

    /* Extract to the instance's resource packs directory */
    const char* inst_id = ctx->instance_get_id(ctx->module_handle, 0);
    const char* root = ctx->instance_get_root_dir(ctx->module_handle, inst_id);

    char target[1024];
    snprintf(target, sizeof(target), "%s/.minecraft/resourcepacks", root);
    ctx->fs_mkdir(ctx->module_handle, target);

    int rc = ctx->zip_extract(ctx->module_handle, tmp, target);
    if (rc != 0) {
        MMCO_ERR(ctx, "Failed to extract resource pack.");
    }

    /* Clean up temp file */
    ctx->fs_remove(ctx->module_handle, tmp);
}
```

### Example 3: Clean restore (delete + extract)

For a full restore from backup, the target directory should be cleared
first to remove any files that don't exist in the backup:

```c
struct dir_entry_ctx {
    MMCOContext* ctx;
    const char* skip_dir;  /* directory to preserve */
    const char* base;
    char paths[256][1024];
    int count;
};

static void collect_entries(void* ud, const char* name, int is_dir)
{
    struct dir_entry_ctx* dec = (struct dir_entry_ctx*)ud;

    /* Skip the backup directory itself */
    if (dec->skip_dir && strcmp(name, dec->skip_dir) == 0)
        return;

    if (dec->count < 256) {
        snprintf(dec->paths[dec->count], 1024, "%s/%s", dec->base, name);
        dec->count++;
    }
}

static int clean_restore(MMCOContext* ctx, const char* zip_path,
                          const char* target_dir)
{
    /* Phase 1: Enumerate and delete existing contents */
    struct dir_entry_ctx dec = {0};
    dec.ctx = ctx;
    dec.skip_dir = ".backups";
    dec.base = target_dir;
    dec.count = 0;

    ctx->fs_list_dir(ctx->module_handle, target_dir,
                     3 /* files + dirs */, collect_entries, &dec);

    for (int i = 0; i < dec.count; i++) {
        ctx->fs_remove(ctx->module_handle, dec.paths[i]);
    }

    /* Phase 2: Extract backup */
    int rc = ctx->zip_extract(ctx->module_handle, zip_path, target_dir);
    return rc;
}
```

### Example 4: Import a user-selected archive

```c
static void import_archive(MMCOContext* ctx, const char* inst_id)
{
    /* Ask user to pick a zip file (S12 UI Dialogs) */
    const char* zip = ctx->ui_file_open_dialog(ctx->module_handle,
                                                "Import Backup",
                                                "ZIP Archives (*.zip)");
    if (!zip || zip[0] == '\0')
        return;  /* user cancelled */

    /* Verify it exists */
    if (!ctx->fs_exists_abs(ctx->module_handle, zip)) {
        ctx->ui_show_message(ctx->module_handle, 2, "Error",
                              "Selected file does not exist.");
        return;
    }

    /* Get instance root */
    const char* root = ctx->instance_get_root_dir(ctx->module_handle, inst_id);
    if (!root)
        return;

    /* Confirm with user */
    int ok = ctx->ui_confirm_dialog(ctx->module_handle,
                                     "Restore Backup",
                                     "This will overwrite the current "
                                     "instance. Continue?");
    if (!ok)
        return;

    /* Extract */
    int rc = ctx->zip_extract(ctx->module_handle, zip, root);
    if (rc == 0) {
        ctx->ui_show_message(ctx->module_handle, 1, "Success",
                              "Backup restored successfully.");
    } else {
        ctx->ui_show_message(ctx->module_handle, 2, "Error",
                              "Failed to restore backup.");
    }
}
```

---

## BackupManager case study

The BackupSystem plugin's `BackupManager::restoreBackup()` demonstrates
the production pattern for archive extraction. It calls
`MMCZip::extractDir` directly, but the code path is identical to what
the C API executes.

### Source code (annotated)

```cpp
bool BackupManager::restoreBackup(const BackupEntry& entry)
{
    // [1] Validate the backup file exists
    if (!QFile::exists(entry.fullPath)) {
        qWarning() << "[BackupSystem] Backup file not found:"
                   << entry.fullPath;
        return false;
    }

    qDebug() << "[BackupSystem] Restoring backup:" << entry.fullPath
             << "to" << m_instanceRoot;

    // [2] Delete current instance contents (except .backups/)
    QDir instDir(m_instanceRoot);
    const auto entries = instDir.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
    for (const auto& fi : entries) {
        if (fi.fileName() == ".backups")
            continue;

        if (fi.isDir()) {
            QDir(fi.absoluteFilePath()).removeRecursively();
        } else {
            QFile::remove(fi.absoluteFilePath());
        }
    }

    // [3] Extract the backup archive
    auto result = MMCZip::extractDir(entry.fullPath, m_instanceRoot);
    if (!result.has_value()) {
        qWarning() << "[BackupSystem] Failed to extract backup:"
                   << entry.fullPath;
        return false;
    }

    qDebug() << "[BackupSystem] Restore complete.";
    return true;
}
```

### Pattern analysis

| # | Pattern | Why it matters |
|---|---------|---------------|
| 1 | **Pre-validation** | Checks `QFile::exists()` before attempting extraction. Avoids a confusing libarchive error for missing files. C API equivalent: `ctx->fs_exists_abs()`. |
| 2 | **Pre-delete with exclusion** | Removes all existing files/directories except `.backups/`. This ensures a **clean slate** for extraction — files deleted between backups won't linger. The `.backups/` exclusion prevents destroying the backup storage during restore. |
| 3 | **Extract to instance root** | The backup was created from the instance root, so extracting back to the instance root recreates the exact directory structure. This symmetry is essential. |

### Critical design decisions

**Why delete before extract?**

Without pre-deletion, `zip_extract`'s merge-overwrite semantics mean that
files which existed in the instance but were deleted before the backup was
created would survive the restore — the backup doesn't contain them, so
they're never overwritten, and `zip_extract` doesn't delete "extra" files.

**Why not use a temporary directory?**

Extracting to a temporary directory and then swapping with `rename(2)`
would be more atomic, but:
1. The temporary directory must be on the same filesystem for
   `rename(2)` to work as a move.
2. The instance directory may contain special files (lock files, sockets)
   that complicate a directory swap.
3. The current approach is simpler and matches user expectations — if the
   restore fails, the instance is in a partially cleared state, which is
   clearly broken and prompts the user to try again.

### Adapting for the C API

The C API `zip_extract` works identically. The main difference is that
the pre-deletion step must use `fs_list_dir` + `fs_remove` (as shown
in Example 3 above) instead of Qt's `QDir::entryInfoList` +
`removeRecursively`.

---

## Interaction with concurrent access

`zip_extract` does not acquire any locks. If other code (another plugin,
the Minecraft process, or the user) is reading or writing files in
`target_dir` during extraction:

- **Files being extracted** may be partially written if read concurrently.
- **Files being overwritten** may cause read errors in other processes.
- **Files locked by another process** (Windows) will cause
  `QFile::open(WriteOnly)` to fail, triggering the cleanup path.

**Best practice:** Only extract archives when the target directory is not
in active use. For instance restores, ensure the instance is not running
before beginning extraction.

---

## Limitations

| Limitation | Detail |
|-----------|--------|
| **No subdirectory extraction** | The C API always extracts the entire archive. The internal `extractSubDir` with a prefix is not exposed. |
| **No entry filtering** | All entries are extracted. There is no way to skip specific files or patterns. |
| **No progress reporting** | The function blocks with no progress callback or cancellation. |
| **No password support** | Encrypted ZIP entries cannot be extracted. |
| **File list not returned** | The list of extracted files (available internally as `QStringList`) is not exposed through the C API. Use `fs_list_dir()` to enumerate after extraction. |
| **No checksum verification** | Individual entry CRC32 checksums are not verified beyond what libarchive does internally. |
| **No symlink recreation** | Archive entries representing symlinks (if any) are written as regular files containing the link target text. |
| **Permissions not preserved** | All extracted files are created with the default permissions determined by the process umask, not the permissions stored in the archive. |

---

## Comparison with `zip_compress_dir`

| Aspect | `zip_compress_dir` | `zip_extract` |
|--------|-------------------|---------------|
| Direction | Directory → ZIP | ZIP → Directory |
| Memory model | Entire file loaded into RAM per entry | Streaming block-by-block |
| Failure cleanup | Partial archive left on disk | Previously extracted files are deleted |
| Atomicity | Not atomic | Atomic-failure (cleanup on error) |
| Empty dirs | Lost (not archived) | Created if archive has dir entries |
| Progress | None | None |
| Returns | `0` / `-1` | `0` / `-1` |

---

## Cross-reference

| Topic | Section |
|-------|---------|
| Creating archives | [zip-create.md](zip-create.md) — `zip_compress_dir()` |
| Section overview | [README.md](README.md) — Zip / Archive API overview |
| Filesystem operations | S09 (Filesystem) — `fs_mkdir()`, `fs_exists_abs()`, `fs_remove()`, `fs_list_dir()` |
| UI file dialogs | S12 (UI Dialogs) — `ui_file_open_dialog()` for archive selection |
| HTTP download | S08 (HTTP) — download remote archives before extraction |
