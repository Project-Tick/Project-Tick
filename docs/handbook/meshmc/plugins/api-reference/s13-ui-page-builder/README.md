# Section 13: UI Page Builder API

> **MMCO Plugin API Reference — Section 13**
>
> Build complete UI pages through the C API without touching Qt directly.
> Pages, layouts, widgets, and event callbacks — all through `MMCOContext`
> function pointers.

---

## Overview

Section 13 of the `MMCOContext` exposes a **widget construction toolkit**
entirely through C function pointers. Plugins can create pages, lay them out,
add buttons, labels, and tree widgets, and wire up callbacks — without ever
including a Qt header or linking against Qt libraries.

Behind the scenes, every S13 call creates real Qt objects (a `QPushButton`, a
`QLabel`, a `QTreeWidget`, a `QVBoxLayout`, etc.). The plugin never sees those
types; it operates on `void*` handles returned by the API. MeshMC owns the
widget lifetime through Qt's parent–child memory model — when the page is
destroyed (i.e. the instance settings dialog closes), all child widgets are
destroyed with it.

### Function pointer summary

| Function Pointer | Category | Returns | Purpose |
|---|---|---|---|
| `ui_page_create` | Page | `void*` | Create a blank `PluginPage` |
| `ui_page_add_to_list` | Page | `int` | Append the page to the hook's page list |
| `ui_page_set_layout` | Page | `int` | Assign a root layout to the page |
| `ui_layout_create` | Layout | `void*` | Create a vertical or horizontal box layout |
| `ui_layout_add_widget` | Layout | `int` | Add a widget to a layout |
| `ui_layout_add_layout` | Layout | `int` | Nest a child layout inside a parent layout |
| `ui_layout_add_spacer` | Layout | `int` | Insert an expanding spacer |
| `ui_button_create` | Widget | `void*` | Create a `QPushButton` with text, icon, callback |
| `ui_button_set_enabled` | Widget | `int` | Enable or disable a button |
| `ui_button_set_text` | Widget | `int` | Change button label text |
| `ui_label_create` | Widget | `void*` | Create a `QLabel` |
| `ui_label_set_text` | Widget | `int` | Change label text |
| `ui_tree_create` | Widget | `void*` | Create a `QTreeWidget` (tabular list) |
| `ui_tree_clear` | Widget | `int` | Remove all rows from a tree |
| `ui_tree_add_row` | Widget | `int` | Append a row with column values |
| `ui_tree_selected_row` | Widget | `int` | Query which row is selected |
| `ui_tree_set_row_data` | Widget | `int` | Attach an `int64_t` tag to a row |
| `ui_tree_get_row_data` | Widget | `int64_t` | Retrieve a row's `int64_t` tag |
| `ui_tree_row_count` | Widget | `int` | Get the number of rows |

All 19 function pointers operate on opaque `void*` handles. The first argument
is always `void* mh` (the module handle from `ctx->module_handle`).

---

## The `PluginPage` Class

When you call `ui_page_create()`, MeshMC internally creates an instance of
`PluginPage` — a private class defined inside `PluginManager.cpp`:

```cpp
class PluginPage : public QWidget, public BasePage
{
    Q_OBJECT
  public:
    PluginPage(const QString& pageId, const QString& displayName,
               const QString& iconName, QWidget* parent = nullptr)
        : QWidget(parent), m_id(pageId), m_displayName(displayName),
          m_iconName(iconName) {}

    QString id() const override          { return m_id; }
    QString displayName() const override { return m_displayName; }
    QIcon icon() const override          { return QIcon::fromTheme(m_iconName); }
    bool shouldDisplay() const override  { return true; }

  private:
    QString m_id;
    QString m_displayName;
    QString m_iconName;
};
```

**Key facts:**

