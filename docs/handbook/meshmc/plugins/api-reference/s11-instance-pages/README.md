# Section 11 — Instance Pages & Toolbar Actions

> **API Surface:** `MMCO_HOOK_UI_INSTANCE_PAGES` hook, `MMCOInstancePagesEvent` payload, `BasePage` interface, `ui_register_instance_action()` function pointer, `PluginManager::InstanceAction` struct.

---

## Overview

This section documents how MMCO plugins extend the **instance settings dialog** with custom tab pages and add **toolbar buttons** to the main window that open instance pages directly.

MeshMC's instance settings dialog (the window opened by double-clicking an instance, or via **Edit Instance** in the context menu) displays a list of pages in a sidebar — Settings, Version, Mods, Worlds, Screenshots, Notes, etc. Plugins can **inject additional pages** into this list, appearing alongside the built-in ones. Additionally, plugins can register **toolbar actions** in the main window so users can jump straight to a plugin page from the instance toolbar without opening the full settings dialog first.

There are **two complementary approaches** a plugin can use:

| Approach | Mechanism | What It Does |
|----------|-----------|--------------|
| **Hook approach** | Register for `MMCO_HOOK_UI_INSTANCE_PAGES` | Inject a native `QWidget` + `BasePage` subclass directly into the instance settings page list |
| **Toolbar approach** | Call `ui_register_instance_action()` | Add a button to the main window's instance toolbar that opens `showInstanceWindow()` navigated to a specific page |

