# Section 09 — Zip / Archive API

> **Header:** `PluginAPI.h` (function pointer declarations) · **SDK:** `mmco_sdk.h` (struct definition) · **Implementation:** `PluginManager.cpp` (static trampolines) · **Backend:** `MMCZip.h` / `MMCZip.cpp` (libarchive wrapper)

---

## Overview

The Zip / Archive API exposes two synchronous operations that let plugins
**create** and **extract** standard ZIP archives. These are thin wrappers
around MeshMC's internal `MMCZip` utility namespace, which itself is built
on top of **libarchive** — the same library that powers `bsdtar(1)` and
the archive support in many Linux distributions.

Plugins use these functions for any task that involves bundling a directory
tree into a single archive file, or unpacking an archive back to disk.
The canonical use case is the **BackupSystem** plugin, which creates
point-in-time snapshots of Minecraft instances and restores them on demand.

```text
┌──────────────────────────────────────────────────────────────────────┐
│  1. zip_compress_dir(zip_path, dir_path)                            │
│        → compress an entire directory tree into a .zip file          │
│                                                                      │
│  2. zip_extract(zip_path, target_dir)                               │
│        → extract every entry from a .zip into a target directory     │
└──────────────────────────────────────────────────────────────────────┘
```

Both function pointers live in `MMCOContext` and are declared in
`PluginAPI.h`:

```c
/* S10 — Zip / Archive (from MMCOContext in PluginAPI.h) */

int (*zip_compress_dir)(void* mh, const char* zip_path,
                        const char* dir_path);

int (*zip_extract)(void* mh, const char* zip_path,
                   const char* target_dir);
```

> **Naming note:** The section is numbered S10 in the C header comments
> and in `PluginManager.cpp`, but this handbook follows the logical
> documentation ordering where it appears as Section 09 (after HTTP,
> before Filesystem). Both numberings refer to the same two functions.

---

## The MMCZip backend

All archive operations ultimately pass through the `MMCZip` namespace
defined in `MMCZip.h` / `MMCZip.cpp`. The namespace is **not** exposed
directly to plugins — only the two `MMCOContext` function pointers are
available — but understanding the backend is essential for reasoning about
behaviour, performance, and error conditions.

### libarchive foundation

MMCZip uses [libarchive](https://www.libarchive.org/) (via the
`archive.h` C API) rather than zlib or minizip. Key implications:

| Aspect | Detail |
|--------|--------|
| **Format** | ZIP (PKWARE). Set via `archive_write_set_format_zip()` and `archive_read_support_format_zip()`. |
| **Compression** | Deflate (the libarchive ZIP default). |
| **Read buffer** | 10 240 bytes per `archive_read_open_filename()` call. |
| **Permissions** | Written entries are stamped `0644` (`-rw-r--r--`). |
| **Symlinks** | Not followed during compression — `QDirIterator` only yields regular files (`QDir::Files`). Symlink targets are therefore **skipped silently**. |
| **Hidden files** | Included. The iterator uses `QDir::Files | QDir::Hidden`. |
| **Empty directories** | **Not preserved** during compression. Only files are added. On extraction, parent directories are created as needed. |
| **Max entry size** | Bounded by available memory — each file is read into a `QByteArray` in full before being written to the archive. |

### Internal helper functions

`MMCZip.cpp` defines several `static` helpers that back the public API:

```
openZipForReading(path)     → ArchiveReadPtr   (unique_ptr + read_free)
createZipForWriting(path)   → ArchiveWritePtr  (unique_ptr + write_free)
writeFileToArchive(aw, name, data)  → bool
writeDiskEntry(ar, absPath)         → bool
```

- **`openZipForReading`** — Creates a libarchive reader configured for
  ZIP format, opens the file, and returns a RAII smart pointer. Returns
  `nullptr` on failure.
- **`createZipForWriting`** — Creates a libarchive writer for ZIP format
  and opens the output file. Returns `nullptr` on failure.
- **`writeFileToArchive`** — Writes a single entry (name + raw bytes)
  into an open archive writer. Sets filetype to `AE_IFREG` and
  permissions to `0644`.
- **`writeDiskEntry`** — Reads data blocks from the archive reader and
  writes them to an absolute file path on disk. Creates parent
  directories automatically via `QDir::mkpath()`. Handles directory
  entries (paths ending in `/`) by creating the directory only.

### Compression flow (`compressDir`)

