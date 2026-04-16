# Hook Approach — Injecting Native Instance Pages

> **API Surface:** `MMCO_HOOK_UI_INSTANCE_PAGES` (`0x0602`), `MMCOInstancePagesEvent` payload, `BasePage` abstract class, `QWidget` multiple inheritance pattern.

---

## Introduction

The hook-based approach is the **recommended** way for MMCO plugins to add pages to the instance settings dialog. The plugin registers a callback for `MMCO_HOOK_UI_INSTANCE_PAGES`, and each time MeshMC builds the page list for an instance settings dialog, the callback fires with an `MMCOInstancePagesEvent*` payload. The plugin creates a `QWidget` + `BasePage` subclass and appends it to the page list.

Pages created this way are **indistinguishable from built-in pages** — they appear in the sidebar with an icon and display name, they receive `openedImpl()` / `closedImpl()` lifecycle calls, and they can use the full Qt widget toolkit for their UI.

### When to use this approach

| Scenario | Recommendation |
|----------|----------------|
| Custom tab with rich UI (tables, trees, buttons, forms) | **Use this approach** |
| Page that needs deep instance interaction (mods, files, components) | **Use this approach** |
| Page with `.ui` files designed in Qt Designer | **Use this approach** |
| Simple page with a few labels and buttons, no Qt dependency | Consider the C API page builder (Section 13) |

---

## Step-by-Step Guide

### Step 1: Register the hook in `mmco_init()`

During initialization, register a static callback function for `MMCO_HOOK_UI_INSTANCE_PAGES`:

```cpp
#include "plugin/sdk/mmco_sdk.h"

static MMCOContext* g_ctx = nullptr;

// Forward-declare the hook callback
static int on_instance_pages(void* mh, uint32_t hook_id, void* payload,
                             void* ud);

extern "C" {

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

    MMCO_LOG(ctx, "Plugin initialized — instance pages hook registered.");
    return 0;
}

} // extern "C"
```

**Key points:**
- `ctx->module_handle` identifies your plugin to the hook system
- `on_instance_pages` is a plain C function pointer (no captures)
- The fourth parameter (`nullptr` here) is `user_data`, passed back to your callback
- Return `0` on success; non-zero aborts plugin initialization

### Step 2: Understand the payload

When the hook fires, your callback receives an `MMCOInstancePagesEvent*` as the `payload`:

```cpp
struct MMCOInstancePagesEvent {
    const char* instance_id;     // Instance ID string (e.g. "1a2b3c4d")
    const char* instance_name;   // Human-readable instance name
    const char* instance_path;   // Absolute path to the instance root
    void* page_list_handle;      // Opaque: QList<BasePage*>*
    void* instance_handle;       // Opaque: InstancePtr raw pointer (BaseInstance*)
};
```

| Field | Type Behind the Opaque | Purpose |
|-------|----------------------|---------|
| `page_list_handle` | `QList<BasePage*>*` | Pointer to the page list being built. Append your `BasePage*` here. |
| `instance_handle` | `BaseInstance*` | Raw pointer to the instance object. Wrap in a non-owning `shared_ptr` to obtain an `InstancePtr`. |

### Step 3: Implement the hook callback

The callback creates your page and appends it to the list:

```cpp
static int on_instance_pages(void* /*mh*/, uint32_t /*hook_id*/,
                             void* payload, void* /*ud*/)
{
    auto* evt = static_cast<MMCOInstancePagesEvent*>(payload);

    // Validate the payload
    if (!evt || !evt->page_list_handle || !evt->instance_handle)
        return 0;

    // Cast the opaque handles to their real types
    auto* pages = static_cast<QList<BasePage*>*>(evt->page_list_handle);
    auto* instRaw = static_cast<BaseInstance*>(evt->instance_handle);

    // Create a non-owning shared_ptr (the instance is managed by MeshMC)
    InstancePtr inst = std::shared_ptr<BaseInstance>(
        instRaw, [](BaseInstance*) { /* no-op deleter */ });

    // Create and append the page
    pages->append(new MyCustomPage(inst));
    return 0;
}
```

**Return value:** Always return `0` to allow the hook chain to continue. Other plugins may also be adding pages. Returning non-zero would cancel the hook chain (only meaningful for "pre" hooks; the instance pages hook is not cancellable in practice, but returning `0` is correct).

### Step 4: Create the `BasePage` subclass

This is where you define your page's identity and UI.

