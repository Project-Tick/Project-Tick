# Platform Support

## Overview

MeshMC targets Linux, macOS, and Windows with platform-specific build configurations, packaging, and runtime behavior. The CMake build system uses presets and conditional compilation to handle platform differences.

## Build Presets

| Preset | Platform | Compiler | Generator |
|---|---|---|---|
| `linux` | Linux (x86_64, aarch64) | GCC / Clang | Ninja Multi-Config |
| `macos` | macOS (x86_64, arm64) | AppleClang | Ninja Multi-Config |
| `windows_msvc` | Windows | MSVC | Ninja Multi-Config |
| `windows_mingw` | Windows | MinGW-w64 | Ninja Multi-Config |

Each preset is defined in `CMakePresets.json` and configures:
- Compiler toolchain
- Platform-specific CMake variables
- Build/install directories

## Linux

### Build Requirements

- CMake 3.28+
- GCC 14+ or Clang 18+ (C++23 support)
- Qt6 (Core, Widgets, Concurrent, Network, NetworkAuth, Test, Xml)
- Extra CMake Modules (ECM) from KDE
- libarchive, zlib, cmark, tomlplusplus

### Nix Build

MeshMC provides a `flake.nix` for reproducible builds:

```bash
nix build .#meshmc        # Build release
nix develop .#meshmc      # Enter dev shell with all dependencies
```

### Desktop Integration

CMake installs standard freedesktop files:

```cmake
# Application desktop entry
install(FILES launcher/package/linux/org.projecttick.MeshMC.desktop
    DESTINATION ${KDE_INSTALL_APPDIR})

# AppStream metainfo
install(FILES launcher/package/linux/org.projecttick.MeshMC.metainfo.xml
    DESTINATION ${KDE_INSTALL_METAINFODIR})

# MIME type for .meshmc files
install(FILES launcher/package/linux/org.projecttick.MeshMC.mime.xml
    DESTINATION ${KDE_INSTALL_MIMEDIR})

# Application icons (various sizes)
ecm_install_icons(ICONS
    launcher/package/linux/16-apps-org.projecttick.MeshMC.png
    launcher/package/linux/24-apps-org.projecttick.MeshMC.png
    launcher/package/linux/32-apps-org.projecttick.MeshMC.png
    launcher/package/linux/48-apps-org.projecttick.MeshMC.png
    launcher/package/linux/64-apps-org.projecttick.MeshMC.png
    launcher/package/linux/128-apps-org.projecttick.MeshMC.png
    launcher/package/linux/256-apps-org.projecttick.MeshMC.png
    launcher/package/linux/scalable-apps-org.projecttick.MeshMC.svg
    DESTINATION ${KDE_INSTALL_ICONDIR}
)
```

### Runtime Paths

```cpp
// KDE install directories used via ECM
KDE_INSTALL_BINDIR      → /usr/bin
KDE_INSTALL_DATADIR     → /usr/share
KDE_INSTALL_APPDIR      → /usr/share/applications
KDE_INSTALL_ICONDIR     → /usr/share/icons
KDE_INSTALL_METAINFODIR → /usr/share/metainfo
KDE_INSTALL_MIMEDIR     → /usr/share/mime/packages
```

### RPATH

```cmake
# Set RPATH for installed binary
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
```

Ensures bundled libraries are found at runtime without LD_LIBRARY_PATH.

### Wayland/X11

Qt6 handles Wayland and X11 transparently. MeshMC does not have platform-specific display code.

## macOS

### Build Requirements

- CMake 3.28+
- Xcode / AppleClang (C++23 support)
- Qt6 via Homebrew
- Same library dependencies as Linux

### App Bundle

CMake creates a standard macOS `.app` bundle:

```cmake
set_target_properties(meshmc PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/launcher/package/macos/Info.plist.in"
    MACOSX_BUNDLE_BUNDLE_NAME "MeshMC"
    MACOSX_BUNDLE_BUNDLE_VERSION "${MeshMC_VERSION_NAME}"
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.projecttick.MeshMC"
    MACOSX_BUNDLE_ICON_FILE "meshmc.icns"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${MeshMC_VERSION_NAME}"
)
```

### Application Icon

```cmake
# Convert SVG to icns
set(MACOSX_ICON "${CMAKE_SOURCE_DIR}/launcher/package/macos/meshmc.icns")
set_source_files_properties(${MACOSX_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
```

### Sparkle Updates

macOS uses the Sparkle framework for auto-updates:

```cmake
if(APPLE)
    find_library(SPARKLE_FRAMEWORK Sparkle)
    if(SPARKLE_FRAMEWORK)
        target_link_libraries(meshmc PRIVATE ${SPARKLE_FRAMEWORK})
    endif()
endif()
```

Sparkle provides:
- Built-in update notification UI
- Differential (delta) updates
- Code signing verification
- Automatic background checks

### Universal Binary

The build supports creating Universal binaries (x86_64 + arm64):

```cmake
# Set via CMake variable
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")
```

