# MeshMC Architecture

## Architectural Overview

MeshMC follows a layered architecture built on top of Qt6's object model. The application is structured as a single-process desktop application with a central `Application` singleton that owns all major subsystems. The architecture can be decomposed into five primary layers:

```
┌─────────────────────────────────────────────────────────────┐
│                    UI Layer (Qt Widgets)                     │
│  MainWindow, InstanceWindow, Dialogs, Pages, SetupWizard    │
├─────────────────────────────────────────────────────────────┤
│                   Controller Layer                           │
│  LaunchController, UpdateController, InstanceTask            │
├─────────────────────────────────────────────────────────────┤
│                    Model Layer                               │
│  InstanceList, PackProfile, AccountList, ModFolderModel      │
│  IconList, JavaInstallList, TranslationsModel, BaseVersion   │
├─────────────────────────────────────────────────────────────┤
│                   Service Layer                              │
│  Network (Download, NetJob), Auth (AuthFlow, MSAStep),       │
│  Meta (Index), HttpMetaCache, Settings, FileSystem           │
├─────────────────────────────────────────────────────────────┤
│                Platform / Infrastructure Layer               │
│  Qt6 Core, LocalPeer, Katabasis, systeminfo, GAnalytics     │
└─────────────────────────────────────────────────────────────┘
```

## Directory Structure

### Top-Level Repository Layout

```
meshmc/
├── CMakeLists.txt            # Root build configuration
├── CMakePresets.json          # Platform-specific build presets
├── BUILD.md                   # Build instructions
├── CONTRIBUTING.md            # Contribution guidelines
├── COPYING.md                 # License text (GPL-3.0-or-later)
├── REUSE.toml                 # REUSE license compliance metadata
├── branding/                  # Icons, desktop files, manifests, NSIS scripts
├── buildconfig/               # BuildConfig.h generation (version, URLs, keys)
├── cmake/                     # Custom CMake modules
├── launcher/                  # Main application source code
├── libraries/                 # Bundled third-party libraries
├── updater/                   # Self-updater binary
├── nix/                       # Nix packaging support
├── scripts/                   # Development scripts
├── default.nix                # Nix expression
├── flake.nix                  # Nix flake
├── flake.lock                 # Nix flake lock
├── shell.nix                  # Nix development shell
├── Containerfile              # Container build (Podman/Docker)
```

### Launcher Source Tree (`launcher/`)

The `launcher/` directory contains all application source code. The organization follows a feature-module pattern:

```
launcher/
├── main.cpp                        # Application entry point
├── Application.{h,cpp}             # Application singleton
├── ApplicationMessage.{h,cpp}      # IPC message handling
├── BaseInstance.{h,cpp}             # Abstract instance interface
├── BaseInstaller.{h,cpp}           # Base installer interface
├── BaseVersion.h                    # Abstract version interface
├── BaseVersionList.{h,cpp}         # Abstract version list model
├── BuildConfig.h                    # Build-time configuration (generated)
├── Commandline.{h,cpp}             # Command-line argument parsing
├── DefaultVariable.h               # Default-valued variable wrapper
├── DesktopServices.{h,cpp}         # Platform desktop integration (open URLs, folders)
├── Exception.h                      # Base exception class
├── ExponentialSeries.h             # Exponential backoff helper
├── FileSystem.{h,cpp}              # Filesystem utilities (copy, move, path combining)
├── Filter.{h,cpp}                  # Log/text filtering
├── GZip.{h,cpp}                    # GZip compression/decompression
├── HoeDown.h                       # Markdown rendering wrapper
├── InstanceCopyTask.{h,cpp}        # Instance cloning logic
├── InstanceCreationTask.{h,cpp}    # New instance creation
├── InstanceImportTask.{h,cpp}      # Instance import from ZIP/URL
├── InstanceList.{h,cpp}            # Instance collection model (QAbstractListModel)
├── InstancePageProvider.h           # Instance page factory interface
├── InstanceTask.{h,cpp}            # Base class for instance operations
├── JavaCommon.{h,cpp}              # Common Java utilities
├── Json.{h,cpp}                    # JSON helper utilities
├── KonamiCode.{h,cpp}              # Easter egg: Konami code detector
├── LaunchController.{h,cpp}        # Launch orchestration controller
├── LoggedProcess.{h,cpp}           # QProcess wrapper with logging
├── MMCStrings.{h,cpp}              # String utilities
├── MMCTime.{h,cpp}                 # Time formatting utilities
├── MMCZip.{h,cpp}                  # ZIP archive utilities
├── MessageLevel.{h,cpp}            # Log message severity levels
├── NullInstance.h                   # Null object pattern for instances
├── ProblemProvider.h                # Interface for reporting problems
├── QObjectPtr.h                     # shared_qobject_ptr smart pointer
├── RWStorage.h                      # Thread-safe key-value cache
├── RecursiveFileSystemWatcher.{h,cpp}  # Recursive directory watcher
├── SeparatorPrefixTree.h           # Prefix tree for path matching
├── SkinUtils.{h,cpp}               # Minecraft skin image utilities
├── UpdateController.{h,cpp}        # Update orchestration
├── Usable.h                        # Reference counting mixin
├── Version.{h,cpp}                 # Semantic version comparison
├── VersionProxyModel.{h,cpp}       # Version list filter/sort proxy
├── WatchLock.h                      # RAII file system watcher pause
│
├── icons/                          # Icon management
│   └── IconList.{h,cpp}            #   Icon collection model
│
├── java/                           # Java detection/management
│   ├── JavaChecker.{h,cpp}         #   JVM validation (spawn + parse)
│   ├── JavaCheckerJob.{h,cpp}      #   Parallel Java checking
│   ├── JavaInstall.{h,cpp}         #   Java installation descriptor
│   ├── JavaInstallList.{h,cpp}     #   Java installation list model
│   ├── JavaUtils.{h,cpp}           #   Platform-specific Java discovery
│   ├── JavaVersion.{h,cpp}         #   Java version parsing/comparison
│   └── download/                   #   Java download support
│
├── launch/                         # Launch infrastructure
│   ├── LaunchStep.{h,cpp}          #   Abstract launch step
│   ├── LaunchTask.{h,cpp}          #   Launch step orchestrator
│   ├── LogModel.{h,cpp}            #   Game log data model
│   └── steps/                      #   Generic launch steps
│
├── meta/                           # Metadata index system
│   ├── Index.{h,cpp}               #   Root metadata index
│   ├── Version.{h,cpp}             #   Metadata version entry
│   ├── VersionList.{h,cpp}         #   Metadata version list
│   ├── BaseEntity.{h,cpp}          #   Base metadata entity
│   └── JsonFormat.{h,cpp}          #   Metadata JSON serialization
│
├── minecraft/                      # Minecraft-specific subsystem
│   ├── MinecraftInstance.{h,cpp}    #   Concrete Minecraft instance
│   ├── MinecraftUpdate.{h,cpp}     #   Game update task
│   ├── MinecraftLoadAndCheck.{h,cpp}  #   Version validation
│   ├── Component.{h,cpp}           #   Version component
│   ├── ComponentUpdateTask.{h,cpp} #   Dependency resolution
│   ├── PackProfile.{h,cpp}         #   Component list model
│   ├── LaunchProfile.{h,cpp}       #   Resolved launch parameters
│   ├── Library.{h,cpp}             #   Java library descriptor
│   ├── VersionFile.{h,cpp}         #   Version JSON file model
│   ├── VersionFilterData.{h,cpp}   #   Version-specific quirks
│   ├── ProfileUtils.{h,cpp}        #   Profile file utilities
│   ├── AssetsUtils.{h,cpp}         #   Asset management
│   ├── GradleSpecifier.h           #   Maven coordinate parser
│   ├── MojangDownloadInfo.h        #   Mojang download descriptor
│   ├── MojangVersionFormat.{h,cpp}  #   Mojang version JSON parser
│   ├── OneSixVersionFormat.{h,cpp}  #   MeshMC version JSON format
│   ├── OpSys.{h,cpp}               #   Operating system identifier
│   ├── ParseUtils.{h,cpp}          #   Version string parsing
│   ├── Rule.{h,cpp}                #   Platform/feature rules
│   ├── World.{h,cpp}               #   Minecraft world descriptor
│   ├── WorldList.{h,cpp}           #   World collection model
│   │
│   ├── auth/                       #   Authentication subsystem
│   │   ├── MinecraftAccount.{h,cpp}  #     Account model
│   │   ├── AccountList.{h,cpp}     #     Account collection model
│   │   ├── AccountData.{h,cpp}     #     Token/profile storage
│   │   ├── AccountTask.{h,cpp}     #     Auth task base class
│   │   ├── AuthRequest.{h,cpp}     #     HTTP auth request helper
│   │   ├── AuthSession.{h,cpp}     #     Resolved auth session
│   │   ├── AuthStep.{h,cpp}        #     Abstract auth pipeline step
│   │   ├── Parsers.{h,cpp}         #     Response JSON parsers
│   │   ├── flows/                  #     Auth flow implementations
│   │   │   ├── AuthFlow.{h,cpp}    #       Base auth flow orchestrator
│   │   │   └── MSA.{h,cpp}         #       MSA login flows
│   │   └── steps/                  #     Individual auth steps
│   │       ├── MSAStep.{h,cpp}     #       Microsoft OAuth2
│   │       ├── XboxUserStep.{h,cpp}  #     Xbox User Token
│   │       ├── XboxAuthorizationStep.{h,cpp}  #  XSTS Token
│   │       ├── XboxProfileStep.{h,cpp}  #   Xbox Profile
│   │       ├── MinecraftProfileStep.{h,cpp}  #  MC Profile
│   │       ├── MeshMCLoginStep.{h,cpp}  #  MeshMC-specific login
│   │       ├── EntitlementsStep.{h,cpp}  #  Entitlement check
│   │       └── GetSkinStep.{h,cpp}  #     Skin retrieval
│   │
│   ├── launch/                     #   Minecraft-specific launch steps
│   │   ├── DirectJavaLaunch.{h,cpp}  #    Direct JVM invocation
│   │   ├── MeshMCPartLaunch.{h,cpp}  #   Java launcher component
│   │   ├── ClaimAccount.{h,cpp}    #     Account claim step
│   │   ├── CreateGameFolders.{h,cpp}  #   Directory setup
│   │   ├── ExtractNatives.{h,cpp}  #     Native library extraction
│   │   ├── ModMinecraftJar.{h,cpp}  #    Jar mod application
│   │   ├── PrintInstanceInfo.{h,cpp}  #   Debug info logging
│   │   ├── ReconstructAssets.{h,cpp}  #   Asset reconstruction
│   │   ├── ScanModFolders.{h,cpp}  #     Mod folder scanning
│   │   ├── VerifyJavaInstall.{h,cpp}  #   Java validation
│   │   └── MinecraftServerTarget.{h,cpp}  # Server join info
│   │
│   ├── mod/                        #   Mod management
│   │   ├── Mod.{h,cpp}            #     Single mod descriptor
│   │   ├── ModDetails.h           #     Mod metadata
│   │   ├── ModFolderModel.{h,cpp}  #    Mod list model
│   │   ├── ModFolderLoadTask.{h,cpp}  #  Async mod folder scan
│   │   ├── LocalModParseTask.{h,cpp}  #  Mod metadata extraction
│   │   ├── ResourcePackFolderModel.{h,cpp}  # Resource pack model
│   │   └── TexturePackFolderModel.{h,cpp}  # Texture pack model
│   │
│   ├── gameoptions/                #   Game options parsing
│   ├── services/                   #   Online service clients
│   ├── update/                     #   Game file update logic
│   ├── legacy/                     #   Legacy version support
│   └── testdata/                   #   Test fixtures
│
├── modplatform/                    # Mod platform integrations
│   ├── atlauncher/                 #   ATLauncher import
│   ├── flame/                      #   CurseForge API client
│   ├── modrinth/                   #   Modrinth API client
│   ├── modpacksch/                 #   FTB/modpacksch API
│   ├── technic/                    #   Technic API client
│   └── legacy_ftb/                 #   Legacy FTB support
│
├── net/                            # Network subsystem
│   ├── NetAction.h                 #   Abstract network action
│   ├── NetJob.{h,cpp}             #   Parallel download manager
│   ├── Download.{h,cpp}           #   Single file download
│   ├── HttpMetaCache.{h,cpp}      #   HTTP caching layer
│   ├── Sink.h                     #   Download output interface
│   ├── FileSink.{h,cpp}           #   File output sink
│   ├── ByteArraySink.h            #   Memory output sink
│   ├── MetaCacheSink.{h,cpp}      #   Cache-aware file sink
│   ├── Validator.h                 #   Download validation interface
│   ├── ChecksumValidator.h         #   Checksum validation
│   ├── Mode.h                     #   Network mode (online/offline)
│   └── PasteUpload.{h,cpp}        #   paste.ee upload
│
├── settings/                       # Settings framework
│   ├── Setting.{h,cpp}            #   Individual setting descriptor
│   ├── SettingsObject.{h,cpp}     #   Settings container
│   ├── INIFile.{h,cpp}            #   INI file reader/writer
│   ├── INISettingsObject.{h,cpp}  #   INI-backed settings
│   ├── OverrideSetting.{h,cpp}    #   Override/gate pattern
│   └── PassthroughSetting.{h,cpp}  #  Passthrough delegation
│
├── tasks/                          # Task infrastructure
│   └── Task.{h,cpp}               #   Abstract async task
│
├── tools/                          # External tool integration
│   ├── BaseProfiler.{h,cpp}       #   Profiler interface
│   ├── JProfiler.{h,cpp}          #   JProfiler integration
│   ├── JVisualVM.{h,cpp}          #   JVisualVM integration
│   └── MCEditTool.{h,cpp}         #   MCEdit integration
│
├── translations/                   # i18n
│   └── TranslationsModel.{h,cpp}  #   Translation management
│
├── updater/                        # Self-updater
│   └── UpdateChecker.{h,cpp}      #   Dual-source update checker
│
├── news/                           # News feed
├── notifications/                  # Notification system
├── screenshots/                    # Screenshot management
├── pathmatcher/                    # File path matching
│   └── IPathMatcher.h             #   Path matcher interface
│
└── ui/                             # User interface
    ├── MainWindow.{h,cpp}          #   Main application window
    ├── InstanceWindow.{h,cpp}      #   Instance console window
    ├── ColorCache.{h,cpp}          #   Color cache for log display
    ├── GuiUtil.{h,cpp}             #   GUI utility functions
    ├── themes/                     #   Theme system
    ├── pages/                      #   Page widgets
    │   ├── BasePage.h              #     Page interface
    │   ├── BasePageProvider.h      #     Page factory interface
    │   ├── global/                 #     Global settings pages
    │   ├── instance/               #     Instance settings pages
    │   └── modplatform/            #     Mod platform pages
    ├── dialogs/                    #   Dialog windows
    ├── widgets/                    #   Custom widgets
    ├── instanceview/               #   Instance grid/list view
    ├── pagedialog/                 #   Page container dialog
    └── setupwizard/                #   First-run wizard
```

