# Toolbar Actions — `ui_register_instance_action()`

> **API Surface:** `MMCOContext::ui_register_instance_action()` function pointer, `PluginManager::InstanceAction` struct, `PluginManager::instanceActions()` getter, `MainWindow` toolbar integration.

---

## Introduction

The `ui_register_instance_action()` API lets plugins add **buttons to the main window's instance toolbar** — the horizontal toolbar that appears when an instance is selected, containing actions like Launch, Edit Instance, Open Folder, etc.

When the user clicks a plugin-registered toolbar button, MeshMC calls `showInstanceWindow()` with the currently selected instance and a `page_id`, opening the instance settings dialog and navigating directly to the specified page.

**This is not a page creation mechanism.** It only creates a shortcut. Your plugin must **also** register a page via `MMCO_HOOK_UI_INSTANCE_PAGES` (see [hook-approach.md](hook-approach.md)) or the C API page builder (Section 13) so the target page exists when the toolbar button is clicked.

### When to use toolbar actions

| Scenario | Use Toolbar Action? |
|----------|-------------------|
| Plugin adds a page visitors use frequently | **Yes** — provides quick access |
| Plugin adds a page used only from the settings dialog | No — the sidebar is sufficient |
| Plugin wants a button that performs an action directly (no page) | No — use `MMCO_HOOK_UI_CONTEXT_MENU` instead |
| Plugin adds multiple pages, one is the "primary" | **Yes** — register one action for the primary page |

---

## API Signature

Declared in `launcher/plugin/PluginAPI.h` within the `MMCOContext` struct:

```cpp
/* Register an action in the main window's instance toolbar.
 * text/tooltip are shown in the toolbar; icon_name refers to a themed icon;
 * page_id is the page opened via showInstanceWindow(). */
int (*ui_register_instance_action)(void* mh, const char* text,
                                   const char* tooltip,
                                   const char* icon_name,
                                   const char* page_id);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle — always pass `ctx->module_handle` |
| `text` | `const char*` | Button text displayed in the toolbar. Keep short (1-3 words). UTF-8 encoded. |
| `tooltip` | `const char*` | Tooltip shown on hover. Can be a full sentence. UTF-8 encoded. |
| `icon_name` | `const char*` | Name of a themed icon. Resolved via `APPLICATION->getThemedIcon()`. Use standard icon names from the MeshMC theme (e.g., `"backup"`, `"refresh"`, `"log"`, `"notes"`). |
| `page_id` | `const char*` | The `id()` of the `BasePage` to navigate to when clicked. **Must exactly match** the `BasePage::id()` return value. |

### Return value

| Value | Meaning |
|-------|---------|
| `1` | Success — action registered |
| `0` | Failure — `PluginManager` not available |

### Thread safety

| Context | Safe? |
|---------|-------|
| Called from `mmco_init()` (main thread) | **Yes** (required) |
| Called from a hook callback | Not recommended — toolbar is built once during setup |
| Called from a background thread | **No** |

### Ownership

The strings (`text`, `tooltip`, `icon_name`, `page_id`) are **copied** by the implementation into `QString` objects. The caller does not need to keep the `const char*` pointers alive after the call returns.

---

## Usage

### Basic registration

```cpp
extern "C" {
MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;

    // 1. Register the hook to add pages (see hook-approach.md)
    ctx->hook_register(ctx->module_handle, MMCO_HOOK_UI_INSTANCE_PAGES,
                       on_instance_pages, nullptr);

    // 2. Register the toolbar action
    int rc = ctx->ui_register_instance_action(
        ctx->module_handle,
        "View Backups",                                // text
        "View and manage backups for this instance.",  // tooltip
        "backup",                                      // icon_name
        "backup-system");                              // page_id
    if (rc != 1) {
        MMCO_ERR(ctx, "Failed to register toolbar action");
    }

    return 0;
}
} // extern "C"
```

### Multiple toolbar actions

A single plugin can register multiple toolbar actions:

```cpp
ctx->ui_register_instance_action(ctx->module_handle,
    "Backups", "Manage instance backups", "backup", "backup-system");

