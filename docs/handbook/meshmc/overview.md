# MeshMC Overview

## What is MeshMC?

MeshMC is a custom, open-source Minecraft launcher developed by Project Tick. It enables players to manage multiple, isolated Minecraft installations simultaneously — each with its own mods, resource packs, settings, and game version. MeshMC is the flagship launcher product of the Project Tick organization, licensed under the GNU General Public License v3.0 (GPL-3.0-or-later).

The launcher binary is named `meshmc` and uses the application ID `org.projecttick.MeshMC`. It is designed for power users who need fine-grained control over their Minecraft setup: modded playthroughs, modpack development, server testing, and version archaeology.

## Core Feature Set

### Multi-Instance Management

MeshMC's defining feature is instance-based Minecraft management. Each instance is a complete, self-contained Minecraft environment:

- **Independent game directories** — each instance has its own `.minecraft`-equivalent folder with saves, mods, configs, resource packs, shader packs, and texture packs
- **Version isolation** — instances can run different Minecraft versions simultaneously (1.7.10, 1.12.2, 1.20.4, etc.)
- **Instance groups** — organize instances into named groups for categorization
- **Instance copying** — clone any instance with optional save data and playtime preservation via `InstanceCopyTask`
- **Instance import/export** — import instances from ZIP archives or export them for sharing via `InstanceImportTask` and `ExportInstanceDialog`
- **Per-instance settings** — override global Java path, memory allocation, JVM arguments, window size, and custom commands on a per-instance basis through the `SettingsObject` override system

### Component-Based Version Management

Instead of monolithic version profiles, MeshMC uses a component system (`PackProfile` / `Component`) that decomposes a Minecraft installation into modular layers:

- **Minecraft base version** — the vanilla game jar
- **Mod loaders** — Forge, Fabric, Quilt, NeoForge, LiteLoader
- **Library overlays** — additional libraries injected into the classpath
- **Jar mods** — modifications applied directly to the game jar
- **Dependency resolution** — automatic resolution of inter-component dependencies via `ComponentUpdateTask`

### Mod Platform Integration

MeshMC integrates with major mod distribution platforms:

- **CurseForge** — browse, search, and install mods/modpacks from CurseForge (`modplatform/flame/`)
- **Modrinth** — browse, search, and install mods/modpacks from Modrinth (`modplatform/modrinth/`)
- **ATLauncher** — import ATLauncher modpacks (`modplatform/atlauncher/`)
- **FTB** — import Feed The Beast modpacks (`modplatform/modpacksch/`)
- **Technic** — import Technic modpacks (`modplatform/technic/`)
- **Legacy FTB** — support for old-format FTB modpacks (`modplatform/legacy_ftb/`)

### Microsoft Account Authentication

MeshMC supports Microsoft Account (MSA) login for Minecraft authentication:

- **OAuth2 Authorization Code Flow** via Qt6 NetworkAuth (`QOAuth2AuthorizationCodeFlow`)
- **Multi-account management** — store and switch between multiple Microsoft accounts
- **Token refresh** — automatic and manual token refresh via `MSASilent`
- **Xbox Live integration** — full authentication chain: MSA → Xbox User Token → XSTS Token → Minecraft Token
- **Profile management** — fetch and display Minecraft profile, skins, capes

### Java Management

- **Automatic Java detection** — `JavaUtils::FindJavaPaths()` scans platform-specific locations (registry on Windows, `/usr/lib/jvm` on Linux, `/Library/Java` on macOS)
- **Java version validation** — `JavaChecker` spawns a JVM process to verify version, architecture, and vendor
- **Per-instance Java configuration** — each instance can specify its own Java binary and JVM arguments
- **Built-in Java downloader** — optional feature to download and manage Java runtimes (can be disabled with `MeshMC_DISABLE_JAVA_DOWNLOADER`)

### Theming and UI Customization

- **Multiple built-in themes** — BrightTheme, DarkTheme, FusionTheme, SystemTheme
- **Custom themes** — user-defined themes via QSS stylesheets and palette definitions (`CustomTheme`)
- **Icon theme system** — multiple icon sets with automatic light/dark variant selection
- **CatPack system** — fun cat images displayed in the launcher background
- **Appearance settings page** — `AppearancePage` for theme, icon set, and cat pack selection

### Update System

MeshMC includes a dual-source update checker (`UpdateChecker`):

- **RSS feed** — checks `BuildConfig.UPDATER_FEED_URL` for version announcements with `projt:` namespace extensions
- **GitHub Releases** — cross-references against `BuildConfig.UPDATER_GITHUB_API_URL` for integrity
- **Platform-aware** — automatically disables for AppImage distributions and non-portable Linux installations
- **macOS Sparkle** — native update framework integration on macOS via Sparkle 2.x