### macOS-Specific Code

```cpp
#ifdef Q_OS_MACOS
    // Set application properties for macOS integration
    QApplication::setAttribute(Qt::AA_DontShowIconsInMenus);

    // Handle macOS dock icon click
    // Handle macOS file open events
#endif
```

## Windows

### Build Requirements

#### MSVC
- Visual Studio 2022 17.10+ (MSVC v143, C++23)
- CMake 3.28+
- Qt6 via installer or Scoop/Chocolatey
- System packages for other dependencies (Scoop, Chocolatey, or winget)

#### MinGW
- MinGW-w64 13+ (GCC 14+ for C++23)
- CMake 3.28+
- Qt6 built for MinGW

### Windows Resource File

```cmake
if(WIN32)
    # Application icon and version info
    configure_file(
        "${CMAKE_SOURCE_DIR}/launcher/package/windows/meshmc.rc.in"
        "${CMAKE_BINARY_DIR}/meshmc.rc"
    )
    target_sources(meshmc PRIVATE "${CMAKE_BINARY_DIR}/meshmc.rc")
endif()
```

The `.rc` file provides:
- Application icon (embedded in `.exe`)
- Version information (shown in file properties)
- Product name and company

### Application Manifest

```cmake
if(WIN32)
    target_sources(meshmc PRIVATE
        "${CMAKE_SOURCE_DIR}/launcher/package/windows/meshmc.manifest"
    )
endif()
```

The manifest declares:
- DPI awareness (per-monitor DPI aware)
- Requested execution level (asInvoker)
- Common controls v6 (modern UI)
- UTF-8 code page

### NSIS Installer

For creating Windows installers:

```cmake
if(WIN32)
    # CPack NSIS configuration
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_DISPLAY_NAME "MeshMC")
    set(CPACK_NSIS_PACKAGE_NAME "MeshMC")
    set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/launcher/package/windows/meshmc.ico")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "meshmc.exe")
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\MeshMC.lnk' '$INSTDIR\\\\meshmc.exe'"
    )
endif()
```

### Windows Registry

The installer registers:
- File associations (`.meshmc` import files)
- Start menu shortcuts
- Uninstall information in Add/Remove Programs

### WSL Rejection

MeshMC detects and rejects running under WSL:

```cpp
#ifdef Q_OS_LINUX
    // Check for WSL
    QFile wslInterop("/proc/sys/fs/binfmt_misc/WSLInterop");
    if (wslInterop.exists()) {
        QMessageBox::critical(nullptr, "Unsupported Platform",
            "MeshMC does not support running under WSL. "
            "Please use the native Windows version.");
        return 1;
    }
#endif
```

WSL cannot run Java GUI applications reliably, so MeshMC refuses to start.

## Portable Mode

All platforms support portable mode:

```cpp
// Check for portable marker file
QFileInfo portableMarker(
    QCoreApplication::applicationDirPath() + "/meshmc_portable.txt"
);

if (portableMarker.exists()) {
    // Use application directory for all data
    m_dataPath = QCoreApplication::applicationDirPath();
} else {
    // Use standard platform data directory
    m_dataPath = QStandardPaths::writableLocation(
        QStandardPaths::GenericDataLocation
    ) + "/MeshMC";
}
```

Create `meshmc_portable.txt` next to the binary to enable portable mode. All data (instances, settings, cache) will be stored alongside the executable.

## OpSys Class

Platform detection utility:

```cpp
class OpSys
{
public:
    enum OS {
        Os_Windows,
        Os_Linux,
        Os_OSX,
        Os_Other
    };

    static OS currentSystem();
    static QString currentSystemString();
    static bool isLinux();
    static bool isMacOS();
    static bool isWindows();
};
```

Used throughout the codebase for platform-conditional logic:
- Library path resolution
- Native path separators
- Platform-specific launch arguments
- Natives extraction (LWJGL)

## Platform-Specific Native Libraries

Minecraft requires platform-specific native libraries (LWJGL, OpenAL, etc.):

```cpp
// From Library class
bool Library::isApplicable() const
{
    // Check OS rules
    for (auto& rule : m_rules) {
        if (rule.os.name == "linux" && OpSys::isLinux()) return rule.action == "allow";
        if (rule.os.name == "osx" && OpSys::isMacOS()) return rule.action == "allow";
        if (rule.os.name == "windows" && OpSys::isWindows()) return rule.action == "allow";
    }
    return true;  // No rules = always applicable
}
```

### Natives Classifiers

```json
{
    "name": "org.lwjgl:lwjgl:3.3.3",
    "natives": {
        "linux": "natives-linux",
        "osx": "natives-macos",
        "windows": "natives-windows"
    }
}
```

## Data Directory Locations

| Platform | Default Data Directory |
|---|---|
| Linux | `~/.local/share/MeshMC` |
| macOS | `~/Library/Application Support/MeshMC` |
| Windows | `%APPDATA%/MeshMC` |
| Portable | `<binary_dir>/` |