ctx->ui_register_instance_action(ctx->module_handle,
    "Sync", "Synchronize instance across devices", "sync", "cloud-sync");
```

Each action appears as a separate button in the toolbar.

---

## Implementation Details

### How `api_ui_register_instance_action()` works

The static trampoline in `launcher/plugin/PluginManager.cpp`:

```cpp
int PluginManager::api_ui_register_instance_action(
    void* mh, const char* text, const char* tooltip,
    const char* icon_name, const char* page_id)
{
    (void)mh;
    auto* pm = APPLICATION->pluginManager();
    if (!pm)
        return 0;
    InstanceAction action;
    action.text = text ? QString::fromUtf8(text) : QString();
    action.tooltip = tooltip ? QString::fromUtf8(tooltip) : QString();
    action.iconName = icon_name ? QString::fromUtf8(icon_name) : QString();
    action.pageId = page_id ? QString::fromUtf8(page_id) : QString();
    pm->m_instanceActions.append(action);
    return 1;
}
```

**Key observations:**

1. **Null-safe** — each `const char*` is checked for `nullptr` before conversion. Passing `nullptr` results in an empty `QString`.
2. **Appends to `m_instanceActions`** — a `QVector<InstanceAction>` on the `PluginManager`.
3. **Returns 1 on success** — the only failure case is if `APPLICATION->pluginManager()` is null (shouldn't happen during normal initialization).
4. **No duplicate checking** — if you register the same action twice, it appears twice in the toolbar. Avoid calling this function multiple times with the same parameters.

### The `InstanceAction` struct

Declared in `launcher/plugin/PluginManager.h`:

```cpp
struct InstanceAction {
    QString text;      // Button text
    QString tooltip;   // Hover tooltip
    QString iconName;  // Theme icon name
    QString pageId;    // Target page ID
};
```

Stored in:

```cpp
QVector<InstanceAction> m_instanceActions;
```

Exposed via:

```cpp
const QVector<InstanceAction>& instanceActions() const
{
    return m_instanceActions;
}
```

---

## How MainWindow Consumes Toolbar Actions

After `MainWindow::setupUi()` completes the built-in toolbar, it queries the `PluginManager` for registered instance actions and creates `QAction` widgets for each one.

### The consumer code (`launcher/ui/MainWindow.cpp`)

```cpp
if (APPLICATION->pluginManager()) {
    // Add plugin-registered instance toolbar actions
    auto* instanceTB = ui->instanceToolBar.operator->();
    QList<QAction*> tbActions = instanceTB->actions();

    // Find the separator right after actionChangeInstGroup
    QAction* changeGroupAct = ui->actionChangeInstGroup.operator->();
    QAction* insertBefore = nullptr;
    int idx = tbActions.indexOf(changeGroupAct);
    if (idx >= 0 && idx + 1 < tbActions.size() &&
        tbActions[idx + 1]->isSeparator()) {
        insertBefore = tbActions[idx + 1];
    }

    for (const auto& act :
         APPLICATION->pluginManager()->instanceActions()) {
        auto* qa = new QAction(
            APPLICATION->getThemedIcon(act.iconName),
            act.text, this);
        qa->setToolTip(act.tooltip);
        QString pageId = act.pageId;
        connect(qa, &QAction::triggered, this, [this, pageId] {
            APPLICATION->showInstanceWindow(m_selectedInstance, pageId);
        });
        if (insertBefore)
            instanceTB->insertAction(insertBefore, qa);
        else
            instanceTB->addAction(qa);
        m_pluginInstanceActions.append(qa);
    }

    APPLICATION->pluginManager()->dispatchHook(MMCO_HOOK_UI_MAIN_READY);
}
```

### Step-by-step breakdown

1. **Get the toolbar** — `ui->instanceToolBar` is the horizontal toolbar on the right side of the main window.

2. **Find insertion point** — Plugin actions are inserted **before the separator** that comes after the "Change Group" action. This places plugin buttons in a logical position within the existing toolbar layout.

3. **Create `QAction`** — For each `InstanceAction`:
   - Icon is resolved via `APPLICATION->getThemedIcon(act.iconName)`
   - Text is set to `act.text`
   - Tooltip is set to `act.tooltip`

4. **Connect** — When triggered, the action calls `showInstanceWindow(m_selectedInstance, pageId)`:
   - `m_selectedInstance` is the currently selected instance in the main window's instance list
   - `pageId` is the `page_id` from the registered action
   - `showInstanceWindow()` opens the instance settings dialog and navigates to the page with that `id()`

5. **Insert or append** — If the insertion point was found, the action is inserted before the separator; otherwise it's appended to the end.

6. **Track for cleanup** — Plugin actions are stored in `m_pluginInstanceActions` for later cleanup if needed.

### Timing

| Event | When |
|-------|------|
| Actions registered | During `mmco_init()`, which runs during `Application` startup |
| Actions consumed | During `MainWindow::setupUi()`, which runs when the main window is first shown |
| `MMCO_HOOK_UI_MAIN_READY` dispatched | After all plugin actions have been added to the toolbar |

**Important:** `ui_register_instance_action()` must be called **before** `MainWindow::setupUi()` runs. Since `mmco_init()` is called during `Application::initializeAll()` (which precedes MainWindow creation), this ordering is guaranteed for actions registered in `mmco_init()`.

---

## The `page_id` Matching Contract

The `page_id` parameter creates a **runtime binding** between the toolbar button and the instance page. Understanding this contract is critical.

### How it works

```
┌──────────────────────────────────────────────────────────────────┐
│                                                                  │
│  ui_register_instance_action(..., page_id="backup-system")       │
│              │                                                    │
│              ▼                                                    │
│  InstanceAction { pageId: "backup-system" }                      │
│              │                                                    │
│              ▼   user clicks toolbar button                       │
│  showInstanceWindow(instance, "backup-system")                   │
│              │                                                    │
│              ▼   dialog opens, iterates page list                 │
│  for each page: if page->id() == "backup-system" → select it    │
│              │                                                    │
│              ▼                                                    │
│  BackupPage::id() returns "backup-system" → MATCH                │
│              │                                                    │
│              ▼                                                    │
│  BackupPage is shown as the active page                          │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

