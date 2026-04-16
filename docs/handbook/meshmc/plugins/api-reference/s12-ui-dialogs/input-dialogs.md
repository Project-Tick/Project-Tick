# Input Dialogs

> **Section 12 — UI Dialogs API**
>
> `ui_input_dialog()`: modal single-line text input using `QInputDialog`.

---

## ui_input_dialog()

Displays a modal text input dialog and returns the user's input, or `nullptr`
if the user cancels.

### C Signature

```c
const char* (*ui_input_dialog)(void* mh,
                               const char* title,
                               const char* prompt,
                               const char* default_value);
```

### Parameters

| Parameter | Type | Description |
|---|---|---|
| `mh` | `void*` | Module handle (`ctx->module_handle`). Used to store the result in the per-module `tempString`. |
| `title` | `const char*` | Dialog window title. May be `nullptr` (empty title bar). Not prefixed with the plugin name. |
| `prompt` | `const char*` | Label text shown above the input field, describing what the user should enter. May be `nullptr` (no label). |
| `default_value` | `const char*` | Pre-filled text in the input field. May be `nullptr` (empty field). Useful for edit-existing-value flows. |

### Return Value

| Value | Meaning |
|---|---|
| `const char*` (non-null) | The text entered by the user. Valid until the next API call on this module handle (tempString lifetime). |
| `nullptr` | The user clicked **Cancel**, pressed **Escape**, or closed the dialog window. |

### Qt Mapping

```cpp
bool ok = false;
QString result = QInputDialog::getText(
    nullptr,
    title   ? QString::fromUtf8(title)   : QString(),
    prompt  ? QString::fromUtf8(prompt)  : QString(),
    QLineEdit::Normal,
    def     ? QString::fromUtf8(def)     : QString(),
    &ok);
if (!ok)
    return nullptr;
r->tempString = result.toStdString();
return r->tempString.c_str();
```

Key details:

- Uses `QInputDialog::getText()` with `QLineEdit::Normal` echo mode (plain
  text, not password dots).
- The parent widget is `nullptr` (top-level modal dialog).
- The boolean `ok` flag is used to distinguish "user clicked OK with empty
  text" (returns `""`) from "user cancelled" (returns `nullptr`).
- The result is stored in the per-module `tempString`.

### Blocking Behavior

Synchronous and application-modal. The function blocks until the user clicks
OK or Cancel. While visible, all MeshMC windows are disabled.

### Return Value Ownership — The tempString Rule

The returned `const char*` points to the module's internal `tempString` buffer.
This pointer is **invalidated by the next API call** on the same module handle.

**You must copy the string immediately if you need it beyond the next call.**

#### Correct Usage

```c
const char* input = ctx->ui_input_dialog(mh,
    "Rename", "Enter new name:", "default");
if (!input) return;          /* user cancelled */

char* name = strdup(input);  /* COPY before next API call */

/* Now safe to make other API calls */
ctx->setting_set(mh, "player_name", name);
ctx->log_info(mh, "Name updated");

free(name);
```

#### Incorrect Usage

```c
const char* input = ctx->ui_input_dialog(mh,
    "Rename", "Enter new name:", "default");
if (!input) return;

/* BAD: setting_set overwrites tempString */
ctx->setting_set(mh, "old_name", "backup");

/* input now points to garbage — tempString was overwritten */
ctx->setting_set(mh, "new_name", input);  /* UNDEFINED BEHAVIOR */
```

### Empty String vs. Null

The function distinguishes between:

- **User types nothing and clicks OK** → Returns `""` (empty string, non-null).
  `result[0] == '\0'` but `result != nullptr`.
- **User clicks Cancel or Escape** → Returns `nullptr`.

This lets you differentiate "user explicitly confirmed empty input" from "user
aborted the dialog":

```c
const char* input = ctx->ui_input_dialog(mh,
    "Tag", "Enter a tag (leave empty for none):", "");
if (!input) {
    /* User cancelled — do nothing */
    return;
}
if (input[0] == '\0') {
    /* User explicitly confirmed with empty input — clear the tag */
    ctx->setting_set(mh, "tag", "");
} else {
    /* User entered a value */
    char* tag = strdup(input);
    ctx->setting_set(mh, "tag", tag);
    free(tag);
}
```

### Examples

#### Basic Text Input

```c
const char* input = ctx->ui_input_dialog(ctx->module_handle,
    "Greeting", "What is your name?", NULL);
if (input) {
    char msg[256];
    snprintf(msg, sizeof(msg), "Hello, %s!", input);
    /* input is still valid here — no API calls since ui_input_dialog */
    ctx->ui_show_message(ctx->module_handle, 0, "Welcome", msg);
}
```

