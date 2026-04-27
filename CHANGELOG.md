# Project Tick 202604272058 Snapshot

Project Tick is a modular ecosystem of tools including MeshMC (Minecraft launcher), MNV (editor), and supporting infrastructure.

## Product versions

- Actions           = N/A
- CGit              = 10.0.3
- CMark             = 0.31.2
- CoreBinUtils      = 10.0.3
- ForgeWrapper      = ProjT
- GenQRCode         = 4.1.1
- Hooks             = 10.0.3
- images4docker     = 10.0.3
- Json++ (json4cpp) = 10.0.3
- LibNBT++          = 3.1
- MeshMC            = 7.16.0
- Meta              = 10.0.3
- MNV               = 10.0.4
- NeoZIP            = 10.1.0
- Toml++            = 10.0.3
- Website           = 10.0.3
- Foreman           = 0.2.0

## MeshMC in-tree Plugin versions
- BackupSystem      = 1.1.0
- Filelink          = 2.1.0
- LinuxPerf         = 1.2.0
- NVIDIAPrime       = 1.0.0

## MeshMC

### Changelog

#### Fixed

Fixed Flatpak MangoHUD integration by detecting the mounted runtime extension directly and using absolute wrapper paths instead of PATH-dependent lookup.

# Project Tick 202604271705 Snapshot

Project Tick is a modular ecosystem of tools including MeshMC (Minecraft launcher), MNV (editor), and supporting infrastructure.

## Product versions

- Actions           = N/A
- CGit              = 10.0.3
- CMark             = 0.31.2
- CoreBinUtils      = 10.0.3
- ForgeWrapper      = ProjT
- GenQRCode         = 4.1.1
- Hooks             = 10.0.3
- images4docker     = 10.0.3
- Json++ (json4cpp) = 10.0.3
- LibNBT++          = 3.1
- MeshMC            = 7.15.0
- Meta              = 10.0.3
- MNV               = 10.0.4
- NeoZIP            = 10.1.0
- Toml++            = 10.0.3
- Website           = 10.0.3
- Foreman           = 0.2.0

## MeshMC

### Changelog

#### Fixed

* Fixed LinuxPerf plugin to improve Flatpak sandbox experience

# Project Tick v202604262010 Snapshot

Project Tick is a modular ecosystem of tools including MeshMC (Minecraft launcher), MNV (editor), and supporting infrastructure.

## Product versions

- Actions           = N/A
- CGit              = 10.0.3
- CMark             = 0.31.2
- CoreBinUtils      = 10.0.3
- ForgeWrapper      = ProjT
- GenQRCode         = 4.1.1
- Hooks             = 10.0.3
- images4docker     = 10.0.3
- Json++ (json4cpp) = 10.0.3
- LibNBT++          = 3.1
- MeshMC            = 7.14.0
- Meta              = 10.0.3
- MNV               = 10.0.4
- NeoZIP            = 10.1.0
- Toml++            = 10.0.3
- Website           = 10.0.3
- Foreman           = N/A

## MeshMC

### Highlights

MeshMC now includes a new Linux performance integration plugin, bringing launcher-level support for MangoHud and GameMode. This improves the Linux gameplay launch path by allowing performance tooling to be injected and managed more cleanly during Minecraft startup, instead of relying on users to wire everything manually like it is 1998 and desktop Linux is still a punishment ritual.

This release also improves launch wrapper state handling by moving transient launch state into `LaunchTask`, making the launch flow cleaner, less fragile, and easier to maintain. Module/version management has also been improved through new SDK version definitions in `mmco_sdk.h`.

And this is the final version of Project Tick before the Beta and LTS channels are released. Just so you know.

### Changelog

#### Added

- Added the `LinuxPerf` plugin for Linux performance tooling integration.
- Added MangoHud integration support through the new Linux performance plugin.
- Added GameMode integration support for Minecraft launches.
- Added versioning definitions to `mmco_sdk.h` to improve module management and compatibility tracking.
- Added a confirmation dialog before deleting skins in `AccountListPage`, reducing the chance of accidental skin removal.

#### Changed

- Refactored launch wrapper handling to use `LaunchTask` for transient state management.
- Improved launch-related documentation after the launch wrapper refactor.

#### Fixed

- Added Apple-specific install RPATH handling for `crashreporter` and `updater` targets.
- Improved platform-specific install behavior for macOS helper targets.

# Project Tick 202604242120 Snapshot

Project Tick is a modular ecosystem of tools including MeshMC (Minecraft launcher), MNV (editor), and supporting infrastructure.

## Product versions

