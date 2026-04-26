# SPDX-FileCopyrightText: 2026 Project Tick
# SPDX-FileContributor: Project Tick
# SPDX-License-Identifier: GPL-3.0-or-later WITH LicenseRef-MeshMC-MMCO-Module-Exception-1.0

cmake_minimum_required(VERSION 3.20)

project(meshmc-crashreporter)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)

# Optional: libqrencode for QR code generation
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(QRENCODE IMPORTED_TARGET QUIET libqrencode)
endif()

# On Windows: use Win32 subsystem so no console window appears.
if(WIN32)
    set(CRASHREPORTER_WIN32_FLAG WIN32)
else()
    set(CRASHREPORTER_WIN32_FLAG "")
endif()

add_executable(meshmc-crashreporter ${CRASHREPORTER_WIN32_FLAG}
    main.cpp
    CrashReportDialog.h
    CrashReportDialog.cpp
)

target_link_libraries(meshmc-crashreporter
    PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::Network
)

if(APPLE)
    set_target_properties(meshmc-crashreporter PROPERTIES
        INSTALL_RPATH "@executable_path/../Frameworks"
    )
endif()

if(QRENCODE_FOUND)
    target_compile_definitions(meshmc-crashreporter PRIVATE MESHMC_HAS_QRENCODE)
    target_link_libraries(meshmc-crashreporter PRIVATE PkgConfig::QRENCODE)
endif()

# Install alongside the main binary.
install(TARGETS meshmc-crashreporter
    RUNTIME DESTINATION "${BINARY_DEST_DIR}"
)