### Libraries Directory (`libraries/`)

```
libraries/
├── LocalPeer/        # Single-instance enforcement via local socket
├── classparser/      # Java .class file parser for mod metadata
├── ganalytics/       # Google Analytics measurement protocol
├── hoedown/          # C Markdown parser (renders changelogs, notes)
├── iconfix/          # Fixes for Qt's icon theme loading
├── javacheck/        # Java process that reports JVM capabilities
├── katabasis/        # OAuth2 authentication framework
├── launcher/         # Java-side launcher (MeshMCPartLaunch)
├── optional-bare/    # nonstd::optional for pre-C++17 compatibility
├── rainbow/          # HSL color manipulation for Qt
├── systeminfo/       # CPU, OS, memory detection
├── tomlc99/          # C99 TOML parser
└── xz-embedded/      # Embedded XZ/LZMA decompressor
```

## Module Dependency Graph

The inter-module dependency relationships form a directed acyclic graph:

```
                    ┌──────────┐
                    │  main()  │
                    └────┬─────┘
                         │
                    ┌────▼─────┐
                    │Application│
                    └────┬─────┘
          ┌──────────────┼──────────────┐
          │              │              │
    ┌─────▼────┐  ┌──────▼─────┐  ┌────▼──────┐
    │ UI Layer │  │InstanceList│  │ AccountList│
    │MainWindow│  │            │  │            │
    └─────┬────┘  └──────┬─────┘  └────┬──────┘
          │              │              │
          │       ┌──────▼──────┐       │
          │       │BaseInstance │       │
          │       │MinecraftInst│       │
          │       └──────┬──────┘       │
          │              │              │
          │       ┌──────▼──────┐       │
          └──────►│LaunchControl│◄──────┘
                  └──────┬──────┘
                         │
                  ┌──────▼──────┐
                  │ LaunchTask  │
                  │ (steps)     │
                  └──────┬──────┘
                         │
              ┌──────────┼──────────┐
              │          │          │
        ┌─────▼──┐ ┌────▼────┐ ┌──▼───────┐
        │AuthFlow│ │PackProf.│ │DirectJava│
        │  MSA   │ │Component│ │  Launch  │
        └────────┘ └─────────┘ └──────────┘
```