- Actions           = N/A
- CGit              = 10.0.3
- CMark             = 0.31.2
- CoreBinUtils      = 10.0.3
- ForgeWrapper      = ProjT
- GenQRCode         = 4.1.1
- Hooks             = 10.0.3
- images4docker     = 10.0.3
- Json++ (json4cpp) = 10.0.3
- LibNBT++          = 3.1
- MeshMC            = 7.13.0
- Meta              = 10.0.3
- MNV               = 10.0.4
- NeoZIP            = 10.1.0
- Toml++            = 10.0.3
- Website           = 10.0.3

## MeshMC

### Changelog

#### Fixed

* Improved app location handling in FilelinkPlugin for Flatpak

# Project Tick 202604241545 Snapshot

Project Tick is a modular ecosystem of tools including MeshMC (Minecraft launcher), MNV (editor), and supporting infrastructure.

## Product versions

- Actions           = N/A
- CGit              = 10.0.3
- CMark             = 0.31.2
- CoreBinUtils      = 10.0.3
- ForgeWrapper      = ProjT
- GenQRCode         = 4.1.1
- Hooks             = 10.0.3
- images4docker     = 10.0.3
- Json++ (json4cpp) = 10.0.3
- LibNBT++          = 3.1
- MeshMC            = 7.12.0
- Meta              = 10.0.3
- MNV               = 10.0.4
- NeoZIP            = 10.1.0
- Toml++            = 10.0.3
- Website           = 10.0.3

## MeshMC

General fixes have been implemented. MeshMC 7.12.0 is here! The Filelink plugin (macOS, you have nothing to do with it) has been updated to version 2.0.0 and made more stable.

### Highlights

* Offline account support with demo mode
* Initial RPM packaging support
* FilelinkPlugin improvements with customizable shortcuts and better icon handling

### Changelog

#### Added

* Offline account support with demo mode functionality
* Initial RPM spec file for MeshMC packaging
* LICENSE file inclusion
* FilelinkPlugin support for customizable shortcut paths

#### Changed

* Git URLs updated across CMakeLists, metainfo, and Flatpak manifest
* README and source URLs aligned with GitLab hosting
* Build and workflow configurations cleaned and standardized

#### Fixed

* Improved icon handling in FilelinkPlugin
* Flatpak workflow path inconsistencies
* Repository path variable inconsistencies in build workflows

# Project Tick 202604232125 Snapshot

Project Tick is a modular ecosystem of tools including MeshMC (Minecraft launcher), MNV (editor), and supporting infrastructure.

## Product versions

- Actions           = N/A
- CGit              = 10.0.3
- CMark             = 0.31.2
- CoreBinUtils      = 10.0.3
- ForgeWrapper      = ProjT
- GenQRCode         = 4.1.1
- Hooks             = 10.0.3
- images4docker     = 10.0.3
- Json++ (json4cpp) = 10.0.3
- LibNBT++          = 3.1
- MeshMC            = 7.11.0
- Meta              = 10.0.3
- MNV               = 10.0.4
- NeoZIP            = 10.1.0
- Toml++            = 10.0.3
- Website           = 10.0.3

## MeshMC

This is a kind of return to the classic theme, but it still includes some solid fixes. Although it might not make much difference to humanity, it's the version that fooled me by making me refresh the screenshots.

### Highlights

* Improved UI responsiveness and stability
* New Instance Settings integration
* Version 7.11.0 with new themes

### Changelog

#### Added

* Instance Settings action in MainWindow toolbar
* GreenDark and GreenLight themes

#### Changed

* Modpack detection moved to background thread
* Replaced QDesktopServices with DesktopServices
* Updated minimum CMake version to 3.20
* Updated Screenshots for new themes

#### Fixed

* Crash caused by improper model reset handling in setSourceModel
* Error handling improvements in archive read/write
* Prevented unnecessary processing for empty logo URLs

# Project Tick 202604221645 Snapshot

Project Tick is a modular ecosystem of tools including MeshMC (Minecraft launcher), MNV (editor), and supporting infrastructure.

## Product versions

- Actions           = N/A
- CGit              = 10.0.3
- CMark             = 0.31.2
- CoreBinUtils      = 10.0.3
- ForgeWrapper      = ProjT
- GenQRCode         = 4.1.1
- Hooks             = 10.0.3
- images4docker     = 10.0.3
- Json++ (json4cpp) = 10.0.3
- LibNBT++          = 3.1
- MeshMC            = 7.10.0
- Meta              = 10.0.3
- MNV               = 10.0.4
- NeoZIP            = 10.1.0
- Toml++            = 10.0.3
- Website           = 10.0.3

## MeshMC

### Changelog

#### Fixed

Fixed regression for dont launching X11

# Project Tick 202604202324 Snapshot