### Rules

1. **Exact string match** — `page_id` must be an **exact, case-sensitive** match with `BasePage::id()`. No normalization, no case folding.

2. **Registration order independence** — It doesn't matter whether you register the hook (page) or the toolbar action first. Both happen during `mmco_init()`, and the toolbar action is only consumed later when `MainWindow` is constructed. The page is only needed when the user actually opens the dialog.

3. **Missing page** — If no page with the matching `id()` exists when the dialog opens, the dialog will open but no page will be pre-selected. The user will see the default first page (usually "Settings"). No error is raised.

4. **Multiple pages, same ID** — If multiple pages have the same `id()` (which should not happen), the first match is selected.

5. **Page without toolbar action** — A page registered via the hook but without a toolbar action is perfectly valid. It simply won't have a shortcut in the main toolbar. Users access it through the instance settings dialog sidebar.

6. **Toolbar action without page** — A toolbar action registered without a corresponding page will open the dialog but select the default page. The button becomes a confusing shortcut to "Edit Instance". Avoid this.

### Recommended convention

Use the same constant for both:

```cpp
// In a shared header or at file scope:
static constexpr const char* PAGE_ID = "backup-system";

// In the hook callback:
pages->append(new BackupPage(inst));  // BackupPage::id() returns PAGE_ID

// In mmco_init():
ctx->ui_register_instance_action(ctx->module_handle,
    "View Backups", "...", "backup", PAGE_ID);
```