### Additional Features

- **Log viewer** — real-time game log display with color-coded message levels (`LogModel`, `LogPage`)
- **Screenshot management** — browse, upload (Imgur), and manage game screenshots (`ScreenshotsPage`)
- **World management** — list, copy, and manage Minecraft worlds (`WorldList`, `WorldListPage`)
- **News feed** — RSS news reader showing MeshMC announcements
- **Notification system** — banner notifications for important updates
- **Proxy support** — HTTP/SOCKS5 proxy configuration for all network traffic
- **Paste service** — upload logs to paste.ee for troubleshooting (`PasteUpload`)
- **Analytics** — optional Google Analytics integration (`GAnalytics`)
- **Profiler integration** — JProfiler and JVisualVM launch hooks (`tools/JProfiler`, `tools/JVisualVM`)
- **MCEdit integration** — launch MCEdit for world editing (`tools/MCEditTool`)
- **Accessibility** — keyboard navigation, screen reader support (`AccessibleInstanceView`)

## Technology Stack

### Core Technologies

| Technology | Version | Purpose |
|---|---|---|
| C++ | C++23 | Primary language |
| Qt | 6.x | GUI framework, networking, data models |
| CMake | ≥ 3.20 | Build system |
| Ninja | Any | Build generator (recommended) |

### Qt6 Modules Used

| Module | Purpose |
|---|---|
| `Qt6::Core` | Core data types, I/O, event loop |
| `Qt6::Widgets` | GUI widgets (QMainWindow, QDialog, etc.) |
| `Qt6::Concurrent` | Asynchronous file operations |
| `Qt6::Network` | HTTP client, download management |
| `Qt6::NetworkAuth` | OAuth2 authentication (MSA login) |
| `Qt6::Test` | Unit testing framework |
| `Qt6::Xml` | XML parsing (RSS feeds, version manifests) |

### Bundled Libraries

| Library | Directory | Purpose |
|---|---|---|
| `ganalytics` | `libraries/ganalytics/` | Google Analytics client |
| `systeminfo` | `libraries/systeminfo/` | System information gathering |
| `hoedown` | `libraries/hoedown/` | Markdown-to-HTML renderer |
| `launcher` | `libraries/launcher/` | Java-based Minecraft launcher component |
| `javacheck` | `libraries/javacheck/` | Java installation validator |
| `xz-embedded` | `libraries/xz-embedded/` | XZ/LZMA decompression |
| `rainbow` | `libraries/rainbow/` | Qt color manipulation |
| `iconfix` | `libraries/iconfix/` | Qt QIcon loader fixes |
| `LocalPeer` | `libraries/LocalPeer/` | Single-instance application enforcer |
| `classparser` | `libraries/classparser/` | Java class file parser |
| `optional-bare` | `libraries/optional-bare/` | `nonstd::optional` polyfill |
| `tomlc99` | `libraries/tomlc99/` | TOML file parser |
| `katabasis` | `libraries/katabasis/` | OAuth2 framework |
| `libnbtplusplus` | `../libnbtplusplus/` | Minecraft NBT format parser |

### External Dependencies

| Dependency | Purpose |
|---|---|
| `libarchive` | Archive extraction (ZIP, tar, etc.) |
| `zlib` | Data compression |
| `Extra CMake Modules (ECM)` | KDE CMake utilities, install directories |
| `cmark` | Markdown rendering |
| `tomlplusplus` | TOML configuration parsing |
| `libqrencode` | QR code generation |
| `QuaZip` | ZIP archive handling |
| `scdoc` | Man page generation (optional) |

## Component Architecture Overview

MeshMC is organized into several major subsystems, each occupying a distinct directory within `launcher/`:

```
launcher/
├── main.cpp                  # Entry point
├── Application.{h,cpp}       # Application singleton, lifecycle management
├── BaseInstance.{h,cpp}       # Abstract instance interface
├── InstanceList.{h,cpp}       # Instance collection model
├── LaunchController.{h,cpp}   # Launch orchestration
├── launch/                    # Launch pipeline (LaunchTask, LaunchStep)
├── minecraft/                 # Minecraft-specific logic
│   ├── Component.{h,cpp}     #   Version component
│   ├── PackProfile.{h,cpp}   #   Component list model
│   ├── MinecraftInstance.{h,cpp}  #   Concrete instance type
│   ├── auth/                  #   Authentication subsystem
│   ├── launch/                #   Minecraft launch steps
│   ├── mod/                   #   Mod management models
│   ├── services/              #   Mojang/Microsoft services
│   └── update/                #   Game update logic
├── modplatform/               # CurseForge, Modrinth, ATL, FTB, Technic
├── net/                       # Network layer (Download, NetJob, cache)
├── settings/                  # Settings framework
├── java/                      # Java detection and validation
├── ui/                        # User interface
│   ├── MainWindow.{h,cpp}    #   Main application window
│   ├── InstanceWindow.{h,cpp} #   Per-instance console window
│   ├── themes/                #   Theme system
│   ├── pages/                 #   Settings/instance pages
│   ├── dialogs/               #   Modal dialogs
│   ├── widgets/               #   Custom widgets
│   └── setupwizard/           #   First-run wizard
├── icons/                     # Icon management
├── meta/                      # Metadata index (version lists)
├── tasks/                     # Task base class
├── tools/                     # External tool integration
├── translations/              # i18n/l10n
├── updater/                   # Self-update system
├── news/                      # News feed
├── notifications/             # Notification system
├── screenshots/               # Screenshot management
├── pathmatcher/               # File path matching utilities
└── resources/                 # Embedded resources (QRC)
```