## Key Design Patterns

### Singleton Application

The `Application` class extends `QApplication` and serves as the central hub. A macro provides convenient access:

```cpp
#define APPLICATION (static_cast<Application*>(QCoreApplication::instance()))
```

All subsystems are accessed through `Application`:
- `APPLICATION->settings()` — global settings
- `APPLICATION->instances()` — instance list
- `APPLICATION->accounts()` — account list
- `APPLICATION->network()` — shared `QNetworkAccessManager`
- `APPLICATION->metacache()` — HTTP metadata cache
- `APPLICATION->metadataIndex()` — version metadata index
- `APPLICATION->javalist()` — Java installation list
- `APPLICATION->icons()` — icon list
- `APPLICATION->translations()` — translation model
- `APPLICATION->themeManager()` — theme manager
- `APPLICATION->updateChecker()` — update checker

### Task System

All asynchronous operations derive from the `Task` base class:

```cpp
class Task : public QObject {
    Q_OBJECT
public:
    enum class State { Inactive, Running, Succeeded, Failed, AbortedByUser };
    virtual bool canAbort() const { return false; }
signals:
    void started();
    void progress(qint64 current, qint64 total);
    void finished();
    void failed(QString reason);
    void succeeded();
    void status(QString status);
    void stepStatus(QString status);
public slots:
    void start();
    virtual bool abort();
protected:
    virtual void executeTask() = 0;
    void emitSucceeded();
    void emitFailed(QString reason);
};
```

