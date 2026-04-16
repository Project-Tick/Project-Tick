cmake_minimum_required(VERSION 3.25)
project(iconfix VERSION 1.0.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(GenerateExportHeader)

find_package(Qt6 COMPONENTS Core Widgets REQUIRED)

set(ICONFIX_SOURCES
xdgicon.h
xdgicon.cpp
internal/qhexstring_p.h
internal/qiconloader.cpp
internal/qiconloader_p.h
)

add_library(iconfix SHARED ${ICONFIX_SOURCES})
target_include_directories(iconfix
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/iconfix>)
target_link_libraries(iconfix PUBLIC Qt6::Core Qt6::Widgets)
set_target_properties(iconfix PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN 1)
generate_export_header(iconfix)

install(TARGETS iconfix
    EXPORT iconfix-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES xdgicon.h ${CMAKE_CURRENT_BINARY_DIR}/iconfix_export.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/iconfix)
install(EXPORT iconfix-targets
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/iconfix)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/iconfixConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/iconfix-config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/iconfix)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/iconfix-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/iconfix-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/iconfix-config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/iconfix)