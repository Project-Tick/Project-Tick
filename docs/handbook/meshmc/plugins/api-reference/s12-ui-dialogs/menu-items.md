# Menu Items

> **Section 12 — UI Dialogs API**
>
> `ui_add_menu_item()`: add custom actions to context menus via the
> `MMCOMenuActionCallback` pattern.

---

## ui_add_menu_item()

Inserts an action into a Qt context menu (`QMenu*`) received from a hook event.
When the user clicks the action, the provided callback is invoked on the main
thread.

### C Signature

```c
int (*ui_add_menu_item)(void* mh,
                        void* menu_handle,
                        const char* label,
                        const char* icon_name,
                        MMCOMenuActionCallback callback,
                        void* user_data);
```

### Callback Typedef

```c
typedef void (*MMCOMenuActionCallback)(void* user_data);
```

The callback receives the `user_data` pointer you passed to
`ui_add_menu_item`. It is invoked on the **main (GUI) thread** when the user
clicks the menu item.

### Parameters

| Parameter | Type | Description |
|---|---|---|
| `mh` | `void*` | Module handle (`ctx->module_handle`). Not actively used in the current implementation (cast away), but required by the ABI. |
| `menu_handle` | `void*` | Opaque pointer to a `QMenu`. Obtained from hook payloads (e.g. `MMCO_HOOK_INSTANCE_CONTEXT_MENU`). **Required** — passing `nullptr` returns `-1`. |
| `label` | `const char*` | Text displayed in the menu item. **Required** — passing `nullptr` returns `-1`. UTF-8 encoded. |
| `icon_name` | `const char*` | Theme icon name for the menu item (e.g. `"edit-copy"`, `"document-save"`). Currently **not implemented** — the parameter is accepted but ignored. May be `nullptr`. |
| `callback` | `MMCOMenuActionCallback` | Function to call when the menu item is clicked. **Required** — passing `nullptr` returns `-1`. |
| `user_data` | `void*` | Arbitrary pointer passed through to `callback`. May be `nullptr` if the callback needs no context. See [Lifetime Requirements](#user_data-lifetime-requirements). |

### Return Value

| Value | Meaning |
|---|---|
| `0` | Success. The menu item was added to the menu. |
| `-1` | Failure. One of the required parameters (`menu_handle`, `label`, or `callback`) was `nullptr`. |

### Qt Mapping

```cpp
if (!menu_handle || !label || !cb)
    return -1;

auto* menu = static_cast<QMenu*>(menu_handle);
QString qlabel = QString::fromUtf8(label);

QAction* action = menu->addAction(qlabel);
QObject::connect(action, &QAction::triggered, [cb, ud]() { cb(ud); });

return 0;
```

Key details:

- The `QAction` is added to the end of the menu (after any existing items).
- The `icon_name` parameter is currently **ignored** in the implementation.
  The action is created without an icon. This may change in future ABI
  versions, so always pass a meaningful icon name for forward compatibility.
- The callback is captured in a lambda connected to `QAction::triggered`.
  The lambda captures `cb` and `ud` **by value** (pointer copy), so the
  function pointer and user_data pointer are retained for the lifetime of
  the `QAction`.
- The `QAction` is owned by the `QMenu`, which is typically destroyed after
  the context menu closes.

### Blocking Behavior

`ui_add_menu_item` itself is **non-blocking**. It adds the item to the menu
and returns immediately. The callback fires later, when the user clicks the
item.

However, once the callback fires, it runs on the main thread. If the callback
itself calls blocking dialog functions (like `ui_show_message` or
`ui_confirm_dialog`), those will block as usual.

---

## Where Menu Handles Come From

The `menu_handle` parameter is an opaque `void*` that you receive from hook
event payloads. The most common source is:

### MMCO_HOOK_INSTANCE_CONTEXT_MENU

This hook fires when the user right-clicks an instance in the main instance
list. The payload provides a menu handle and the instance ID:

```c
typedef struct {
    const char* instance_id;    /* ID of the right-clicked instance */
    void*       menu_handle;    /* QMenu* — pass to ui_add_menu_item */
} MMCOInstanceContextMenuPayload;
```

You register for this hook in `mmco_init` and add items in the callback:

```c
static MMCOContext* g_ctx = NULL;

static void on_context_menu(void* user_data, const void* payload)
{
    (void)user_data;
    const MMCOInstanceContextMenuPayload* p =
        (const MMCOInstanceContextMenuPayload*)payload;

    g_ctx->ui_add_menu_item(g_ctx->module_handle,
        p->menu_handle,
        "Backup This Instance",
        "document-save",
        on_backup_clicked,
        (void*)p->instance_id);
}

int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;
    ctx->hook_register(ctx->module_handle,
        MMCO_HOOK_INSTANCE_CONTEXT_MENU,
        on_context_menu, NULL);
    return 0;
}
```

### Other Menu Hooks

Future API versions may add hooks for other menus (e.g. the main menu bar, the
system tray menu). The pattern is the same: the hook payload includes a
`menu_handle` that you pass to `ui_add_menu_item`.

**Never fabricate a `menu_handle`.** It must always come from a hook payload.
Passing an arbitrary pointer will cause undefined behavior (likely a crash when
Qt tries to call methods on a non-QMenu object).

---

## MMCOMenuActionCallback

### Typedef

```c
typedef void (*MMCOMenuActionCallback)(void* user_data);
```

### Semantics

- **Invoked on the main (GUI) thread.** You can safely call any MMCO API
  function from within the callback.
- **Called at most once per menu display.** Each time the context menu is
  shown, new `QAction` objects are created. Clicking one triggers the callback
  once, then the menu closes and the actions are destroyed.
- **Return type is `void`.** There is no way to signal success or failure back
  to the menu system. Handle errors inside the callback (e.g. show an error
  dialog).

### What You Can Do Inside the Callback

Since the callback runs on the main thread, you can call any MMCO API:

```c
static void on_backup_clicked(void* user_data)
{
    const char* instance_id = (const char*)user_data;
    void* mh = g_ctx->module_handle;

    /* All of these are safe inside a menu callback: */
    g_ctx->log_info(mh, "User requested backup");

    int yes = g_ctx->ui_confirm_dialog(mh, "Backup",
        "Create a backup of this instance?");
    if (!yes) return;

    const char* path = g_ctx->instance_get_path(mh, instance_id);
    if (!path) {
        g_ctx->ui_show_message(mh, 2, "Error", "Instance not found.");
        return;
    }
    char* inst_path = strdup(path);

    const char* save = g_ctx->ui_file_save_dialog(mh,
        "Save Backup", "backup.zip",
        "Zip Archives (*.zip);;All Files (*)");
    if (save) {
        char* zip_path = strdup(save);
        g_ctx->zip_compress_dir(mh, zip_path, inst_path);
        free(zip_path);
    }

    free(inst_path);
}
```

---

## user_data Lifetime Requirements

The `user_data` pointer is captured by the `QAction`'s signal-slot connection.
It must remain valid for as long as the menu item could be clicked. In
practice:

### Context Menu Lifetime

For instance context menus, the `QMenu` is **destroyed when it closes**. This
means:

1. The hook fires → you add menu items → the menu displays.
2. The user clicks an item (or clicks away to dismiss the menu).
3. The callback fires (if an item was clicked).
4. The menu is destroyed, along with all `QAction` objects.

The callback fires **before** the menu is destroyed, so `user_data` only needs
to be valid through the callback execution.

### Safe user_data Patterns

#### Pattern 1: Pointer to Hook Payload Data

The hook payload data (like `instance_id`) is valid for the duration of the
hook callback. Since the menu is displayed synchronously during the hook
callback, and the menu item callback fires before the menu closes, the payload
pointers are valid:

```c
static void on_context_menu(void* user_data, const void* payload)
{
    const MMCOInstanceContextMenuPayload* p =
        (const MMCOInstanceContextMenuPayload*)payload;

    /* p->instance_id is valid through the menu's lifetime */
    g_ctx->ui_add_menu_item(g_ctx->module_handle,
        p->menu_handle, "My Action", NULL,
        my_callback, (void*)p->instance_id);
}
```

#### Pattern 2: Static / Global Data

If you need to pass multiple values, use a static or global struct:

```c
static struct {
    MMCOContext* ctx;
    char instance_id[128];
} g_menu_data;

static void on_context_menu(void* user_data, const void* payload)
{
    const MMCOInstanceContextMenuPayload* p =
        (const MMCOInstanceContextMenuPayload*)payload;

    g_menu_data.ctx = (MMCOContext*)user_data;
    strncpy(g_menu_data.instance_id, p->instance_id,
            sizeof(g_menu_data.instance_id) - 1);
    g_menu_data.instance_id[sizeof(g_menu_data.instance_id) - 1] = '\0';

    g_ctx->ui_add_menu_item(g_ctx->module_handle,
        p->menu_handle, "My Action", NULL,
        my_callback, &g_menu_data);
}

static void my_callback(void* user_data)
{
    struct { MMCOContext* ctx; char instance_id[128]; }* data = user_data;
    /* Use data->ctx and data->instance_id */
}
```

#### Pattern 3: Heap-Allocated Data (With Ownership)

If you allocate `user_data` on the heap, the callback must free it:

```c
static void my_callback(void* user_data)
{
    char* id = (char*)user_data;
    /* ... use id ... */
    free(id);  /* callback owns the allocation */
}

static void on_context_menu(void* user_data, const void* payload)
{
    const MMCOInstanceContextMenuPayload* p =
        (const MMCOInstanceContextMenuPayload*)payload;

    char* id_copy = strdup(p->instance_id);
    g_ctx->ui_add_menu_item(g_ctx->module_handle,
        p->menu_handle, "My Action", NULL,
        my_callback, id_copy);
}
```

**Warning:** If the user dismisses the menu without clicking the item, the
callback never fires, and the heap allocation leaks. For context menus, prefer
Pattern 1 or Pattern 2 to avoid this issue. If you must use heap allocation,
consider tracking the allocation in a global and freeing it in the next hook
invocation.

### Unsafe user_data Patterns

```c
/* BAD: stack variable goes out of scope before callback fires */
static void on_context_menu(void* user_data, const void* payload)
{
    const MMCOInstanceContextMenuPayload* p = ...;
    char local_buffer[128];
    strcpy(local_buffer, p->instance_id);

    g_ctx->ui_add_menu_item(g_ctx->module_handle,
        p->menu_handle, "My Action", NULL,
        my_callback, local_buffer);
    /* local_buffer is destroyed when this function returns */
    /* but the menu and callback still reference it! */
}
```

Wait — actually for context menus, the hook callback doesn't return until the
menu closes (the menu is displayed modally during the hook). So stack variables
*are* valid in this case. However, this is an implementation detail that could
change. **Use static or global storage for safety.**

---

## Adding Multiple Menu Items

You can call `ui_add_menu_item` multiple times within the same hook callback to
add several items:

```c
static void on_context_menu(void* user_data, const void* payload)
{
    MMCOContext* ctx = (MMCOContext*)user_data;
    const MMCOInstanceContextMenuPayload* p =
        (const MMCOInstanceContextMenuPayload*)payload;
    void* mh = ctx->module_handle;

    ctx->ui_add_menu_item(mh, p->menu_handle,
        "Backup Instance", "document-save",
        on_backup, (void*)p->instance_id);

    ctx->ui_add_menu_item(mh, p->menu_handle,
        "Restore from Backup", "document-open",
        on_restore, (void*)p->instance_id);

    ctx->ui_add_menu_item(mh, p->menu_handle,
        "View Backup History", "view-list-details",
        on_history, (void*)p->instance_id);
}
```

Items are appended in order. They appear at the end of the menu, after any
items added by MeshMC itself or by other plugins that registered for the same
hook.

### Menu Item Ordering

There is no API to control the position of menu items within the menu. Items
are always appended at the end. If multiple plugins add items, the order
depends on plugin load order, which is not guaranteed.

There is also no API for:

- Adding separators between items.
- Adding submenus.
- Disabling (greying out) menu items.
- Adding checkable (toggle) menu items.
- Removing previously added items.

These features may be added in future ABI versions.

---

## icon_name Parameter

The `icon_name` parameter is declared in the API but **currently ignored** in
the implementation. The `QAction` is created without an icon:

```cpp
QAction* action = menu->addAction(qlabel);
/* icon_name is not used */
```

Nevertheless, you should pass meaningful icon names for forward compatibility.
Use standard [FreeDesktop icon names](https://specifications.freedesktop.org/icon-naming-spec/latest/)
where possible:

| Action | Suggested Icon Name |
|---|---|
| Backup / Save | `"document-save"` |
| Restore / Open | `"document-open"` |
| Delete | `"edit-delete"` |
| Copy | `"edit-copy"` |
| Settings | `"preferences-system"` |
| Info | `"dialog-information"` |
| Refresh | `"view-refresh"` |
| Export | `"document-export"` |

Pass `nullptr` if you have no appropriate icon.

---

## Complete Example: Plugin with Context Menu Integration

```c
#include "mmco_sdk.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static MMCOContext* g_ctx = NULL;

/* ---- Callback implementations ---- */

static void on_backup_clicked(void* user_data)
{
    const char* instance_id = (const char*)user_data;
    void* mh = g_ctx->module_handle;

    g_ctx->log_info(mh, "Backup requested from context menu");

    /* Ask where to save */
    const char* name = g_ctx->instance_get_name(mh, instance_id);
    char default_name[256] = "backup.zip";
    if (name) {
        /* name is tempString — use immediately */
        snprintf(default_name, sizeof(default_name), "%s_backup.zip", name);
    }

    const char* save_path = g_ctx->ui_file_save_dialog(mh,
        "Save Backup", default_name,
        "Zip Archives (*.zip);;All Files (*)");
    if (!save_path) return;
    char* zip = strdup(save_path);

    /* Get instance path */
    const char* ipath = g_ctx->instance_get_path(mh, instance_id);
    if (!ipath) {
        g_ctx->ui_show_message(mh, 2, "Error",
                               "Cannot find instance path.");
        free(zip);
        return;
    }
    char* src = strdup(ipath);

    /* Compress */
    int ret = g_ctx->zip_compress_dir(mh, zip, src);
    if (ret == 0)
        g_ctx->ui_show_message(mh, 0, "Success", "Backup complete!");
    else
        g_ctx->ui_show_message(mh, 2, "Failed", "Backup failed.");

    free(zip);
    free(src);
}

static void on_info_clicked(void* user_data)
{
    const char* instance_id = (const char*)user_data;
    void* mh = g_ctx->module_handle;

    const char* name = g_ctx->instance_get_name(mh, instance_id);
    char* n = name ? strdup(name) : strdup("(unknown)");

    const char* mc = g_ctx->instance_get_mc_version(mh, instance_id);
    char* v = mc ? strdup(mc) : strdup("(unknown)");

    char msg[512];
    snprintf(msg, sizeof(msg),
             "Instance: %s\nMinecraft: %s", n, v);
    g_ctx->ui_show_message(mh, 0, "Instance Info", msg);

    free(n);
    free(v);
}

/* ---- Hook callback ---- */

static void on_instance_context_menu(void* user_data, const void* payload)
{
    (void)user_data;
    const MMCOInstanceContextMenuPayload* p =
        (const MMCOInstanceContextMenuPayload*)payload;
    void* mh = g_ctx->module_handle;

    g_ctx->ui_add_menu_item(mh, p->menu_handle,
        "Quick Backup", "document-save",
        on_backup_clicked, (void*)p->instance_id);

    g_ctx->ui_add_menu_item(mh, p->menu_handle,
        "Instance Info", "dialog-information",
        on_info_clicked, (void*)p->instance_id);
}

/* ---- Module entry points ---- */

MMCO_MODULE_INFO {
    .name = "ContextMenuDemo",
    .version = "1.0.0",
    .description = "Adds backup and info items to the instance context menu.",
    .abi_version = MMCO_ABI_VERSION
};

int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;

    ctx->hook_register(ctx->module_handle,
        MMCO_HOOK_INSTANCE_CONTEXT_MENU,
        on_instance_context_menu, NULL);

    ctx->log_info(ctx->module_handle,
                  "ContextMenuDemo loaded — right-click an instance!");
    return 0;
}

void mmco_unload(MMCOContext* ctx)
{
    ctx->hook_unregister(ctx->module_handle,
        MMCO_HOOK_INSTANCE_CONTEXT_MENU,
        on_instance_context_menu);
    g_ctx = NULL;
}
```

---

## Error Conditions

| Condition | Behavior |
|---|---|
| `menu_handle` is `nullptr` | Returns `-1`. |
| `label` is `nullptr` | Returns `-1`. |
| `callback` is `nullptr` | Returns `-1`. |
| `icon_name` is `nullptr` | Accepted. Item has no icon (no-op anyway since icon is currently ignored). |
| `user_data` is `nullptr` | Accepted. Callback receives `nullptr`. |
| `menu_handle` is not a valid `QMenu*` | Undefined behavior. Likely crash in `QMenu::addAction`. |
| Callback calls blocking dialogs | Works correctly — dialogs are modal on the main thread. |
| Called outside a menu hook | The menu_handle would be stale/invalid. Undefined behavior. |
| Called from background thread | Undefined behavior. |

---

*Back to [Section 12 Overview](README.md)*
