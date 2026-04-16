# UI Hooks

> `MMCO_HOOK_UI_MAIN_READY` · `MMCO_HOOK_UI_INSTANCE_PAGES` · `MMCO_HOOK_UI_CONTEXT_MENU`

UI hooks let plugins extend the MeshMC graphical interface — adding custom pages
to instance dialogs, injecting items into context menus, and performing
setup that depends on the main window being fully constructed.

All UI hooks are dispatched on the **main (GUI) thread** and are
**non-cancellable**.

---

## `MMCO_HOOK_UI_MAIN_READY`

### Summary

| Property | Value |
|---|---|
| **Constant** | `MMCO_HOOK_UI_MAIN_READY` |
| **Hex value** | `0x0600` |
| **Payload** | `nullptr` |
| **Cancellable** | No |
| **Thread** | Main (GUI) thread |
| **Fires** | Exactly once, when the main window is fully visible |

### When it fires

Dispatched at the end of `MainWindow`'s initialization, after all toolbars,
menus, views, and plugin-registered instance actions have been set up.

**Exact dispatch location** — `MainWindow.cpp`:

```cpp
// ... toolbar and plugin instance actions set up ...

APPLICATION->pluginManager()->dispatchHook(MMCO_HOOK_UI_MAIN_READY);
```

At the point this hook fires:

- The `MainWindow` is fully constructed and **visible** on screen.
- All built-in toolbars, menus, and views are in place.
- Plugin-registered instance actions (via `ui_register_instance_action()`) have
  been added to the instance toolbar.
- The instance list view is populated.
- The status bar is active.
- `APP_INITIALIZED` has **already fired** (it fires before the window is shown).

### Payload

`nullptr`.  Do not dereference the `payload` parameter.

### Return value

The launcher ignores the return value.  Callbacks should return `0`.

### `APP_INITIALIZED` vs. `UI_MAIN_READY`

| Property | `APP_INITIALIZED` | `UI_MAIN_READY` |
|---|---|---|
| **When** | After all `mmco_init()` calls | After `MainWindow` is fully shown |
| **UI available** | No | Yes |
| **Instance list** | Loaded | Loaded and displayed |
| **Use** | Non-UI initialization | UI-dependent setup |

If your plugin only needs the MMCO API (logging, settings, hooks), register in
`APP_INITIALIZED`.  If your plugin needs to interact with the UI or assumes
the main window exists, use `UI_MAIN_READY`.

### Use cases

- **Deferred UI initialization** — create floating widgets, dialogs, or
  overlays that depend on the main window.
- **Status bar messages** — display a plugin status indicator.
- **UI state inspection** — query the selected instance, window geometry, etc.
- **Welcome messages** — show a first-run dialog after the UI is visible.

### Example — Log when UI is ready

```cpp
static int on_ui_ready(void* mh, uint32_t hook_id,
                       void* payload, void* ud)
{
    MMCO_LOG(g_ctx, "Main window is ready — UI operations are now safe.");
    return 0;
}

MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;
    ctx->hook_register(ctx->module_handle,
                       MMCO_HOOK_UI_MAIN_READY,
                       on_ui_ready, nullptr);
    return 0;
}
```

### Example — Register for UI hooks only after the window is ready

```cpp
static int on_context_menu(void* mh, uint32_t hook_id,
                           void* payload, void* ud);  /* forward decl */

static int on_ui_ready(void* mh, uint32_t hook_id,
                       void* payload, void* ud)
{
    /* Now safe to register hooks that interact with UI elements */
    g_ctx->hook_register(g_ctx->module_handle,
                         MMCO_HOOK_UI_CONTEXT_MENU,
                         on_context_menu, nullptr);

    MMCO_LOG(g_ctx, "Context menu hook registered.");
    return 0;
}

MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;
    ctx->hook_register(ctx->module_handle,
                       MMCO_HOOK_UI_MAIN_READY,
                       on_ui_ready, nullptr);
    return 0;
}
```

### Notes

- This hook fires **once**.  It does not fire again if the main window is
  hidden and re-shown, or if a new window is created.
- Registering for `UI_MAIN_READY` in `mmco_init()` is the standard pattern.
  Registering in `APP_INITIALIZED` also works (the hook fires later than both).
- Heavy UI setup in this callback will delay the window from becoming
  interactive.  Keep it brief.

---

## `MMCO_HOOK_UI_INSTANCE_PAGES`

### Summary

