# File Dialogs

> **Section 12 — UI Dialogs API**
>
> `ui_file_open_dialog()` and `ui_file_save_dialog()`: native OS file pickers
> backed by `QFileDialog`.

---

## ui_file_open_dialog()

Opens a native "Open File" dialog and returns the selected file path, or
`nullptr` if the user cancels.

### C Signature

```c
const char* (*ui_file_open_dialog)(void* mh,
                                   const char* title,
                                   const char* filter);
```

### Parameters

| Parameter | Type | Description |
|---|---|---|
| `mh` | `void*` | Module handle (`ctx->module_handle`). Used for `tempString` storage. |
| `title` | `const char*` | Dialog window title (e.g. `"Open Configuration"`). May be `nullptr` (platform default title). |
| `filter` | `const char*` | File type filter string in Qt format. May be `nullptr` (show all files). See [Filter Format](#filter-format) below. |

### Return Value

| Value | Meaning |
|---|---|
| `const char*` (non-null) | Absolute path to the selected file. Stored in `tempString` — valid until the next API call on this module. |
| `nullptr` | User clicked Cancel, pressed Escape, or closed the dialog. |

### Qt Mapping

```cpp
QString result = QFileDialog::getOpenFileName(
    nullptr,
    title  ? QString::fromUtf8(title)  : QString(),
    QString(),                          /* no initial directory */
    filter ? QString::fromUtf8(filter) : QString());
if (result.isEmpty())
    return nullptr;
r->tempString = result.toStdString();
return r->tempString.c_str();
```

Key details:

- Uses `QFileDialog::getOpenFileName()` — the native file dialog on each
  platform (Windows Explorer dialog, macOS Finder sheet, or GTK/KDE dialog
  on Linux depending on the Qt platform theme).
- The initial directory is **not configurable** — it defaults to the last
  directory used by `QFileDialog` in this application session, or the user's
  home directory.
- The parent is `nullptr` (top-level modal dialog).
- Only a **single file** can be selected. There is no multi-file variant in
  the MMCO API.

### Blocking Behavior

Synchronous and application-modal. Blocks until user selects a file or cancels.

### Examples

#### Open Any File

```c
const char* path = ctx->ui_file_open_dialog(ctx->module_handle,
    "Select a File", NULL);
if (path) {
    char* selected = strdup(path);  /* copy tempString */
    ctx->log_info(ctx->module_handle, selected);
    free(selected);
}
```

#### Open with Filter

```c
const char* path = ctx->ui_file_open_dialog(ctx->module_handle,
    "Open Configuration",
    "JSON Files (*.json);;TOML Files (*.toml);;All Files (*)");
if (path) {
    char* config_path = strdup(path);
    load_config(ctx, config_path);
    free(config_path);
}
```

#### Open Image File

```c
const char* path = ctx->ui_file_open_dialog(ctx->module_handle,
    "Select Instance Icon",
    "Images (*.png *.jpg *.jpeg *.gif *.bmp);;PNG Files (*.png);;All Files (*)");
if (path) {
    char* icon_path = strdup(path);
    /* ... process the image ... */
    free(icon_path);
}
```

#### Import Mod from Disk

```c
static void import_mod(MMCOContext* ctx, const char* instance_id)
{
    void* mh = ctx->module_handle;

    const char* file = ctx->ui_file_open_dialog(mh,
        "Import Mod",
        "Mod Files (*.jar *.zip);;JAR Files (*.jar);;All Files (*)");
    if (!file) return;

    char* mod_path = strdup(file);  /* tempString copy */

    int ret = ctx->mod_install(mh, instance_id, "mods", mod_path);
    if (ret == 0) {
        ctx->ui_show_message(mh, 0, "Imported",
                             "Mod imported successfully.");
    } else {
        ctx->ui_show_message(mh, 2, "Import Failed",
                             "Could not install the mod file.");
    }

    free(mod_path);
}
```

---

## ui_file_save_dialog()

Opens a native "Save File" dialog with an optional default file name and
returns the chosen path, or `nullptr` if the user cancels.

### C Signature

```c
const char* (*ui_file_save_dialog)(void* mh,
                                   const char* title,
                                   const char* default_name,
                                   const char* filter);
```

### Parameters

| Parameter | Type | Description |
|---|---|---|
| `mh` | `void*` | Module handle (`ctx->module_handle`). Used for `tempString` storage. |
| `title` | `const char*` | Dialog window title (e.g. `"Save Backup As"`). May be `nullptr`. |
| `default_name` | `const char*` | Pre-filled file name in the save dialog. May be `nullptr` (empty name field). Can include a path component to suggest an initial directory. |
| `filter` | `const char*` | File type filter string in Qt format. May be `nullptr` (all files). See [Filter Format](#filter-format) below. |

### Return Value

| Value | Meaning |
|---|---|
| `const char*` (non-null) | Absolute path chosen by the user (may or may not exist yet). Stored in `tempString`. |
| `nullptr` | User clicked Cancel, pressed Escape, or closed the dialog. |

### Qt Mapping

```cpp
QString result = QFileDialog::getSaveFileName(
    nullptr,
    title  ? QString::fromUtf8(title)  : QString(),
    def    ? QString::fromUtf8(def)    : QString(),
    filter ? QString::fromUtf8(filter) : QString());
if (result.isEmpty())
    return nullptr;
r->tempString = result.toStdString();
return r->tempString.c_str();
```

Key details:

- Uses `QFileDialog::getSaveFileName()`.
- The `default_name` parameter maps to the `dir` argument of
  `getSaveFileName`. In Qt, this can be:
  - A **file name only** (e.g. `"backup.zip"`) — uses the last-used directory.
  - An **absolute path** (e.g. `"/home/user/backup.zip"`) — opens that
    directory with the file name pre-filled.
  - A **directory path** (e.g. `"/home/user/"`) — opens that directory with
    an empty name field.
- If the user types a name that already exists, Qt may show a native
  "overwrite?" confirmation depending on the platform. This is handled by the
  OS-level dialog, not by the MMCO API.
- Parent is `nullptr`.

### Blocking Behavior

Synchronous and application-modal.

### Overwrite Confirmation

On most platforms, the native save dialog will ask the user to confirm if the
chosen file already exists. This is **not controlled by the MMCO API** — it is
a feature of the OS file dialog. Behavior varies:

| Platform | Overwrite Prompt |
|---|---|
| **Windows** | "File already exists. Do you want to replace it?" |
| **macOS** | "A file with the name ... already exists. Do you want to replace it?" |
| **Linux (GTK)** | "A file named ... already exists. Do you want to replace it?" |
| **Linux (KDE)** | "The file ... already exists. Do you wish to overwrite it?" |

### Examples

#### Save a Configuration File

```c
const char* path = ctx->ui_file_save_dialog(ctx->module_handle,
    "Save Configuration",
    "my_plugin_config.json",
    "JSON Files (*.json);;All Files (*)");
if (path) {
    char* save_path = strdup(path);
    save_config_to(ctx, save_path);
    free(save_path);
}
```

#### Export Backup with Timestamp

```c
static void export_backup(MMCOContext* ctx, const char* instance_id)
{
    void* mh = ctx->module_handle;

    /* Build default name with timestamp */
    const char* name = ctx->instance_get_name(mh, instance_id);
    char default_name[256];
    if (name) {
        char* safe_name = strdup(name);  /* copy tempString */
        snprintf(default_name, sizeof(default_name),
                 "%s_backup.zip", safe_name);
        free(safe_name);
    } else {
        snprintf(default_name, sizeof(default_name), "backup.zip");
    }

    const char* path = ctx->ui_file_save_dialog(mh,
        "Export Backup",
        default_name,
        "Zip Archives (*.zip);;All Files (*)");
    if (!path) return;

    char* zip_path = strdup(path);  /* copy tempString */

    /* Get the instance path for compression */
    const char* inst_path = ctx->instance_get_path(mh, instance_id);
    if (!inst_path) {
        free(zip_path);
        ctx->ui_show_message(mh, 2, "Error",
                             "Could not resolve instance path.");
        return;
    }
    char* src_dir = strdup(inst_path);  /* copy tempString */

    int ret = ctx->zip_compress_dir(mh, zip_path, src_dir);
    if (ret == 0) {
        char msg[512];
        snprintf(msg, sizeof(msg), "Backup saved to:\n%s", zip_path);
        ctx->ui_show_message(mh, 0, "Export Complete", msg);
    } else {
        ctx->ui_show_message(mh, 2, "Export Failed",
                             "Failed to create the backup archive.");
    }

    free(zip_path);
    free(src_dir);
}
```

#### Save with Suggested Directory

Use an absolute path in `default_name` to suggest both the directory and file
name:

```c
/* Get plugin data dir as starting point */
const char* data_dir = ctx->fs_plugin_data_dir(ctx->module_handle);
if (!data_dir) return;
char* dir = strdup(data_dir);  /* copy tempString */

char default_path[1024];
snprintf(default_path, sizeof(default_path), "%s/export.csv", dir);
free(dir);

const char* path = ctx->ui_file_save_dialog(ctx->module_handle,
    "Export Data", default_path,
    "CSV Files (*.csv);;All Files (*)");
```

---

## Filter Format

Both file dialogs accept a `filter` string that controls which files are
visible. The format is the standard Qt filter format:

### Syntax

```
"Description (*.ext1 *.ext2);;Description2 (*.ext3);;All Files (*)"
```

Rules:

1. **Each filter** has a human-readable description followed by extensions in
   parentheses: `Description (*.ext)`.
2. **Multiple extensions** within one filter are space-separated:
   `Images (*.png *.jpg *.gif)`.
3. **Multiple filters** are separated by `;;` (double semicolon):
   `JSON (*.json);;XML (*.xml)`.
4. **The wildcard `*`** matches all files. Convention is to include
   `All Files (*)` as the last option.
5. **Case sensitivity** of extension matching depends on the platform:
   - Windows and macOS: case-insensitive (`*.JSON` matches `file.json`).
   - Linux: case-sensitive by default. Include both cases if needed:
     `Images (*.png *.PNG *.jpg *.JPG)`.

### Common Filter Strings

| Use Case | Filter String |
|---|---|
| All files | `"All Files (*)"` or `NULL` |
| JSON only | `"JSON Files (*.json)"` |
| JSON + fallback | `"JSON Files (*.json);;All Files (*)"` |
| Images | `"Images (*.png *.jpg *.jpeg *.gif *.bmp *.webp)"` |
| Archives | `"Archives (*.zip *.tar.gz *.7z);;Zip Files (*.zip);;All Files (*)"` |
| Mod files | `"Mod Files (*.jar *.zip);;JAR Files (*.jar);;All Files (*)"` |
| Config files | `"Config (*.json *.toml *.yaml *.yml);;All Files (*)"` |
| World saves | `"World Archives (*.zip *.tar.gz *.mcworld);;All Files (*)"` |
| Text files | `"Text Files (*.txt *.log *.md);;All Files (*)"` |

### Filter Displayed to User

The dialog shows a dropdown with each filter as an option. The user can switch
between filters. The first filter in the string is selected by default.

Example:

```c
"Mod Files (*.jar *.zip);;JAR Files (*.jar);;All Files (*)"
/*     ↑ default selection       ↑ option 2        ↑ option 3     */
```

### No Extension Enforcement

The MMCO API does not enforce that saved files have the correct extension. If
the user types `backup` without `.zip`, the path returned will be just
`backup`. If you need a specific extension, check and append it yourself:

```c
const char* path = ctx->ui_file_save_dialog(mh,
    "Save", "backup.zip", "Zip Archives (*.zip)");
if (path) {
    char* save_path = strdup(path);
    size_t len = strlen(save_path);
    /* Append .zip if missing */
    if (len < 4 || strcmp(save_path + len - 4, ".zip") != 0) {
        char* fixed = malloc(len + 5);
        snprintf(fixed, len + 5, "%s.zip", save_path);
        free(save_path);
        save_path = fixed;
    }
    /* ... use save_path ... */
    free(save_path);
}
```

---

## Comparison: Open vs. Save

| Aspect | `ui_file_open_dialog` | `ui_file_save_dialog` |
|---|---|---|
| Qt function | `QFileDialog::getOpenFileName` | `QFileDialog::getSaveFileName` |
| File must exist | Yes (can only select existing files) | No (can type a new name) |
| `default_name` param | Not available | Available (pre-fills name field) |
| Overwrite warning | N/A | Platform-native prompt if file exists |
| Filter support | Yes | Yes |
| Return value | Absolute path or `nullptr` | Absolute path or `nullptr` |
| tempString | Yes | Yes |
| Multiple selection | No | N/A |

---

## Initial Directory Behavior

Neither dialog exposes an explicit "initial directory" parameter. The starting
directory is determined by Qt's internal state:

1. **First call in session** — user's home directory (or platform default).
2. **Subsequent calls** — the directory from the previous file dialog in this
   application session.
3. **`ui_file_save_dialog` with absolute `default_name`** — the directory
   component of that path.

If you need to start in a specific directory, use an absolute path in the save
dialog's `default_name`:

```c
/* Start in the instance's mods directory */
const char* mods_dir = ctx->instance_get_mods_root(mh, instance_id);
if (mods_dir) {
    char start_path[1024];
    snprintf(start_path, sizeof(start_path), "%s/", mods_dir);
    /* Note: mods_dir is tempString, used immediately in snprintf — safe */
    path = ctx->ui_file_save_dialog(mh, "Save", start_path, filter);
}
```

For `ui_file_open_dialog`, there is no way to set the initial directory through
the MMCO API. The dialog will always start in Qt's remembered directory.

---

## Error Conditions

| Condition | Behavior |
|---|---|
| `mh` is invalid | Crash (dereferences invalid module runtime). |
| `title` is `nullptr` | Platform default title (usually "Open" or "Save"). |
| `filter` is `nullptr` | All files shown (no type filtering). |
| `default_name` is `nullptr` (save only) | Empty file name field. |
| User selects a file they cannot read | Path is still returned. The dialog does not check permissions. |
| File path contains Unicode | Fully supported. Returns UTF-8 encoded path. |
| Called from background thread | Undefined behavior. Likely crash. |
| No display server (headless Linux) | Dialog will fail. Qt requires a display. |

### Security Note

The file dialogs return whatever path the user selects. The MMCO sandbox does
**not** restrict which files a plugin can reference through these dialogs. The
path returned can point anywhere on the filesystem. However, the plugin can
only read or write files through the `fs_read`/`fs_write` API (sandboxed to
the plugin data directory) or `fs_exists_abs`/`fs_copy_file` (absolute path
operations). The file dialog itself does not grant any additional file access.

---

## Combined Example: Import / Export Flow

```c
static void handle_menu_action(void* user_data)
{
    MMCOContext* ctx = (MMCOContext*)user_data;
    void* mh = ctx->module_handle;

    int choice = ctx->ui_confirm_dialog(mh,
        "Import or Export?",
        "Would you like to import a configuration?\n\n"
        "Click Yes to import, No to export.");

    if (choice) {
        /* Import */
        const char* file = ctx->ui_file_open_dialog(mh,
            "Import Configuration",
            "Config Files (*.json *.toml);;All Files (*)");
        if (file) {
            char* import_path = strdup(file);
            /* Read and apply the config... */
            ctx->ui_show_message(mh, 0, "Imported",
                "Configuration imported successfully.");
            free(import_path);
        }
    } else {
        /* Export */
        const char* file = ctx->ui_file_save_dialog(mh,
            "Export Configuration",
            "my_config.json",
            "JSON Files (*.json);;All Files (*)");
        if (file) {
            char* export_path = strdup(file);
            /* Write the config... */
            ctx->ui_show_message(mh, 0, "Exported",
                "Configuration exported successfully.");
            free(export_path);
        }
    }
}
```

---

*Back to [Section 12 Overview](README.md)*