#### Header file (`MyCustomPage.h`)

```cpp
#pragma once

#include "plugin/sdk/mmco_sdk.h"  // includes BasePage, QWidget, etc.

namespace Ui {
    class MyCustomPage;  // forward-declare if using .ui file
}

class MyCustomPage : public QWidget, public BasePage
{
    Q_OBJECT

  public:
    explicit MyCustomPage(InstancePtr instance, QWidget* parent = nullptr);
    ~MyCustomPage() override;

    // ── BasePage required overrides ──
    QString id() const override { return "my-custom-page"; }
    QString displayName() const override { return tr("My Custom Page"); }
    QIcon icon() const override;

    // ── BasePage optional overrides ──
    QString helpPage() const override { return "My-Custom-Page"; }
    bool shouldDisplay() const override { return true; }
    void openedImpl() override;
    void closedImpl() override;

  private:
    Ui::MyCustomPage* ui;
    InstancePtr m_instance;
};
```

#### Implementation file (`MyCustomPage.cpp`)

```cpp
#include "MyCustomPage.h"
#include "ui_MyCustomPage.h"   // generated from .ui file

MyCustomPage::MyCustomPage(InstancePtr instance, QWidget* parent)
    : QWidget(parent), ui(new Ui::MyCustomPage), m_instance(instance)
{
    ui->setupUi(this);
    // Initial setup: populate widgets with instance data, connect signals, etc.
}

MyCustomPage::~MyCustomPage()
{
    delete ui;
}

QIcon MyCustomPage::icon() const
{
    return APPLICATION->getThemedIcon("my-icon");
}

void MyCustomPage::openedImpl()
{
    // Called when the user navigates to this page
    // Refresh data, update widgets
}

void MyCustomPage::closedImpl()
{
    // Called when the user navigates away
    // Save state if needed
}
```

---

## The Non-Owning `shared_ptr` Pattern

This is the most critical pattern to understand when writing instance pages. The `instance_handle` field in `MMCOInstancePagesEvent` is a **raw pointer** to a `BaseInstance` that is owned and managed by MeshMC's instance list. Your page must **not** take ownership of it.

### Why `shared_ptr` with a no-op deleter?

MeshMC's codebase uses `InstancePtr` (which is `std::shared_ptr<BaseInstance>`) pervasively. Many internal APIs expect an `InstancePtr`. Your page needs one to interact with instance subsystems — but you must not delete the instance when your page is destroyed.

The solution is a `shared_ptr` with a **no-op deleter**:

```cpp
auto* instRaw = static_cast<BaseInstance*>(evt->instance_handle);
InstancePtr inst = std::shared_ptr<BaseInstance>(
    instRaw, [](BaseInstance*) { /* no-op — we don't own this */ });
```

### What the no-op deleter does

| Event | Standard `shared_ptr` | No-op deleter `shared_ptr` |
|-------|----------------------|---------------------------|
| `shared_ptr` refcount → 0 | Calls `delete ptr` | Calls the lambda (which does nothing) |
| Instance destroyed by MeshMC | N/A | Your `shared_ptr` becomes **dangling** |

### Lifetime guarantee

The `InstancePtr` obtained from the hook is safe to use for the **lifetime of the page**. Pages are destroyed when the instance settings dialog closes. The instance itself is never destroyed while a settings dialog is open for it (MeshMC holds a reference in the dialog).

**DO NOT** store the `InstancePtr` beyond the page's lifetime. Do not cache it in a global. Do not pass it to background threads that outlive the page.

### Common mistake: copying the raw pointer only

```cpp
// ❌ WRONG — BaseInstance* without shared_ptr can't be used with APIs
//    expecting InstancePtr
auto* instRaw = static_cast<BaseInstance*>(evt->instance_handle);
pages->append(new MyPage(instRaw));  // MyPage stores BaseInstance*
// Many internal getters require InstancePtr, not BaseInstance*
```

```cpp
// ✅ CORRECT — wrap in non-owning shared_ptr
InstancePtr inst = std::shared_ptr<BaseInstance>(
    instRaw, [](BaseInstance*) {});
pages->append(new MyPage(inst));  // MyPage stores InstancePtr
```

---

## The Multiple Inheritance Pattern

Plugin pages use **dual inheritance**: `QWidget` (for the widget tree) and `BasePage` (for the page protocol). This is the same pattern used by every built-in page in MeshMC (e.g., `VersionPage`, `ModFolderPage`, `WorldListPage`).