| Property | Value |
|---|---|
| **Constant** | `MMCO_HOOK_UI_INSTANCE_PAGES` |
| **Hex value** | `0x0602` |
| **Payload** | `MMCOInstancePagesEvent*` |
| **Cancellable** | No |
| **Thread** | Main (GUI) thread |
| **Fires** | Each time an instance settings/edit dialog is opened |

### When it fires

Dispatched in `InstancePageProvider` (declared in `InstancePageProvider.h`)
during the construction of the page list for an instance's edit dialog.

**Exact dispatch location** — `InstancePageProvider.h`:

```cpp
// Let plugins add their own instance pages
if (APPLICATION->pluginManager()) {
    QByteArray idUtf8 = inst->id().toUtf8();
    QByteArray nameUtf8 = inst->name().toUtf8();
    QByteArray pathUtf8 = inst->instanceRoot().toUtf8();
    MMCOInstancePagesEvent evt{};
    evt.instance_id = idUtf8.constData();
    evt.instance_name = nameUtf8.constData();
    evt.instance_path = pathUtf8.constData();
    evt.page_list_handle = &values;
    evt.instance_handle = inst.get();
    APPLICATION->pluginManager()->dispatchHook(
        MMCO_HOOK_UI_INSTANCE_PAGES, &evt);
}

return values;
```

At the point this hook fires:

- The instance edit dialog is being constructed.
- All built-in pages (Version, Mods, Settings, Screenshots, Logs, etc.) have
  already been added to the `values` list.
- The plugin can append additional pages to the end of the list.
- The dialog has **not** been shown to the user yet — pages are still being
  assembled.

### Payload — `MMCOInstancePagesEvent`

```cpp
struct MMCOInstancePagesEvent {
    const char* instance_id;       /* Instance being edited */
    const char* instance_name;     /* Instance display name */
    const char* instance_path;     /* Instance root directory */
    void*       page_list_handle;  /* Opaque: QList<BasePage*>* */
    void*       instance_handle;   /* Opaque: InstancePtr raw pointer */
};
```

#### Field reference

| Field | Type | Nullable | Allocator | Lifetime | Description |
|---|---|---|---|---|---|
| `instance_id` | `const char*` | No | Launcher (stack `QByteArray`) | Callback duration | The unique ID of the instance whose dialog is being built. |
| `instance_name` | `const char*` | No | Launcher (stack `QByteArray`) | Callback duration | The display name of the instance. |
| `instance_path` | `const char*` | No | Launcher (stack `QByteArray`) | Callback duration | Absolute filesystem path to the instance root. |
| `page_list_handle` | `void*` | No | Launcher (stack variable) | Callback duration | Opaque pointer to `QList<BasePage*>*`.  Cast and `append()` to add pages. |
| `instance_handle` | `void*` | No | Launcher (shared_ptr raw) | Callback duration | Opaque pointer to the raw `BaseInstance*`.  Use to construct a non-owning `InstancePtr`. |

#### `page_list_handle` — adding pages

This is a pointer to a `QList<BasePage*>` on the caller's stack.  To add a
custom page:

```cpp
auto* pages = static_cast<QList<BasePage*>*>(evt->page_list_handle);
pages->append(new MyCustomPage(/* ... */));
```

The page you append must be a subclass of `BasePage` (provided by the SDK
header `mmco_sdk.h`).  It will be owned by the dialog and deleted when the
dialog closes.

#### `instance_handle` — constructing a non-owning `InstancePtr`

The `instance_handle` is a raw `BaseInstance*`.  The instance is owned by the
launcher's `InstanceList`, not by this pointer.  To use it with APIs that expect
`InstancePtr` (a `std::shared_ptr<BaseInstance>`), create a **non-owning**
shared pointer with a no-op deleter:

```cpp
auto* instRaw = static_cast<BaseInstance*>(evt->instance_handle);
InstancePtr inst = std::shared_ptr<BaseInstance>(
    instRaw, [](BaseInstance*) { /* no-op deleter */ });
```

> **Critical:** Never use a default `shared_ptr` constructor here — it would
> attempt to delete the `BaseInstance` when the last `shared_ptr` goes out of
> scope, causing a double-free crash.

### Return value

The launcher ignores the return value.  Callbacks should return `0`.

### Use cases

This is the **primary hook for adding custom UI pages** to instance dialogs.
The BackupSystem plugin uses it to add a "Backups" page.

- **Custom management pages** — backups, diagnostics, mod conflict analysis,
  performance profiles.
