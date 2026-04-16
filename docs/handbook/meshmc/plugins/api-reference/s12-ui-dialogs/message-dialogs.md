# Message Dialogs

> **Section 12 — UI Dialogs API**
>
> `ui_show_message()` and `ui_confirm_dialog()`: modal message boxes for
> notifications and Yes/No confirmations.

---

## ui_show_message()

Displays a modal message dialog and blocks until the user clicks **OK**.

### C Signature

```c
void (*ui_show_message)(void* mh, int type, const char* title, const char* msg);
```

### Parameters

| Parameter | Type | Description |
|---|---|---|
| `mh` | `void*` | Module handle (`ctx->module_handle`). Used to look up the plugin's display name for title prefixing. |
| `type` | `int` | Message severity. Determines the icon shown in the dialog. See the [type values](#message-type-values) table below. |
| `title` | `const char*` | Dialog window title. Automatically prefixed with `[PluginName] `. May be `nullptr` (results in `[PluginName] `). |
| `msg` | `const char*` | Message body text. May be `nullptr` (blank body). Supports plain text only — no HTML or rich formatting. |

### Return Value

None (`void`). The function returns only after the user dismisses the dialog.

### Message Type Values

| Value | Constant | Qt Class | Icon | Use For |
|---|---|---|---|---|
| `0` | (default) | `QMessageBox::information()` | ℹ️ Blue circle | Status updates, success messages, general notifications |
| `1` | — | `QMessageBox::warning()` | ⚠️ Yellow triangle | Non-critical problems, deprecation notices, recoverable issues |
| `2` | — | `QMessageBox::critical()` | ❌ Red circle | Fatal errors, unrecoverable failures, data loss warnings |

Any value other than `1` or `2` is treated as `0` (information).

### Title Prefixing

The dialog title is formatted as:

```
[PluginName] <title>
```

Where `PluginName` is the `name` field from the plugin's `MMCOModuleInfo`
(the value set in `mmco_module_info.name`). This is done automatically by the
implementation and cannot be suppressed. For example, if a plugin named
"BackupSystem" calls:

```c
ctx->ui_show_message(mh, 0, "Status", "Backup complete.");
```

The user sees a dialog titled **`[BackupSystem] Status`** with the body
**Backup complete.**

### Qt Mapping

Internally, `ui_show_message` maps directly to static `QMessageBox` methods:

```cpp
switch (type) {
    case 1:  QMessageBox::warning(nullptr, qtitle, qmsg);     break;
    case 2:  QMessageBox::critical(nullptr, qtitle, qmsg);    break;
    default: QMessageBox::information(nullptr, qtitle, qmsg); break;
}
```

The parent is always `nullptr`, making the dialog a top-level application-modal
window. The dialog has a single **OK** button.

### Blocking Behavior

`ui_show_message` is **synchronous and modal**. The function does not return
until the user clicks OK (or presses Enter / Escape). While the dialog is
visible:

- All MeshMC windows are disabled (greyed out).
- No hook callbacks or timer events will fire.
- The plugin's execution is paused at the call site.

### Thread Safety

Must be called from the **main (GUI) thread** only. Calling from a background
thread is undefined behavior.

### Examples

#### Information Message

```c
void mmco_init(MMCOContext* ctx)
{
    ctx->ui_show_message(ctx->module_handle, 0,
        "Welcome",
        "MyPlugin v1.0 loaded successfully.\n"
        "Configure settings in the Plugins menu.");
}
```

#### Warning Message

```c
static void on_pre_launch(void* user_data, const void* payload)
{
    MMCOContext* ctx = (MMCOContext*)user_data;
    const MMCOPreLaunchPayload* p = (const MMCOPreLaunchPayload*)payload;

    const char* mc = ctx->instance_get_mc_version(ctx->module_handle,
                                                  p->instance_id);
    if (mc && strcmp(mc, "1.20.1") < 0) {
        ctx->ui_show_message(ctx->module_handle, 1,
            "Version Warning",
            "This plugin is designed for Minecraft 1.20.1+.\n"
            "Some features may not work on older versions.");
    }
}
```

#### Error Message

```c
int64_t bytes = ctx->fs_read(mh, "config.json", buf, sizeof(buf));
if (bytes < 0) {
    ctx->ui_show_message(mh, 2,
        "Configuration Error",
        "Failed to read config.json.\n"
        "Please check file permissions and try again.");
    return;
}
```

#### Multiline Messages

The message body supports newline characters (`\n`). Qt will wrap long lines
automatically, but explicit newlines give you control over the layout:

```c
ctx->ui_show_message(mh, 0, "Report",
    "Backup Summary\n"
    "==============\n"
    "Instances backed up: 5\n"
    "Total size: 142 MB\n"
    "Duration: 3.2 seconds");
```

### Error Conditions

| Condition | Behavior |
|---|---|
| `mh` is invalid | Crash (dereferences garbage pointer). Always pass `ctx->module_handle`. |
| `title` is `nullptr` | Title becomes `[PluginName] ` (just the prefix). |
| `msg` is `nullptr` | Blank dialog body — only the OK button is shown. |
| `type` is out of range | Treated as `0` (information). |
| Called from background thread | Undefined behavior. Likely crash. |

---

## ui_confirm_dialog()

Displays a modal Yes / No confirmation dialog and returns the user's choice.

### C Signature

```c
int (*ui_confirm_dialog)(void* mh, const char* title, const char* message);
```

### Parameters

| Parameter | Type | Description |
|---|---|---|
| `mh` | `void*` | Module handle (`ctx->module_handle`). Not actively used by the implementation (cast away), but must be valid for ABI consistency. |
| `title` | `const char*` | Dialog window title. **Not** prefixed with the plugin name (unlike `ui_show_message`). May be `nullptr`. |
| `message` | `const char*` | The question or prompt text. May be `nullptr`. |

### Return Value

| Value | Meaning |
|---|---|
| `1` | User clicked **Yes**. |
| `0` | User clicked **No**, pressed Escape, or closed the dialog. |

### Qt Mapping

```cpp
auto ret = QMessageBox::question(
    nullptr,
    title ? QString::fromUtf8(title) : QString(),
    msg ? QString::fromUtf8(msg) : QString(),
    QMessageBox::Yes | QMessageBox::No,
    QMessageBox::No);
return ret == QMessageBox::Yes ? 1 : 0;
```

Key details:

- Uses `QMessageBox::question()` which shows a **?** icon.
- Two buttons: **Yes** and **No**.
- The **default button is No** (`QMessageBox::No` is the last argument). This
  means pressing Enter without explicitly clicking Yes will return `0`. This is
  a safety measure for destructive confirmations.
- The parent is `nullptr` (top-level modal dialog).

### Blocking Behavior

Same as `ui_show_message`: synchronous, application-modal, blocks until the
user makes a choice.

### Title Differences from ui_show_message

Unlike `ui_show_message`, the confirm dialog does **not** prefix the title with
the plugin name. The `title` parameter is passed directly to `QMessageBox::question`.
If you want the plugin name in the title, include it yourself:

```c
char title_buf[256];
snprintf(title_buf, sizeof(title_buf), "[MyPlugin] Confirm Delete");
int yes = ctx->ui_confirm_dialog(mh, title_buf,
    "Are you sure you want to delete this backup?");
```

### Examples

#### Simple Confirmation

```c
int yes = ctx->ui_confirm_dialog(ctx->module_handle,
    "Delete Instance",
    "Are you sure you want to delete this instance?\n"
    "This action cannot be undone.");
if (yes) {
    ctx->instance_delete(ctx->module_handle, instance_id);
    ctx->ui_show_message(ctx->module_handle, 0,
        "Deleted", "Instance deleted successfully.");
} else {
    ctx->log_info(ctx->module_handle, "User cancelled deletion.");
}
```

#### Guard Pattern for Destructive Operations

A common pattern wraps destructive operations in a confirm guard:

```c
static int confirm_and_delete_world(MMCOContext* ctx, const char* inst_id,
                                    int world_idx)
{
    const char* name = ctx->world_get_name(ctx->module_handle,
                                           inst_id, world_idx);
    if (!name) return -1;

    char msg[512];
    snprintf(msg, sizeof(msg),
             "Delete world \"%s\"?\n\n"
             "All world data will be permanently removed.", name);

    if (!ctx->ui_confirm_dialog(ctx->module_handle, "Confirm Delete", msg))
        return 0;  /* user said no */

    return ctx->world_delete(ctx->module_handle, inst_id, world_idx);
}
```

#### Chained Confirmations

For especially dangerous operations, you might chain two confirmations:

```c
static int confirm_factory_reset(MMCOContext* ctx)
{
    void* mh = ctx->module_handle;

    if (!ctx->ui_confirm_dialog(mh, "Factory Reset",
            "This will delete ALL plugin data.\nContinue?"))
        return 0;

    if (!ctx->ui_confirm_dialog(mh, "Are You Sure?",
            "This is your last chance to cancel.\n"
            "All settings and saved data will be lost permanently."))
        return 0;

    /* User confirmed twice — proceed */
    return 1;
}
```

#### Using the Return Value in Conditionals

The return value is designed to work naturally in C conditionals:

```c
/* Direct if-check (idiomatic) */
if (ctx->ui_confirm_dialog(mh, "Save", "Save changes before closing?")) {
    save_config(ctx);
}

/* Negated check */
if (!ctx->ui_confirm_dialog(mh, "Discard", "Discard unsaved changes?")) {
    return;  /* user said No, abort */
}

/* Store for later use */
int user_wants_backup = ctx->ui_confirm_dialog(mh,
    "Backup", "Create a backup before updating?");
/* ... other logic ... */
if (user_wants_backup) {
    perform_backup(ctx);
}
```

### Error Conditions

| Condition | Behavior |
|---|---|
| `mh` is invalid | No crash (parameter is cast away), but maintain valid handle for ABI. |
| `title` is `nullptr` | Dialog has no title (empty title bar). |
| `message` is `nullptr` | Blank dialog body — only Yes/No buttons shown. |
| User closes dialog via window close button | Returns `0` (same as No). |
| User presses Escape | Returns `0` (same as No). |
| User presses Enter without clicking | Returns `0` (No is default). |
| Called from background thread | Undefined behavior. |

---

## Comparison: ui_show_message vs. ui_confirm_dialog

| Aspect | `ui_show_message` | `ui_confirm_dialog` |
|---|---|---|
| Purpose | Notification | Decision |
| Buttons | OK | Yes, No |
| Return | `void` | `int` (1/0) |
| Icon | Info / Warning / Error | Question mark |
| Title prefix | `[PluginName] title` | `title` (no prefix) |
| Default action | OK (Enter) | No (Enter) |
| Qt class | `QMessageBox::information/warning/critical` | `QMessageBox::question` |

---

## Combined Example: Backup with Confirmation

```c
static void perform_backup(MMCOContext* ctx, const char* instance_id)
{
    void* mh = ctx->module_handle;

    /* Get instance name for display */
    const char* name = ctx->instance_get_name(mh, instance_id);
    if (!name) {
        ctx->ui_show_message(mh, 2, "Error", "Invalid instance.");
        return;
    }

    /* Build confirmation message */
    char msg[512];
    snprintf(msg, sizeof(msg),
             "Create a backup of \"%s\"?\n"
             "This may take a few minutes.", name);

    /* Ask for confirmation */
    if (!ctx->ui_confirm_dialog(mh, "Backup Instance", msg)) {
        ctx->log_info(mh, "Backup cancelled by user.");
        return;
    }

    /* Perform the backup (example — uses zip API) */
    const char* path = ctx->instance_get_path(mh, instance_id);
    if (!path) {
        ctx->ui_show_message(mh, 2, "Error",
                             "Could not resolve instance path.");
        return;
    }
    char* inst_path = strdup(path);  /* copy tempString */

    char zip_path[1024];
    snprintf(zip_path, sizeof(zip_path), "%s/../%s_backup.zip",
             inst_path, instance_id);

    int ret = ctx->zip_compress_dir(mh, zip_path, inst_path);
    free(inst_path);

    if (ret == 0) {
        ctx->ui_show_message(mh, 0, "Success",
                             "Backup created successfully!");
    } else {
        ctx->ui_show_message(mh, 2, "Backup Failed",
                             "An error occurred while creating the backup.\n"
                             "Check logs for details.");
    }
}
```

This example demonstrates the typical flow: confirm with `ui_confirm_dialog`,
perform an operation, then report the result with `ui_show_message` (info for
success, error for failure). Note the `strdup(path)` call to preserve the
instance path across subsequent API calls — the `tempString` would be
overwritten by `zip_compress_dir`.

---

*Back to [Section 12 Overview](README.md)*