Project Tick is a modular ecosystem of tools including MeshMC (Minecraft launcher), MNV (editor), and supporting infrastructure.

## Product versions

- Actions           = N/A
- CGit              = 10.0.3
- CMark             = 0.31.2
- CoreBinUtils      = 10.0.3
- ForgeWrapper      = ProjT
- GenQRCode         = 4.1.1
- Hooks             = 10.0.3
- images4docker     = 10.0.3
- Json++ (json4cpp) = 10.0.3
- LibNBT++          = 3.1
- MeshMC            = 7.9.0
- Meta              = 10.0.3
- MNV               = 10.0.4
- NeoZIP            = 10.1.0
- Toml++            = 10.0.3
- Website           = 10.0.3

## MeshMC

### Changelog

#### Fixed

Removed icon tag in MeshMC metainfo

# Project Tick 202604202152 Snapshot

Project Tick is a modular ecosystem of tools including MeshMC (Minecraft launcher), MNV (editor), and supporting infrastructure.

## Product versions

- Actions           = N/A
- CGit              = 10.0.3
- CMark             = 0.31.2
- CoreBinUtils      = 10.0.3
- ForgeWrapper      = ProjT
- GenQRCode         = 4.1.1
- Hooks             = 10.0.3
- images4docker     = 10.0.3
- Json++ (json4cpp) = 10.0.3
- LibNBT++          = 3.1
- MeshMC            = 7.8.0
- Meta              = 10.0.3
- MNV               = 10.0.4
- NeoZIP            = 10.1.0
- Toml++            = 10.0.3
- Website           = 10.0.3

## MeshMC

### Highlights

* Major feature expansion across CLI, plugins, and Linux integration
* First-class Wayland support for better modern Linux compatibility
* New Filelink plugin enabling cross-instance file linking and desktop shortcut creation
* Improved distribution readiness with enhanced metainfo, branding, and Flathub presence

### Changelog

#### Added

* CLI support for instance management and export functionality
* Filelink plugin for desktop shortcut creation and cross-instance file linking
* Wayland support for instance window management and improved Linux environment handling
* Callback-based action registration system for instance toolbar
* GA4 Measurement Protocol support with new API secret and measurement ID
* Branding colors for light and dark themes in metainfo.xml
* SVG asset for MeshMC graphical resources
* Documentation for Environment Variables and Application Settings APIs
* Snapshot management system via GenerateLatestJsonCommand and SnapshotService
* mmcmodules inclusion in Windows installer

#### Changed

* Updated metainfo with new features, screenshots, and licensing information
* Refactored dialog window modality and improved layout constraints
* Reformatted minecraft subdirectory structure
* Replaced zlib with zlib-ng in Arch installation script
* Improved Linux environment handling and integration details

#### Fixed

* Removed internal visibility check for zlib symbols to resolve LTO incompatibility issues

#### Removed

* Internal zlib symbol visibility enforcement (due to incompatibility with LTO)

# Project Tick 202604191455 Snapshot

Project Tick is a modular ecosystem of tools including MeshMC (Minecraft launcher), MNV (editor), and supporting infrastructure.

## Product versions

- Actions           = N/A
- CGit              = 10.0.3
- CMark             = 0.31.2
- CoreBinUtils      = 10.0.3
- ForgeWrapper      = ProjT
- GenQRCode         = 4.1.1
- Hooks             = 10.0.3
- images4docker     = 10.0.3
- Json++ (json4cpp) = 10.0.3
- LibNBT++          = 3.1
- MeshMC            = 7.7.0
- Meta              = 10.0.3
- MNV               = 10.0.4
- NeoZIP            = 10.1.0
- Toml++            = 10.0.3
- Website           = 10.0.3

## MeshMC

### Changelog

#### Fixed

* Updated _256 logo
* Fixed Java search dirs

# Project Tick 202604191350 Snapshot

Project Tick is a modular ecosystem of tools including MeshMC (Minecraft launcher), MNV (editor), and supporting infrastructure.

## Product versions

- Actions           = N/A
- CGit              = 10.0.3
- CMark             = 0.31.2
- CoreBinUtils      = 10.0.3
- ForgeWrapper      = ProjT
- GenQRCode         = 4.1.1
- Hooks             = 10.0.3
- images4docker     = 10.0.3
- Json++ (json4cpp) = 10.0.3
- LibNBT++          = 3.1
- MeshMC            = 7.6.0
- Meta              = 10.0.3
- MNV               = 10.0.4
- NeoZIP            = 10.1.0
- Toml++            = 10.0.3
- Website           = 10.0.3

## MeshMC

### Highlights

