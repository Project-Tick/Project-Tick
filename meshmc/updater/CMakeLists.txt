# SPDX-FileCopyrightText: 2026 Project Tick
# SPDX-FileContributor: Project Tick
# SPDX-License-Identifier: GPL-3.0-or-later WITH LicenseRef-MeshMC-MMCO-Module-Exception-1.0

cmake_minimum_required(VERSION 3.20)

project(meshmc-updater)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt6 REQUIRED COMPONENTS Core Network)
find_package(LibArchive REQUIRED)

# On Windows: use Win32 subsystem so no console window appears.
if(WIN32)
    set(UPDATER_WIN32_FLAG WIN32)
else()
    set(UPDATER_WIN32_FLAG "")
endif()

add_executable(meshmc-updater ${UPDATER_WIN32_FLAG}
    main.cpp
    Installer.h
    Installer.cpp
)

target_link_libraries(meshmc-updater
    PRIVATE
        Qt6::Core
        Qt6::Network
        LibArchive::LibArchive
)

if(APPLE)
    set_target_properties(meshmc-updater PROPERTIES
        INSTALL_RPATH "@executable_path/../Frameworks"
    )
endif()

# Install alongside the main binary so UpdateController can find it.
install(TARGETS meshmc-updater
    RUNTIME DESTINATION "${BINARY_DEST_DIR}"
)