### Subsystem Interactions

The major data flow through MeshMC:

1. **Startup** — `main()` creates `Application`, which initializes all subsystems: settings, network, accounts, instances, themes, translations, icons, metadata index, and analytics
2. **Instance Discovery** — `InstanceList` scans the instances directory, loading instance metadata from `instance.cfg` files via `INISettingsObject`
3. **User Interaction** — `MainWindow` presents the instance list via `InstanceView` (custom `QAbstractItemView`); user actions trigger instance operations
4. **Launch Flow** — User clicks Launch → `LaunchController::executeTask()` → account selection → `MinecraftInstance::createLaunchTask()` → `LaunchTask` with ordered `LaunchStep` chain → game process spawned via `DirectJavaLaunch` or `MeshMCPartLaunch`
5. **Mod Installation** — User browses mod platform → platform API query → `NetJob` download → mod file placed in instance mods directory → `ModFolderModel` updated

## Versioning

MeshMC follows semantic versioning. The current version is defined in the root `CMakeLists.txt`:

```cmake
set(MeshMC_VERSION_MAJOR    7)
set(MeshMC_VERSION_MINOR    0)
set(MeshMC_VERSION_HOTFIX   0)
```

The full version string is assembled as `7.0.0`. Git commit hash and tag information are captured at build time via `GetGitRevisionDescription.cmake` and embedded into the binary via `BuildConfig`.

## Configuration Files

MeshMC uses several configuration file formats:

| File | Format | Purpose |
|---|---|---|
| `meshmc.cfg` | INI | Global application settings |
| `instance.cfg` | INI | Per-instance settings |
| `accounts.json` | JSON | Account credentials and tokens |
| `instgroups.json` | JSON | Instance group assignments |
| `metacache` | JSON | HTTP cache metadata |
| `mmc-pack.json` | JSON | Component list (PackProfile) |
| `patches/*.json` | JSON | Custom component overrides |

## Licensing

MeshMC is licensed under the **GNU General Public License v3.0 or later** (GPL-3.0-or-later). The project uses REUSE-compliant licensing headers. Some files incorporate code from MultiMC Contributors under the Apache License 2.0, which is GPL-compatible for combined works.

The `REUSE.toml` file at the repository root and individual SPDX headers in each source file provide the authoritative licensing information.

## Project URLs

| Resource | URL |
|---|---|
| Source Code | `https://github.com/Project-Tick/Project-Tick` |
| Bug Tracker | `https://bugs.projecttick.org/enter_bug.cgi?product=MeshMC` |
| Metadata Server | `https://meta.projecttick.org/` |
| News Feed | `https://projecttick.org/product/meshmc/feed.xml` |
| Application ID | `org.projecttick.MeshMC` |

## Relationship to Other Launchers

MeshMC descends architecturally from the MultiMC launcher codebase (the Apache 2.0-licensed portions). It has been significantly extended and maintained as an independent project by Project Tick. Key differences from upstream include:

- Microsoft-only authentication (Mojang/legacy auth removed)
- Dual-source update system (RSS + GitHub Releases)
- CurseForge and Modrinth integration
- Qt6 migration (from Qt5)
- C++23 standard requirement
- Custom branding, theming, and icon sets
- Nix/flake-based build infrastructure
- REUSE-compliant licensing
- Built-in Java downloader

## Next Steps

For deeper understanding of MeshMC's internals, continue with:

- [Architecture](architecture.md) — detailed code architecture and module interactions
- [Building](building.md) — build instructions for all platforms
- [Application Lifecycle](application-lifecycle.md) — startup, shutdown, and event loop
- [Instance Management](instance-management.md) — instance storage, creation, and groups
- [Component System](component-system.md) — version resolution and dependency management
- [Launch System](launch-system.md) — process building and game execution
- [Account Management](account-management.md) — Microsoft OAuth2 authentication
- [UI System](ui-system.md) — Qt6 widget architecture