* Major cleanup of build system dependencies across macOS and cross-platform targets
* Reduced optional compression and crypto surface (LZ4, ZSTD, OpenSSL disabled)
* Improved licensing compliance coverage via REUSE updates
* Continued stabilization of cross-platform build pipeline (macOS, Windows, MinGW)

### Changelog

#### Added

* macOS support for building libarchive from source
* tomlplusplus integration into the build process

#### Changed

* Refactored macOS dependency setup and streamlined CMake flags
* Adjusted dependency management for macOS and Windows builds
* Updated REUSE configuration to expand license annotation coverage
* Simplified build configuration by disabling optional components (LZ4, ZSTD, OpenSSL)

#### Fixed

* Build issues in dependency handling across macOS and Windows environments
* Inconsistencies in CMake configuration related to optional libraries

#### Removed

* qrencode from macOS dependency installation
* vcpkg meson tool port and related patches

## NeoZIP

### Highlights

* Version bump to **10.1.0** with extensive low-level optimizations
* Significant improvements in architecture-specific performance (ARMv8, S390, LoongArch64)
* Enhanced sanitizer support and code quality

### Changelog

#### Added

* Slide hash optimization for S390 VX
* Compatibility headers and intrinsic improvements for multiple architectures
* New CRC32 optimizations and reusable implementations across platforms
* Additional benchmarks for inflate operations

#### Changed

* Refactored CRC32 implementations (chorba, SSE2, SSE4.1, C paths)
* Improved structure of architecture-specific code (shared headers, extracted functions)
* Updated sanitizer configuration (UBSAN improvements and new checks)

#### Fixed

* Multiple UBSAN warnings (implicit conversions, test suite issues)
* Build issues for partial AVX-512 environments
* Compatibility issues in LoongArch64 and big-endian systems
* Formatting inconsistencies across CRC32 implementations

#### Removed

* Redundant macros and legacy inline benchmark definitions

## Build System & Tooling

### Highlights

* Aggressive simplification of dependency stack
* Better maintainability and reduced external reliance

### Changelog

#### Added

* Improved internal handling for optional dependencies

#### Changed

* Disabled multiple optional libraries to reduce build complexity
* Streamlined cross-platform build scripts and configuration

#### Fixed

* General dependency resolution issues across platforms

#### Removed

* Unnecessary external tooling and redundant dependency paths

# Project Tick 202604181950 Snapshot

Project Tick is a modular ecosystem of tools including MeshMC (Minecraft launcher), MNV (editor), and supporting infrastructure.

## Product versions

- Actions           = N/A
- CGit              = 10.0.3
- CMark             = 0.31.2
- CoreBinUtils      = 10.0.3
- ForgeWrapper      = ProjT
- GenQRCode         = 4.1.1
- Hooks             = 10.0.3
- images4docker     = 10.0.3
- Json++ (json4cpp) = 10.0.3
- LibNBT++          = 3.1
- MeshMC            = 7.5.0
- Meta              = 10.0.3
- MNV               = 10.0.4
- NeoZIP            = 10.0.3
- Toml++            = 10.0.3
- Website           = 10.0.3

## MeshMC

### Highlights

* Improved Java management with **auto-download vendor selection** and refined settings handling
* Added **cross-platform dependency build scripts** (Linux & macOS & Windows)
* Internal improvements to **file watcher lifecycle and cleanup stability**

### Changelog

#### Added

* Java auto-download **vendor selection support**
* Java-related **settings management system**
* Build scripts for dependencies on:

  * Linux
  * macOS
  * Windows

#### Changed

* Refactored Java auto-download configuration logic
* Improved internal handling of Java settings
* Enhanced file watcher lifecycle management and cleanup behavior

#### Fixed

* Potential issues related to file watcher cleanup and resource handling

# Project Tick 202604172110 Snapshot

Project Tick is a modular ecosystem of tools including MeshMC (Minecraft launcher), MNV (editor), and supporting infrastructure.

## Product versions

- Actions           = N/A
- CGit              = 10.0.3
- CMark             = 0.31.2
- CoreBinUtils      = 10.0.3
- ForgeWrapper      = ProjT
- GenQRCode         = 4.1.1
- Hooks             = 10.0.3
- images4docker     = 10.0.3
- Json++ (json4cpp) = 10.0.3
- LibNBT++          = 3.1
- MeshMC            = 7.4.0
- Meta              = 10.0.3
- MNV               = 10.0.4
- NeoZIP            = 10.0.3
- Toml++            = 10.0.3
- Website           = 10.0.3

## MNV / CI

### Changelog

#### Fixed

* Fixed MNV uf_name size
* Fixed MeshMC Packaging instructions

# Project Tick 202604172100 Snapshot

Project Tick is a modular ecosystem of tools including MeshMC (Minecraft launcher), MNV (editor), and supporting infrastructure.