```
                    QObject
                      │
                   QWidget
                      │
             ┌────────┴────────┐
             │                 │
        MyCustomPage      BasePage (non-QObject)
         (Q_OBJECT)
```

### Why not inherit `BasePage` from `QWidget`?

`BasePage` is intentionally **not** a QWidget. This allows lightweight non-widget pages and avoids diamond inheritance with `QObject`. The convention throughout MeshMC is explicit dual inheritance.

### The `Q_OBJECT` macro

Your page class must include `Q_OBJECT` if it uses signals, slots, or `tr()` for translations:

```cpp
class MyPage : public QWidget, public BasePage
{
    Q_OBJECT   // Required for tr(), signals, slots
  public:
    // ...
};
```

### MOC and build system considerations

If you use `.ui` files or `Q_OBJECT`, your CMake build must process your class with Qt's Meta-Object Compiler (MOC). The SDK's CMake integration handles this when you use `meshmc_add_plugin()`:

```cmake
meshmc_add_plugin(my-plugin
    SOURCES
        MyPlugin.cpp
        MyCustomPage.cpp
        MyCustomPage.h   # MOC will process this
    UI_FILES
        MyCustomPage.ui   # uic will process this
)
```

---

## Hook Dispatch Context

Understanding **when** and **where** the hook fires is essential for writing correct page code.

### Dispatch site: `InstancePageProvider::getPages()`

The hook is dispatched from `InstancePageProvider::getPages()` in `launcher/InstancePageProvider.h`. This method is called whenever MeshMC constructs the page list for an instance settings dialog:

```cpp
// Simplified from InstancePageProvider.h
QList<BasePage*> getPages() override
{
    QList<BasePage*> values;

    // ... built-in pages (Settings, Version, Mods, Worlds, etc.) ...

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
}
```

### Timing

| Event | When |
|-------|------|
| Hook fires | Each time an instance settings dialog is **opened** (or re-opened) |
| Multiple instances | A separate hook dispatch occurs for each instance. Your callback may fire multiple times if multiple dialogs are open simultaneously. |
| Thread | Always dispatched on the **main (GUI) thread**. Safe to create widgets. |

### Page ownership

The `QList<BasePage*>` returned by `getPages()` takes **ownership** of the pages via raw pointers. The dialog's `PageContainer` (a `QStackedWidget`) becomes the Qt parent of each page widget, and Qt's ownership system handles deletion when the dialog closes.

**You must allocate pages with `new`.** Do not return stack-allocated or `unique_ptr`-managed pages:

```cpp
// ✅ Correct: heap-allocated, ownership transfers to the dialog
pages->append(new BackupPage(inst));

// ❌ Wrong: unique_ptr would delete the page prematurely
auto page = std::make_unique<BackupPage>(inst);
pages->append(page.get());  // page deleted when unique_ptr goes out of scope!
```

### Multiple pages

A single plugin can add **multiple pages** in one callback invocation:

```cpp
static int on_instance_pages(void* /*mh*/, uint32_t /*hook_id*/,
                             void* payload, void* /*ud*/)
{
    auto* evt = static_cast<MMCOInstancePagesEvent*>(payload);
    auto* pages = static_cast<QList<BasePage*>*>(evt->page_list_handle);
    auto* instRaw = static_cast<BaseInstance*>(evt->instance_handle);
    InstancePtr inst = std::shared_ptr<BaseInstance>(
        instRaw, [](BaseInstance*) {});

    pages->append(new BackupPage(inst));
    pages->append(new AnalyticsPage(inst));
    pages->append(new SyncPage(inst));
    return 0;
}
```

---

## Conditional Page Display

### Via `shouldDisplay()`

Override `shouldDisplay()` to conditionally show or hide your page. This is checked when the dialog builds its sidebar:

```cpp
bool shouldDisplay() const override
{
    // Only show the backup page for instances that have a game root
    return !m_instance->gameRoot().isEmpty();
}
```

### Via the hook callback

You can also skip page creation entirely in the callback:

