# BackupSystem — A Production Plugin Case Study

## Table of Contents

1. [Introduction](#introduction)
2. [Architecture Overview](#architecture-overview)
3. [File-by-File Walkthrough](#file-by-file-walkthrough)
   - [CMakeLists.txt — Build Configuration](#cmakeliststxt--build-configuration)
   - [BackupPlugin.cpp — The Entry Point](#backupplugincpp--the-entry-point)
   - [BackupManager.h — Core Logic Interface](#backupmanagerh--core-logic-interface)
   - [BackupManager.cpp — Core Logic Implementation](#backupmanagercpp--core-logic-implementation)
   - [BackupPage.h — UI Page Interface](#backuppageh--ui-page-interface)
   - [BackupPage.ui — Qt Designer Layout](#backuppageui--qt-designer-layout)
   - [BackupPage.cpp — UI Page Implementation](#backuppagecpp--ui-page-implementation)
4. [Key Design Decisions](#key-design-decisions)
5. [The QDir::Hidden Bug Fix](#the-qdirhidden-bug-fix)
6. [How Toolbar Actions Work](#how-toolbar-actions-work)
7. [How the Help Button Works](#how-the-help-button-works)
8. [Connecting It All Together](#connecting-it-all-together)
9. [Lessons for Your Own Plugins](#lessons-for-your-own-plugins)

---

## Introduction

The **BackupSystem** plugin is a shipping, production-quality MMCO plugin that adds instance backup/restore functionality to MeshMC. It demonstrates every major plugin capability:

- Multi-file architecture with separation of concerns
- Hook-driven page injection into the instance settings dialog
- Toolbar action registration on the main window
- A full Qt UI page built with Qt Designer (`.ui` file)
- Subclassing `BasePage` (a MeshMC type provided through the SDK)
- Real use of `MMCZip::compressDir` and `MMCZip::extractDir`
- Signal/slot connections for interactive widgets

This walkthrough dissects the entire plugin, file by file, line by line. By the end, you will understand how to build a complex, UI-driven plugin from scratch.

### What BackupSystem Does

| Feature | Description |
|---|---|
| **Create backup** | Zips the entire instance directory (excluding `.backups/`) into a timestamped archive |
| **Restore backup** | Deletes all current instance files (except `.backups/`) and extracts the archive |
| **Export backup** | Copies a backup zip to a user-chosen location |
| **Import backup** | Copies an external zip into the backups directory |
| **Delete backup** | Removes a backup archive |

---

## Architecture Overview

BackupSystem consists of five source files in `meshmc/plugins/BackupSystem/`:

```
BackupSystem/
├── CMakeLists.txt       # Build configuration
├── BackupPlugin.cpp     # MMCO entry point (mmco_init, mmco_unload, hooks)
├── BackupManager.h      # Core backup logic — interface
├── BackupManager.cpp    # Core backup logic — implementation
├── BackupPage.h         # UI page — interface
├── BackupPage.cpp       # UI page — implementation
└── BackupPage.ui        # Qt Designer form for the UI layout
```

The architecture follows a clean three-layer pattern:

```
┌─────────────────┐
│ BackupPlugin.cpp │  ← MMCO entry point (hooks + toolbar action)
└────────┬────────┘
         │ creates
         ▼
┌─────────────────┐
│   BackupPage    │  ← UI (QWidget + BasePage subclass)
└────────┬────────┘
         │ owns
         ▼
┌─────────────────┐
│  BackupManager  │  ← Core logic (zip, restore, file ops)
└─────────────────┘
```

**BackupPlugin.cpp** is the thinnest layer — it exports `mmco_module_info`, `mmco_init`, and `mmco_unload`, registers one hook and one toolbar action. It creates `BackupPage` instances in the hook callback.

**BackupPage** is the user interface. It owns a `BackupManager` and translates user interactions (button clicks) into `BackupManager` calls.

**BackupManager** is pure logic — no UI, no MMCO API. It could be unit-tested independently. It uses `MMCZip` (provided through the SDK header) for archive operations and `QDir`/`QFile` for filesystem work.

---

## File-by-File Walkthrough

### CMakeLists.txt — Build Configuration

```cmake
set(BACKUP_SOURCES
    BackupPlugin.cpp
    BackupManager.h
    BackupManager.cpp
    BackupPage.h
    BackupPage.cpp
)

set(BACKUP_UIS
    BackupPage.ui
)

qt_wrap_ui(BACKUP_UI_HEADERS ${BACKUP_UIS})

add_library(BackupSystem MODULE ${BACKUP_SOURCES} ${BACKUP_UI_HEADERS})

target_link_libraries(BackupSystem PRIVATE
    MeshMC_logic
    MeshMC::SDK
)

target_include_directories(BackupSystem PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
)

set_target_properties(BackupSystem PROPERTIES
    PREFIX ""
    SUFFIX ".mmco"
    LIBRARY_OUTPUT_DIRECTORY "${MESHMC_PLUGIN_STAGING_DIR}"
)
```

**Key points:**

| Line | Purpose |
|---|---|
| `qt_wrap_ui(BACKUP_UI_HEADERS ${BACKUP_UIS})` | Runs Qt's `uic` tool on `BackupPage.ui`, generating `ui_BackupPage.h` in the build directory. This header defines the `Ui::BackupPage` class with all the widgets declared in the designer file. |
| `add_library(BackupSystem MODULE ...)` | `MODULE` — loaded at runtime with `dlopen`. Not linked statically. |
| `MeshMC_logic` | **In-tree link**: because BackupSystem lives inside the MeshMC source tree (`meshmc/plugins/`), it links directly against the launcher's logic library for symbols. Out-of-tree plugins would not use this. |
| `MeshMC::SDK` | Provides all include paths and Qt dependencies. |
| `${CMAKE_CURRENT_BINARY_DIR}` | Needed so `#include "ui_BackupPage.h"` finds the generated header. |
| `PREFIX ""` / `SUFFIX ".mmco"` | Output: `BackupSystem.mmco` (not `libBackupSystem.so`). |
| `LIBRARY_OUTPUT_DIRECTORY` | Stage the built `.mmco` into a common directory so MeshMC can find it during development builds. |

The install rule at the bottom places the `.mmco` alongside the MeshMC binary:

```cmake
install(TARGETS BackupSystem
    LIBRARY DESTINATION "${BINARY_DEST_DIR}/mmcmodules"
)
```

#### In-Tree vs Out-of-Tree

BackupSystem ships as a **built-in plugin** — it is compiled as part of the MeshMC build. It uses `MeshMC_logic` for direct symbol access. An out-of-tree plugin would only use `MeshMC::SDK` and `find_package(MeshMC_SDK)`. The CMakeLists.txt would look like the one in the [Hello World tutorial](hello-world.md).

---

### BackupPlugin.cpp — The Entry Point

This is the MMCO glue layer. It is the only file that interacts with the MMCO C API.

#### The Include and Global State

```cpp
#include "plugin/sdk/mmco_sdk.h"
#include "BackupPage.h"

static MMCOContext* g_ctx = nullptr;
```

Note the include path: `"plugin/sdk/mmco_sdk.h"`. Since BackupSystem is in-tree, it uses the source-tree relative path. Out-of-tree plugins would use `"mmco_sdk.h"` (the installed path).

#### The Instance Pages Hook

```cpp
static int on_instance_pages(void* /*mh*/, uint32_t /*hook_id*/, void* payload,
                             void* /*ud*/)
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
```

This is the core of the plugin's integration with MeshMC. Let's break it down:

**The hook**: `MMCO_HOOK_UI_INSTANCE_PAGES` fires whenever MeshMC builds the list of pages for the instance settings dialog. MeshMC calls all registered callbacks, giving each one a chance to add pages.

**The payload**: `MMCOInstancePagesEvent` has five fields:

```cpp
struct MMCOInstancePagesEvent {
    const char* instance_id;    // which instance
    const char* instance_name;  // human name
    const char* instance_path;  // filesystem path
    void* page_list_handle;     // opaque — actually QList<BasePage*>*
    void* instance_handle;      // opaque — actually BaseInstance*
};
```

The `page_list_handle` and `instance_handle` are typed as `void*` in the C API but are actually Qt/MeshMC types. Since the SDK header includes `BasePage.h` and `BaseInstance.h`, you can safely cast them.

**The non-owning shared_ptr trick**:

```cpp
InstancePtr inst = std::shared_ptr<BaseInstance>(
    instRaw, [](BaseInstance*) { /* no-op deleter */ });
```

`InstancePtr` is `std::shared_ptr<BaseInstance>`. MeshMC types expect `InstancePtr`, but the instance passed through the hook is **not owned by the plugin** — it is owned by MeshMC's `InstanceList`. Creating a `shared_ptr` with a no-op deleter wraps the raw pointer without taking ownership. When the `BackupPage` is destroyed, the `shared_ptr` destructs but does nothing.

**Appending the page**:

```cpp
pages->append(new BackupPage(inst));
```

The `BackupPage` is heap-allocated and added to the page list. MeshMC takes ownership of the page (it will be deleted when the instance dialog closes).

#### Module Declaration

```cpp
MMCO_DEFINE_MODULE(
    "BackupSystem", "1.0.0", "Project Tick",
    "Instance backup snapshots — create, restore, export, import",
    "GPL-3.0-or-later");
```

Same pattern as Hello World. The name `"BackupSystem"` appears in log prefixes.

#### mmco_init

```cpp
extern "C" {

MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;

    MMCO_LOG(ctx, "BackupSystem v1.0.0 initializing...");

    int rc = ctx->hook_register(ctx->module_handle, MMCO_HOOK_UI_INSTANCE_PAGES,
                                on_instance_pages, nullptr);
    if (rc != 0) {
        MMCO_ERR(ctx, "Failed to register INSTANCE_PAGES hook");
        return rc;
    }

    ctx->ui_register_instance_action(
        ctx->module_handle, "View Backups",
        "View and manage backups for this instance.", "backup",
        "backup-system");

    MMCO_LOG(ctx, "BackupSystem initialized successfully.");
    return 0;
}
```

Two things happen here:

1. **Hook registration**: Subscribe to `MMCO_HOOK_UI_INSTANCE_PAGES`. Note the error handling — if registration fails, the plugin returns the error code, which tells MeshMC initialization failed.

2. **Toolbar action registration**: `ui_register_instance_action` adds a button to the main window's instance toolbar.

#### mmco_unload

```cpp
MMCO_EXPORT void mmco_unload()
{
    if (g_ctx) {
        MMCO_LOG(g_ctx, "BackupSystem unloading.");
    }
    g_ctx = nullptr;
}
```

Minimal cleanup. No resources to free — the `BackupPage` instances are owned by MeshMC's dialog system.

---

### BackupManager.h — Core Logic Interface

```cpp
struct BackupEntry {
    QString name;        // human-readable label
    QString fileName;    // zip file name (e.g. "2026-01-15_14-30-00.zip")
    QString fullPath;    // absolute path to the backup zip
    QDateTime timestamp; // when the backup was created
    qint64 sizeBytes;   // file size
};
```

`BackupEntry` is a plain data struct representing a single backup. It uses Qt types (`QString`, `QDateTime`) which are available through the SDK header. The struct is passed by value between `BackupManager` and `BackupPage`.

```cpp
class BackupManager
{
  public:
    explicit BackupManager(const QString& instanceId,
                           const QString& instanceRoot);

    QString backupDir() const;
    QList<BackupEntry> listBackups() const;
    BackupEntry createBackup(const QString& label = {});
    bool restoreBackup(const BackupEntry& entry);
    bool exportBackup(const BackupEntry& entry, const QString& destPath);
    BackupEntry importBackup(const QString& srcZipPath,
                             const QString& label = {});
    bool deleteBackup(const BackupEntry& entry);

  private:
    void ensureBackupDir();
    QString generateFileName(const QString& label) const;
    BackupEntry entryFromFile(const QString& filePath) const;

    QString m_instanceId;
    QString m_instanceRoot;
    QString m_backupDir;
};
```

The public API is intentionally simple — five operations plus a query. No MMCO API, no `MMCOContext`. This class is pure business logic.

**Construction**: Takes an instance ID and root path. Both come from `InstancePtr` (via `m_instance->id()` and `m_instance->instanceRoot()`).

---

### BackupManager.cpp — Core Logic Implementation

#### Constructor and Backup Directory

```cpp
BackupManager::BackupManager(const QString& instanceId,
                             const QString& instanceRoot)
    : m_instanceId(instanceId), m_instanceRoot(instanceRoot)
{
    m_backupDir = QDir(m_instanceRoot).filePath(".backups");
    ensureBackupDir();
}
```

Backups are stored in a `.backups` subdirectory inside the instance root. The leading dot hides it on Unix systems. `ensureBackupDir()` calls `QDir().mkpath(m_backupDir)` — `mkpath` creates intermediate directories and is a no-op if the directory already exists.

#### listBackups()

```cpp
QList<BackupEntry> BackupManager::listBackups() const
{
    QDir dir(m_backupDir);
    if (!dir.exists())
        return result;

    const auto files = dir.entryInfoList({"*.zip"}, QDir::Files, QDir::Time);
    for (const auto& fi : files) {
        result.append(entryFromFile(fi.absoluteFilePath()));
    }

    return result;
}
```

Lists all `.zip` files in the backup directory, sorted by modification time (`QDir::Time`). Each file is converted to a `BackupEntry` through `entryFromFile()`.

#### createBackup()

```cpp
BackupEntry BackupManager::createBackup(const QString& label)
{
    ensureBackupDir();

    QString fileName = generateFileName(label);
    QString zipPath = QDir(m_backupDir).filePath(fileName);

    bool ok = MMCZip::compressDir(zipPath, m_instanceRoot,
                                  [this](const QString& entry) -> bool {
                                      return entry.startsWith(".backups/") ||
                                             entry.startsWith(".backups\\");
                                  });

    if (!ok) {
        qWarning() << "[BackupSystem] Failed to create backup:" << zipPath;
        QFile::remove(zipPath);
        return {};
    }

    return entryFromFile(zipPath);
}
```

**`MMCZip::compressDir`** is a MeshMC utility (available through the SDK) that creates a zip archive from a directory. The third argument is a **filter callback**: it receives each relative path that would be included in the archive and returns `true` to **exclude** it. Here, the filter excludes the `.backups/` directory itself — you don't want backups containing other backups.

Note the cross-platform filter: it checks for both `/` and `\\` path separators.

**Error handling**: If compression fails, the partially-written zip is removed to avoid leaving corrupt files.

**`generateFileName()`** creates a timestamp-based filename:

```cpp
QString BackupManager::generateFileName(const QString& label) const
{
    QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
    if (label.isEmpty()) {
        return ts + ".zip";
    }
    QString safe = label;
    safe.replace(QRegularExpression("[^a-zA-Z0-9_-]"), "_");
    safe.truncate(64);
    return ts + "_" + safe + ".zip";
}
```

If the user provides a label like `"Before mods"`, the filename becomes `2026-01-15_14-30-00_Before_mods.zip`. Special characters are replaced with underscores, and the label is truncated to 64 characters to avoid filesystem limits.

#### restoreBackup() — The Most Critical Operation

```cpp
bool BackupManager::restoreBackup(const BackupEntry& entry)
{
    if (!QFile::exists(entry.fullPath)) {
        qWarning() << "[BackupSystem] Backup file not found:" << entry.fullPath;
        return false;
    }

    QDir instDir(m_instanceRoot);
    const auto entries = instDir.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
    for (const auto& fi : entries) {
        if (fi.fileName() == ".backups")
            continue;

        if (fi.isDir()) {
            QDir(fi.absoluteFilePath()).removeRecursively();
        } else {
            QFile::remove(fi.absoluteFilePath());
        }
    }

    auto result = MMCZip::extractDir(entry.fullPath, m_instanceRoot);
    if (!result.has_value()) {
        qWarning() << "[BackupSystem] Failed to extract backup:"
                   << entry.fullPath;
        return false;
    }

    return true;
}
```

Restore is a **two-phase operation**:

1. **Delete everything** in the instance root except `.backups/`
2. **Extract the backup archive** into the instance root

The deletion loop uses `QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden`. The `QDir::Hidden` flag is critical — see [The QDir::Hidden Bug Fix](#the-qdirhidden-bug-fix) section below.

**`MMCZip::extractDir`** returns `std::optional<QStringList>` — the list of extracted files on success, or `std::nullopt` on failure. We only check whether the result has a value.

#### exportBackup() and importBackup()

```cpp
bool BackupManager::exportBackup(const BackupEntry& entry,
                                 const QString& destPath)
{
    if (!QFile::exists(entry.fullPath))
        return false;

    QDir().mkpath(QFileInfo(destPath).absolutePath());
    return QFile::copy(entry.fullPath, destPath);
}
```

Export is a simple file copy. `mkpath` ensures the destination directory exists.

```cpp
BackupEntry BackupManager::importBackup(const QString& srcZipPath,
                                        const QString& label)
{
    if (!QFile::exists(srcZipPath))
        return {};

    ensureBackupDir();

    QString fileName = generateFileName(label);
    QString destPath = QDir(m_backupDir).filePath(fileName);

    if (!QFile::copy(srcZipPath, destPath))
        return {};

    return entryFromFile(destPath);
}
```

Import copies an external zip into the `.backups` directory with a new generated filename. This means the imported backup gets a fresh timestamp in its filename.

#### entryFromFile() — Parsing Backup Metadata

```cpp
BackupEntry BackupManager::entryFromFile(const QString& filePath) const
{
    QFileInfo fi(filePath);
    BackupEntry entry;
    entry.fileName = fi.fileName();
    entry.fullPath = fi.absoluteFilePath();
    entry.sizeBytes = fi.size();
    entry.timestamp =
        fi.birthTime().isValid() ? fi.birthTime() : fi.lastModified();

    QString base = fi.completeBaseName();
    int thirdUs = base.indexOf('_', 20);
    if (thirdUs > 0 && thirdUs < base.size() - 1) {
        entry.name = base.mid(thirdUs + 1).replace('_', ' ');
    } else {
        entry.name = base;
    }

    return entry;
}
```

This parses a `BackupEntry` from a zip file path:

- **Timestamp**: Prefers the file's birth time (creation time). Falls back to last modified if birth time is unavailable (common on Linux ext4).
- **Name extraction**: Filenames follow the pattern `yyyy-MM-dd_HH-mm-ss_Label.zip`. The timestamp part is always 19 characters (`2026-01-15_14-30-00`), so the code looks for an underscore after position 20. If found, everything after it is the label (with underscores replaced by spaces). If not found, the entire base name is used.

---

### BackupPage.h — UI Page Interface

```cpp
class BackupPage : public QWidget, public BasePage
{
    Q_OBJECT
```

`BackupPage` uses **multiple inheritance**: it is a `QWidget` (for Qt's widget system) and a `BasePage` (for MeshMC's page system). `BasePage` is a non-QObject interface that defines how pages integrate with MeshMC's tabbed settings dialog.

#### BasePage Overrides

```cpp
QString id() const override { return "backup-system"; }
QString displayName() const override { return tr("Backups"); }
QIcon icon() const override;
QString helpPage() const override { return "Instance-Backups"; }
bool shouldDisplay() const override { return true; }
void openedImpl() override;
```

| Method | Purpose |
|---|---|
| `id()` | Unique identifier for the page. Used by `ui_register_instance_action` to navigate to this page. |
| `displayName()` | Text shown in the settings dialog sidebar. |
| `icon()` | Icon shown next to the display name. |
| `helpPage()` | Wiki page name for the help button (see [How the Help Button Works](#how-the-help-button-works)). |
| `shouldDisplay()` | Whether to show this page. Could return `false` for pages that are conditionally visible. |
| `openedImpl()` | Called when the user navigates to this tab. Used to refresh the backup list. |

#### Slots

```cpp
private slots:
    void on_btnCreate_clicked();
    void on_btnRestore_clicked();
    void on_btnExport_clicked();
    void on_btnImport_clicked();
    void on_btnDelete_clicked();
    void onSelectionChanged();
```

These follow Qt's **auto-connect naming convention**: `on_<objectName>_<signal>()`. When `setupUi()` is called, Qt automatically connects `btnCreate`'s `clicked()` signal to `on_btnCreate_clicked()`. This is a Qt feature (via `QMetaObject::connectSlotsByName`), not an MMCO feature.

`onSelectionChanged()` is manually connected in the constructor because it follows the `QTreeWidget::itemSelectionChanged` signal, which doesn't follow the auto-connect naming pattern.

---

### BackupPage.ui — Qt Designer Layout

The `.ui` file is an XML document created with Qt Designer. It defines the visual layout without any C++ code.

```xml
<widget class="QWidget" name="BackupPage">
  <layout class="QVBoxLayout" name="verticalLayout">
    <item>
      <widget class="QTreeWidget" name="backupList">
        <column><property name="text"><string>Name</string></property></column>
        <column><property name="text"><string>Date</string></property></column>
        <column><property name="text"><string>Size</string></property></column>
      </widget>
    </item>
    <item>
      <layout class="QHBoxLayout" name="buttonLayout">
        <!-- Create, Restore, Export, Import, Delete buttons -->
      </layout>
    </item>
    <item>
      <widget class="QLabel" name="statusLabel">
        <property name="text"><string>No backups</string></property>
      </widget>
    </item>
  </layout>
</widget>
```

The layout hierarchy:

```
QVBoxLayout (verticalLayout)
├── QTreeWidget (backupList)       — 3 columns: Name, Date, Size
├── QHBoxLayout (buttonLayout)
│   ├── QPushButton (btnCreate)    — always enabled
│   ├── QPushButton (btnRestore)   — disabled until selection
│   ├── QPushButton (btnExport)    — disabled until selection
│   ├── QPushButton (btnImport)    — always enabled
│   ├── QPushButton (btnDelete)    — disabled until selection
│   └── QSpacerItem                — pushes buttons to the left
└── QLabel (statusLabel)           — "No backups" / "3 backup(s)"
```

Important `.ui` properties:

- **`selectionMode: SingleSelection`** — Only one backup can be selected at a time.
- **`rootIsDecorated: false`** — No expand/collapse arrows (it is a flat list, not a tree).
- **`sortingEnabled: true`** — Clicking column headers sorts the list.
- **Restore, Export, Delete start `enabled: false`** — They are enabled dynamically when the user selects a backup.
- Each button has a **theme icon** (`list-add`, `edit-undo`, `document-save-as`, `document-open`, `edit-delete`) for consistent platform look.

The `qt_wrap_ui()` CMake command processes this file and generates `ui_BackupPage.h`, which contains:

```cpp
namespace Ui {
class BackupPage {
public:
    QTreeWidget* backupList;
    QPushButton* btnCreate;
    QPushButton* btnRestore;
    QPushButton* btnExport;
    QPushButton* btnImport;
    QPushButton* btnDelete;
    QLabel* statusLabel;

    void setupUi(QWidget* widget);
};
}
```

---

### BackupPage.cpp — UI Page Implementation

#### Constructor

```cpp
BackupPage::BackupPage(InstancePtr instance, QWidget* parent)
    : QWidget(parent), ui(new Ui::BackupPage), m_instance(instance)
{
    ui->setupUi(this);

    m_manager = std::make_unique<BackupManager>(m_instance->id(),
                                                m_instance->instanceRoot());

    ui->backupList->header()->setStretchLastSection(false);
    ui->backupList->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->backupList->header()->setSectionResizeMode(
        1, QHeaderView::ResizeToContents);
    ui->backupList->header()->setSectionResizeMode(
        2, QHeaderView::ResizeToContents);

    connect(ui->backupList, &QTreeWidget::itemSelectionChanged, this,
            &BackupPage::onSelectionChanged);

    refreshList();
}
```

Step by step:

1. **`ui->setupUi(this)`** — Inflates the `.ui` layout onto this widget. After this call, `ui->backupList`, `ui->btnCreate`, etc. all point to live widgets.

2. **Create BackupManager** — Uses the instance's ID and root path. `std::make_unique` ensures the manager's lifetime is tied to the page.

3. **Configure column headers**:
   - Column 0 (Name): stretches to fill available space
   - Column 1 (Date): resizes to fit content
   - Column 2 (Size): resizes to fit content
   - `setStretchLastSection(false)`: prevents the Size column from stretching

4. **Manual signal connection** — The `itemSelectionChanged` signal doesn't follow the auto-connect naming convention, so it needs an explicit `connect()`.

5. **`refreshList()`** — Populates the backup list immediately.

#### refreshList()

```cpp
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
```

This clears the tree, queries `BackupManager` for the current list, and creates a `QTreeWidgetItem` for each entry. The full path is stored in `Qt::UserRole` data on column 0 — this allows `selectedEntry()` to map the UI selection back to a `BackupEntry`.

The `tr("%n backup(s)", "", count)` uses Qt's plural-form translation, which handles "1 backup" vs "3 backups" correctly in all languages.

#### Button Handlers

Each button handler follows the same pattern:

1. Get the selected entry (if needed)
2. Show a dialog (confirmation, file chooser, or input)
3. Call `BackupManager`
4. Show a result dialog (success or failure)
5. Refresh the list

**Create** (`on_btnCreate_clicked`):

```cpp
bool ok = false;
QString label = QInputDialog::getText(this, tr("Create Backup"),
                                      tr("Backup label (optional):"),
                                      QLineEdit::Normal, {}, &ok);
if (!ok)
    return;

ui->statusLabel->setText(tr("Creating backup..."));
QApplication::processEvents();

auto entry = m_manager->createBackup(label);
```

The `QApplication::processEvents()` call forces the UI to update the status label before the potentially long `createBackup()` operation blocks the UI thread. Without it, the label would stay at its previous text until the backup finished.

**Restore** (`on_btnRestore_clicked`):

```cpp
auto ret = QMessageBox::warning(
    this, tr("Restore Backup"),
    tr("This will replace the current instance contents with the "
       "backup:\n\n%1\n\nThis action cannot be undone. Continue?")
        .arg(entry.fileName),
    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

if (ret != QMessageBox::Yes)
    return;
```

The restore dialog uses `QMessageBox::warning` (not `information`) with `No` as the default button. This is a destructive operation — the UX should make the user pause and confirm.

**Export** (`on_btnExport_clicked`):

```cpp
QString dest = QFileDialog::getSaveFileName(
    this, tr("Export Backup"), entry.fileName, tr("Zip Files (*.zip)"));
```

Pre-populates the save dialog with the backup's filename as the default.

**Import** (`on_btnImport_clicked`):

```cpp
QString src = QFileDialog::getOpenFileName(this, tr("Import Backup"), {},
                                           tr("Zip Files (*.zip)"));
```

Opens a file picker filtered to `.zip` files.

**Delete** (`on_btnDelete_clicked`):

```cpp
auto ret = QMessageBox::question(
    this, tr("Delete Backup"),
    tr("Delete the selected backup?\n\n%1\n\nThis cannot be undone.")
        .arg(entry.fileName),
    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
```

Another destructive operation with `No` as default.

#### humanFileSize()

```cpp
QString BackupPage::humanFileSize(qint64 bytes) const
{
    if (bytes < 1024)
        return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return QString("%1 KiB").arg(bytes / 1024.0, 0, 'f', 1);
    if (bytes < 1024LL * 1024 * 1024)
        return QString("%1 MiB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    return QString("%1 GiB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
}
```

Converts bytes to human-readable sizes using binary units (KiB, MiB, GiB). Note the `1024LL` — without the `LL` suffix, `1024 * 1024 * 1024` overflows a 32-bit `int`.

---

## Key Design Decisions

### 1. Separation of Plugin Entry Point from Logic

BackupPlugin.cpp is only ~75 lines. It does exactly three things: declare the module, register a hook, register a toolbar action. All actual logic is in BackupManager and BackupPage.

**Why**: If the MMCO API changes, only BackupPlugin.cpp needs updating. BackupManager and BackupPage don't use the MMCO C API at all — they use Qt and MeshMC types directly (via the SDK header).

### 2. Non-Owning shared_ptr for Instance

```cpp
InstancePtr inst = std::shared_ptr<BaseInstance>(
    instRaw, [](BaseInstance*) { /* no-op deleter */ });
```

**Why**: The `BaseInstance*` is owned by MeshMC's `InstanceList`. The plugin must not delete it. But `InstancePtr` is `std::shared_ptr<BaseInstance>`, which MeshMC's API expects. A no-op deleter bridges this gap safely.

**Alternative considered**: Storing a raw pointer. Rejected because `BackupPage` wants to call methods that take `InstancePtr`, such as `id()` on the stored instance. Using `InstancePtr` is consistent with MeshMC's internal code.

### 3. BackupManager Has No UI Dependency

BackupManager.h does not include any Qt widget headers. It uses `QString`, `QDateTime`, `QFileInfo`, `QDir` — core types — plus `MMCZip` for archive operations.

**Why**: This makes it testable in isolation and reusable. If someone wanted to build a command-line backup tool, they could use BackupManager directly.

### 4. Backups Stored Inside the Instance

Backups go to `<instance_root>/.backups/`. An alternative would be a global backup directory.

**Why instance-local**: Backups travel with the instance if copied/moved. No global state to manage. Simple cleanup: deleting the instance deletes its backups.

**Tradeoff**: The instance's disk usage grows. Users who want off-disk backups can use the Export feature.

### 5. QApplication::processEvents() for Long Operations

```cpp
ui->statusLabel->setText(tr("Creating backup..."));
QApplication::processEvents();
```

**Why**: `createBackup()` can take seconds for large instances. Without `processEvents()`, the UI appears frozen during the operation. This is a pragmatic solution for a single-threaded plugin.

**Better alternative**: Use a `QThread` or `QtConcurrent::run` for truly async backup creation. The current approach was chosen for simplicity — the backup operation is fast enough (typically < 5 seconds) that a brief UI freeze is acceptable.

---

## The QDir::Hidden Bug Fix

This section discusses a real bug that was discovered during development and the fix that was applied.

### The Problem

The original `restoreBackup()` code listed files for deletion like this:

```cpp
// BUGGY VERSION
const auto entries = instDir.entryInfoList(
    QDir::AllEntries | QDir::NoDotAndDotDot);
```

This **did not include hidden files and directories**. On Unix systems, files starting with `.` (like `.fabric/`, `.pack.mcmeta`, `.game_version`) are hidden. The `QDir::AllEntries` flag includes regular files, directories, and symlinks — but **not hidden entries** unless `QDir::Hidden` is explicitly set.

### The Symptom

After restoring a backup:
1. Files that existed **in the backup** were correctly restored
2. Files added **after** the backup was created were supposed to be deleted
3. But **hidden files** added after the backup survived the restore

For example, if a user installed a mod loader after taking the backup, the loader's hidden config directories (`.fabric/`, `.quilt/`) would remain, potentially causing conflicts with the restored state.

### The Fix

```cpp
// FIXED VERSION
const auto entries = instDir.entryInfoList(
    QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
```

Adding `QDir::Hidden` ensures that hidden files and directories are included in the deletion pass before extraction.

### The Lesson

When using `QDir::entryInfoList()` for deletion or synchronization operations, always include `QDir::Hidden`. The default behavior of excluding hidden files is designed for user-facing file listings (where `.hidden` files should be invisible), not for programmatic operations that need a complete view of the filesystem.

---

## How Toolbar Actions Work

In `mmco_init`, BackupSystem registers a toolbar action:

```cpp
ctx->ui_register_instance_action(
    ctx->module_handle, "View Backups",
    "View and manage backups for this instance.", "backup",
    "backup-system");
```

### Parameters

| Parameter | Value | Purpose |
|---|---|---|
| `module_handle` | `ctx->module_handle` | Identifies the plugin |
| `text` | `"View Backups"` | Button label in the toolbar |
| `tooltip` | `"View and manage backups..."` | Tooltip on hover |
| `icon_name` | `"backup"` | MeshMC theme icon name |
| `page_id` | `"backup-system"` | The `id()` of the page to navigate to |

### How It Works Internally

1. **Registration**: `ui_register_instance_action` stores the action metadata in MeshMC's `PluginManager`.

2. **MainWindow picks it up**: When the `MainWindow` builds its toolbar (on startup or when actions change), it queries the `PluginManager` for all registered instance actions.

3. **Button appears**: A `QAction` is created with the specified text, tooltip, and icon, and added to the instance toolbar.

4. **User clicks it**: When clicked, MeshMC opens the instance settings dialog and navigates to the page whose `id()` matches the registered `page_id` (`"backup-system"`).

5. **Page matching**: The `"backup-system"` string must exactly match the value returned by `BackupPage::id()`. If they don't match, the dialog opens but no page is selected.

### The Connection

This is why `BackupPage::id()` returns `"backup-system"`:

```cpp
QString id() const override { return "backup-system"; }
```

And the action registration uses the same string:

```cpp
ctx->ui_register_instance_action(..., "backup-system");
```

---

## How the Help Button Works

MeshMC's instance settings dialog includes a "Help" button. When clicked, it opens a documentation page based on the currently selected tab. Each `BasePage` subclass provides a help page identifier:

```cpp
QString helpPage() const override { return "Instance-Backups"; }
```

This string is used as a wiki/documentation page identifier. MeshMC constructs a URL from it (typically `https://projecttick.org/wiki/<helpPage>`) and opens it in the user's default browser.

**If you don't want a help button** for your page, return an empty string:

```cpp
QString helpPage() const override { return {}; }
```

---

## Connecting It All Together

Here is the complete flow when a user interacts with the BackupSystem plugin:

### Creating a Backup

```
1. User opens instance settings (right-click → "Instance Settings")
2. MeshMC fires MMCO_HOOK_UI_INSTANCE_PAGES
3. BackupPlugin::on_instance_pages() runs
4. Creates a new BackupPage(instance) and appends it to the page list
5. MeshMC displays the settings dialog with "Backups" tab
6. User navigates to the "Backups" tab
7. BackupPage::openedImpl() fires → refreshList()
8. BackupManager::listBackups() scans .backups/ directory
9. QTreeWidget shows existing backups
10. User clicks "Create"
11. BackupPage::on_btnCreate_clicked() runs
12. QInputDialog asks for an optional label
13. BackupManager::createBackup("My label") runs
14. MMCZip::compressDir() zips the instance (excluding .backups/)
15. Success dialog shown to user
16. refreshList() updates the tree with the new backup
```

### Via Toolbar Button

```
1. User selects an instance in the main window
2. User clicks the "View Backups" toolbar button
3. MeshMC opens instance settings and navigates to page_id "backup-system"
4. BackupPage for that instance is displayed
5. (Same as steps 7-16 above for any operation)
```

---

## Lessons for Your Own Plugins

### Architecture Patterns

1. **Keep the MMCO entry point thin**. `BackupPlugin.cpp` is under 80 lines. Your `mmco_init` should register hooks and actions, not implement business logic.

2. **Separate logic from UI**. `BackupManager` can be tested without a running MeshMC instance or a display. Your core logic should be similarly independent.

3. **Use BasePage for tabbed pages**. If your plugin adds a page to the instance settings dialog, subclass `QWidget` + `BasePage`, provide the required overrides, and inject via `MMCO_HOOK_UI_INSTANCE_PAGES`.

4. **Use .ui files for complex layouts**. Hand-coding a layout with 5 buttons, a tree widget, and a status label would be verbose and error-prone. Qt Designer generates cleaner, more maintainable layouts.

### Common Pitfalls

1. **Forgetting `QDir::Hidden`** in file operations. Always include it when you need a complete filesystem view.

2. **Owning instance pointers**. Never store a `shared_ptr` with a real deleter for instances you don't own. Use the no-op deleter pattern.

3. **Blocking the UI thread**. For operations under 5 seconds, `processEvents()` is acceptable. For longer operations, use threading.

4. **Mismatched page IDs**. The string passed to `ui_register_instance_action` must exactly match the value returned by `BasePage::id()`.

5. **Not checking hook registration return values**. If `hook_register` fails, log and return an error from `mmco_init`. Don't continue with half-initialized state.

### What to Study Next

- **Page Builder API** (Section 13 of MMCOContext): For plugins that want to build UI entirely through the C API without Qt Designer or `BasePage` subclassing.
- **Network API** (Section 11): For plugins that need to communicate with external services.
- **Mod Management API** (Section 5): For plugins that automate mod installation or configuration.
- **Hooks Reference**: For the full list of events you can subscribe to.