- **Configuration pages** — per-instance plugin settings.
- **Information pages** — instance analytics, play time, version history.

### Example — The BackupSystem pattern (from the reference plugin)

This is the canonical example, taken from
[`BackupPlugin.cpp`](../../../../../meshmc/plugins/BackupSystem/BackupPlugin.cpp):

```cpp
#include "plugin/sdk/mmco_sdk.h"
#include "BackupPage.h"

static MMCOContext* g_ctx = nullptr;

static int on_instance_pages(void* /*mh*/, uint32_t /*hook_id*/,
                             void* payload, void* /*ud*/)
{
    auto* evt = static_cast<MMCOInstancePagesEvent*>(payload);
    if (!evt || !evt->page_list_handle || !evt->instance_handle)
        return 0;

    auto* pages = static_cast<QList<BasePage*>*>(evt->page_list_handle);
    auto* instRaw = static_cast<BaseInstance*>(evt->instance_handle);

    // Non-owning shared_ptr — the instance is managed elsewhere
    InstancePtr inst = std::shared_ptr<BaseInstance>(
        instRaw, [](BaseInstance*) { /* no-op deleter */ });

    pages->append(new BackupPage(inst));
    return 0;
}

MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;

    int rc = ctx->hook_register(ctx->module_handle,
                                MMCO_HOOK_UI_INSTANCE_PAGES,
                                on_instance_pages, nullptr);
    if (rc != 0) {
        MMCO_ERR(ctx, "Failed to register INSTANCE_PAGES hook");
        return rc;
    }

    MMCO_LOG(ctx, "BackupSystem initialized successfully.");
    return 0;
}
```

### Example — Conditional page: only for instances with mods

```cpp
static int on_instance_pages(void* mh, uint32_t hook_id,
                             void* payload, void* ud)
{
    auto* evt = static_cast<MMCOInstancePagesEvent*>(payload);
    if (!evt || !evt->page_list_handle || !evt->instance_handle)
        return 0;

    auto* instRaw = static_cast<BaseInstance*>(evt->instance_handle);
    InstancePtr inst = std::shared_ptr<BaseInstance>(
        instRaw, [](BaseInstance*) {});

    /* Only add the page if the instance has mods */
    QString modsDir = inst->instanceRoot() + "/mods";
    if (!QDir(modsDir).exists())
        return 0;

    auto* pages = static_cast<QList<BasePage*>*>(evt->page_list_handle);
    pages->append(new ModAnalysisPage(inst));
    return 0;
}
```

### Example — Multiple pages from one plugin

```cpp
static int on_instance_pages(void* mh, uint32_t hook_id,
                             void* payload, void* ud)
{
    auto* evt = static_cast<MMCOInstancePagesEvent*>(payload);
    if (!evt || !evt->page_list_handle || !evt->instance_handle)
        return 0;

    auto* pages = static_cast<QList<BasePage*>*>(evt->page_list_handle);
    auto* instRaw = static_cast<BaseInstance*>(evt->instance_handle);
    InstancePtr inst = std::shared_ptr<BaseInstance>(
        instRaw, [](BaseInstance*) {});

    pages->append(new BackupPage(inst));
    pages->append(new PerformancePage(inst));
    pages->append(new DiagnosticsPage(inst));
    return 0;
}
```

### Notes

- This hook fires **every time** the user opens an instance's edit dialog.
  Pages are created fresh each time — do not cache page pointers across
  invocations.
- The dialog takes ownership of appended `BasePage*` objects and deletes them
  when the dialog closes.  Do not delete them yourself.
- Pages appear in the dialog in the order they are in the `QList`.  Built-in
  pages are added first, then each plugin appends in hook registration order.
- The `mmco_sdk.h` header provides the necessary Qt and MeshMC types
  (`QList`, `BasePage`, `BaseInstance`, `InstancePtr`).  Out-of-tree plugins
  must link against the MeshMC SDK.

---

## `MMCO_HOOK_UI_CONTEXT_MENU`

### Summary

| Property | Value |
|---|---|
| **Constant** | `MMCO_HOOK_UI_CONTEXT_MENU` |
| **Hex value** | `0x0601` |
| **Payload** | `MMCOMenuEvent*` |
| **Cancellable** | No |
| **Thread** | Main (GUI) thread |
| **Fires** | Each time a context menu is about to be shown |

### When it fires

Dispatched in `MainWindow.cpp` just before a context menu is displayed to the
user (via `QMenu::exec()`).