Note: `input` is used in `snprintf` *before* the next API call
(`ui_show_message`), so it is still valid. The `snprintf` writes to a local
buffer, not through the API.

#### Rename Instance Flow

```c
static void rename_instance(MMCOContext* ctx, const char* id)
{
    void* mh = ctx->module_handle;

    /* Get current name (tempString #1) */
    const char* current = ctx->instance_get_name(mh, id);
    if (!current) return;
    char* old_name = strdup(current);  /* preserve across next call */

    /* Ask for new name with current name as default */
    const char* input = ctx->ui_input_dialog(mh,
        "Rename Instance",
        "Enter the new instance name:",
        old_name);

    if (!input || input[0] == '\0') {
        free(old_name);
        return;  /* cancelled or empty */
    }

    char* new_name = strdup(input);  /* copy before setting */

    int ok = ctx->instance_set_name(mh, id, new_name);
    if (ok == 0) {
        char msg[512];
        snprintf(msg, sizeof(msg), "Renamed \"%s\" to \"%s\".",
                 old_name, new_name);
        ctx->ui_show_message(mh, 0, "Renamed", msg);
    } else {
        ctx->ui_show_message(mh, 2, "Error", "Failed to rename instance.");
    }

    free(old_name);
    free(new_name);
}
```

#### Input Validation Loop

The MMCO API does not provide a built-in input validation mechanism. If you
need to validate user input, re-show the dialog in a loop:

```c
static char* get_valid_port(MMCOContext* ctx)
{
    void* mh = ctx->module_handle;
    char default_val[16] = "25565";

    while (1) {
        const char* input = ctx->ui_input_dialog(mh,
            "Server Port",
            "Enter a port number (1024-65535):",
            default_val);

        if (!input) return NULL;  /* user cancelled */

        /* Copy before any further API calls */
        char* text = strdup(input);

        int port = atoi(text);
        if (port >= 1024 && port <= 65535) {
            return text;  /* caller must free() */
        }

        /* Invalid — prepare for next iteration */
        snprintf(default_val, sizeof(default_val), "%s", text);
        free(text);

        ctx->ui_show_message(mh, 1, "Invalid Port",
            "Port must be between 1024 and 65535.\nPlease try again.");
    }
}
```

#### Password / Secret Input

`ui_input_dialog` uses `QLineEdit::Normal` echo mode, which means text is
visible as typed. There is **no password mode** available through this API. If
you need to collect sensitive input, consider:

1. Using `ui_file_open_dialog` to select a key file instead.
2. Reading secrets from a configuration file via `fs_read`.
3. Building a custom page with Section 13's widget API (which does not
   currently expose a password line edit either).

**Do not collect passwords via `ui_input_dialog`.** The text is fully visible
on screen with no masking.

### Multi-Value Input

Since the API provides only a single text field, collecting multiple values
requires sequential calls:

```c
static int collect_server_info(MMCOContext* ctx,
                               char** out_host, int* out_port)
{
    void* mh = ctx->module_handle;

    /* First: server host */
    const char* host = ctx->ui_input_dialog(mh,
        "Server Setup (1/2)", "Server hostname:", "localhost");
    if (!host) return 0;
    *out_host = strdup(host);

    /* Second: port */
    const char* port_str = ctx->ui_input_dialog(mh,
        "Server Setup (2/2)", "Server port:", "25565");
    if (!port_str) {
        free(*out_host);
        *out_host = NULL;
        return 0;
    }
    *out_port = atoi(port_str);

    return 1;  /* success */
}
```

### Error Conditions

| Condition | Behavior |
|---|---|
| `mh` is invalid | Crash (dereferences invalid pointer for tempString). |
| `title` is `nullptr` | Empty title bar. |
| `prompt` is `nullptr` | No label above input field. |
| `default_value` is `nullptr` | Input field starts empty. |
| User clicks Cancel | Returns `nullptr`. |
| User presses Escape | Returns `nullptr`. |
| User closes window via close button | Returns `nullptr`. |
| User clicks OK with empty field | Returns `""` (non-null empty string). |
| Called from background thread | Undefined behavior. |
| Input contains UTF-8 characters | Fully supported. `QString::fromUtf8` handles multi-byte. |
| Very long input (>4096 chars) | No explicit limit. Constrained by `std::string` capacity (effectively unlimited). |

### Platform Notes

| Platform | Behavior |
|---|---|
| **Linux (X11/Wayland)** | Standard Qt input dialog. Matches system theme if configured. |
| **macOS** | Native-looking Qt dialog. Respects system font and accent color. |
| **Windows** | Qt-styled dialog. Not the native Windows input dialog. |

The dialog is always a Qt dialog, not a native OS dialog. Its appearance adapts
to the active Qt style / theme.

---

*Back to [Section 12 Overview](README.md)*
