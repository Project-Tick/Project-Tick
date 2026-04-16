# SPDX-FileCopyrightText: 2013, 2015 ljfa-ag <ljfa-ag@web.de>
#
# SPDX-License-Identifier: LGPL-3.0-or-later

cmake_minimum_required(VERSION 3.15)
project(libnbt++
    VERSION 3.1
    LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(GenerateExportHeader)

# supported configure options
option(NBT_BUILD_SHARED "Build shared libraries" ON)
option(NBT_USE_ZLIB "Build additional zlib stream functionality" ON)
option(NBT_BUILD_TESTS "Build the unit tests. Requires CxxTest." OFF)

set(BUILD_SHARED_LIBS ${NBT_BUILD_SHARED})

set(NBT_SOURCES
    src/endian_str.cpp
    src/tag.cpp
    src/tag_compound.cpp
    src/tag_list.cpp
    src/tag_string.cpp
    src/value.cpp
    src/value_initializer.cpp

    src/io/stream_reader.cpp
    src/io/stream_writer.cpp

    src/text/json_formatter.cpp)

set(NBT_SOURCES_Z
    src/io/izlibstream.cpp
    src/io/ozlibstream.cpp)

if(NBT_USE_ZLIB)
    find_package(ZLIB REQUIRED)
    list(APPEND NBT_SOURCES ${NBT_SOURCES_Z})
    add_definitions("-DNBT_HAVE_ZLIB")
endif()

add_library(nbt++ ${NBT_SOURCES})
add_library(nbt++::nbt++ ALIAS nbt++)

target_include_directories(nbt++
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nbt++>)

if(NBT_USE_ZLIB)
    target_link_libraries(nbt++ PUBLIC ZLIB::ZLIB)
endif()
set_property(TARGET nbt++ PROPERTY CXX_STANDARD 11)
generate_export_header(nbt++ BASE_NAME nbt)

if(${BUILD_SHARED_LIBS})
    set_target_properties(nbt++ PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN 1
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR})
endif()

# Install
install(TARGETS nbt++
    EXPORT nbt++-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nbt++)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/nbt_export.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nbt++)

install(EXPORT nbt++-targets
    NAMESPACE nbt++::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/nbt++)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/nbt++Config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/nbt++-config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/nbt++)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/nbt++-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/nbt++-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/nbt++-config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/nbt++)

if(NBT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