```text
createZipForWriting(zipFile)
    │
    ├── QDirIterator(dir, Files | Hidden, Subdirectories)
    │       │
    │       ├── compute relPath = directory.relativeFilePath(it.filePath())
    │       ├── (skip if excludeFilter(relPath) is true)
    │       ├── QFile::readAll() → QByteArray
    │       └── writeFileToArchive(aw, relPath, data)
    │
    └── archive_write_close(aw)
```

The exclude filter is a `std::function<bool(const QString&)>`. When
called through the plugin API, **no filter is provided** (`nullptr`),
meaning every file in the directory tree is archived. The BackupManager
(which calls `MMCZip::compressDir` directly as a Qt plugin, not through
the C API) provides a filter that skips the `.backups/` subdirectory to
avoid recursive self-inclusion.

### Extraction flow (`extractDir` → `extractSubDir`)

```text
openZipForReading(zipPath)
    │
    ├── for each entry:
    │       ├── read entry pathname
    │       ├── (skip if not under subdir prefix)
    │       ├── compute relName = name.mid(subdir.size())
    │       ├── absFilePath = targetDir / relName
    │       └── writeDiskEntry(ar, absFilePath)
    │
    └── return extracted file list (or nullopt on error)
```

When called through the plugin API, `subdir` is always `""` (empty
string), so **all entries** are extracted. The overload that accepts a
subdirectory prefix is only available to internal C++ code.

On extraction failure, all previously extracted files are **cleaned up**
(deleted) before returning `nullopt`. This provides atomic-failure
semantics — either the entire archive is extracted or the target
directory is left unchanged (minus any files that were already there).

---

## Archive format support

| Format | Create | Extract | Notes |
|--------|--------|---------|-------|
| ZIP (PKWARE / Deflate) | Yes | Yes | Only supported format via the plugin API |
| ZIP64 | Partial | Yes | libarchive transparently reads ZIP64; writes ZIP64 extensions only when entry sizes exceed 4 GiB |
| Encrypted ZIP | No | No | libarchive does not support AES/ZipCrypto |
| TAR, 7z, RAR, etc. | No | No | `archive_read_support_format_zip()` restricts the reader to ZIP only |

If a plugin needs to handle `.tar.gz`, `.7z`, or other formats, it must
bundle its own decompression library or delegate to an external tool via
the Filesystem API (S10).

---

## Function summary