Subclasses include `NetJob`, `LaunchTask`, `LaunchController`, `AccountTask`, `InstanceTask`, `InstanceCopyTask`, `InstanceImportTask`, `ComponentUpdateTask`, `JavaCheckerJob`, and more. Tasks emit Qt signals for progress tracking and completion.

### Model-View Architecture

MeshMC extensively uses Qt's Model-View framework:

| Model Class | Base Class | Purpose |
|---|---|---|
| `InstanceList` | `QAbstractListModel` | Instance collection |
| `PackProfile` | `QAbstractListModel` | Component list per instance |
| `AccountList` | `QAbstractListModel` | Microsoft accounts |
| `ModFolderModel` | `QAbstractListModel` | Mods in an instance |
| `WorldList` | `QAbstractListModel` | Worlds in an instance |
| `JavaInstallList` | `BaseVersionList` → `QAbstractListModel` | Detected Java installations |
| `TranslationsModel` | `QAbstractListModel` | Available translations |
| `IconList` | `QAbstractListModel` | Available icons |
| `LogModel` | `QAbstractListModel` | Game log lines |
| `VersionProxyModel` | `QSortFilterProxyModel` | Filtered/sorted version list |

### Smart Pointer Usage

MeshMC uses a custom smart pointer for QObject-derived types:

