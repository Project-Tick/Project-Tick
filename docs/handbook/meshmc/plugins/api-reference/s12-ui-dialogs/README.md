# Section 12: UI Dialogs API

> **MMCO Plugin API Reference — Section 12**
>
> Modal dialog functions for user interaction: messages, confirmations,
> text input, file selection, and menu item registration.

---

## Overview

Section 12 of the `MMCOContext` provides plugins with a set of **pre-built
dialog functions** that let you interact with the user through native Qt dialog
windows. These are the simplest form of UI available to plugins — no widget
tree construction is needed, and every call is **synchronous and modal**: the
function blocks the calling thread until the user responds.

The six function pointers in this section are:

| Function Pointer | Purpose | Returns |
|---|---|---|
| `ui_show_message` | Display an informational / warning / error message | `void` |
| `ui_confirm_dialog` | Ask a Yes / No question | `int` (1 or 0) |
| `ui_input_dialog` | Prompt for a single line of text | `const char*` or `nullptr` |
| `ui_file_open_dialog` | Native "Open File" picker | `const char*` or `nullptr` |
| `ui_file_save_dialog` | Native "Save File" picker | `const char*` or `nullptr` |
| `ui_add_menu_item` | Insert an item into a context menu | `int` (0 or −1) |

Additionally, Section 12 contains `ui_register_instance_action`, which adds a
toolbar button to the main instance view. That function is documented in
[Section 11: Instance Pages](../s11-instance-pages/README.md) because it is
conceptually part of instance-level UI integration.

---

## Dialog Types at a Glance

### One-Shot Notifications — `ui_show_message()`

Fire-and-forget. Displays a message box and blocks until the user clicks OK.
Three severity levels are supported: **info** (0), **warning** (1), and
**error** (2). The dialog title is automatically prefixed with the plugin's
display name so the user always knows which plugin originated the message.