## Product versions

- Actions           = N/A
- CGit              = 10.0.3
- CMark             = 0.31.2
- CoreBinUtils      = 10.0.3
- ForgeWrapper      = ProjT
- GenQRCode         = 4.1.1
- Hooks             = 10.0.3
- images4docker     = 10.0.3
- Json++ (json4cpp) = 10.0.3
- LibNBT++          = 3.1
- MeshMC            = 7.4.0
- Meta              = 10.0.3
- MNV               = 10.0.3
- NeoZIP            = 10.0.3
- Toml++            = 10.0.3
- Website           = 10.0.3

## MeshMC / Project Tick Monorepo

### Highlights

* Major build system stabilization across CMake, CI, and monorepo library handling
* Full Nix integration with derivations for all Project Tick components
* Transition toward system-provided dependencies and cleaner packaging model
* Improved library bundling strategy and source archive consistency
* Compression backend migrated from zlib to neozip

### Changelog

#### Added

* Nix derivations for all Project Tick projects
* `default.nix` and `shell.nix` for flake compatibility
* CI steps for installing **cxxtest** on Linux and macOS
* Support for building and installing monorepo libraries (including neozip and vcpkg dependencies) in CI
* CMake support for collecting binary directories from `CMAKE_PREFIX_PATH`
* Additional CMake configuration for handling monorepo library bin paths
* `ldconfig` step to refresh dynamic linker cache for shared libraries
* README documentation for bundled libraries

#### Changed

* Reworked GitHub Actions to support flexible library bundling and cleanup
* Refactored artifact naming in MeshMC workflow (removed Qt6 suffix)
* Updated CMake configuration to:

  * Use dynamic build types in CI workflows
  * Improve Release build handling
  * Resolve headers from installed system packages
  * Support multi-config generator mappings
* Dependencies are now assumed to be system-provided instead of bundled by default
* Library integration model updated to allow independent compilation of subprojects
* Replaced zlib with neozip for compression
* MeshMC source tree reformatted for consistency

#### Fixed

* Issues with missing binary paths for monorepo libraries during build/link stages
* CI inconsistencies related to dependency installation and build configuration
* General CMake configuration inconsistencies affecting multi-platform builds

#### Removed

* Legacy assumptions around bundled dependencies in favor of system-based linkage
* Redundant or conflicting commits (cleanup via targeted revert)

# Project Tick 202604161945 Snapshot

Project Tick is a modular ecosystem of tools including MeshMC (Minecraft launcher), MNV (editor), and supporting infrastructure.

## Product versions

- Actions           = N/A
- CGit              = 10.0.3
- CMark             = 0.31.2
- CoreBinUtils      = 10.0.3
- ForgeWrapper      = ProjT
- GenQRCode         = 4.1.1
- Hooks             = 10.0.3
- images4docker     = 10.0.3
- Json++ (json4cpp) = 10.0.3
- LibNBT++          = 3.1
- MeshMC            = 7.3.0
- Meta              = 10.0.3
- MNV               = 10.0.3
- NeoZIP            = 10.0.3
- Toml++            = 10.0.3
- Website           = 10.0.3

## MeshMC

### Highlights

* Introduction of the **MMCO plugin system**, establishing a formal extension architecture with a defined SDK and lifecycle. 
* Addition of the **MMCO Module Exception 1.0**, enabling non-GPL compatible plugins under controlled conditions. 
* First-party plugins introduced, including **BackupSystem** and **NVIDIAPrime**, demonstrating real-world MMCO usage. 
* Significant improvements to **plugin management, UI integration, and runtime behavior**. 
* Initial **fuzzing infrastructure** added for multiple core libraries (cmark, json4cpp, neozip, tomlplusplus). 

### Changelog

#### Added

* MMCO plugin system (SDK, loader, lifecycle management, metadata, hooks) 
* MeshMC MMCO Module Exception 1.0 and updated licensing across the codebase 
* Plugin SDK CMake integration and example plugin 
* BackupSystem plugin for instance backup management (initial implementation) 
* Pre-launch backup support and settings integration for BackupSystem 
* NVIDIAPrime plugin for NVIDIA GPU offloading support 
* Plugin launch modifiers and improved plugin management system
* Plugin metadata enhancements (code links, About dialog integration) 
* Plugin build and staging configuration options
* Backup-related UI assets (icons across themes)

#### Changed

* Core architecture transitioned to a **plugin-first model with MMCO as a first-class subsystem** 
* Licensing model updated from plain GPL-3.0-or-later to **GPL-3.0-or-later WITH MMCO exception** 
* Plugin tab location updated in the UI 
* Backup plugin UI labels and messaging improved 
* Help menu actions refactored in MainWindow 
* LoggedProcess and ModFolderPage enhanced to support runtime resource/shader pack addition 
* CMake configuration updated for SDK and plugin staging 
* Platform compatibility improvements (conditional `unistd.h`) 