```cpp
static int on_instance_pages(void* /*mh*/, uint32_t /*hook_id*/,
                             void* payload, void* /*ud*/)
{
    auto* evt = static_cast<MMCOInstancePagesEvent*>(payload);
    if (!evt || !evt->page_list_handle || !evt->instance_handle)
        return 0;

    // Only add pages for instances with "forge" or "fabric" in their type
    std::string type(evt->instance_id);  // or use instance_handle to check
    auto* instRaw = static_cast<BaseInstance*>(evt->instance_handle);

    // Check instance type before creating the page
    InstancePtr inst = std::shared_ptr<BaseInstance>(
        instRaw, [](BaseInstance*) {});
    if (!inst->getPackProfile()->getComponent("net.minecraftforge"))
        return 0;  // skip — no Forge, no page

    auto* pages = static_cast<QList<BasePage*>*>(evt->page_list_handle);
    pages->append(new ForgeDiagnosticsPage(inst));
    return 0;
}
```

---

## Complete Example: BackupPage

The BackupSystem plugin is the canonical reference implementation. Here is the full annotated source.

### `BackupPage.h`

```cpp
#pragma once

#include "plugin/sdk/mmco_sdk.h"
#include "BackupManager.h"

namespace Ui {
    class BackupPage;
}

class BackupPage : public QWidget, public BasePage
{
    Q_OBJECT

  public:
    explicit BackupPage(InstancePtr instance, QWidget* parent = nullptr);
    ~BackupPage() override;

    // ── BasePage identity ──
    QString id() const override { return "backup-system"; }
    QString displayName() const override { return tr("Backups"); }
    QIcon icon() const override;
    QString helpPage() const override { return "Instance-Backups"; }
    bool shouldDisplay() const override { return true; }

    // ── Lifecycle ──
    void openedImpl() override;

  private slots:
    void on_btnCreate_clicked();
    void on_btnRestore_clicked();
    void on_btnExport_clicked();
    void on_btnImport_clicked();
    void on_btnDelete_clicked();
    void onSelectionChanged();

  private:
    void refreshList();
    void updateButtons();
    BackupEntry selectedEntry() const;
    QString humanFileSize(qint64 bytes) const;

    Ui::BackupPage* ui;
    InstancePtr m_instance;                     // Non-owning shared_ptr
    std::unique_ptr<BackupManager> m_manager;
    QList<BackupEntry> m_entries;
};
```

**Key observations:**

1. **`id()` returns `"backup-system"`** — this is the page ID that matches the toolbar action registered in `BackupPlugin.cpp`.

2. **`m_instance` is `InstancePtr`** — a non-owning `shared_ptr<BaseInstance>` created with a no-op deleter in the hook callback.

3. **`m_manager` is `unique_ptr<BackupManager>`** — the business logic lives in a separate class. The page is purely a UI shell.

4. **`Q_OBJECT` macro** — enables `tr()` for all user-facing strings and `Q_SLOTS` for signal/slot connections.

### `BackupPage.cpp`