```cpp
template <class T>
using shared_qobject_ptr = std::shared_ptr<T>;
```

This is defined in `QObjectPtr.h` and used throughout the codebase for shared ownership of QObject-derived instances (`AccountList`, `LaunchTask`, `QNetworkAccessManager`, etc.). Standard `std::shared_ptr` is used for non-QObject types (`SettingsObject`, `InstanceList`, `IconList`).

### Settings Override Pattern

The settings system supports hierarchical overrides:

```
Global Settings (meshmc.cfg)
    └── Instance Settings (instance.cfg)
         └── OverrideSetting (gate-controlled)
```

Each instance has its own `SettingsObject`. Instance settings can either use the global value (passthrough) or override it. The `OverrideSetting` class implements this gate pattern — a boolean gate setting controls whether the override value or the global value is used.

### Launch Step Pipeline

Game launching uses a sequential step pipeline:

```cpp
class LaunchTask : public Task {
    void appendStep(shared_qobject_ptr<LaunchStep> step);
    void prependStep(shared_qobject_ptr<LaunchStep> step);
    void proceed();  // advance to next step
};
```

Each `LaunchStep` can:
- Execute immediately and complete
- Emit `readyForLaunch()` to pause the pipeline (awaiting user interaction)
- Call `proceed()` on the parent to advance to the next step

The pipeline for a Minecraft launch typically includes:
1. `VerifyJavaInstall` — check Java exists and is compatible
2. `CreateGameFolders` — ensure directory structure exists
3. `ScanModFolders` — scan and index mod files
4. `ExtractNatives` — extract platform-native libraries
5. `ModMinecraftJar` — apply jar mods
6. `ReconstructAssets` — legacy asset management
7. `ClaimAccount` — lock the account for this session
8. `PrintInstanceInfo` — log debug information
9. `DirectJavaLaunch` or `MeshMCPartLaunch` — spawn the JVM

### Authentication Pipeline

Authentication uses a step-based pipeline similar to launch:

```cpp
class AuthFlow : public AccountTask {
    QList<AuthStep::Ptr> m_steps;
    AuthStep::Ptr m_currentStep;
    void nextStep();         // advance pipeline
    void stepFinished(...);  // handle step completion
};
```

MSA interactive login steps:
1. `MSAStep(Login)` — OAuth2 browser-based login
2. `XboxUserStep` — exchange MSA token for Xbox User Token
3. `XboxAuthorizationStep` — exchange for XSTS token
4. `MinecraftProfileStep` — get Minecraft profile
5. `EntitlementsStep` — verify game ownership
6. `GetSkinStep` — fetch player skin

### Network Download Architecture

Downloads use a Sink/Validator pattern:

```
NetJob (manages multiple concurrent downloads)
  └── Download (single network request)
       ├── Sink (output destination)
       │   ├── FileSink          — write to file
       │   ├── ByteArraySink     — write to memory
       │   └── MetaCacheSink     — write with cache metadata
       └── Validator (verify content)
           └── ChecksumValidator — MD5/SHA1/SHA256 check
```

`NetJob` extends `Task` and manages a collection of `NetAction` objects (typically `Download`). It handles concurrent execution, progress aggregation, retry logic, and failure reporting.

### Page System

Settings and instance configuration use a page-based UI:

```cpp
class BasePage {
public:
    virtual QString displayName() const = 0;
    virtual QIcon icon() const = 0;
    virtual QString id() const = 0;
    virtual QString helpPage() const { return QString(); }
    virtual bool apply() { return true; }
};
```