This eliminates the risk of typos causing a mismatch.

---

## Toolbar Button Appearance

### Text

The `text` parameter is displayed as the button label in the toolbar. MeshMC's instance toolbar uses text-under-icon style by default.

**Guidelines:**
- Keep it to 1-3 words: `"View Backups"`, `"Sync"`, `"Diagnostics"`
- Use title case
- Avoid verbs that duplicate existing toolbar actions (e.g., don't use `"Edit"` or `"Launch"`)

### Tooltip

The `tooltip` is shown when hovering over the button. It can be a full sentence:

```cpp
"View and manage backups for this instance."
```

### Icon

The `icon_name` is resolved via `APPLICATION->getThemedIcon(name)`, which looks up icons from MeshMC's theme system. Available icons include all icons in the `resources/multimc/` theme directories.

**Common themed icons:**

| Name | Description |
|------|-------------|
| `"backup"` | Backup/restore |
| `"refresh"` | Refresh/reload |
| `"log"` | Log file |
| `"notes"` | Notes/text |
| `"settings"` | Gear/settings |
| `"copy"` | Copy/duplicate |
| `"export"` | Export/arrow-out |
| `"import"` | Import/arrow-in |
| `"bug"` | Bug/diagnostics |
| `"status-running"` | Running indicator |

If your icon name doesn't match any theme icon, the button will show a blank/missing icon. Test your icon name before shipping.

### Toolbar positioning

Plugin actions are inserted **just before the separator** after the "Change Group" action in the instance toolbar. This places them in a consistent, predictable location:

```
[Launch] [Kill] | [Edit] [Folder] [Export] | [Copy] [Delete] [Change Group]
                                                                    ↓
                                              [View Backups] [Sync] ← plugin actions
                                              ──────────── ← separator
```

The exact position depends on how many built-in actions exist, but plugin actions always appear as a group near the end of the toolbar.

---

## Complete Example: BackupSystem

From `plugins/BackupSystem/BackupPlugin.cpp`:

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

    InstancePtr inst = std::shared_ptr<BaseInstance>(
        instRaw, [](BaseInstance*) { /* no-op deleter */ });

    pages->append(new BackupPage(inst));
    return 0;
}

MMCO_DEFINE_MODULE(
    "BackupSystem", "1.0.0", "Project Tick",
    "Instance backup snapshots — create, restore, export, import",
    "GPL-3.0-or-later");

extern "C" {

MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;

    MMCO_LOG(ctx, "BackupSystem v1.0.0 initializing...");

    // Step 1: Register hook for instance pages
    int rc = ctx->hook_register(ctx->module_handle,
                                MMCO_HOOK_UI_INSTANCE_PAGES,
                                on_instance_pages, nullptr);
    if (rc != 0) {
        MMCO_ERR(ctx, "Failed to register INSTANCE_PAGES hook");
        return rc;
    }

    // Step 2: Register toolbar action
    ctx->ui_register_instance_action(
        ctx->module_handle,
        "View Backups",                                  // text
        "View and manage backups for this instance.",    // tooltip
        "backup",                                        // icon_name
        "backup-system");                                // page_id

    MMCO_LOG(ctx, "BackupSystem initialized successfully.");
    return 0;
}

