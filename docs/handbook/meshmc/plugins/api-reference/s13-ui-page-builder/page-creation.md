# Page Creation — `ui_page_create`, `ui_page_add_to_list`, `ui_page_set_layout`

> **Section 13 sub-page.**
> Creating a `PluginPage`, inserting it into the instance settings dialog,
> and assigning its root layout.

---

## Table of Contents

- [`ui_page_create()`](#ui_page_create)
  - [Signature](#signature)
  - [Parameters](#parameters)
  - [Return Value](#return-value)
  - [How It Works](#how-it-works)
  - [The `page_id` Contract](#the-page_id-contract)
  - [Icon Resolution](#icon-resolution)
  - [Error Conditions](#error-conditions)
  - [Examples](#examples)
- [`ui_page_add_to_list()`](#ui_page_add_to_list)
  - [Signature](#signature-1)
  - [Parameters](#parameters-1)
  - [Return Value](#return-value-1)
  - [How It Works](#how-it-works-1)
  - [When to Call](#when-to-call)
  - [Error Conditions](#error-conditions-1)
  - [Examples](#examples-1)
- [`ui_page_set_layout()`](#ui_page_set_layout)
  - [Signature](#signature-2)
  - [Parameters](#parameters-2)
  - [Return Value](#return-value-2)
  - [How It Works](#how-it-works-2)
  - [Layout Reassignment](#layout-reassignment)
  - [Calling Order](#calling-order)
  - [Error Conditions](#error-conditions-2)
  - [Examples](#examples-2)
- [End-to-End Workflow](#end-to-end-workflow)
  - [Minimal Page](#minimal-page)
  - [Settings Page with Form](#settings-page-with-form)
  - [Multiple Pages from One Plugin](#multiple-pages-from-one-plugin)

---

## `ui_page_create`

Creates a new `PluginPage` instance — a `QWidget` + `BasePage` wrapper that
integrates into MeshMC's instance settings dialog. This is the starting point
for every S13 page.

### Signature

```c
void* (*ui_page_create)(void* mh,
                        const char* page_id,
                        const char* display_name,
                        const char* icon_name);
```

From `PluginAPI.h`:

```c
/* Create a page widget. Returns opaque page handle. */
void* (*ui_page_create)(void* mh, const char* page_id,
                        const char* display_name, const char* icon_name);
```

### Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `mh` | `void*` | Yes | Module handle (`ctx->module_handle`). Reserved for future per-module validation. |
| `page_id` | `const char*` | **Yes** | Unique identifier for the page. Used for sidebar selection and toolbar action page matching. Must be a valid, non-null, non-empty UTF-8 string. |
| `display_name` | `const char*` | **Yes** | Human-readable name shown in the instance settings sidebar. Must be non-null. |
| `icon_name` | `const char*` | No | Freedesktop/system theme icon name (e.g. `"folder-sync"`, `"document-save"`, `"help-about"`). If `nullptr`, defaults to `"plugin"`. |

### Return Value

| Value | Meaning |
|-------|---------|
| Non-null `void*` | Opaque handle to the created `PluginPage`. Internally a `QWidget*`. |
| `nullptr` | Either `page_id` or `display_name` was `nullptr`. |

The returned handle is used as:
- The `page` argument to `ui_page_set_layout()` and `ui_page_add_to_list()`
- The `parent` argument to widget creation functions (`ui_button_create()`,
  `ui_label_create()`, `ui_tree_create()`)
- The `parent` argument to `ui_layout_create()` (though the layout is not
  actually set on the widget until `ui_page_set_layout()` is called)

### How It Works

The implementation in `PluginManager.cpp`:

```cpp
void* PluginManager::api_ui_page_create(void* mh, const char* id,
                                        const char* name, const char* iconName)
{
    (void)mh;
    if (!id || !name)
        return nullptr;
    auto* page = new PluginPage(QString::fromUtf8(id),
                                QString::fromUtf8(name),
                                iconName ? QString::fromUtf8(iconName)
                                         : QStringLiteral("plugin"));
    return static_cast<QWidget*>(page);
}
```

Walkthrough:

1. **Null guard**: If `id` or `name` is `nullptr`, returns `nullptr` immediately.
   An empty string (`""`) is allowed, though not useful.

2. **`PluginPage` construction**: Allocates a `PluginPage` on the heap with:
   - `pageId` = UTF-8 conversion of `id`
   - `displayName` = UTF-8 conversion of `name`
   - `iconName` = UTF-8 conversion of `iconName`, or `"plugin"` if null

3. **Return type**: The `PluginPage*` is `static_cast` to `QWidget*` before
   returning as `void*`. This is safe because `PluginPage` inherits `QWidget`.
   Later, when the page is cast back (e.g. in `ui_page_add_to_list()`), a
   `dynamic_cast<BasePage*>` recovers the `BasePage` interface.

4. **No parent set**: The page is created as a **top-level widget** (no parent).
   Qt's ownership kicks in when the page is eventually inserted into the
   instance settings dialog's widget hierarchy by the framework (not by the
   plugin). Until then, the plugin is logically responsible for the pointer.

5. **No layout set**: The page starts with no layout. You must call
   `ui_page_set_layout()` to assign one before widgets will be positioned.

### The `page_id` Contract

The `page_id` string serves three purposes:

1. **Sidebar selection**: The instance settings dialog uses `BasePage::id()`
   to match pages when navigating. If you pass `"my-backup-page"` as the ID,
   the dialog can programmatically select it.

2. **Toolbar action matching**: If you call `ui_register_instance_action()`
   (Section 12) with a `page_id`, that ID must exactly match the ID you pass
   to `ui_page_create()`. When the user clicks the toolbar button, MeshMC
   calls `showInstanceWindow(instance, pageId)` and selects the page by ID.

   ```cpp
   // In mmco_init():
   ctx->ui_register_instance_action(mh, "Backups", "Manage instance backups",
                                    "folder-sync", "backup-page");

   // In the hook callback:
   void* page = ctx->ui_page_create(mh, "backup-page", "Backups",
                                    "folder-sync");
   //                                    ^^^^^^^^^^^
   //                          Must match the toolbar action's page_id
   ```

3. **Uniqueness expectation**: While MeshMC does not currently enforce unique
   IDs across pages, duplicate IDs will cause undefined sidebar navigation
   behavior (the first match wins). Use your plugin name as a namespace:
   `"myplugin-settings"`, `"myplugin-stats"`, etc.

**Naming conventions:**

| Pattern | Example | Notes |
|---------|---------|-------|
| `<plugin>-<purpose>` | `"backup-page"` | Recommended |
| `<plugin>-<subpage>` | `"backup-settings"`, `"backup-history"` | For multi-page plugins |
| Short identifiers | `"bp"` | Works but not recommended (collision risk) |

### Icon Resolution

The `icon_name` is resolved at display time via `QIcon::fromTheme()`. This
means the icon depends on the user's **system icon theme** (on Linux) or
MeshMC's bundled icon set (on Windows/macOS).

**Common icon names that work across platforms:**

| Icon Name | Visual | Typical Use |
|-----------|--------|-------------|
| `"plugin"` | Generic plugin icon | Default fallback |
| `"folder-sync"` | Folder with sync arrows | Backup / sync pages |
| `"document-save"` | Floppy disk | Save-related pages |
| `"help-about"` | Info circle | About / info pages |
| `"preferences-system"` | Gear | Settings pages |
| `"dialog-information"` | Blue info icon | Informational pages |
| `"utilities-terminal"` | Terminal | Debug / log pages |
| `"security-high"` | Padlock | Security pages |

If the theme does not contain the requested icon, Qt returns a null `QIcon`
and the sidebar entry will have no icon — it still functions correctly.

### Error Conditions

| Condition | Behavior |
|-----------|----------|
| `page_id` is `nullptr` | Returns `nullptr` |
| `display_name` is `nullptr` | Returns `nullptr` |
| `icon_name` is `nullptr` | Falls back to `"plugin"` |
| `page_id` is `""` (empty) | Succeeds but sidebar navigation may not work |
| `display_name` is `""` (empty) | Succeeds but sidebar shows a blank entry |
| `mh` is `nullptr` | Succeeds (parameter is currently unused) |
| Called from non-GUI thread | Undefined behavior (Qt requirement) |

### Examples

**Minimal page creation:**

```cpp
void* page = ctx->ui_page_create(ctx->module_handle,
                                 "my-info-page",
                                 "My Plugin Info",
                                 nullptr); // default "plugin" icon
```

**Page with custom icon:**

```cpp
void* page = ctx->ui_page_create(ctx->module_handle,
                                 "backup-manager",
                                 "Backups",
                                 "folder-sync");
```

**Defensive creation with error check:**

```cpp
void* page = ctx->ui_page_create(ctx->module_handle,
                                 "stats-page", "Statistics",
                                 "dialog-information");
if (!page) {
    MMCO_ERR(ctx, "Failed to create statistics page");
    return 0;
}
```

---

## `ui_page_add_to_list`

Appends a `PluginPage` to the page list being built for the instance settings
dialog. This is how the page actually appears in the sidebar.

### Signature

```c
int (*ui_page_add_to_list)(void* mh,
                           void* page_handle,
                           void* page_list_handle);
```

From `PluginAPI.h`:

```c
/* Add the created page to the page list from a hook event. */
int (*ui_page_add_to_list)(void* mh, void* page_handle,
                           void* page_list_handle);
```

### Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `mh` | `void*` | Yes | Module handle. Reserved. |
| `page_handle` | `void*` | **Yes** | The handle returned by `ui_page_create()`. |
| `page_list_handle` | `void*` | **Yes** | The `page_list_handle` field from the `MMCOInstancePagesEvent` payload. |

### Return Value

| Value | Meaning |
|-------|---------|
| `0` | Page successfully added to the list |
| `-1` | Error: a parameter was null, or the page handle is not a valid `BasePage*` |

### How It Works

The implementation:

```cpp
int PluginManager::api_ui_page_add_to_list(void* mh, void* page, void* list)
{
    (void)mh;
    if (!page || !list)
        return -1;
    auto* pageWidget = static_cast<QWidget*>(page);
    auto* pageBase = dynamic_cast<BasePage*>(pageWidget);
    if (!pageBase)
        return -1;
    auto* pages = static_cast<QList<BasePage*>*>(list);
    pages->append(pageBase);
    return 0;
}
```

Step by step:

1. **Null guard**: Returns `-1` if either `page` or `list` is null.

2. **Cast to `QWidget*`**: The `page` handle is treated as a `QWidget*`
   (matching what `ui_page_create()` returned).

3. **`dynamic_cast` to `BasePage*`**: This is the safety check. If the `void*`
   is not actually a `PluginPage` (or any other `QWidget` + `BasePage` dual
   inheritor), the `dynamic_cast` returns `nullptr` and the function returns
   `-1`. This protects against passing an arbitrary widget handle.

4. **Append to list**: The `list` handle is cast to `QList<BasePage*>*` — this
   is the same list that `InstancePageProvider::getPages()` builds. The page is
   appended at the end, meaning plugin pages always appear **after** all
   built-in pages in the sidebar.

5. **Ownership transfer**: Once appended, the page's lifetime is managed by
   the instance settings dialog. When the dialog closes, Qt destroys the page
   widget and all its children.

### When to Call

This function **must** be called inside an `MMCO_HOOK_UI_INSTANCE_PAGES`
callback. That is the only context where you have a valid `page_list_handle`.

The dispatch flow:

```
InstancePageProvider::getPages()
  → builds built-in pages (Settings, Version, Mods, Worlds, ...)
  → PluginManager::dispatchHook(MMCO_HOOK_UI_INSTANCE_PAGES, &evt)
    → your callback receives MMCOInstancePagesEvent*
      → you create page via ui_page_create()
      → you build layout + widgets
      → you call ui_page_add_to_list(mh, page, evt->page_list_handle)  ← HERE
  → dialog displays all pages including yours
```

If you call `ui_page_add_to_list()` outside of a hook callback with a stale
or fabricated `page_list_handle`, the behavior is **undefined** (likely a
crash, since the `QList` no longer exists).

### Error Conditions

| Condition | Return | Behavior |
|-----------|--------|----------|
| `page_handle` is `nullptr` | `-1` | — |
| `page_list_handle` is `nullptr` | `-1` | — |
| `page_handle` does not point to a `BasePage` | `-1` | `dynamic_cast` fails safely |
| `page_list_handle` is stale (dialog closed) | Undefined | Potential crash |
| Called after hook callback returned | Undefined | The list may have been consumed |
| Same page added twice | `0` | Page appears twice in sidebar (not useful) |

### Examples

**Standard usage inside hook callback:**

```cpp
static int on_instance_pages(void* mh, uint32_t /*hook_id*/,
                             void* payload, void* /*ud*/)
{
    auto* evt = static_cast<MMCOInstancePagesEvent*>(payload);
    if (!evt || !evt->page_list_handle)
        return 0;

    void* page = g_ctx->ui_page_create(mh, "my-page", "My Page", nullptr);
    if (!page)
        return 0;

    // ... build layout and widgets ...

    g_ctx->ui_page_set_layout(mh, page, root_layout);

    int rc = g_ctx->ui_page_add_to_list(mh, page, evt->page_list_handle);
    if (rc != 0) {
        MMCO_ERR(g_ctx, "Failed to add page to instance dialog");
    }
    return 0;
}
```

**Adding multiple pages from one callback:**

```cpp
void* settings_page = g_ctx->ui_page_create(mh, "bp-settings",
                                            "Backup Settings",
                                            "preferences-system");
void* history_page = g_ctx->ui_page_create(mh, "bp-history",
                                           "Backup History",
                                           "folder-sync");

// ... build each page's UI ...

g_ctx->ui_page_add_to_list(mh, settings_page, evt->page_list_handle);
g_ctx->ui_page_add_to_list(mh, history_page, evt->page_list_handle);
```

---

## `ui_page_set_layout`

Assigns a root layout to a page widget. Without this call, widgets added to
the page will not be positioned — they will overlap at (0, 0).

### Signature

```c
int (*ui_page_set_layout)(void* mh,
                          void* page,
                          void* layout);
```

From `PluginAPI.h`:

```c
int (*ui_page_set_layout)(void* mh, void* page, void* layout);
```

### Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `mh` | `void*` | Yes | Module handle. Reserved. |
| `page` | `void*` | **Yes** | Page handle from `ui_page_create()`. |
| `layout` | `void*` | **Yes** | Layout handle from `ui_layout_create()`. |

### Return Value

| Value | Meaning |
|-------|---------|
| `0` | Layout successfully assigned |
| `-1` | A parameter was null |

### How It Works

The implementation:

```cpp
int PluginManager::api_ui_page_set_layout(void* mh, void* page, void* layout)
{
    (void)mh;
    if (!page || !layout)
        return -1;
    auto* w = static_cast<QWidget*>(page);
    w->setLayout(static_cast<QLayout*>(layout));
    return 0;
}
```

This is a thin wrapper around `QWidget::setLayout()`. Qt's rules apply:

1. **The layout takes ownership of all widgets added to it.** Widgets that were
   added to the layout via `ui_layout_add_widget()` become children of the
   page widget.

2. **A widget can only have one layout.** Calling `ui_page_set_layout()` on a
   page that already has a layout will produce a Qt warning:
   `QWidget::setLayout: Attempting to set QLayout "" on PluginPage ""`
   `which already has a layout`. The call succeeds (Qt does not crash), but the
   old layout is not replaced — it remains active.

3. **Margins and spacing**: The layout's default margins and spacing are
   determined by the current Qt style. On most themes this is ~9px margins with
   ~6px spacing between items.

### Layout Reassignment

**Do not call `ui_page_set_layout()` twice on the same page.** Qt does not
support replacing a layout after one has been set. If you need to restructure
the page dynamically, you have two options:

1. **Build the full layout before assigning it.** Create all child layouts and
   widgets, nest them properly, then call `ui_page_set_layout()` once at the
   end. This is the recommended approach.

2. **Use the S11 hook approach.** Full Qt access lets you manage layouts with
   `QWidget::setLayout()` / `delete` patterns.

### Calling Order

The recommended sequence for building a page:

```
ui_page_create()          → page handle
ui_layout_create()        → root layout handle
ui_label_create()         → widget handle
ui_layout_add_widget()    → add widget to layout
ui_button_create()        → widget handle
ui_layout_add_widget()    → add widget to layout
...
ui_page_set_layout()      → assign root layout to page   ← BEFORE add_to_list
ui_page_add_to_list()     → insert page into dialog
```

`ui_page_set_layout()` should be called **before** `ui_page_add_to_list()`.
While calling it after technically works (the page object still exists), it is
better practice to fully construct the page before presenting it to MeshMC.

### Error Conditions

| Condition | Return | Behavior |
|-----------|--------|----------|
| `page` is `nullptr` | `-1` | — |
| `layout` is `nullptr` | `-1` | — |
| `page` already has a layout | `0` | Qt prints a warning; old layout remains |
| `layout` was already set on another widget | `0` | Qt reparents the layout (removes from old parent) |
| `layout` is not a `QLayout*` | Undefined | Incorrect cast leads to UB |

### Examples

**Standard usage:**

```cpp
void* page = ctx->ui_page_create(mh, "demo", "Demo", nullptr);
void* layout = ctx->ui_layout_create(mh, page, 0); // vertical

void* label = ctx->ui_label_create(mh, page, "Hello, World!");
ctx->ui_layout_add_widget(mh, layout, label);

ctx->ui_page_set_layout(mh, page, layout);  // assigns the root layout
ctx->ui_page_add_to_list(mh, page, evt->page_list_handle);
```

---

## End-to-End Workflow

### Minimal Page

The simplest possible S13 page — a single label:

```cpp
static int on_instance_pages(void* mh, uint32_t /*hook_id*/,
                             void* payload, void* /*ud*/)
{
    auto* evt = static_cast<MMCOInstancePagesEvent*>(payload);
    if (!evt || !evt->page_list_handle)
        return 0;

    void* page = g_ctx->ui_page_create(mh, "minimal", "Minimal", nullptr);
    if (!page) return 0;

    void* layout = g_ctx->ui_layout_create(mh, page, 0);
    void* label = g_ctx->ui_label_create(mh, page,
        "This page was created entirely through the C API.");
    g_ctx->ui_layout_add_widget(mh, layout, label);
    g_ctx->ui_layout_add_spacer(mh, layout, 0); // push content to top
    g_ctx->ui_page_set_layout(mh, page, layout);
    g_ctx->ui_page_add_to_list(mh, page, evt->page_list_handle);

    return 0;
}
```

Result in the instance settings dialog:

```
╔═══════════════╗  ┌─────────────────────────────────────────┐
║  Settings     ║  │ This page was created entirely through  │
║  Version      ║  │ the C API.                              │
║  Mods         ║  │                                         │
║  ...          ║  │                                         │
║ ►Minimal      ║  │                                         │
╚═══════════════╝  └─────────────────────────────────────────┘
```

### Settings Page with Form

A more complex page: a title, a descriptive label, a tree widget showing
backup entries, and a row of action buttons.

```cpp
/* Forward-declare callbacks */
static void on_create_backup(void* ud);
static void on_restore_backup(void* ud);
static void on_delete_backup(void* ud);
static void on_backup_selected(void* ud, int row);

/* Plugin state */
static void* g_tree = nullptr;
static void* g_restore_btn = nullptr;
static void* g_delete_btn = nullptr;

static int on_instance_pages(void* mh, uint32_t /*hook_id*/,
                             void* payload, void* /*ud*/)
{
    auto* evt = static_cast<MMCOInstancePagesEvent*>(payload);
    if (!evt || !evt->page_list_handle)
        return 0;

    /* ---- Page ---- */
    void* page = g_ctx->ui_page_create(mh, "backup-manager",
                                       "Backups", "folder-sync");
    if (!page) return 0;

    /* ---- Root layout (vertical) ---- */
    void* root = g_ctx->ui_layout_create(mh, page, 0);

    /* ---- Title ---- */
    void* title = g_ctx->ui_label_create(mh, page, "Instance Backups");
    g_ctx->ui_layout_add_widget(mh, root, title);

    /* ---- Description ---- */
    char desc_buf[256];
    snprintf(desc_buf, sizeof(desc_buf),
             "Manage backups for \"%s\".", evt->instance_name);
    void* desc = g_ctx->ui_label_create(mh, page, desc_buf);
    g_ctx->ui_layout_add_widget(mh, root, desc);

    /* ---- Tree widget (backup list) ---- */
    const char* columns[] = {"Name", "Date", "Size"};
    g_tree = g_ctx->ui_tree_create(mh, page, columns, 3,
                                   on_backup_selected, nullptr);
    g_ctx->ui_layout_add_widget(mh, root, g_tree);

    /* Populate with some example rows */
    const char* row1[] = {"Pre-modding backup", "2026-04-10 14:30", "142 MB"};
    const char* row2[] = {"Before update 1.21", "2026-04-12 09:15", "156 MB"};
    g_ctx->ui_tree_add_row(mh, g_tree, row1, 3);
    g_ctx->ui_tree_add_row(mh, g_tree, row2, 3);

    /* ---- Button row (horizontal) ---- */
    void* btn_row = g_ctx->ui_layout_create(mh, page, 1); // horizontal

    void* create_btn = g_ctx->ui_button_create(mh, page, "Create Backup",
                                               "document-save",
                                               on_create_backup, nullptr);
    g_ctx->ui_layout_add_widget(mh, btn_row, create_btn);

    g_restore_btn = g_ctx->ui_button_create(mh, page, "Restore",
                                            "edit-undo",
                                            on_restore_backup, nullptr);
    g_ctx->ui_button_set_enabled(mh, g_restore_btn, 0); // disabled until selected
    g_ctx->ui_layout_add_widget(mh, btn_row, g_restore_btn);

    g_delete_btn = g_ctx->ui_button_create(mh, page, "Delete",
                                           "edit-delete",
                                           on_delete_backup, nullptr);
    g_ctx->ui_button_set_enabled(mh, g_delete_btn, 0);
    g_ctx->ui_layout_add_widget(mh, btn_row, g_delete_btn);

    g_ctx->ui_layout_add_spacer(mh, btn_row, 1); // push buttons left
    g_ctx->ui_layout_add_layout(mh, root, btn_row);

    /* ---- Finalize ---- */
    g_ctx->ui_page_set_layout(mh, page, root);
    g_ctx->ui_page_add_to_list(mh, page, evt->page_list_handle);

    return 0;
}

/* Callback: enable Restore/Delete when a row is selected */
static void on_backup_selected(void* /*ud*/, int row)
{
    int has_sel = (row >= 0) ? 1 : 0;
    g_ctx->ui_button_set_enabled(g_ctx->module_handle, g_restore_btn, has_sel);
    g_ctx->ui_button_set_enabled(g_ctx->module_handle, g_delete_btn, has_sel);
}

static void on_create_backup(void* /*ud*/)
{
    MMCO_LOG(g_ctx, "Create backup clicked");
    // Real implementation: zip the instance directory, etc.
}

static void on_restore_backup(void* /*ud*/)
{
    int row = g_ctx->ui_tree_selected_row(g_ctx->module_handle, g_tree);
    if (row < 0) return;
    MMCO_LOG(g_ctx, "Restore backup clicked");
    // Real implementation: extract the backup zip, etc.
}

static void on_delete_backup(void* /*ud*/)
{
    int row = g_ctx->ui_tree_selected_row(g_ctx->module_handle, g_tree);
    if (row < 0) return;

    int confirmed = g_ctx->ui_confirm_dialog(g_ctx->module_handle,
        "Delete Backup", "Are you sure you want to delete this backup?");
    if (confirmed) {
        MMCO_LOG(g_ctx, "Delete confirmed");
        // Real implementation: remove the backup file, refresh tree, etc.
    }
}
```

### Multiple Pages from One Plugin

A single plugin can create and insert any number of pages in one hook callback:

```cpp
static int on_instance_pages(void* mh, uint32_t /*hook_id*/,
                             void* payload, void* /*ud*/)
{
    auto* evt = static_cast<MMCOInstancePagesEvent*>(payload);
    if (!evt || !evt->page_list_handle)
        return 0;

    /* Page 1: Overview */
    void* overview = g_ctx->ui_page_create(mh, "myplugin-overview",
                                           "Plugin Overview",
                                           "dialog-information");
    if (overview) {
        void* layout = g_ctx->ui_layout_create(mh, overview, 0);
        void* lbl = g_ctx->ui_label_create(mh, overview,
            "This plugin provides backup and statistics features.");
        g_ctx->ui_layout_add_widget(mh, layout, lbl);
        g_ctx->ui_layout_add_spacer(mh, layout, 0);
        g_ctx->ui_page_set_layout(mh, overview, layout);
        g_ctx->ui_page_add_to_list(mh, overview, evt->page_list_handle);
    }

    /* Page 2: Statistics */
    void* stats = g_ctx->ui_page_create(mh, "myplugin-stats",
                                        "Statistics",
                                        "utilities-system-monitor");
    if (stats) {
        void* layout = g_ctx->ui_layout_create(mh, stats, 0);
        const char* cols[] = {"Metric", "Value"};
        void* tree = g_ctx->ui_tree_create(mh, stats, cols, 2, nullptr, nullptr);

        const char* r1[] = {"Total Launches", "42"};
        const char* r2[] = {"Total Play Time", "128h 34m"};
        const char* r3[] = {"Backups Created", "7"};
        g_ctx->ui_tree_add_row(mh, tree, r1, 2);
        g_ctx->ui_tree_add_row(mh, tree, r2, 2);
        g_ctx->ui_tree_add_row(mh, tree, r3, 2);

        g_ctx->ui_layout_add_widget(mh, layout, tree);
        g_ctx->ui_page_set_layout(mh, stats, layout);
        g_ctx->ui_page_add_to_list(mh, stats, evt->page_list_handle);
    }

    return 0;
}
```

Each page appears as a separate entry in the sidebar. There is no practical
limit to the number of pages a plugin can add, but adding too many degrades
user experience. One to three pages per plugin is typical.