```cpp
#include "BackupPage.h"
#include "ui_BackupPage.h"

BackupPage::BackupPage(InstancePtr instance, QWidget* parent)
    : QWidget(parent), ui(new Ui::BackupPage), m_instance(instance)
{
    ui->setupUi(this);

    // Create the BackupManager with the instance's ID and root path
    m_manager = std::make_unique<BackupManager>(
        m_instance->id(), m_instance->instanceRoot());

    // Configure the tree widget columns
    ui->backupList->header()->setStretchLastSection(false);
    ui->backupList->header()->setSectionResizeMode(
        0, QHeaderView::Stretch);           // Name
    ui->backupList->header()->setSectionResizeMode(
        1, QHeaderView::ResizeToContents);  // Date
    ui->backupList->header()->setSectionResizeMode(
        2, QHeaderView::ResizeToContents);  // Size

    // Connect selection changes to button state updates
    connect(ui->backupList, &QTreeWidget::itemSelectionChanged,
            this, &BackupPage::onSelectionChanged);

    refreshList();
}

BackupPage::~BackupPage()
{
    delete ui;
}

QIcon BackupPage::icon() const
{
    return APPLICATION->getThemedIcon("backup");
}

void BackupPage::openedImpl()
{
    // Refresh the backup list every time the page becomes visible
    refreshList();
}

void BackupPage::refreshList()
{
    ui->backupList->clear();
    m_entries = m_manager->listBackups();

    for (const auto& entry : m_entries) {
        auto* item = new QTreeWidgetItem(ui->backupList);
        item->setText(0, entry.name.isEmpty() ? entry.fileName : entry.name);
        item->setText(1, entry.timestamp.toString("yyyy-MM-dd HH:mm:ss"));
        item->setText(2, humanFileSize(entry.sizeBytes));
        item->setData(0, Qt::UserRole, entry.fullPath);
    }

    int count = m_entries.size();
    ui->statusLabel->setText(tr("%n backup(s)", "", count));
    updateButtons();
}

void BackupPage::updateButtons()
{
    bool hasSelection = !ui->backupList->selectedItems().isEmpty();
    ui->btnRestore->setEnabled(hasSelection);
    ui->btnExport->setEnabled(hasSelection);
    ui->btnDelete->setEnabled(hasSelection);
}

void BackupPage::onSelectionChanged()
{
    updateButtons();
}

BackupEntry BackupPage::selectedEntry() const
{
    auto items = ui->backupList->selectedItems();
    if (items.isEmpty())
        return {};

    QString path = items.first()->data(0, Qt::UserRole).toString();
    for (const auto& e : m_entries) {
        if (e.fullPath == path)
            return e;
    }
    return {};
}

void BackupPage::on_btnCreate_clicked()
{
    bool ok = false;
    QString label = QInputDialog::getText(
        this, tr("Create Backup"),
        tr("Backup label (optional):"),
        QLineEdit::Normal, {}, &ok);

    if (!ok)
        return;

    ui->statusLabel->setText(tr("Creating backup..."));
    QApplication::processEvents();

    auto entry = m_manager->createBackup(label);
    if (entry.fullPath.isEmpty()) {
        QMessageBox::warning(this, tr("Backup Failed"),
            tr("Failed to create backup. Check the logs for details."));
    } else {
        QMessageBox::information(this, tr("Backup Created"),
            tr("Backup created successfully:\n%1").arg(entry.fileName));
    }

    refreshList();
}

void BackupPage::on_btnRestore_clicked()
{
    auto entry = selectedEntry();
    if (entry.fullPath.isEmpty())
        return;

    auto ret = QMessageBox::warning(this, tr("Restore Backup"),
        tr("This will replace the current instance contents with the "
           "backup:\n\n%1\n\nThis action cannot be undone. Continue?")
            .arg(entry.fileName),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (ret != QMessageBox::Yes)
        return;

    ui->statusLabel->setText(tr("Restoring backup..."));
    QApplication::processEvents();

    if (m_manager->restoreBackup(entry)) {
        QMessageBox::information(this, tr("Restore Complete"),
            tr("Backup restored successfully."));
    } else {
        QMessageBox::warning(this, tr("Restore Failed"),
            tr("Failed to restore backup. Check the logs for details."));
    }

    refreshList();
}

void BackupPage::on_btnExport_clicked()
{
    auto entry = selectedEntry();
    if (entry.fullPath.isEmpty())
        return;

    QString dest = QFileDialog::getSaveFileName(
        this, tr("Export Backup"), entry.fileName,
        tr("Zip Files (*.zip)"));

    if (dest.isEmpty())
        return;

    if (m_manager->exportBackup(entry, dest)) {
        QMessageBox::information(this, tr("Export Complete"),
            tr("Backup exported to:\n%1").arg(dest));
    } else {
        QMessageBox::warning(this, tr("Export Failed"),
            tr("Failed to export backup."));
    }
}

void BackupPage::on_btnImport_clicked()
{
    QString src = QFileDialog::getOpenFileName(
        this, tr("Import Backup"), {},
        tr("Zip Files (*.zip)"));

    if (src.isEmpty())
        return;

    bool ok = false;
    QString label = QInputDialog::getText(
        this, tr("Import Backup"),
        tr("Label for imported backup (optional):"),
        QLineEdit::Normal, {}, &ok);

    if (!ok)
        return;

    auto entry = m_manager->importBackup(src, label);
    if (entry.fullPath.isEmpty()) {
        QMessageBox::warning(this, tr("Import Failed"),
            tr("Failed to import backup."));
    } else {
        QMessageBox::information(this, tr("Import Complete"),
            tr("Backup imported successfully."));
    }

    refreshList();
}

void BackupPage::on_btnDelete_clicked()
{
    auto entry = selectedEntry();
    if (entry.fullPath.isEmpty())
        return;

    auto ret = QMessageBox::question(this, tr("Delete Backup"),
        tr("Delete the selected backup?\n\n%1\n\nThis cannot be undone.")
            .arg(entry.fileName),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (ret != QMessageBox::Yes)
        return;

    m_manager->deleteBackup(entry);
    refreshList();
}

QString BackupPage::humanFileSize(qint64 bytes) const
{
    if (bytes < 1024)
        return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return QString("%1 KiB").arg(bytes / 1024.0, 0, 'f', 1);
    if (bytes < 1024LL * 1024 * 1024)
        return QString("%1 MiB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    return QString("%1 GiB").arg(
        bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
}
```