MMCO_EXPORT void mmco_unload()
{
    if (g_ctx) {
        MMCO_LOG(g_ctx, "BackupSystem unloading.");
    }
    g_ctx = nullptr;
}

} /* extern "C" */
```

**The `page_id` `"backup-system"` ties it all together:**
- `BackupPage::id()` returns `"backup-system"`
- `ui_register_instance_action()` passes `"backup-system"` as `page_id`
- When the toolbar button fires, `showInstanceWindow(instance, "backup-system")` opens the dialog and selects `BackupPage`

---

## Error Handling

### Action not appearing in toolbar

| Symptom | Cause | Fix |
|---------|-------|-----|
| Button missing | `ui_register_instance_action()` called after `MainWindow::setupUi()` | Move the call to `mmco_init()` |
| Button missing | `PluginManager` not initialized | Ensure plugin is loaded before MainWindow |
| Button missing | `ui_register_instance_action()` returned `0` | Check that `APPLICATION->pluginManager()` is not null |
| Button has no icon | `icon_name` doesn't match any theme icon | Use a valid icon name (see table above) |
| Button opens wrong page | `page_id` doesn't match `BasePage::id()` | Ensure exact string match |
| Button opens default page | No page with matching `id()` registered | Ensure hook is registered AND the callback creates the page |

### Debugging toolbar actions

Add logging to verify the action was registered:

```cpp
int rc = ctx->ui_register_instance_action(ctx->module_handle,
    "View Backups", "...", "backup", "backup-system");
if (rc == 1) {
    MMCO_LOG(ctx, "Toolbar action registered: View Backups → backup-system");
} else {
    MMCO_ERR(ctx, "Failed to register toolbar action");
}
```

Check MeshMC's log output for the message. If the message appears but the button doesn't, the issue is in `MainWindow` setup timing.

---

## Unregistration

There is currently **no API to unregister** a toolbar action once registered. The `m_instanceActions` vector is populated during initialization and consumed once by `MainWindow`. To "remove" a toolbar action, the plugin must be unloaded and MeshMC restarted.

If dynamic action management is needed in the future, it would likely be implemented via a `ui_unregister_instance_action()` function and a signal to refresh the toolbar.

---

## Comparison with Context Menu Items

MMCO provides two ways to add instance-related UI entry points:

| Feature | Toolbar Action | Context Menu Item |
|---------|---------------|-------------------|
| API | `ui_register_instance_action()` | `ui_add_menu_item()` in `MMCO_HOOK_UI_CONTEXT_MENU` callback |
| Location | Main window toolbar (always visible) | Right-click context menu (on-demand) |
| Trigger | Opens `showInstanceWindow()` with `page_id` | Invokes a callback function |
| Timing | Registered once in `mmco_init()` | Registered each time the context menu opens |
| Flexibility | Opens a page only | Can do anything in the callback |
| Requires a page | Yes | No |

For most plugins that add instance pages, **use both**: a toolbar action for the primary page (quick access) and a context menu item for additional actions.

---

## Best Practices

1. **Always pair with a page** — Don't register a toolbar action without also registering a corresponding page via the hook. The button would open the dialog but not navigate to any specific page.

2. **Use a shared constant for `page_id`** — Define it once and use it in both `ui_register_instance_action()` and `BasePage::id()` to avoid string mismatches.

3. **Keep button text short** — The toolbar has limited horizontal space. `"Backups"` is better than `"View and Manage Instance Backups"`.

4. **Test the icon** — Run with MeshMC's default theme and verify your icon appears correctly. Missing icons show as blank buttons.

5. **Log registration** — Always log whether the toolbar action registered successfully. This helps with debugging.

6. **One primary action** — If your plugin adds multiple pages, register a toolbar action only for the most commonly used one. Too many toolbar buttons create clutter.

7. **Don't duplicate built-in actions** — Check that your toolbar button doesn't overlap with existing functionality (Launch, Edit, Folder, etc.).

---

## Related Documentation

| Document | Relevance |
|----------|-----------|
| [README.md](README.md) | Section overview, the `page_id` matching contract, BackupSystem case study |
| [hook-approach.md](hook-approach.md) | How to create the page that the toolbar action navigates to |
| [S01 — Lifecycle](../s01-lifecycle/) | `mmco_init()` lifecycle, when registration calls are valid |
| [S12 — UI Dialogs](../s12-ui-dialogs/) | `ui_add_menu_item()` for context menu integration |
| [S13 — UI Page Builder](../s13-ui-page-builder/) | Alternative page creation via C API |