- **Dual inheritance**: `QWidget` (for the widget tree) and `BasePage` (for the
  instance settings dialog's page protocol).
- **`id()`**: The page identifier. Must match the `page_id` passed to
  `ui_register_instance_action()` if you want toolbar buttons to navigate to
  this page.
- **`displayName()`**: Shown in the instance settings sidebar.
- **`icon()`**: Resolved from the system/freedesktop icon theme via
  `QIcon::fromTheme()`. Falls back to `"plugin"` if you pass `nullptr` to
  `ui_page_create()`.
- **`shouldDisplay()`**: Always returns `true`. Plugin pages are always visible.
- **No `openedImpl()` / `closedImpl()` overrides**: Unlike a hand-written
  `BasePage` subclass (Section 11), `PluginPage` does not run custom logic on
  page open/close. If you need lifecycle callbacks, use the hook approach.

The `void*` returned by `ui_page_create()` is actually a `QWidget*` obtained
via `static_cast<QWidget*>(page)`. This means all layout and widget calls that
accept a `void* parent` can take this handle — and Qt's ownership model will
attach child widgets to the page automatically.

---

## S11 Hook Approach vs. S13 C API Approach

Both Section 11 and Section 13 add pages to the same instance settings dialog.
They use the **same hook** (`MMCO_HOOK_UI_INSTANCE_PAGES`, `0x0602`) and the
**same page list**. The difference is how you build the page widget.

### Section 11: Full Qt Power

In Section 11, you write a `QWidget` + `BasePage` subclass in C++, include
Qt headers, use `.ui` files from Qt Designer, and link against Qt libraries.
You `#include <QWidget>`, `#include <QVBoxLayout>`, etc. Your plugin's
`.mmco` binary depends on the exact same Qt version MeshMC was compiled
against.

```cpp
// S11 approach: full Qt, full control
#include <QWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include "ui/pages/BasePage.h"

class MyPage : public QWidget, public BasePage {
    Q_OBJECT
  public:
    MyPage(QWidget* parent) : QWidget(parent) {
        auto* layout = new QVBoxLayout(this);
        auto* btn = new QPushButton("Click me", this);
        layout->addWidget(btn);
        connect(btn, &QPushButton::clicked, this, &MyPage::onClicked);
    }
    QString id() const override { return "my-plugin-page"; }
    QString displayName() const override { return "My Plugin"; }
    QIcon icon() const override { return QIcon::fromTheme("plugin"); }
    bool shouldDisplay() const override { return true; }

  private slots:
    void onClicked() { /* full Qt signal/slot */ }
};
```

### Section 13: Pure C API

In Section 13, you call `ui_page_create()` to get a blank `PluginPage`, then
construct the UI by calling layout and widget functions on it. No Qt headers.
No Qt link dependency in your plugin binary.

```cpp
// S13 approach: pure C API, no Qt includes
void build_page(MMCOContext* ctx, MMCOInstancePagesEvent* evt)
{
    void* mh = ctx->module_handle;

    // 1. Create the page
    void* page = ctx->ui_page_create(mh, "my-plugin-page",
                                     "My Plugin", "plugin");

    // 2. Create a layout
    void* layout = ctx->ui_layout_create(mh, page, 0); // 0 = vertical

    // 3. Add a button
    void* btn = ctx->ui_button_create(mh, page, "Click me", nullptr,
                                      on_button_clicked, nullptr);
    ctx->ui_layout_add_widget(mh, layout, btn);

    // 4. Assign layout to page
    ctx->ui_page_set_layout(mh, page, layout);

    // 5. Add to the instance dialog's page list
    ctx->ui_page_add_to_list(mh, page, evt->page_list_handle);
}
```

### Comparison Table

| Aspect | S11 (Hook + Qt) | S13 (C API Page Builder) |
|--------|-----------------|--------------------------|
| **Qt headers required** | Yes | No |
| **Qt link dependency** | Yes (same Qt version as MeshMC) | No |
| **ABI coupling** | Tight — Qt ABI must match | Loose — only `MMCOContext` struct |
| **Widget set** | Unlimited — any `QWidget` subclass | Limited — labels, buttons, trees |
| **Layout options** | Any `QLayout` subclass | `QVBoxLayout` or `QHBoxLayout` |
| **Signals & slots** | Full `QObject::connect()` | Callback function pointers |
| **Lifecycle hooks** | `openedImpl()`, `closedImpl()` | Not available |
| **Qt Designer support** | Yes (`.ui` files) | No |
| **Complexity** | Higher — C++ / Qt knowledge needed | Lower — C function calls only |
| **Binary portability** | Fragile across Qt versions | Stable across MeshMC builds |
| **Page appearance** | Identical (you control everything) | Identical (real Qt widgets behind the API) |

### When to Use Each

**Use Section 13 (C API Page Builder) when:**

- Your page has a simple structure: headings, buttons, and a list/table
- You want to avoid Qt header/library dependencies entirely
- You are writing a plugin in pure C or want maximum ABI stability
- Your page is informational (status display, simple controls)
- A typical "settings page" with a few buttons and a table is all you need

**Use Section 11 (Hook Approach) when:**

- You need widgets not exposed by S13 (e.g., `QLineEdit`, `QCheckBox`,
  `QComboBox`, `QProgressBar`, `QTabWidget`, custom painting)
- You want lifecycle callbacks (`openedImpl()` / `closedImpl()`)
- You need complex layouts (grid, form, stacked, splitter)
- You want to use Qt Designer `.ui` files
- You need signal/slot chains between widgets

**You can also combine both**: use S13 to create the `PluginPage` (getting the
`BasePage` contract for free), then cast the returned `void*` back to
`QWidget*` in an S11-style hook to do additional Qt manipulation. This is an
advanced technique — see [Hybrid Approach](#hybrid-approach) below.

---

## Hybrid Approach

A plugin can use S13 to create the page and register it, then use the returned
`void*` as a `QWidget*` parent for Qt-based child widgets:

```cpp
#include <QWidget>
#include <QVBoxLayout>
#include <QLineEdit>

static int on_instance_pages(void* mh, uint32_t /*hook_id*/,
                             void* payload, void* ud)
{
    auto* ctx = static_cast<MMCOContext*>(ud);
    auto* evt = static_cast<MMCOInstancePagesEvent*>(payload);

    // Use S13 to create the page (gets BasePage for free)
    void* page = ctx->ui_page_create(mh, "hybrid-page", "Hybrid", "plugin");

    // Cast to QWidget* for direct Qt access
    auto* pageWidget = static_cast<QWidget*>(page);
    auto* layout = new QVBoxLayout(pageWidget);
    auto* edit = new QLineEdit(pageWidget);
    layout->addWidget(edit);

    // Use S13 to add to the page list
    ctx->ui_page_add_to_list(mh, page, evt->page_list_handle);
    return 0;
}
```

This is useful when you want to avoid writing a full `BasePage` subclass but
still need Qt widgets beyond labels, buttons, and trees. The trade-off: you
regain Qt header/library dependencies.

---

## Sub-Pages

Detailed documentation for each part of the S13 API:

| Document | Contents |
|----------|----------|
| [Page Creation](page-creation.md) | `ui_page_create()`, `ui_page_add_to_list()`, `ui_page_set_layout()` — creating a page, adding it to the instance dialog, and assigning its root layout |
| [Layout System](layout-system.md) | `ui_layout_create()`, `ui_layout_add_widget()`, `ui_layout_add_layout()`, `ui_layout_add_spacer()` — building layout hierarchies |

Widget creation functions (`ui_button_create`, `ui_label_create`,
`ui_tree_create`, and their accessors) are documented in-line within the layout
system page, as they are always used inside a layout context.

---

## Complete Example: Simple Info Page

A plugin that adds an "About" page to instance settings showing the plugin
version and a link button:

```cpp
#include "plugin/sdk/mmco_sdk.h"
#include <cstdio>

MMCO_DEFINE_MODULE("InfoPageDemo", "1.0.0", "Example Author",
                   "Demonstrates S13 page builder", "GPL-3.0-or-later");

static MMCOContext* g_ctx = nullptr;

static void on_website_clicked(void* /*ud*/)
{
    // In a real plugin, you'd open a URL.
    // For this demo, just log a message.
    MMCO_LOG(g_ctx, "Website button clicked!");
}

static int on_instance_pages(void* mh, uint32_t /*hook_id*/,
                             void* payload, void* /*ud*/)
{
    auto* evt = static_cast<MMCOInstancePagesEvent*>(payload);
    if (!evt || !evt->page_list_handle)
        return 0;

    /* === Build the page === */

    // 1. Page
    void* page = g_ctx->ui_page_create(mh, "info-demo",
                                       "Info Demo", "help-about");
    if (!page)
        return 0;

    // 2. Root layout (vertical)
    void* root = g_ctx->ui_layout_create(mh, page, 0);

    // 3. Title label
    void* title = g_ctx->ui_label_create(mh, page, "InfoPageDemo v1.0.0");
    g_ctx->ui_layout_add_widget(mh, root, title);

    // 4. Description label
    void* desc = g_ctx->ui_label_create(mh, page,
        "This plugin demonstrates the Section 13 UI Page Builder API.");
    g_ctx->ui_layout_add_widget(mh, root, desc);

    // 5. Instance info label
    char buf[512];
    snprintf(buf, sizeof(buf), "Current instance: %s (%s)",
             evt->instance_name, evt->instance_id);
    void* info = g_ctx->ui_label_create(mh, page, buf);
    g_ctx->ui_layout_add_widget(mh, root, info);

    // 6. Spacer pushes the button to the bottom
    g_ctx->ui_layout_add_spacer(mh, root, 0); // 0 = vertical spacer

    // 7. Button row (horizontal layout inside the vertical root)
    void* btnRow = g_ctx->ui_layout_create(mh, page, 1); // 1 = horizontal
    g_ctx->ui_layout_add_spacer(mh, btnRow, 1); // horizontal spacer (push right)
    void* btn = g_ctx->ui_button_create(mh, page, "Visit Website",
                                        "internet-web-browser",
                                        on_website_clicked, nullptr);
    g_ctx->ui_layout_add_widget(mh, btnRow, btn);
    g_ctx->ui_layout_add_layout(mh, root, btnRow);

    // 8. Set layout and add to list
    g_ctx->ui_page_set_layout(mh, page, root);
    g_ctx->ui_page_add_to_list(mh, page, evt->page_list_handle);

    return 0;
}

extern "C" {

MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;
    ctx->hook_register(ctx->module_handle, MMCO_HOOK_UI_INSTANCE_PAGES,
                       on_instance_pages, nullptr);
    MMCO_LOG(ctx, "InfoPageDemo loaded.");
    return 0;
}

MMCO_EXPORT void mmco_unload()
{
    MMCO_LOG(g_ctx, "InfoPageDemo unloaded.");
}

} // extern "C"
```

---

## Error Handling Summary

All S13 functions that return `int` use the convention:

| Return Value | Meaning |
|---|---|
| `0` | Success |
| `-1` | Error (null handle, invalid argument) |

All S13 functions that return `void*` use the convention:

| Return Value | Meaning |
|---|---|
| Non-null | Valid opaque handle |
| `nullptr` | Error (null/empty required argument) |

The `mh` parameter is accepted but currently unused by all S13 functions (it
is reserved for future per-module validation). Passing `nullptr` for `mh` will
not cause a crash, but you should always pass `ctx->module_handle` for forward
compatibility.

---

## Thread Safety

All S13 functions **must be called from the main (GUI) thread**. Since hook
callbacks are dispatched on the main thread, this is naturally satisfied when
building pages inside an `MMCO_HOOK_UI_INSTANCE_PAGES` callback. Do not call
S13 functions from background HTTP callbacks or other non-GUI threads.