**Implementation notes:**

1. **Constructor** — Creates the `BackupManager`, configures the tree widget column sizing, wires up selection-change signals.

2. **`openedImpl()`** — Refreshes the backup list every time the page becomes visible. This is the correct place for lazy-loading or refresh logic.

3. **Button handlers** — Each uses `selectedEntry()` to find the currently selected backup, then delegates to `BackupManager` for business logic. All operations end with `refreshList()`.

4. **`humanFileSize()`** — A utility for display; unrelated to the page API but common in real plugins.

---

## `BasePage` Method Reference (Detailed)

### `id() const → QString` <span style="color:red">[pure virtual]</span>

Returns the unique identifier for this page. This string is used in two critical places:

1. **Sidebar selection** — `BasePageContainer::selectPage(pageId)` uses this to navigate to the page.
2. **Toolbar action matching** — `showInstanceWindow(instance, pageId)` opens the dialog and selects the page with this `id()`.

**Conventions:**
- Use kebab-case: `"backup-system"`, `"forge-diagnostics"`, `"my-plugin-stats"`
- Prefix with your plugin name to avoid collisions with built-in pages
- Must be stable across versions (changing it breaks toolbar action linkage)

**Built-in page IDs for reference** (do NOT reuse these):

| ID | Page |
|----|------|
| `"settings"` | Instance Settings |
| `"version"` | Version |
| `"mods"` | Mods |
| `"resourcepacks"` | Resource Packs |
| `"texturepacks"` | Texture Packs |
| `"shaderpacks"` | Shader Packs |
| `"worlds"` | Worlds |
| `"screenshots"` | Screenshots |
| `"notes"` | Notes |
| `"other_logs"` | Other Logs |

### `displayName() const → QString` <span style="color:red">[pure virtual]</span>

The human-readable name shown in the page list sidebar. Wrap in `tr()` for localization:

```cpp
QString displayName() const override { return tr("Backups"); }
```

Keep it short — the sidebar has limited width.

### `icon() const → QIcon` <span style="color:red">[pure virtual]</span>

The icon shown next to the display name in the sidebar. Use themed icons for consistency:

```cpp
QIcon icon() const override
{
    return APPLICATION->getThemedIcon("backup");
}
```

If your icon is not in the theme, you can use `QIcon::fromTheme("icon-name")` or load from a resource file.

### `apply() → bool`

Called when the dialog is about to close or when the user switches to another page. Return `false` to **block** the navigation (e.g., to prompt for unsaved changes):

```cpp
bool apply() override
{
    if (hasUnsavedChanges()) {
        auto ret = QMessageBox::question(this, tr("Unsaved Changes"),
            tr("Save changes before leaving?"),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        if (ret == QMessageBox::Cancel)
            return false;   // Block navigation
        if (ret == QMessageBox::Yes)
            saveChanges();
    }
    return true;  // Allow navigation
}
```

### `shouldDisplay() const → bool`

Return `false` to hide this page from the sidebar entirely. Useful for pages that only apply to certain instance types:

```cpp
bool shouldDisplay() const override
{
    // Only show for instances with a Minecraft version >= 1.13
    // (where data packs are supported)
    return m_instance->getPackProfile()->getMinecraftVersion() >= "1.13";
}
```

### `helpPage() const → QString`

Return a non-empty string to enable the "?" help button in the dialog and link to a handbook page:

```cpp
QString helpPage() const override { return "Instance-Backups"; }
```

The string is used as a path fragment in the handbook URL.

### `openedImpl()`

Called when this page becomes the active (visible) page in the dialog. The default implementation does nothing. Override this to refresh data:

```cpp
void openedImpl() override
{
    refreshBackupList();
    updateStatusLabel();
}
```

**This is called on the GUI thread.** Do not perform blocking I/O here. For expensive operations, kick off a background task and update the UI when it completes.

### `closedImpl()`

Called when the user navigates away from this page. Override to save state or release resources:

```cpp
void closedImpl() override
{
    saveColumnWidths();
}
```

---

## Error Handling

### Hook registration failure