**Exact dispatch location** — `MainWindow.cpp`:

```cpp
// Let plugins add items to the context menu
if (APPLICATION->pluginManager()) {
    QByteArray ctxName = onInstance ? "instance" : "main";
    MMCOMenuEvent menuEvt{};
    menuEvt.context = ctxName.constData();
    menuEvt.menu_handle = &myMenu;
    APPLICATION->pluginManager()->dispatchHook(MMCO_HOOK_UI_CONTEXT_MENU,
                                               &menuEvt);
}

myMenu.exec(view->mapToGlobal(pos));
```

At the point this hook fires:

- The `QMenu` has been created and populated with built-in actions.
- The menu has **not** been shown yet — plugins can still add items.
- The `context` field indicates **where** the menu is being shown.
- If `context` is `"instance"`, an instance is right-clicked and selected.
- If `context` is `"main"`, the background of the instance list view is
  right-clicked (no instance selected).

### Payload — `MMCOMenuEvent`

```cpp
struct MMCOMenuEvent {
    const char* context;      /* "main" or "instance" */
    void*       menu_handle;  /* Opaque: QMenu* */
};
```

#### Field reference

| Field | Type | Nullable | Allocator | Lifetime | Description |
|---|---|---|---|---|---|
| `context` | `const char*` | No | Launcher (stack `QByteArray`) | Callback duration | Identifies the menu context. Current values: `"main"` (main window background), `"instance"` (instance right-click). |
| `menu_handle` | `void*` | No | Launcher (stack `QMenu`) | Callback duration | Opaque pointer to the `QMenu*` being constructed.  Pass to `ctx->ui_add_menu_item()` to add items. |

#### The `context` field

| Value | Meaning | User action |
|---|---|---|
| `"main"` | Main window background context menu | Right-click on empty space in the instance list view |
| `"instance"` | Instance-specific context menu | Right-click on a specific instance |

Future launcher versions may introduce additional context values (e.g.,
`"toolbar"`, `"statusbar"`).  Your callback should check `context` and return
early for unknown values.

#### Using `menu_handle`

The `menu_handle` is a `QMenu*`.  To add items, use the MMCO API function
`ctx->ui_add_menu_item()`:

```cpp
ctx->ui_add_menu_item(ctx->module_handle, evt->menu_handle,
                      "My Action Label", my_action_callback, user_data);
```

The function signature for `ui_add_menu_item()`:

```cpp
int (*ui_add_menu_item)(void* mh, void* menu_handle,
                        const char* label,
                        void (*action_callback)(void* user_data),
                        void* user_data);
```

When the user clicks the added menu item, `action_callback` is called with the
`user_data` pointer.  The callback runs on the main thread.

### Return value

The launcher ignores the return value.  Callbacks should return `0`.

### Use cases

- **Custom instance actions** — add "Create Backup", "Export Modlist",
  "Open Terminal Here" to the instance context menu.
- **Global actions** — add "Import Instance from URL", "Plugin Manager"
  to the main context menu.
- **Conditional items** — show different items depending on instance state
  (running, has mods, has backups, etc.).

### Example — Add an item to the instance context menu

```cpp
static void on_backup_clicked(void* ud)
{
    auto* ctx = static_cast<MMCOContext*>(ud);
    MMCO_LOG(ctx, "User requested a backup from context menu.");
    /* ... create backup ... */
}

static int on_context_menu(void* mh, uint32_t hook_id,
                           void* payload, void* ud)
{
    auto* evt = static_cast<MMCOMenuEvent*>(payload);
    if (!evt)
        return 0;

    /* Only add to instance context menus */
    if (strcmp(evt->context, "instance") != 0)
        return 0;

    g_ctx->ui_add_menu_item(g_ctx->module_handle,
                            evt->menu_handle,
                            "Create Backup",
                            on_backup_clicked,
                            g_ctx);
    return 0;
}

MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;
    ctx->hook_register(ctx->module_handle,
                       MMCO_HOOK_UI_CONTEXT_MENU,
                       on_context_menu, nullptr);
    return 0;
}
```

### Example — Add items to both menu types with different labels