[Full documentation →](message-dialogs.md#ui_show_message)

### Yes / No Confirmation — `ui_confirm_dialog()`

Presents a question with **Yes** and **No** buttons. Returns `1` for Yes and
`0` for No. The default-focused button is **No**, which protects the user from
accidentally confirming destructive operations.

[Full documentation →](message-dialogs.md#ui_confirm_dialog)

### Text Input — `ui_input_dialog()`

Opens a single-line text input dialog with an optional default value. Returns
the entered string on OK, or `nullptr` if the user cancels. The returned
pointer is a `tempString` — valid until the next API call on the same module.

[Full documentation →](input-dialogs.md)

### File Pickers — `ui_file_open_dialog()` / `ui_file_save_dialog()`

Native OS file dialogs (backed by `QFileDialog`). Both return a full absolute
file path as a `tempString`, or `nullptr` on cancel. The save dialog
additionally accepts a default file name.

[Full documentation →](file-dialogs.md)

### Menu Items — `ui_add_menu_item()`

Inserts an action into a `QMenu` handle received from a hook event. The plugin
provides a label, an optional icon name, a callback function, and a `user_data`
pointer. The callback fires on the main thread when the user clicks the item.

[Full documentation →](menu-items.md)

---

## Modality

Every dialog function in Section 12 is **application-modal**. This means:

1. **The calling thread blocks** until the user dismisses the dialog.
2. **All other windows in MeshMC are disabled** while the dialog is open.
3. The user cannot interact with the launcher behind the dialog — they must
   respond to the dialog first.

This is the safest default for plugin dialogs because it prevents re-entrant
API calls while a dialog is visible. It also means you should **never** call a
dialog function from a callback that itself is running on a time-critical path
(e.g. inside an HTTP callback). Dialog calls must be made from the main thread
or from a context where blocking is acceptable.

### Thread Safety

All Section 12 functions **must be called from the main (GUI) thread**. In
practice this means:

- From `mmco_init()` — which runs on the main thread.
- From hook callbacks — which are dispatched on the main thread.
- From menu action callbacks — which fire on the main thread.
- From UI widget callbacks (buttons, tree selections) — main thread.

Calling a dialog function from a background thread (e.g. inside an
`MMCOHttpCallback`) is **undefined behavior** and will likely crash. If you
need to show a dialog in response to an async event, store the result and
present the dialog from the next hook invocation or via `QMetaObject::invokeMethod`
(which is not exposed in the MMCO API — design your flow accordingly).

---

## Parent Window

All dialog functions pass `nullptr` as the Qt parent widget. This means:

- Dialogs appear as **top-level windows** centered on the screen.
- They are not attached to any specific MeshMC window.
- On some platforms (notably macOS), top-level modal dialogs may appear above
  all application windows but will not be tied to a specific one.

This is intentional: the MMCO API does not expose a `QWidget*` for the main
window, so plugins cannot — and should not — assume a specific parent.

---

## The `tempString` Pattern

Three functions in this section return `const char*` pointers:
`ui_file_open_dialog`, `ui_file_save_dialog`, and `ui_input_dialog`. These
returned strings follow the **tempString** lifetime rule used throughout the
MMCO API:

> The returned pointer is valid until the **next API call** on the same module
> handle. If you need the value longer, copy it immediately.

Internally, each module runtime has a single `std::string tempString` member.
When a function returns a string result, it writes into `tempString` and
returns `.c_str()`. Any subsequent API call that writes to `tempString` will
invalidate the previous pointer.

**Safe pattern:**

```c
const char* path = ctx->ui_file_open_dialog(ctx->module_handle,
                                            "Open Config", "*.json");
if (path) {
    /* Copy immediately before any other API call */
    char* my_path = strdup(path);

    /* Now safe to call other API functions */
    ctx->log_info(ctx->module_handle, my_path);

    free(my_path);
}
```

**Dangerous pattern:**

```c
const char* path = ctx->ui_file_open_dialog(ctx->module_handle,
                                            "Open Config", "*.json");
if (path) {
    /* BAD: log_info overwrites tempString, invalidating 'path' */
    ctx->log_info(ctx->module_handle, "User chose a file");
    printf("Path: %s\n", path);  /* undefined — path may be garbage */
}
```

---

## Title Prefixing

For `ui_show_message()`, the dialog title is automatically prefixed with the
plugin's display name in the format `[PluginName] Your Title`. This helps
users identify which plugin is showing the dialog. The other dialog functions
(`ui_confirm_dialog`, `ui_input_dialog`, `ui_file_open_dialog`,
`ui_file_save_dialog`) pass the title through to Qt unchanged.

---

## Null Parameter Handling

All functions in this section are defensive about `nullptr` parameters:

| Parameter | Behavior when `nullptr` |
|---|---|
| `title` | Passed as empty `QString()` — dialog has no title |
| `msg` / `message` / `prompt` | Passed as empty `QString()` — blank body |
| `filter` | No file type filter — all files shown |
| `default_name` / `default_value` | No default text pre-filled |
| `label` (menu item) | Returns `-1` — label is required |
| `callback` (menu item) | Returns `-1` — callback is required |
| `menu_handle` | Returns `-1` — menu handle is required |

---

## Sub-Pages

| File | Contents |
|---|---|
| [message-dialogs.md](message-dialogs.md) | `ui_show_message()`, `ui_confirm_dialog()` |
| [input-dialogs.md](input-dialogs.md) | `ui_input_dialog()` |
| [file-dialogs.md](file-dialogs.md) | `ui_file_open_dialog()`, `ui_file_save_dialog()` |
| [menu-items.md](menu-items.md) | `ui_add_menu_item()` with `MMCOMenuActionCallback` |

---

## Quick Example — Combined Preferences Flow

A common plugin pattern is to present a small "preferences" flow using only
Section 12 dialogs (without building a full page via Section 13):

```c
static void show_preferences(MMCOContext* ctx)
{
    void* mh = ctx->module_handle;

    /* 1. Ask for a display name */
    const char* name = ctx->ui_input_dialog(mh,
        "Preferences", "Enter your display name:", "Player");
    if (!name) return;              /* user cancelled */
    char* saved_name = strdup(name); /* copy before next call */

    /* 2. Ask for a data directory */
    const char* dir = ctx->ui_file_open_dialog(mh,
        "Select Data Folder", nullptr);
    if (!dir) { free(saved_name); return; }
    char* saved_dir = strdup(dir);

    /* 3. Confirm */
    int ok = ctx->ui_confirm_dialog(mh, "Confirm",
        "Save these preferences?");
    if (ok) {
        ctx->setting_set(mh, "display_name", saved_name);
        ctx->setting_set(mh, "data_dir", saved_dir);
        ctx->ui_show_message(mh, 0, "Preferences",
                             "Preferences saved successfully.");
    }

    free(saved_name);
    free(saved_dir);
}
```

This pattern chains multiple dialog calls safely by copying `tempString`
results before the next API call.

---

*Next section: [Section 13 — UI Page Builder](../s13-ui-page-builder/README.md)*