#### Fixed

* Fixed launcher icon issues 
* Improved macOS icon rendering (Tahoe compatibility) 
* REUSE compliance issues resolved 
* General formatting and minor internal fixes 

#### Removed

* Removed parallel execution in CI test commands for stability

# Project Tick 202604151700 Snapshot

Project Tick is a modular ecosystem of tools including MeshMC (Minecraft launcher), MNV (editor), and supporting infrastructure. This snapshot marks the first unified release of the platform.

## Product versions

- Actions           = N/A
- CGit              = 10.0.3
- CMark             = 0.31.2
- CoreBinUtils      = 10.0.3
- ForgeWrapper      = ProjT
- GenQRCode         = 4.1.1
- Hooks             = 10.0.3
- images4docker     = 10.0.3
- Json++ (json4cpp) = 10.0.3
- LibNBT++          = 3.1
- MeshMC            = 7.2.0
- Meta              = 10.0.3
- MNV               = 10.0.3
- NeoZIP            = 10.0.3
- Toml++            = 10.0.3
- Website           = 10.0.3

## MeshMC

### Highlights

* Added a full **crash reporting and log management system**
* Improved the **update pipeline**, including feed parsing, version comparison, and updater integration
* Strengthened **error handling** during update checks and archive extraction
* Cleaned up launcher internals, compiler flags, and several code paths for clarity and stability
* Added initial **Pacman packaging**
* Introduced **AUR automation** through GitHub Actions
* Improved packaging directory structure and release patching workflow

### Changelog

#### Added

* Added a new **crash reporter** application and dialog flow for handling launcher crashes
* Added **CrashReportDialog** with QR code support for full logs and paste.ee links
* Added **MeshMCLogsDialog** for viewing and managing MeshMC logs
* Added support for **log uploading to paste.ee**
* Added **UpdateProgressDialog** to show updater progress and logs
* Added `MESHMC_BINARY` to `BuildConfig` for application binary naming
* Added a shell wrapper for the `meshmc` binary in portable tarball installations
* Added unit tests for `UpdateChecker`
* Added **Pacman package** support
* Added packaging directory placeholders with `.gitkeep`
* Added `.gitignore` for Pacman packaging outputs
* Added a GitHub Actions workflow for **automatic AUR package updates** for MeshMC

#### Changed

* Refactored version comparison in `UpdateChecker` to use `qint64` for better accuracy
* Refactored installer behavior to store downloaded archives in the install root
* Refactored stable feed item parsing in the update system
* Refactored process management in `ModernLauncher`
* Improved applet instantiation in `OneSixLauncher`
* Refactored `getKernelInfo` to use `QOperatingSystemVersion`
* Updated Java-related compile flags to use `--release`
* Adjusted CMake CXX flags by removing redundant warning flags for MSVC and refining non-MSVC flags
* Enhanced compiler flags for both MSVC and Unix builds to improve warning coverage and stricter handling
* Refactored multiple methods, parameters, and internal declarations for improved clarity and maintainability
* Reformatted MeshMC source code
* Updated the release workflow patching process by removing an unnecessary copy step

#### Fixed

* Fixed updater binary name references in `UpdateController`
* Added error handling for update check failures in `MainWindow`
* Improved extraction error handling in the installer
* Fixed type casting issues in several areas, including:

  * `rowCount`
  * `JsonFormat.cpp`
  * `VersionList::count`
* Fixed warnings in integrated `libnbtplusplus` code
* Improved packaging-related workflow structure for release automation

#### Removed

* Removed unused MeshMC documentation
* Removed unused helper functions and variables in several source files
* Removed unused parameters across multiple methods to simplify interfaces
* Removed unnecessary packaging-related copy behavior from the release workflow

## libnbtplusplus

### Highlights

* Performed warning cleanup for integrated library code

### Changelog

#### Added

* No user-facing additions in this set

#### Changed

* Updated integrated code to reduce compiler warning noise

#### Fixed

* Fixed `libnbtplusplus` warnings

#### Removed

* No removals in this set

## Project Tick Wide Changes

_This **citizen** has changed enough to be able to participate here for the first time, but anyway..._

### Highlights

* Completely removed **all Nix infrastructure** from the repository
* Simplified the repository structure by removing unused docs, archived projects, and obsolete license material
* Updated bootstrap and tooling configuration to align with the new repository direction
* Improved workflow permission handling
* Expanded automation around packaging and release flows

### Changelog

#### Added