| Function | Sub-page | Purpose | Returns |
|----------|----------|---------|---------|
| [`zip_compress_dir()`](zip-create.md#zip_compress_dir) | Create | Compress a directory tree into a ZIP archive | `int` (0 or −1) |
| [`zip_extract()`](zip-extract.md#zip_extract) | Extract | Extract a ZIP archive into a target directory | `int` (0 or −1) |

---

## Synchronous execution model

Unlike the HTTP API (S08), both zip functions are **synchronous** and
**blocking**. They run on the calling thread — which is always the
**main (GUI) thread** in the current MMCO architecture.

Consequences:

- **The UI will freeze** while a large archive is being created or
  extracted. There is no progress callback, cancellation mechanism, or
  background-thread dispatch.
- For small to medium archives (< 100 MB), the operation completes in
  well under a second and the freeze is imperceptible.
- For large archives, consider showing a message to the user before
  starting (via `ui_show_message`) and keeping the operation as
  infrequent as possible.

Future versions of the MMCO API may introduce an asynchronous variant
with progress reporting. The current design favours simplicity.

---

## Security considerations

### Path traversal (Zip Slip)

A malicious archive could contain entries with paths like
`../../../etc/passwd` or absolute paths like `/tmp/evil`. The standard
mitigation — verifying that every extracted path resolves within the
target directory — is handled **implicitly** by the extraction logic:

1. `extractSubDir` computes the relative name by stripping the subdir
   prefix: `relName = name.mid(subdir.size())`.
2. The absolute output path is built with
   `directory.absoluteFilePath(relName)`, where `directory` is a `QDir`
   anchored at the target.
3. `QDir::absoluteFilePath()` canonicalises the path. However, the
   current implementation **does not explicitly reject** `..` components
   after resolution. The practical risk is low because:
   - Archives created by `compressDir` only ever contain relative paths
     produced by `QDir::relativeFilePath()`, which never emits `..`.
   - Archives from untrusted sources (e.g. user-imported backups) go
     through the same code path. Plugin authors who extract user-supplied
     archives should validate the resulting paths independently.

**Recommendation:** If your plugin extracts archives from untrusted
sources, verify after extraction (using S10 Filesystem functions) that no
files were written outside the intended target directory.

### Disk exhaustion

Neither `compressDir` nor `extractDir` checks available disk space
before starting. An archive that decompresses to a very large size can
fill the disk. Plugins should check available space (via platform APIs
or heuristics) before extracting large archives when feasible.

### Temporary file atomicity

`compressDir` writes directly to the target zip path. If the process
crashes or is killed mid-write, a **partial (corrupt) archive** will be
left on disk. The BackupManager shows the robust pattern: after a
failed `compressDir`, it calls `QFile::remove(zipPath)` to clean up.
Plugins should follow the same pattern.

---

## Relationship to other API sections

| Section | Relationship |
|---------|-------------|
| **S09 — Filesystem** | Use `fs_mkdir()` to create target directories before extraction; use `fs_exists_abs()` / `fs_file_size()` to verify archives; use `fs_remove()` to clean up failed archives. |
| **S08 — HTTP** | Download a remote archive with `http_get()`, write it to disk with `fs_write()`, then extract with `zip_extract()`. |
| **S12 — UI Dialogs** | Use `ui_file_open_dialog()` to let the user pick an archive for import; use `ui_file_save_dialog()` for export destinations. |

---

## Quick-start example

A minimal plugin that backs up an instance's `.minecraft` directory:

```c
#include "plugin/sdk/mmco_sdk.h"

static MMCOContext* g_ctx = NULL;

static void do_backup(void* ud)
{
    MMCOContext* ctx = (MMCOContext*)ud;

    /* 1. Resolve paths -------------------------------------------------- */
    const char* inst_id  = ctx->instance_get_id(ctx->module_handle, 0);
    const char* inst_dir = ctx->instance_get_root_dir(ctx->module_handle,
                                                       inst_id);
    char zip_path[512];
    snprintf(zip_path, sizeof(zip_path), "%s/.backups/snapshot.zip", inst_dir);

    /* 2. Ensure the backup folder exists (S09 Filesystem) --------------- */
    char backup_dir[512];
    snprintf(backup_dir, sizeof(backup_dir), "%s/.backups", inst_dir);
    ctx->fs_mkdir(ctx->module_handle, backup_dir);

    /* 3. Create the archive --------------------------------------------- */
    int rc = ctx->zip_compress_dir(ctx->module_handle, zip_path, inst_dir);
    if (rc != 0) {
        ctx->log(ctx->module_handle, 3, "Backup failed!");
        /* Clean up partial file */
        ctx->fs_remove(ctx->module_handle, zip_path);
        return;
    }

    ctx->log(ctx->module_handle, 0, "Backup created successfully.");
}

MMCO_DEFINE_MODULE("QuickBackup", "0.1.0", "Example Author",
                   "Minimal backup example", "MIT");

extern "C" {
MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;
    /* Hook into a menu item, toolbar action, etc. */
    return 0;
}
MMCO_EXPORT void mmco_unload() { g_ctx = NULL; }
}
```

See the sub-pages for full parameter documentation, return-value
semantics, and production-quality examples.

---

## BackupSystem as a case study

The **BackupSystem** plugin (`plugins/BackupSystem/`) is the canonical
real-world consumer of the zip API. Although BackupSystem is compiled as
a Qt plugin and calls `MMCZip::compressDir` / `MMCZip::extractDir`
directly (not through the `MMCOContext` function pointers), the underlying
code path is **identical** — the `MMCOContext` trampolines simply forward
to the same `MMCZip` functions.

Key patterns from BackupManager that every plugin author should study:

1. **Self-exclusion filter** — `compressDir` is called with a filter
   lambda that skips `.backups/` to prevent the backup directory from
   being included in its own archive.

2. **Cleanup on failure** — If `compressDir` returns `false`, the
   partial zip file is immediately deleted with `QFile::remove()`.

3. **Atomic restore** — Before extracting a backup, the restore function
   deletes all existing directory contents (except `.backups/`), then
   calls `extractDir`. If extraction fails, the instance is left in a
   cleared state — this is a design trade-off documented in the plugin.

4. **Timestamp-based naming** — Backup filenames are generated from
   `QDateTime::currentDateTime()` in the format
   `yyyy-MM-dd_HH-mm-ss[_label].zip`, ensuring chronological sort order.

5. **Export / Import** — Exporting is a simple `QFile::copy` of the
   zip to a user-chosen path. Importing copies the external zip into
   the `.backups/` directory. No re-archiving is needed.

See [zip-create.md](zip-create.md) § *BackupManager case study* and
[zip-extract.md](zip-extract.md) § *BackupManager case study* for
line-by-line analysis of the relevant code.