If `hook_register()` returns non-zero, your plugin should abort initialization:

```cpp
int rc = ctx->hook_register(ctx->module_handle,
                            MMCO_HOOK_UI_INSTANCE_PAGES,
                            on_instance_pages, nullptr);
if (rc != 0) {
    MMCO_ERR(ctx, "Failed to register INSTANCE_PAGES hook");
    return rc;  // Abort mmco_init()
}
```

### Null payload fields

Always validate the payload before casting:

```cpp
if (!evt || !evt->page_list_handle || !evt->instance_handle)
    return 0;  // Silently skip — don't crash
```

### Page constructor exceptions

If your page constructor throws (or fails to allocate), the hook callback should catch and log:

```cpp
try {
    pages->append(new MyPage(inst));
} catch (const std::exception& e) {
    if (g_ctx)
        MMCO_ERR(g_ctx, (std::string("Failed to create page: ") + e.what()).c_str());
}
```

---

## Cleanup: `mmco_unload()`

The hook is automatically unregistered when `PluginManager::shutdownAll()` tears down all plugins. You do **not** need to manually unregister the hook in `mmco_unload()`. However, you may explicitly unregister it if your plugin needs to stop injecting pages before full shutdown:

```cpp
extern "C" {
MMCO_EXPORT void mmco_unload()
{
    if (g_ctx) {
        g_ctx->hook_unregister(g_ctx->module_handle,
                               MMCO_HOOK_UI_INSTANCE_PAGES,
                               on_instance_pages);
        MMCO_LOG(g_ctx, "Plugin unloaded — hook unregistered.");
    }
    g_ctx = nullptr;
}
} // extern "C"
```

---

## Thread Safety

| Operation | Thread | Safe? |
|-----------|--------|-------|
| `hook_register()` in `mmco_init()` | Main thread | Yes |
| Hook callback invocation | Main thread | Yes |
| Creating QWidgets in callback | Main thread | Yes (required) |
| Accessing `m_instance` in page methods | Main thread | Yes |
| Using `QApplication::processEvents()` in page | Main thread | Yes, with caution |
| Calling `m_container->selectPage()` from page | Main thread | Yes |

**All UI operations must occur on the main (GUI) thread.** The hook is always dispatched from the main thread, so page creation within the callback is safe. Page methods (`openedImpl()`, `closedImpl()`, button handlers) are also called from the main thread.

---

## Common Pitfalls

### 1. Forgetting the no-op deleter

```cpp
// ❌ CRASH — standard shared_ptr will delete the instance on page destruction
InstancePtr inst(instRaw);

// ✅ CORRECT — no-op deleter
InstancePtr inst(instRaw, [](BaseInstance*) {});
```

### 2. Using the wrong page ID

```cpp
// In the page:
QString id() const override { return "backup-system"; }

// In ui_register_instance_action():
ctx->ui_register_instance_action(ctx->module_handle,
    "View Backups", "...", "backup", "backup_system");  // ❌ underscore!

// The toolbar button will open the dialog but fail to select the page
// because "backup_system" != "backup-system"
```

### 3. Returning non-zero from the callback

```cpp
static int on_instance_pages(void*, uint32_t, void* payload, void*)
{
    // ... create page ...
    return 1;  // ❌ This cancels the hook chain!
    // Other plugins won't get to add their pages
}
```

### 4. Storing instance data globally

```cpp
static InstancePtr g_instance;  // ❌ NEVER DO THIS

static int on_instance_pages(void*, uint32_t, void* payload, void*)
{
    // ...
    g_instance = inst;  // ❌ Outlives the dialog, dangling pointer
    // ...
}
```

### 5. Not using `Q_OBJECT` when calling `tr()`

```cpp
class MyPage : public QWidget, public BasePage
{
    // Missing Q_OBJECT! tr() will fail to compile or use wrong context
  public:
    QString displayName() const override { return tr("My Page"); }
};
```

---

## Related Documentation

| Document | Relevance |
|----------|-----------|
| [README.md](README.md) | Section overview, BasePage interface summary, case study |
| [toolbar-actions.md](toolbar-actions.md) | Registering toolbar shortcuts that navigate to your page |
| [S01 — Hooks Reference](../s01-lifecycle/hooks.md) | `hook_register()`, `hook_unregister()`, all hook IDs |
| [S13 — UI Page Builder](../s13-ui-page-builder/) | C API alternative for building pages without Qt |