* Added `.mise.toml` to `REUSE.toml` for tool configuration tracking
* Added explicit permissions for actions in CI workflows
* Added new GitHub Actions automation for AUR package updates

#### Changed

* Updated bootstrap scripts to reflect **Project Tick branding**
* Improved LLVM installation checks in bootstrap tooling
* Shifted repository tooling away from Nix-based environment assumptions
* Updated release workflow patching logic for a cleaner packaging pipeline

#### Fixed

* Improved repository consistency after removal of old infrastructure and obsolete metadata
* Improved CI behavior by making permissions and workflow responsibilities more explicit

#### Removed

* Removed **all Nix infrastructure, tooling, CI integration, shell environments, flakes, and packaging metadata**
* Removed unused documentation
* Removed unused archived projects
* Removed an unused license file
* Removed obsolete Nix-related CI automation as part of the wider repository cleanup

# Project Tick 202604141638 Snapshot

Project Tick is a modular ecosystem of tools including MeshMC (Minecraft launcher), MNV (editor), and supporting infrastructure. This snapshot marks the first unified release of the platform.

## Product versions

- Actions           = N/A
- CGit              = 10.0.3
- CMark             = 0.31.2
- CoreBinUtils      = 10.0.3
- ForgeWrapper      = ProjT
- GenQRCode         = 4.1.1
- Hooks             = 10.0.3
- images4docker     = 10.0.3
- Json++ (json4cpp) = 10.0.3
- LibNBT++          = 3.0
- MeshMC            = 7.1.0
- Meta              = 10.0.3
- MNV               = 10.0.3
- NeoZIP            = 10.0.3
- TickBorg (ofborg) = 10.0.3
- Toml++            = 10.0.3
- Website           = 10.0.3

## MeshMC

### Highlights

* Added **in-launcher installation support** for mods, resource packs, and shader packs with a dependency solver
* Improved **update system and release pipeline**, including checksum generation and updater handling
* Completed large-scale **Qt6 signal-slot modernization refactor**
* General **build system, CI, and documentation cleanup**

### Changelog

#### Added

* In-launcher installation support for mods, resource packs, and shader packs
* Dependency solver for resolving content dependencies
* SHA-256 checksum generation in release workflow
* Updater binary discovery next to the running executable
* Improved portable mode detection logic

#### Changed

* Refactored update mechanism and updater execution flow
* Updated version handling to prioritize release tags
* Migrated signal-slot connections to modern Qt syntax across the codebase
* Updated CMake configuration, including Qt version constraints

#### Removed

* Removed obsolete MeshMC man page documentation
* Cleaned up unused files
* Dropped `qt5compat` dependency
* Removed unused `libnbtplusplus` inputs from flake configuration
* Removed CI tag trigger

## Website

### Changelog

#### Changed

* Improved LauncherFeedService and MainController behavior
* Enhanced appcast/feed handling, including MeshMC binary support

#### Fixed

* Corrected version resolution issues in product feed handling
* Fixed inconsistencies in update/feed processing

# Project Tick 202604131638 Snapshot 

Project Tick is a modular ecosystem of tools including MeshMC (Minecraft launcher), MNV (editor), and supporting infrastructure. This snapshot marks the first unified release of the platform.

## Product versions

- Actions           = N/A
- CGit              = 10.0.3
- CMark             = 0.31.2
- CoreBinUtils      = 10.0.3
- ForgeWrapper      = ProjT
- GenQRCode         = 4.1.1
- Hooks             = 10.0.3
- images4docker     = 10.0.3
- Json++ (json4cpp) = 10.0.3
- LibNBT++          = 3.0
- MeshMC            = 7.0.1
- Meta              = 10.0.3
- MNV               = 10.0.3
- NeoZIP            = 10.0.3
- TickBorg (ofborg) = 10.0.3
- Toml++            = 10.0.3
- Website           = 10.0.3

## MeshMC

### Changelog

#### Fixed

- Fixed build timestamp not being set in generated files due to CMake configure order

# Project Tick 202604121918 Snapshot 

Project Tick is a modular ecosystem of tools including MeshMC (Minecraft launcher), MNV (editor), and supporting infrastructure. This snapshot marks the first unified release of the platform.

## Product versions

- Actions           = N/A
- CGit              = 10.0.3
- CMark             = 0.31.2
- CoreBinUtils      = 10.0.3
- ForgeWrapper      = ProjT
- GenQRCode         = 4.1.1
- Hooks             = 10.0.3
- images4docker     = 10.0.3
- Json++ (json4cpp) = 10.0.3
- LibNBT++          = 3.0
- MeshMC            = 7.0.0
- Meta              = 10.0.3
- MNV               = 10.0.3
- NeoZIP            = 10.0.3
- TickBorg (ofborg) = 10.0.3
- Toml++            = 10.0.3
- Website           = 10.0.3