```cpp
static void on_instance_action(void* ud)
{
    MMCO_LOG(g_ctx, "Instance-specific action triggered.");
}

static void on_global_action(void* ud)
{
    MMCO_LOG(g_ctx, "Global action triggered.");
}

static int on_context_menu(void* mh, uint32_t hook_id,
                           void* payload, void* ud)
{
    auto* evt = static_cast<MMCOMenuEvent*>(payload);
    if (!evt)
        return 0;

    if (strcmp(evt->context, "instance") == 0) {
        g_ctx->ui_add_menu_item(g_ctx->module_handle,
                                evt->menu_handle,
                                "Analyze Mods",
                                on_instance_action, nullptr);
    } else if (strcmp(evt->context, "main") == 0) {
        g_ctx->ui_add_menu_item(g_ctx->module_handle,
                                evt->menu_handle,
                                "Open Plugin Settings",
                                on_global_action, nullptr);
    }

    return 0;
}
```

### Example — Conditional menu item (only if backups exist)

```cpp
static void on_restore_latest(void* ud)
{
    auto* ctx = static_cast<MMCOContext*>(ud);
    /* ... restore latest backup ... */
}

static int on_context_menu(void* mh, uint32_t hook_id,
                           void* payload, void* ud)
{
    auto* evt = static_cast<MMCOMenuEvent*>(payload);
    if (!evt || strcmp(evt->context, "instance") != 0)
        return 0;

    /* Check if the currently selected instance has backups.
       We need to get the instance ID — but MMCOMenuEvent does not
       contain it directly.  Use the instance API to inspect. */

    /* Alternative: store the selected instance ID from a pre-launch
       hook or track selection changes.  For simplicity, always add
       the item and check at action time. */

    g_ctx->ui_add_menu_item(g_ctx->module_handle,
                            evt->menu_handle,
                            "Restore Latest Backup",
                            on_restore_latest,
                            g_ctx);
    return 0;
}
```

### Notes

- Menu items added by plugins appear **after** all built-in items.  There is
  currently no API to control the position (before/after specific built-in
  items) or to add separators.
- The `menu_handle` and `context` pointers are only valid during the callback.
  Do not store them.
- Adding many items can make the context menu unwieldy.  Consider using a
  sub-menu pattern: add one item labeled "My Plugin ▸" that opens a sub-menu
  with multiple actions (this requires direct `QMenu` manipulation via the
  opaque handle, which is possible but not part of the stable MMCO API).
- The `MMCOMenuEvent` does **not** include the instance ID for `"instance"`
  context.  If your action needs to know which instance was right-clicked,
  track the selected instance via other hooks or query the instance API.
- Each time the user right-clicks, a new `QMenu` is created and the hook fires
  again.  Items from previous invocations do not persist.

---

## UI hook dispatch timeline

```
MainWindow construction complete
  │
  ├─ Plugin instance actions added to toolbar
  │    (via ui_register_instance_action() during mmco_init())
  │
  └─ dispatchHook(MMCO_HOOK_UI_MAIN_READY)  ◄── [1]
       ├─ Plugin A callback → returns 0
       └─ Plugin B callback → returns 0

  ... user interacts with the UI ...

User opens "Edit Instance" dialog for instance "My SMP"
  │
  └─ InstancePageProvider builds page list
       ├─ Built-in pages added: Version, Mods, Worlds, ...
       │
       └─ dispatchHook(MMCO_HOOK_UI_INSTANCE_PAGES, &evt)  ◄── [2]
            │
            │  evt = {
            │    instance_id   = "abc123",
            │    instance_name = "My SMP",
            │    instance_path = "/home/user/.meshmc/instances/abc123",
            │    page_list_handle = &values,    // QList<BasePage*>*
            │    instance_handle  = inst.get()  // BaseInstance*
            │  }
            │
            ├─ Plugin A → appends BackupPage
            └─ Plugin B → appends DiagnosticsPage
            │
            └─ Dialog shown with all pages (built-in + plugin)

User right-clicks on instance "My SMP"
  │
  └─ MainWindow builds context menu
       ├─ Built-in items: Launch, Edit, Copy, Delete, ...
       │
       └─ dispatchHook(MMCO_HOOK_UI_CONTEXT_MENU, &menuEvt)  ◄── [3]
            │
            │  menuEvt = {
            │    context     = "instance",
            │    menu_handle = &myMenu   // QMenu*
            │  }
            │
            ├─ Plugin A → adds "Create Backup" item
            └─ Plugin B → adds "Run Diagnostics" item
            │
            └─ myMenu.exec()  →  menu shown to user

User right-clicks on empty space in instance list
  │
  └─ MainWindow builds context menu
       ├─ Built-in items: Add Instance, Refresh, ...
       │
       └─ dispatchHook(MMCO_HOOK_UI_CONTEXT_MENU, &menuEvt)  ◄── [3]
            │
            │  menuEvt = {
            │    context     = "main",
            │    menu_handle = &myMenu
            │  }
            │
            ├─ Plugin A → no items for "main" context, returns 0
            └─ Plugin B → adds "Open Plugin Settings" item
            │
            └─ myMenu.exec()
```