Pages are organized into providers:
- **Global pages** — `MeshMCPage`, `MinecraftPage`, `JavaPage`, `LanguagePage`, `ProxyPage`, `ExternalToolsPage`, `AccountListPage`, `PasteEEPage`, `CustomCommandsPage`, `AppearancePage`
- **Instance pages** — `VersionPage`, `ModFolderPage`, `LogPage`, `NotesPage`, `InstanceSettingsPage`, `ScreenshotsPage`, `WorldListPage`, `GameOptionsPage`, `ServersPage`, `OtherLogsPage`, `ResourcePackPage`, `ShaderPackPage`, `TexturePackPage`
- **Mod platform pages** — integration-specific browse/search pages

## Build System Architecture

### CMake Target Structure

```
MeshMC (root)
├── MeshMC_nbt++          # NBT library (from ../libnbtplusplus)
├── ganalytics             # Analytics library
├── systeminfo             # System information
├── hoedown                # Markdown parser
├── MeshMC_launcher        # Java launcher component
├── MeshMC_javacheck       # Java checker
├── xz-embedded            # XZ decompression
├── MeshMC_rainbow         # Color utilities
├── MeshMC_iconfix         # Icon loader fixes
├── MeshMC_LocalPeer       # Single-instance
├── MeshMC_classparser     # Class file parser
├── optional-bare          # optional polyfill
├── tomlc99                # TOML parser
├── MeshMC_katabasis       # OAuth2
├── BuildConfig            # Generated build constants
├── meshmc-updater         # Updater binary
└── meshmc                 # Main launcher executable
```

### Generated Files

The build system generates several files from templates:

| Template | Generated | Purpose |
|---|---|---|
| `branding/*.in` | `build/*.desktop`, `*.metainfo.xml`, `*.rc`, `*.qrc`, `*.manifest` | Platform packaging |
| `launcher/MeshMC.in` | `MeshMCScript` | Linux runner script |
| `buildconfig/BuildConfig.cpp.in` | `BuildConfig.cpp` | Compile-time constants |
| `branding/win_install.nsi.in` | `win_install.nsi` | NSIS installer script |

### Qt Meta-Object Compiler (MOC)

Qt's MOC is enabled globally via `CMAKE_AUTOMOC ON`. All classes using `Q_OBJECT` have their MOC files generated automatically. The `Q_OBJECT` macro is used extensively throughout the codebase for:
- Signal/slot connections
- Property system
- Dynamic type identification

## Data Flow Diagrams

### Instance Launch Sequence

```
User clicks "Launch"
    │
    ▼
MainWindow::on_actionLaunchInstance_triggered()
    │
    ▼
Application::launch(instance, online, profiler, server, account)
    │
    ▼
LaunchController::executeTask()
    │
    ├── decideAccount()         ← select/prompt for account
    ├── login()                 ← authenticate (MSAInteractive/MSASilent)
    │     │
    │     ▼
    │   AuthFlow::executeTask() → step pipeline
    │     │
    │     ▼
    │   AuthSession created with tokens
    │
    ▼
launchInstance()
    │
    ▼
MinecraftInstance::createLaunchTask(session, server)
    │
    ▼
LaunchTask (step pipeline)
    │
    ├── VerifyJavaInstall
    ├── CreateGameFolders
    ├── ScanModFolders
    ├── ExtractNatives
    ├── ModMinecraftJar
    ├── ReconstructAssets
    ├── ClaimAccount
    ├── PrintInstanceInfo
    └── DirectJavaLaunch/MeshMCPartLaunch
         │
         ▼
    LoggedProcess (QProcess wrapper)
         │
         ▼
    JVM subprocess running Minecraft
```

### Mod Installation Flow

```
User browses mod platform page
    │
    ▼
API query (CurseForge/Modrinth)
    │
    ▼
NetJob → Download mod file
    │
    ▼
File placed in instance mods/ directory
    │
    ▼
ModFolderModel::update()
    │
    ▼
LocalModParseTask extracts metadata
    │
    ▼
UI refreshed with new mod entry
```