Most plugins that add instance pages will use **both approaches together**: the hook adds the page itself, and the toolbar action provides a convenient shortcut. The BackupSystem plugin demonstrates this pattern perfectly (see [Case Study](#case-study-backupsystem) below).

### Architectural context

```
┌──────────────────────────────────────────────────────────────┐
│                     MainWindow                               │
│  ┌──────────┐   ┌────────────────────────────────────────┐   │
│  │ Instance  │   │          Instance Toolbar              │   │
│  │   List    │   │  [Launch] [Edit] [Folder] [▸Backups]  │   │
│  │           │   │                            ▲           │   │
│  │           │   │           plugin-registered─┘           │   │
│  └──────────┘   └────────────────────────────────────────┘   │
│                        │ click "View Backups"                 │
│                        ▼                                      │
│  ┌────────────────────────────────────────────────────────┐   │
│  │          Instance Settings Dialog                      │   │
│  │  ╔══════════╗  ┌────────────────────────────────────┐  │   │
│  │  ║ Settings ║  │                                    │  │   │
│  │  ║ Version  ║  │  (active page content)             │  │   │
│  │  ║ Mods     ║  │                                    │  │   │
│  │  ║ Worlds   ║  │                                    │  │   │
│  │  ║ Notes    ║  │                                    │  │   │
│  │  ║──────────║  │                                    │  │   │
│  │  ║ Backups  ║◄─│── plugin-injected BasePage         │  │   │
│  │  ╚══════════╝  └────────────────────────────────────┘  │   │
│  └────────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────┘
```

### Data flow

1. **Plugin initialization** (`mmco_init()`):
   - Plugin calls `hook_register()` for `MMCO_HOOK_UI_INSTANCE_PAGES`
   - Plugin calls `ui_register_instance_action()` to add a toolbar button

2. **Dialog construction** (`InstancePageProvider::getPages()`):
   - MeshMC builds the built-in page list
   - `PluginManager::dispatchHook(MMCO_HOOK_UI_INSTANCE_PAGES, &evt)` fires
   - Each plugin's callback receives `MMCOInstancePagesEvent*`, appends pages to `page_list_handle`

3. **Toolbar construction** (`MainWindow::setupUi()` epilogue):
   - MeshMC iterates `PluginManager::instanceActions()`
   - Creates `QAction` for each `InstanceAction`, connecting to `showInstanceWindow(instance, pageId)`

4. **Page ID matching**:
   - The `page_id` passed to `ui_register_instance_action()` must **exactly match** the `id()` returned by the corresponding `BasePage` subclass
   - When the toolbar action fires, `showInstanceWindow()` uses this ID to select the correct page in the sidebar

---

## The `BasePage` Interface

Every page in the instance settings dialog — built-in or plugin-provided — implements the `BasePage` abstract class. This is declared in `launcher/ui/pages/BasePage.h`:

```cpp
class BasePage
{
  public:
    virtual ~BasePage() {}

    // ── Required overrides ──────────────────────────────────────
    virtual QString id() const = 0;           // Unique page identifier
    virtual QString displayName() const = 0;  // Shown in the sidebar
    virtual QIcon icon() const = 0;           // Sidebar icon

    // ── Optional overrides ──────────────────────────────────────
    virtual bool apply() { return true; }
    virtual bool shouldDisplay() const { return true; }
    virtual QString helpPage() const { return QString(); }
    virtual void openedImpl() {}
    virtual void closedImpl() {}

    // ── Container management ────────────────────────────────────
    virtual void setParentContainer(BasePageContainer* container)
    {
        m_container = container;
    }

    // ── Framework plumbing (do not override) ────────────────────
    void opened()  { isOpened = true;  openedImpl(); }
    void closed()  { isOpened = false; closedImpl(); }

  public:
    int stackIndex = -1;
    int listIndex = -1;

  protected:
    BasePageContainer* m_container = nullptr;
    bool isOpened = false;
};

typedef std::shared_ptr<BasePage> BasePagePtr;
```

### Pure virtual methods (must override)

| Method | Return Type | Purpose |
|--------|-------------|---------|
| `id()` | `QString` | Unique string identifier. Used by `showInstanceWindow()` to navigate to this page. Must match the `page_id` registered via `ui_register_instance_action()`. Convention: use your plugin's kebab-case name (e.g., `"backup-system"`). |
| `displayName()` | `QString` | Human-readable name shown in the page list sidebar. Wrap in `tr()` for localization. |
| `icon()` | `QIcon` | Icon displayed next to the name in the sidebar. Use `APPLICATION->getThemedIcon("name")` for theme-aware icons. |

### Optional virtual methods

| Method | Default | Purpose |
|--------|---------|---------|
| `apply()` | `return true;` | Called when the user clicks "Close" or switches pages. Return `false` to block the switch (e.g., unsaved changes). |
| `shouldDisplay()` | `return true;` | Return `false` to conditionally hide this page. Checked each time the dialog is built. |
| `helpPage()` | `return QString();` | If non-empty, enables the "Help" button and navigates to this page in the handbook. Convention: `"Instance-Backups"`. |
| `openedImpl()` | no-op | Called when the page becomes the active (visible) page. Use this to refresh data. |
| `closedImpl()` | no-op | Called when the user navigates away from this page. Use this to save state if needed. |

### `BasePageContainer` interface

The parent dialog implements `BasePageContainer`, giving pages access to dialog-level operations:

```cpp
class BasePageContainer
{
  public:
    virtual ~BasePageContainer() {};
    virtual bool selectPage(QString pageId) = 0;   // Navigate to another page
    virtual void refreshContainer() = 0;            // Refresh the page list
    virtual bool requestClose() = 0;                // Request dialog close
};
```

Pages access this via the `m_container` member set by `setParentContainer()`. For example, a plugin page could programmatically switch to the built-in "Mods" page:

```cpp
if (m_container)
    m_container->selectPage("mods");
```

---

## Two Approaches

### Approach 1: Hook-based native pages (recommended)

The plugin registers for `MMCO_HOOK_UI_INSTANCE_PAGES`, receives the `MMCOInstancePagesEvent*` payload containing a `QList<BasePage*>*`, creates a `QWidget` + `BasePage` subclass, and appends it.

**Advantages:**
- Full access to Qt widgets, layouts, signals/slots
- Complete control over the page's UI (`.ui` files, custom widgets)
- The page is a first-class citizen — indistinguishable from built-in pages
- Can hold a reference to the `InstancePtr` for deep instance interaction

**See:** [hook-approach.md](hook-approach.md) for the complete guide.

### Approach 2: C API page builder (simpler)

The plugin uses `ui_page_create()` + `ui_page_add_to_list()` from the C API to create a `PluginPage` (an internal `BasePage` subclass) and populate it with layout/widget calls (`ui_layout_create()`, `ui_button_create()`, etc.).

**Advantages:**
- No Qt headers needed — pure C API
- Simpler for plugins that only need a few buttons and labels

**Note:** This approach is documented in **Section 13 — UI Page Builder**. It uses the same hook (`MMCO_HOOK_UI_INSTANCE_PAGES`) for injection but builds the page through the C API rather than direct Qt code.

### Approach 3: Toolbar actions (complementary)

The plugin calls `ui_register_instance_action()` during `mmco_init()` to add a button to the main window's instance toolbar. The button opens `showInstanceWindow()` navigated to a specific `page_id`.

**This is not a page creation mechanism** — it only creates a shortcut to an existing page. It is almost always used **in combination** with Approach 1 or 2.

**See:** [toolbar-actions.md](toolbar-actions.md) for the complete guide.

---

## Case Study: BackupSystem

The BackupSystem plugin demonstrates the **recommended pattern** of using both approaches together:

1. In `mmco_init()`, it registers for `MMCO_HOOK_UI_INSTANCE_PAGES` → injects a `BackupPage` (QWidget + BasePage subclass) into each instance settings dialog
2. In `mmco_init()`, it calls `ui_register_instance_action()` → adds "View Backups" to the main window toolbar
3. The `page_id` `"backup-system"` ties the two together: `BackupPage::id()` returns `"backup-system"`, and the toolbar action's `page_id` is `"backup-system"`

```cpp
// BackupPlugin.cpp — mmco_init() excerpt
int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;

    // 1. Register hook to inject BackupPage into instance dialogs
    ctx->hook_register(ctx->module_handle, MMCO_HOOK_UI_INSTANCE_PAGES,
                       on_instance_pages, nullptr);

    // 2. Register toolbar action for quick access
    ctx->ui_register_instance_action(
        ctx->module_handle,
        "View Backups",                                  // button text
        "View and manage backups for this instance.",    // tooltip
        "backup",                                        // icon name
        "backup-system");                                // page_id

    return 0;
}
```

The hook callback creates the page with the matching ID:

```cpp
static int on_instance_pages(void* /*mh*/, uint32_t /*hook_id*/, void* payload,
                             void* /*ud*/)
{
    auto* evt = static_cast<MMCOInstancePagesEvent*>(payload);
    auto* pages = static_cast<QList<BasePage*>*>(evt->page_list_handle);
    auto* instRaw = static_cast<BaseInstance*>(evt->instance_handle);

    // Non-owning shared_ptr — the instance is managed elsewhere
    InstancePtr inst = std::shared_ptr<BaseInstance>(
        instRaw, [](BaseInstance*) { /* no-op deleter */ });

    pages->append(new BackupPage(inst));  // BackupPage::id() == "backup-system"
    return 0;
}
```

When the user clicks "View Backups" in the toolbar, `MainWindow` calls:
```cpp
APPLICATION->showInstanceWindow(m_selectedInstance, "backup-system");
```
The dialog opens and selects the page whose `id()` equals `"backup-system"`.

Detailed walkthroughs of each approach are in the sub-pages:

- **[hook-approach.md](hook-approach.md)** — Full guide to the hook-based approach with `BasePage` subclassing
- **[toolbar-actions.md](toolbar-actions.md)** — Full guide to `ui_register_instance_action()` and toolbar integration

---

## Related Sections

| Section | Relevance |
|---------|-----------|
| [S01 — Lifecycle, Logging & Hooks](../s01-lifecycle/) | `hook_register()`, `hook_unregister()`, hook dispatch semantics |
| [S13 — UI Page Builder](../s13-ui-page-builder/) | C API alternative to native Qt pages: `ui_page_create()`, `ui_layout_create()`, `ui_button_create()`, etc. |
| [S12 — UI Dialogs](../s12-ui-dialogs/) | `ui_show_message()`, `ui_confirm_dialog()`, `ui_file_open_dialog()` — used within page implementations |
| [S03 — Instance Query](../s03-instances/) | Querying instance properties from within page code |

---

## Source Files

| File | Role |
|------|------|
| `launcher/plugin/PluginHooks.h` | `MMCO_HOOK_UI_INSTANCE_PAGES` constant (`0x0602`), `MMCOInstancePagesEvent` struct |
| `launcher/plugin/PluginAPI.h` | `ui_register_instance_action()` function pointer declaration |
| `launcher/plugin/PluginManager.h` | `InstanceAction` struct, `instanceActions()` getter |
| `launcher/plugin/PluginManager.cpp` | `api_ui_register_instance_action()` implementation, `PluginPage` internal class |
| `launcher/ui/pages/BasePage.h` | `BasePage` abstract class, `BasePagePtr` typedef |
| `launcher/ui/pages/BasePageContainer.h` | `BasePageContainer` interface (selectPage, refreshContainer, requestClose) |
| `launcher/InstancePageProvider.h` | Where `MMCO_HOOK_UI_INSTANCE_PAGES` is dispatched during dialog construction |
| `launcher/ui/MainWindow.cpp` | Where `instanceActions()` are consumed to create toolbar `QAction`s |
| `plugins/BackupSystem/BackupPlugin.cpp` | Reference implementation using both approaches |
| `plugins/BackupSystem/BackupPage.h` | Reference `BasePage` subclass declaration |
| `plugins/BackupSystem/BackupPage.cpp` | Reference `BasePage` subclass implementation |