## CGit

### Highlights

This release introduces major CGit improvements, including CLA display, subtree browsing, and enhanced repository metadata detection.

### Changelog

#### Added

- Viewing subtrees is now much easier in CGit. With this feature, you can list and view your subtrees.
- A menu to display CLA has been added to the homepage.
- The ability to display multiple links in a single repository and to link to multiple Forge instances has been added.
- The ability to display Code of Conduct has been added to the homepage.
- Scanning files like CODEOWNERS in the repository allowed us to find the main branch and display it in the repository summary.

#### Changed

- CSS themes have been updated.
- Git version updated to 2.54.0.rc1.65.g8c9303b1ff
- URLs have been updated.

#### Fixed

- Internal APIs that changed in the new Git version have been updated.
- Minor issues have been resolved.

## CoreBinUtils

### Highlights

We are honored to announce CoreBinUtils. This toolset, an alternative to GNU Coreutils, aims to replicate and preserve FreeBSD-style behavior. It contains tons of commands and is very easy to install! Configure CoreBinUtils with `./configure`, but make sure you have musl installed on your system. Then compile with `make`. Finally, install it on your system with `sudo make install`. That's it!

## Hooks

### Highlights

Project Tick hooks enable instant mirroring in your self-hosted Git solutions. Currently experimental, but usable. Remember, there is no guarantee.

## Images4Docker

### Highlights

Images4Docker lets you easily build your MeshMC files using specially designed Docker containers. It provides approximately 40 containers, allowing you to use your preferred package manager.

## LibNBT++

### Highlights

### Changelog

LibNBT++ version 3 is now available. This version includes custom bug fixes developed by Project Tick and aims to positively impact behavior.

#### Fixed

- Behavioral corrections
- Test results were fixed.

## MeshMC

Today, we aren't just talking about ~~MultiMC~~. This is not true. ~~PrismLauncher~~? Again no. Okay, ~~ProjTLauncher~~? Again and again, no. This is **MeshMC** 7.0.0. This release ports the project from Qt5 to Qt6, migrates from the old CurseForge API to the new one, adds Modrinth support, and more!

### Welcome to customizable Catpacks

Catpacks are now customizable. To build your own cat empire, you can add your catpacks to the catpacks folder located in the binary folder within `%AppData%\MeshMC` or `.local/share/MeshMC`, or if you are using a portable binary.

### Modrinth support, now available

Now you'll be able to install Modrinth packages, and I hope this will make everyone happy.

### Say hello to a launcher that includes Qt6

MeshMC 7.0.0 has completed the Qt6 migration by default, which is especially important for our future-proofing and for you to use the launcher more comfortably. If you encounter a bug or problem, please start an issue using <https://github.com/Project-Tick/MeshMC/issues>. Because this migration can cause tons of problems.

### Other Changes

- Download your Java applications easily through your launcher with **JavaDownloader**.
- **Neoforge** and **Quilt** support has been added. You can quickly install **Neoforge** and **Quilt** from the instance edit menu.
- The **libnbt++** module now comes with meticulously crafted patches from **Project Tick**.
- We are now sharing our **Client IDs** so that developers and users can get custom builds.
- Added more themes and icons.
- Now you can log in to your Microsoft account more easily with endpoint.
- Refined AboutPage.
- Changed Quazip dependency to libarchive
- Fixed more bugs.

### Note

MeshMC is a continuation of MultiMC. We've implemented AI Usage Policies and GPL transitions to make MeshMC more free, and you can be sure our code is more open to use. Our difference is that we aim to create a Minecraft launcher that people can compile even 20 years from now, freely package wherever they want, maintain the simplicity and iconic appeal of MultiMC, while also adding new features and always keeping that line. We are very happy to see you with this release, but we also want to guarantee that the problems that befell ProjT Launcher will not occur here. The reasons we abandoned ProjT Launcher were primarily licensing issues and problems with the codebase due to it being a PrismLauncher fork, but we want to preserve the MultiMC foundation in MeshMC. Therefore, we shut down ProjT Launcher. Thank you for your support. **Stay with your cats.**

## MNV

### Highlights

MNV is not Vim. Welcome to MNV. As the name suggests, this is not Vim, and we bring you the upcoming version of Vim, 10.0. We don't have any major changes to offer, but don't forget to migrate your scripts from Vim because MNV is not Vim.

## NeoZIP

### Highlights

Welcome to NeoZip. It's a fork of Zlib-NG and its goal, along with Project Tick, is to make the Deflate and Inflate algorithms more flexible.