### Settings Lookup Flow

```
Code requests setting value:
  instance->settings()->get("JavaPath")
    │
    ▼
OverrideSetting checks gate:
  instance->settings()->get("OverrideJava")
    │
    ├── gate ON → return instance-local value
    │                from instance.cfg
    │
    └── gate OFF → passthrough to global
                    from meshmc.cfg
```

## Threading Model

MeshMC is primarily single-threaded (Qt main/GUI thread), with specific operations dispatched to background threads:

- **File I/O** — `QFuture`/`QFutureWatcher` via `QtConcurrent` for file copy operations (`InstanceCopyTask`)
- **Mod parsing** — `LocalModParseTask` runs in background thread to parse mod metadata from JAR files
- **Mod folder scanning** — `ModFolderLoadTask` enumerates mod directories asynchronously
- **Java checking** — `JavaChecker` spawns external JVM processes (via `QProcess`) and parses output
- **Network** — `QNetworkAccessManager` handles HTTP requests asynchronously via Qt's event loop (not separate threads)

All UI updates happen on the main thread. Background tasks communicate results back via Qt's signal-slot mechanism with automatic cross-thread marshalling (`Qt::QueuedConnection`).

## Error Handling Strategy

MeshMC uses several error handling approaches:

1. **Task failure signals** — `Task::emitFailed(QString reason)` propagates errors through the task chain
2. **Problem providers** — `ProblemProvider` interface reports issues with severity levels (`ProblemSeverity`)
3. **Status enums** — `Application::Status`, `LoggedProcess::State`, `AccountState`, `JobStatus`
4. **Fatal errors** — `Application::showFatalErrorMessage()` displays critical errors and sets status to `Failed`
5. **Exception class** — `Exception` base class for recoverable errors (rarely used; signals preferred)

## Configuration Architecture

### Build-Time Configuration (`BuildConfig`)

The `buildconfig/` directory generates a `BuildConfig` struct containing all compile-time constants:

```cpp
struct Config {
    QString MESHMC_NAME;           // "MeshMC"
    QString MESHMC_BINARY;         // "meshmc"
    QString MESHMC_APP_ID;         // "org.projecttick.MeshMC"
    QString MESHMC_META_URL;       // "https://meta.projecttick.org/"
    QString MICROSOFT_CLIENT_ID;   // Azure AD app client ID
    QString CURSEFORGE_API_KEY;    // CurseForge API key
    QString PASTE_EE_API_KEY;      // paste.ee key
    QString IMGUR_CLIENT_ID;       // Imgur API client ID
    QString ANALYTICS_ID;          // Google Analytics measurement ID
    QString NEWS_RSS_URL;          // RSS feed URL
    QString BUG_TRACKER_URL;       // Issue tracker URL
    QString UPDATER_FEED_URL;      // Update RSS feed
    QString UPDATER_GITHUB_API_URL; // GitHub releases API
    QString BUILD_ARTIFACT;        // Artifact identifier
    QString BUILD_PLATFORM;        // Platform string
    // ... version numbers, git info, etc.
};
```

### Runtime Configuration (Settings)

Runtime settings are stored in INI files and managed through the `SettingsObject` → `INISettingsObject` hierarchy. Individual `Setting` objects are registered with the settings container and support:
- Default values
- Synonym keys (for migration)
- Change notification signals
- Override/gate patterns for instance-level settings

## Metadata System

The `meta/` subsystem provides access to version metadata from the MeshMC metadata server (default: `https://meta.projecttick.org/`):

```
Meta::Index (root)
  └── Meta::VersionList (per component UID)
       └── Meta::Version (individual version)
            └── VersionFile (parsed JSON with libraries, rules, etc.)
```

All metadata entities extend `Meta::BaseEntity`, which provides:
- Local cache loading
- Remote fetching via `NetJob`
- Staleness tracking
- JSON serialization/deserialization

The metadata index is the authoritative source for available Minecraft versions, mod loader versions, and their associated libraries and launch parameters.