**Legend:**

- **[1] `UI_MAIN_READY`** — fires once, window visible, no payload
- **[2] `UI_INSTANCE_PAGES`** — fires per dialog open, payload has page list and instance handle
- **[3] `UI_CONTEXT_MENU`** — fires per right-click, payload has menu handle and context string

---

## Complete plugin: Instance pages + context menu + UI ready

This example combines all three UI hooks in a single plugin:

```cpp
#include "plugin/sdk/mmco_sdk.h"
#include "MyCustomPage.h"

static MMCOContext* g_ctx = nullptr;

/* ---------- UI_INSTANCE_PAGES ---------- */

static int on_instance_pages(void* mh, uint32_t hook_id,
                             void* payload, void* ud)
{
    auto* evt = static_cast<MMCOInstancePagesEvent*>(payload);
    if (!evt || !evt->page_list_handle || !evt->instance_handle)
        return 0;

    auto* pages = static_cast<QList<BasePage*>*>(evt->page_list_handle);
    auto* instRaw = static_cast<BaseInstance*>(evt->instance_handle);
    InstancePtr inst = std::shared_ptr<BaseInstance>(
        instRaw, [](BaseInstance*) {});

    pages->append(new MyCustomPage(inst));
    return 0;
}

/* ---------- UI_CONTEXT_MENU ---------- */

static void on_open_settings(void* ud)
{
    MMCO_LOG(static_cast<MMCOContext*>(ud),
             "Opening plugin settings dialog...");
}

static int on_context_menu(void* mh, uint32_t hook_id,
                           void* payload, void* ud)
{
    auto* evt = static_cast<MMCOMenuEvent*>(payload);
    if (!evt)
        return 0;

    if (strcmp(evt->context, "instance") == 0) {
        g_ctx->ui_add_menu_item(g_ctx->module_handle,
                                evt->menu_handle,
                                "My Plugin Action",
                                on_open_settings, g_ctx);
    }
    return 0;
}

/* ---------- UI_MAIN_READY ---------- */

static int on_ui_ready(void* mh, uint32_t hook_id,
                       void* payload, void* ud)
{
    MMCO_LOG(g_ctx, "UI is ready — plugin fully active.");
    return 0;
}

/* ---------- Module entry ---------- */

MMCO_DEFINE_MODULE(
    "MyPlugin", "1.0.0", "Author Name",
    "Example plugin using all three UI hooks",
    "GPL-3.0-or-later");

extern "C" {

MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;

    ctx->hook_register(ctx->module_handle,
                       MMCO_HOOK_UI_MAIN_READY,
                       on_ui_ready, nullptr);
    ctx->hook_register(ctx->module_handle,
                       MMCO_HOOK_UI_INSTANCE_PAGES,
                       on_instance_pages, nullptr);
    ctx->hook_register(ctx->module_handle,
                       MMCO_HOOK_UI_CONTEXT_MENU,
                       on_context_menu, nullptr);

    MMCO_LOG(ctx, "MyPlugin initialized — UI hooks registered.");
    return 0;
}

MMCO_EXPORT void mmco_unload()
{
    if (g_ctx)
        MMCO_LOG(g_ctx, "MyPlugin unloading.");
    g_ctx = nullptr;
}

} /* extern "C" */
```

---

## See also

- [Application hooks](app-hooks.md) — `APP_INITIALIZED`, `APP_SHUTDOWN`, `SETTINGS_CHANGED`
- [Instance hooks](instance-hooks.md) — `INSTANCE_PRE_LAUNCH`, `INSTANCE_POST_LAUNCH`, `INSTANCE_CREATED`, `INSTANCE_REMOVED`
- [Hooks Reference overview](README.md) — dispatch model, registration API recap, common patterns
- [API Reference § UI](../api-reference/s07-ui/) — `ui_add_menu_item()`, `ui_register_instance_action()`, `ui_add_page_builder()` internals
- [BackupSystem Case Study](../backup-system-case-study.md) — complete `UI_INSTANCE_PAGES` usage in production
