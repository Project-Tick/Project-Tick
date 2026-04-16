# Layout System — Layouts, Widgets, and Spacers

> **Section 13 sub-page.**
> Creating layout hierarchies with `ui_layout_create()`, populating them with
> widgets and child layouts via `ui_layout_add_widget()` /
> `ui_layout_add_layout()`, and controlling spacing with
> `ui_layout_add_spacer()`.

---

## Table of Contents

- [`ui_layout_create()`](#ui_layout_create)
  - [Signature](#signature)
  - [Parameters](#parameters)
  - [Layout Types](#layout-types)
  - [Return Value](#return-value)
  - [How It Works](#how-it-works)
  - [Error Conditions](#error-conditions)
  - [Examples](#examples)
- [`ui_layout_add_widget()`](#ui_layout_add_widget)
  - [Signature](#signature-1)
  - [Parameters](#parameters-1)
  - [Return Value](#return-value-1)
  - [How It Works](#how-it-works-1)
  - [Widget Types You Can Add](#widget-types-you-can-add)
  - [Error Conditions](#error-conditions-1)
  - [Examples](#examples-1)
- [`ui_layout_add_layout()`](#ui_layout_add_layout)
  - [Signature](#signature-2)
  - [Parameters](#parameters-2)
  - [Return Value](#return-value-2)
  - [How It Works](#how-it-works-2)
  - [Nesting Patterns](#nesting-patterns)
  - [Error Conditions](#error-conditions-2)
  - [Examples](#examples-2)
- [`ui_layout_add_spacer()`](#ui_layout_add_spacer)
  - [Signature](#signature-3)
  - [Parameters](#parameters-3)
  - [Return Value](#return-value-3)
  - [How It Works](#how-it-works-3)
  - [Spacer Direction Reference](#spacer-direction-reference)
  - [Error Conditions](#error-conditions-3)
  - [Examples](#examples-3)
- [Widget Creation Functions](#widget-creation-functions)
  - [`ui_button_create()`](#ui_button_create)
  - [`ui_button_set_enabled()`](#ui_button_set_enabled)
  - [`ui_button_set_text()`](#ui_button_set_text)
  - [`ui_label_create()`](#ui_label_create)
  - [`ui_label_set_text()`](#ui_label_set_text)
  - [`ui_tree_create()`](#ui_tree_create)
  - [`ui_tree_clear()`](#ui_tree_clear)
  - [`ui_tree_add_row()`](#ui_tree_add_row)
  - [`ui_tree_selected_row()`](#ui_tree_selected_row)
  - [`ui_tree_set_row_data()` / `ui_tree_get_row_data()`](#ui_tree_set_row_data--ui_tree_get_row_data)
  - [`ui_tree_row_count()`](#ui_tree_row_count)
- [Layout Composition Cookbook](#layout-composition-cookbook)
  - [Vertical Stack](#vertical-stack)
  - [Horizontal Button Bar](#horizontal-button-bar)
  - [Sidebar + Content (Horizontal Split)](#sidebar--content-horizontal-split)
  - [Header / Content / Footer](#header--content--footer)
  - [Right-Aligned Buttons](#right-aligned-buttons)

---

## `ui_layout_create`

Creates a box layout — either vertical (`QVBoxLayout`) or horizontal
(`QHBoxLayout`).

### Signature

```c
void* (*ui_layout_create)(void* mh,
                          void* parent,
                          int type);
```

From `PluginAPI.h`:

```c
/* Layouts: type 0=vertical, 1=horizontal */
void* (*ui_layout_create)(void* mh, void* parent, int type);
```

### Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `mh` | `void*` | Yes | Module handle. Reserved. |
| `parent` | `void*` | No | Page or widget handle to associate with. Currently accepted but **not used** — the layout is not immediately installed on the parent. Use `ui_page_set_layout()` to install a root layout. Pass `nullptr` or the page handle. |
| `type` | `int` | **Yes** | Layout direction: `0` = vertical, `1` = horizontal. |

### Layout Types

| `type` Value | Qt Class Created | Direction | Items Flow |
|---|---|---|---|
| `0` | `QVBoxLayout` | Vertical | Top to bottom |
| `1` | `QHBoxLayout` | Horizontal | Left to right |
| Any other value | `QVBoxLayout` | Vertical | Top to bottom (fallback) |

There is **no grid layout** or **form layout** exposed by S13. If you need a
grid or form layout, use the S11 hook approach with direct Qt access, or
simulate a grid by nesting horizontal layouts inside a vertical layout.

**Simulating a two-column form with nested layouts:**

```
Vertical root layout
├── Horizontal row 1
│   ├── Label "Name:"
│   └── Label (value)
├── Horizontal row 2
│   ├── Label "Version:"
│   └── Label (value)
└── ...
```

### Return Value

| Value | Meaning |
|-------|---------|
| Non-null `void*` | Opaque handle to a `QBoxLayout*`. |
| *(never null)* | This function always succeeds — it does not validate `parent` or `type`. |

> **Note:** Unlike other S13 functions, `ui_layout_create()` currently never
> returns `nullptr`. It always allocates a layout. However, you should still
> check for null returns for forward compatibility.

### How It Works

The implementation:

```cpp
void* PluginManager::api_ui_layout_create(void* mh, void* parent, int type)
{
    (void)mh;
    QWidget* pw = parent ? static_cast<QWidget*>(parent) : nullptr;
    QBoxLayout* layout;
    if (type == 1)
        layout = new QHBoxLayout();
    else
        layout = new QVBoxLayout();
    // Don't set on parent yet — let page_set_layout do that
    (void)pw;
    return layout;
}
```

Key observations:

1. **`parent` is accepted but not used.** The parent widget pointer is cast but
   then deliberately discarded (`(void)pw`). The layout is created as a
   **free-standing** object, not installed on any widget. This is intentional:
   `ui_page_set_layout()` handles the root layout assignment, and
   `ui_layout_add_layout()` handles nesting.

2. **Type fallback**: Any value other than `1` creates a vertical layout.
   There is no error for out-of-range values.

3. **Memory ownership**: The layout is a heap-allocated `QBoxLayout*`. It
   becomes owned by Qt once it is installed on a widget (via
   `ui_page_set_layout()`) or added to another layout (via
   `ui_layout_add_layout()`). If you create a layout but never install it,
   it is a **memory leak**.

### Error Conditions

| Condition | Behavior |
|-----------|----------|
| `parent` is `nullptr` | Succeeds (parent is not used) |
| `type` is not 0 or 1 | Falls back to vertical (`QVBoxLayout`) |
| `mh` is `nullptr` | Succeeds |
| Created but never installed | Memory leak (no crash) |

### Examples

**Create a vertical layout:**

```cpp
void* root = ctx->ui_layout_create(mh, page, 0); // vertical
```

**Create a horizontal layout for a button bar:**

```cpp
void* btn_bar = ctx->ui_layout_create(mh, page, 1); // horizontal
```

**Recommended pattern — pass page as parent for documentation purposes:**

```cpp
// The parent parameter is not used, but passing the page makes
// the code self-documenting about which page owns this layout.
void* layout = ctx->ui_layout_create(mh, page, 0);
```

---

## `ui_layout_add_widget`

Adds a widget (button, label, tree, or any handle returned by a widget
creation function) to a layout.

### Signature

```c
int (*ui_layout_add_widget)(void* mh,
                            void* layout,
                            void* widget);
```

### Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `mh` | `void*` | Yes | Module handle. Reserved. |
| `layout` | `void*` | **Yes** | Layout handle from `ui_layout_create()`. |
| `widget` | `void*` | **Yes** | Widget handle from any `ui_*_create()` function. |

### Return Value

| Value | Meaning |
|-------|---------|
| `0` | Widget successfully added |
| `-1` | `layout` or `widget` is `nullptr` |

### How It Works

```cpp
int PluginManager::api_ui_layout_add_widget(void* mh, void* layout,
                                            void* widget)
{
    (void)mh;
    if (!layout || !widget)
        return -1;
    auto* l = static_cast<QBoxLayout*>(layout);
    l->addWidget(static_cast<QWidget*>(widget));
    return 0;
}
```

This calls `QBoxLayout::addWidget()`, which:

1. **Reparents** the widget to the layout's parent widget (if the layout has
   been installed). If the layout is still free-standing, the reparenting
   happens when the layout is later installed.

2. **Appends** the widget to the end of the layout. Widgets appear in the
   order they were added.

3. **Takes ownership**: Qt owns the widget's lifetime through the parent–child
   relationship.

### Widget Types You Can Add

Any `void*` returned by these S13 functions is a valid `QWidget*` and can be
added to a layout:

| Function | Creates | Underlying Qt Type |
|---|---|---|
| `ui_button_create()` | Push button | `QPushButton` |
| `ui_label_create()` | Text label | `QLabel` |
| `ui_tree_create()` | Tabular tree/list | `QTreeWidget` |
| `ui_page_create()` | Page widget | `PluginPage` (not typically added to layouts) |

You should **not** add a page to a layout — pages are top-level containers,
not child widgets. However, it is technically possible (it embeds one page
widget inside another).

### Error Conditions

| Condition | Return | Behavior |
|-----------|--------|----------|
| `layout` is `nullptr` | `-1` | — |
| `widget` is `nullptr` | `-1` | — |
| `widget` is already in another layout | `0` | Qt reparents it (removes from old layout) |
| `layout` is not a `QBoxLayout*` | Undefined | Incorrect cast |

### Examples

```cpp
void* layout = ctx->ui_layout_create(mh, page, 0);
void* label = ctx->ui_label_create(mh, page, "Status: OK");
void* btn = ctx->ui_button_create(mh, page, "Refresh", nullptr,
                                  on_refresh, nullptr);

ctx->ui_layout_add_widget(mh, layout, label);
ctx->ui_layout_add_widget(mh, layout, btn);
// Layout now has: [label] above [button] (vertical)
```

---

## `ui_layout_add_layout`

Nests a child layout inside a parent layout. This is how you build complex
UI structures from simple horizontal and vertical building blocks.

### Signature

```c
int (*ui_layout_add_layout)(void* mh,
                            void* parent_layout,
                            void* child_layout);
```

### Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `mh` | `void*` | Yes | Module handle. Reserved. |
| `parent_layout` | `void*` | **Yes** | Parent layout handle. |
| `child_layout` | `void*` | **Yes** | Child layout handle to nest inside the parent. |

### Return Value

| Value | Meaning |
|-------|---------|
| `0` | Child layout successfully added |
| `-1` | Either parameter is `nullptr` |

### How It Works

```cpp
int PluginManager::api_ui_layout_add_layout(void* mh, void* parent,
                                            void* child)
{
    (void)mh;
    if (!parent || !child)
        return -1;
    auto* p = static_cast<QBoxLayout*>(parent);
    p->addLayout(static_cast<QLayout*>(child));
    return 0;
}
```

`QBoxLayout::addLayout()` nests the child layout. The child layout's widgets
will be arranged according to the child's direction (vertical or horizontal),
and the child itself occupies one "slot" in the parent layout.

**Important**: Once a layout is added to a parent, it is **owned** by the
parent. Do not add the same layout to multiple parents.

### Nesting Patterns

The power of S13 comes from nesting. By combining vertical and horizontal
layouts, you can build most common UI patterns:

**Pattern 1: Horizontal row inside a vertical stack**

```
Vertical (root)
├── Label "Title"
├── Tree widget
└── Horizontal (button row)
    ├── Button "Create"
    ├── Button "Delete"
    └── Spacer (pushes buttons left)
```

```cpp
void* root = ctx->ui_layout_create(mh, page, 0);       // vertical
void* btn_row = ctx->ui_layout_create(mh, page, 1);     // horizontal

ctx->ui_layout_add_widget(mh, root, title_label);
ctx->ui_layout_add_widget(mh, root, tree);
ctx->ui_layout_add_widget(mh, btn_row, create_btn);
ctx->ui_layout_add_widget(mh, btn_row, delete_btn);
ctx->ui_layout_add_spacer(mh, btn_row, 1);              // horizontal spacer
ctx->ui_layout_add_layout(mh, root, btn_row);           // nest btn_row in root
```

**Pattern 2: Two-column split**

```
Horizontal (root)
├── Vertical (left column)
│   ├── Label "Categories"
│   └── Tree (category list)
└── Vertical (right column)
    ├── Label "Details"
    └── Label (detail text)
```

```cpp
void* root = ctx->ui_layout_create(mh, page, 1);    // horizontal
void* left = ctx->ui_layout_create(mh, page, 0);    // vertical
void* right = ctx->ui_layout_create(mh, page, 0);   // vertical

ctx->ui_layout_add_widget(mh, left, cat_label);
ctx->ui_layout_add_widget(mh, left, cat_tree);
ctx->ui_layout_add_widget(mh, right, detail_label);
ctx->ui_layout_add_widget(mh, right, detail_text);

ctx->ui_layout_add_layout(mh, root, left);
ctx->ui_layout_add_layout(mh, root, right);
```

### Error Conditions

| Condition | Return | Behavior |
|-----------|--------|----------|
| `parent_layout` is `nullptr` | `-1` | — |
| `child_layout` is `nullptr` | `-1` | — |
| `child_layout` already has a parent | `0` | Qt reparents (removes from old parent) |
| Circular nesting (parent added to child) | Undefined | Do not do this |

### Examples

```cpp
void* root = ctx->ui_layout_create(mh, page, 0);
void* row = ctx->ui_layout_create(mh, page, 1);

void* left_btn = ctx->ui_button_create(mh, page, "Left", nullptr, nullptr, nullptr);
void* right_btn = ctx->ui_button_create(mh, page, "Right", nullptr, nullptr, nullptr);

ctx->ui_layout_add_widget(mh, row, left_btn);
ctx->ui_layout_add_widget(mh, row, right_btn);
ctx->ui_layout_add_layout(mh, root, row);
```

---

## `ui_layout_add_spacer`

Inserts an expanding spacer item into a layout. Spacers push adjacent items
to one side, consuming all available empty space.

### Signature

```c
int (*ui_layout_add_spacer)(void* mh,
                            void* layout,
                            int horizontal);
```

### Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `mh` | `void*` | Yes | Module handle. Reserved. |
| `layout` | `void*` | **Yes** | Layout handle to add the spacer to. |
| `horizontal` | `int` | **Yes** | Spacer expansion direction: `0` = vertical, `1` = horizontal. |

### Return Value

| Value | Meaning |
|-------|---------|
| `0` | Spacer successfully added |
| `-1` | `layout` is `nullptr` |

### How It Works

```cpp
int PluginManager::api_ui_layout_add_spacer(void* mh, void* layout,
                                            int horizontal)
{
    (void)mh;
    if (!layout)
        return -1;
    auto* l = static_cast<QBoxLayout*>(layout);
    if (horizontal)
        l->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding,
                                   QSizePolicy::Minimum));
    else
        l->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum,
                                   QSizePolicy::Expanding));
    return 0;
}
```

The function creates a `QSpacerItem` with:

- **Horizontal spacer** (`horizontal=1`): width expands, height stays minimum.
  Use in horizontal layouts to push items left or right.
- **Vertical spacer** (`horizontal=0`): height expands, width stays minimum.
  Use in vertical layouts to push items up or down.

The initial size is `(0, 0)` — the spacer starts invisible and only occupies
space that other widgets don't claim.

### Spacer Direction Reference

| `horizontal` | Expansion | Use In | Effect |
|---|---|---|---|
| `0` | Vertical (↕) | Vertical layout | Pushes items above it upward, items below it downward |
| `1` | Horizontal (↔) | Horizontal layout | Pushes items before it leftward, items after it rightward |

**Common spacer patterns:**

| Pattern | Implementation |
|---------|---------------|
| Push all content to the top | Add vertical spacer (`0`) as last item in vertical layout |
| Push all content to the bottom | Add vertical spacer (`0`) as first item in vertical layout |
| Right-align buttons | Add horizontal spacer (`1`) before the buttons in horizontal layout |
| Left-align buttons | Add horizontal spacer (`1`) after the buttons in horizontal layout |
| Center content | Add horizontal spacer (`1`) before AND after the content |
| Separate header from footer | Add vertical spacer (`0`) between header and footer sections |

### Error Conditions

| Condition | Return | Behavior |
|-----------|--------|----------|
| `layout` is `nullptr` | `-1` | — |
| Wrong spacer direction for layout type | `0` | Works but has no visual effect (e.g. horizontal spacer in a vertical layout does not expand) |
| `mh` is `nullptr` | `0` | Succeeds |

### Examples

**Push content to the top of a vertical layout:**

```cpp
void* root = ctx->ui_layout_create(mh, page, 0);

ctx->ui_layout_add_widget(mh, root, title_label);
ctx->ui_layout_add_widget(mh, root, desc_label);
ctx->ui_layout_add_spacer(mh, root, 0); // vertical — eats remaining space
```

Result:
```
┌────────────────────┐
│ Title              │
│ Description        │
│                    │  ← spacer expands to fill
│                    │
│                    │
└────────────────────┘
```

**Right-align buttons in a horizontal layout:**

```cpp
void* row = ctx->ui_layout_create(mh, page, 1);

ctx->ui_layout_add_spacer(mh, row, 1);  // horizontal — pushes buttons right
ctx->ui_layout_add_widget(mh, row, ok_btn);
ctx->ui_layout_add_widget(mh, row, cancel_btn);
```

Result:
```
┌──────────────────────────────────────┐
│                        [OK] [Cancel] │
└──────────────────────────────────────┘
```

**Center a single widget:**

```cpp
void* row = ctx->ui_layout_create(mh, page, 1);

ctx->ui_layout_add_spacer(mh, row, 1);  // left spacer
ctx->ui_layout_add_widget(mh, row, centered_btn);
ctx->ui_layout_add_spacer(mh, row, 1);  // right spacer
```

Result:
```
┌──────────────────────────────────────┐
│          [Centered Button]           │
└──────────────────────────────────────┘
```

---

## Widget Creation Functions

These functions create the actual interactive and display widgets that are
placed inside layouts. All return opaque `void*` handles.

---

### `ui_button_create`

Creates a push button with optional text, icon, and click callback.

**Signature:**

```c
void* (*ui_button_create)(void* mh,
                          void* parent,
                          const char* text,
                          const char* icon_name,
                          MMCOButtonCallback callback,
                          void* user_data);
```

**Callback type:**

```c
typedef void (*MMCOButtonCallback)(void* user_data);
```

**Parameters:**

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `mh` | `void*` | Yes | Module handle. Reserved. |
| `parent` | `void*` | No | Parent widget handle. Sets the Qt parent for memory management. Typically the page handle. If `nullptr`, the button has no parent until added to a layout. |
| `text` | `const char*` | No | Button label text. `nullptr` or `""` creates a button with no text. |
| `icon_name` | `const char*` | No | Freedesktop theme icon name. `nullptr` or `""` means no icon. |
| `callback` | `MMCOButtonCallback` | No | Function called when the button is clicked. `nullptr` means no callback (button does nothing on click). |
| `user_data` | `void*` | No | Opaque pointer passed to the callback. |

**Return:** Non-null `void*` (always succeeds — a `QPushButton*` is always created).

**Implementation detail:** The callback is connected via `QObject::connect()`:

```cpp
if (cb) {
    QObject::connect(btn, &QPushButton::clicked, [cb, ud]() { cb(ud); });
}
```

The callback fires on the **main (GUI) thread** — it is a Qt signal/slot
connection. You can safely call other S13 functions from within the callback.

**Example:**

```cpp
static void on_save_clicked(void* ud)
{
    auto* ctx = static_cast<MMCOContext*>(ud);
    MMCO_LOG(ctx, "Save button clicked");
}

void* btn = ctx->ui_button_create(mh, page, "Save", "document-save",
                                  on_save_clicked, ctx);
```

---

### `ui_button_set_enabled`

Enables or disables a button. Disabled buttons are grayed out and do not
fire callbacks.

**Signature:**

```c
int (*ui_button_set_enabled)(void* mh, void* button, int enabled);
```

**Parameters:**

| Parameter | Type | Description |
|-----------|------|-------------|
| `button` | `void*` | Button handle from `ui_button_create()`. |
| `enabled` | `int` | `0` = disabled, non-zero = enabled. |

**Return:** `0` on success, `-1` if `button` is `nullptr`.

**Example:**

```cpp
// Disable until a tree row is selected
ctx->ui_button_set_enabled(mh, delete_btn, 0);

// In selection callback:
static void on_selection(void* ud, int row) {
    ctx->ui_button_set_enabled(ctx->module_handle, delete_btn, row >= 0 ? 1 : 0);
}
```

---

### `ui_button_set_text`

Changes the text label of an existing button.

**Signature:**

```c
int (*ui_button_set_text)(void* mh, void* button, const char* text);
```

**Return:** `0` on success, `-1` if `button` is `nullptr`.

**Example:**

```cpp
ctx->ui_button_set_text(mh, btn, "Processing...");
// After operation:
ctx->ui_button_set_text(mh, btn, "Save");
```

---

### `ui_label_create`

Creates a text label.

**Signature:**

```c
void* (*ui_label_create)(void* mh, void* parent, const char* text);
```

**Parameters:**

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `parent` | `void*` | No | Parent widget. Sets Qt parent. |
| `text` | `const char*` | No | Label text. `nullptr` creates an empty label. |

**Return:** Non-null `void*` (always succeeds — a `QLabel*` is always created).

**Implementation:**

```cpp
void* PluginManager::api_ui_label_create(void* mh, void* parent,
                                         const char* text)
{
    (void)mh;
    auto* lbl = new QLabel(text ? QString::fromUtf8(text) : QString());
    if (parent)
        lbl->setParent(static_cast<QWidget*>(parent));
    return lbl;
}
```

Labels support plain text only through this API. HTML/rich text is not
supported. If you need styled text, use the S11 approach with direct
`QLabel::setTextFormat(Qt::RichText)`.

**Example:**

```cpp
void* title = ctx->ui_label_create(mh, page, "Backup Manager v2.0");
void* desc = ctx->ui_label_create(mh, page,
    "Create and manage backups of your Minecraft instances.");
```

---

### `ui_label_set_text`

Changes the text of an existing label.

**Signature:**

```c
int (*ui_label_set_text)(void* mh, void* label, const char* text);
```

**Return:** `0` on success, `-1` if `label` is `nullptr`.

**Example:**

```cpp
char buf[128];
snprintf(buf, sizeof(buf), "Backups: %d", count);
ctx->ui_label_set_text(mh, count_label, buf);
```

---

### `ui_tree_create`

Creates a `QTreeWidget` configured as a flat tabular list with column headers,
sorting, alternating row colors, and single selection. This is the most
powerful widget available in S13.

**Signature:**

```c
void* (*ui_tree_create)(void* mh,
                        void* parent,
                        const char** column_names,
                        int column_count,
                        MMCOTreeSelectionCallback on_select,
                        void* user_data);
```

**Callback type:**

```c
typedef void (*MMCOTreeSelectionCallback)(void* user_data, int row);
```

The `row` parameter is the zero-based index of the newly selected row, or
`-1` if the selection was cleared.

**Parameters:**

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `parent` | `void*` | No | Parent widget. |
| `column_names` | `const char**` | Yes | Array of column header strings. |
| `column_count` | `int` | Yes | Number of columns (length of `column_names` array). |
| `on_select` | `MMCOTreeSelectionCallback` | No | Called when selection changes. `nullptr` = no callback. |
| `user_data` | `void*` | No | Passed to the selection callback. |

**Return:** Non-null `void*` — a `QTreeWidget*`.

**Implementation details and automatically applied configuration:**

```cpp
tree->setRootIsDecorated(false);           // no expand/collapse arrows
tree->setSortingEnabled(true);             // clickable column headers
tree->setAlternatingRowColors(true);       // zebra striping
tree->setSelectionMode(QAbstractItemView::SingleSelection);
```

Column sizing:
- **First column**: stretches to fill available space (`QHeaderView::Stretch`)
- **Remaining columns**: sized to fit content (`QHeaderView::ResizeToContents`)

This makes the tree ideal for data tables: a primary column (e.g. name)
stretches, while metadata columns (date, size, status) auto-fit.

**Example:**

```cpp
const char* cols[] = {"Backup Name", "Date", "Size"};
void* tree = ctx->ui_tree_create(mh, page, cols, 3,
                                 on_backup_selected, my_data);
```

---

### `ui_tree_clear`

Removes all rows from a tree widget.

**Signature:**

```c
int (*ui_tree_clear)(void* mh, void* tree);
```

**Return:** `0` on success, `-1` if `tree` is `nullptr`.

Use this before repopulating the tree:

```cpp
ctx->ui_tree_clear(mh, tree);
// Re-add rows:
ctx->ui_tree_add_row(mh, tree, new_vals, ncols);
```

---

### `ui_tree_add_row`

Appends a row to the tree widget with values for each column.

**Signature:**

```c
int (*ui_tree_add_row)(void* mh,
                       void* tree,
                       const char** values,
                       int col_count);
```

**Parameters:**

| Parameter | Type | Description |
|-----------|------|-------------|
| `tree` | `void*` | Tree handle. |
| `values` | `const char**` | Array of strings, one per column. `nullptr` entries produce empty cells. |
| `col_count` | `int` | Number of values (should match column count). |

**Return:** The zero-based row index of the newly added row, or `-1` if `tree`
is `nullptr`.

**Implementation:**

```cpp
int PluginManager::api_ui_tree_add_row(void* mh, void* tree,
                                       const char** vals, int ncols)
{
    (void)mh;
    if (!tree)
        return -1;
    auto* tw = static_cast<QTreeWidget*>(tree);
    auto* item = new QTreeWidgetItem(tw);
    for (int i = 0; i < ncols; ++i)
        item->setText(i, vals[i] ? QString::fromUtf8(vals[i]) : QString());
    return tw->indexOfTopLevelItem(item);
}
```

The returned row index can be used with `ui_tree_set_row_data()` to
immediately tag the row with associated data.

**Example:**

```cpp
const char* vals[] = {"World Backup", "2026-04-15", "89 MB"};
int row = ctx->ui_tree_add_row(mh, tree, vals, 3);
// Tag the row with the backup's internal ID
ctx->ui_tree_set_row_data(mh, tree, row, backup_id);
```

---

### `ui_tree_selected_row`

Returns the index of the currently selected row.

**Signature:**

```c
int (*ui_tree_selected_row)(void* mh, void* tree);
```

**Return:** Zero-based row index, or `-1` if nothing is selected or `tree` is
`nullptr`.

**Example:**

```cpp
int row = ctx->ui_tree_selected_row(mh, tree);
if (row >= 0) {
    int64_t id = ctx->ui_tree_get_row_data(mh, tree, row);
    // Operate on backup with this id
}
```

---

### `ui_tree_set_row_data` / `ui_tree_get_row_data`

Attach and retrieve an `int64_t` tag on a specific row. This is stored in the
row's `Qt::UserRole` data on column 0.

**Signatures:**

```c
int (*ui_tree_set_row_data)(void* mh, void* tree, int row, int64_t data);
int64_t (*ui_tree_get_row_data)(void* mh, void* tree, int row);
```

**`ui_tree_set_row_data` returns:** `0` on success, `-1` if `tree` is null or
`row` is out of range.

**`ui_tree_get_row_data` returns:** The stored `int64_t`, or `0` if `tree` is
null or `row` is out of range.

**Use case:** Associate a database ID, file index, or other numeric identifier
with each row so callbacks can identify which data entry corresponds to the
selected row without parsing the display strings.

**Example:**

```cpp
// When adding rows:
int row = ctx->ui_tree_add_row(mh, tree, vals, 3);
ctx->ui_tree_set_row_data(mh, tree, row, (int64_t)backup->id);

// When operating on the selection:
int sel = ctx->ui_tree_selected_row(mh, tree);
if (sel >= 0) {
    int64_t backup_id = ctx->ui_tree_get_row_data(mh, tree, sel);
    delete_backup(backup_id);
}
```

---

### `ui_tree_row_count`

Returns the number of top-level rows in the tree.

**Signature:**

```c
int (*ui_tree_row_count)(void* mh, void* tree);
```

**Return:** Row count, or `0` if `tree` is `nullptr`.

**Example:**

```cpp
int n = ctx->ui_tree_row_count(mh, tree);
char buf[64];
snprintf(buf, sizeof(buf), "Total backups: %d", n);
ctx->ui_label_set_text(mh, count_label, buf);
```

---

## Layout Composition Cookbook

Recipes for common page layouts using only S13 API calls.

---

### Vertical Stack

The simplest layout — items stacked top to bottom:

```cpp
void* root = ctx->ui_layout_create(mh, page, 0);
ctx->ui_layout_add_widget(mh, root, title_label);
ctx->ui_layout_add_widget(mh, root, description_label);
ctx->ui_layout_add_widget(mh, root, tree);
ctx->ui_layout_add_spacer(mh, root, 0);
ctx->ui_page_set_layout(mh, page, root);
```

```
┌─────────────────────────┐
│ Title                   │
│ Description text        │
│ ┌─────────────────────┐ │
│ │ Tree widget         │ │
│ │                     │ │
│ └─────────────────────┘ │
│                         │  ← vertical spacer
└─────────────────────────┘
```

---

### Horizontal Button Bar

Buttons in a row with a trailing spacer to left-align:

```cpp
void* bar = ctx->ui_layout_create(mh, page, 1);
ctx->ui_layout_add_widget(mh, bar, btn_create);
ctx->ui_layout_add_widget(mh, bar, btn_delete);
ctx->ui_layout_add_widget(mh, bar, btn_refresh);
ctx->ui_layout_add_spacer(mh, bar, 1);
```

```
┌──────────────────────────────────────┐
│ [Create] [Delete] [Refresh]          │
└──────────────────────────────────────┘
```

---

### Sidebar + Content (Horizontal Split)

A two-column layout:

```cpp
void* root = ctx->ui_layout_create(mh, page, 1);   // horizontal

void* sidebar = ctx->ui_layout_create(mh, page, 0); // vertical
ctx->ui_layout_add_widget(mh, sidebar, category_tree);

void* content = ctx->ui_layout_create(mh, page, 0); // vertical
ctx->ui_layout_add_widget(mh, content, detail_label);
ctx->ui_layout_add_widget(mh, content, action_btn);
ctx->ui_layout_add_spacer(mh, content, 0);

ctx->ui_layout_add_layout(mh, root, sidebar);
ctx->ui_layout_add_layout(mh, root, content);
ctx->ui_page_set_layout(mh, page, root);
```

```
┌─────────────┬──────────────────────┐
│ Category    │ Detail: ...          │
│ ┌─────────┐ │ [Perform Action]     │
│ │ Cat 1   │ │                      │
│ │ Cat 2   │ │                      │
│ │ Cat 3   │ │                      │
│ └─────────┘ │                      │
└─────────────┴──────────────────────┘
```

> **Note:** Without S13 splitter support, the two columns will share space
> equally. Users cannot drag to resize. For resizable splits, use S11.

---

### Header / Content / Footer

A common three-section layout:

```cpp
void* root = ctx->ui_layout_create(mh, page, 0);

// Header
void* header = ctx->ui_layout_create(mh, page, 1);
ctx->ui_layout_add_widget(mh, header, title_label);
ctx->ui_layout_add_spacer(mh, header, 1);
ctx->ui_layout_add_widget(mh, header, version_label);
ctx->ui_layout_add_layout(mh, root, header);

// Content (takes all remaining space)
ctx->ui_layout_add_widget(mh, root, main_tree);

// Footer
void* footer = ctx->ui_layout_create(mh, page, 1);
ctx->ui_layout_add_widget(mh, footer, status_label);
ctx->ui_layout_add_spacer(mh, footer, 1);
ctx->ui_layout_add_widget(mh, footer, save_btn);
ctx->ui_layout_add_widget(mh, footer, cancel_btn);
ctx->ui_layout_add_layout(mh, root, footer);

ctx->ui_page_set_layout(mh, page, root);
```

```
┌──────────────────────────────────────┐
│ Plugin Title              v1.2.3     │  ← header
├──────────────────────────────────────┤
│ ┌──────────────────────────────────┐ │
│ │ Name    │ Value    │ Status     │ │  ← content
│ │ Item 1  │ abc      │ OK         │ │
│ │ Item 2  │ def      │ Warning    │ │
│ └──────────────────────────────────┘ │
├──────────────────────────────────────┤
│ Ready                 [Save] [Close] │  ← footer
└──────────────────────────────────────┘
```

---

### Right-Aligned Buttons

Standard dialog button placement (buttons flush right):

```cpp
void* btn_row = ctx->ui_layout_create(mh, page, 1);
ctx->ui_layout_add_spacer(mh, btn_row, 1);  // eats left space
ctx->ui_layout_add_widget(mh, btn_row, ok_btn);
ctx->ui_layout_add_widget(mh, btn_row, cancel_btn);
```

```
┌──────────────────────────────────────┐
│                         [OK] [Cancel]│
└──────────────────────────────────────┘
```

This is the same technique used in MeshMC's built-in dialog footers and
is the recommended button placement for consistency.
