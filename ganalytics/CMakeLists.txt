cmake_minimum_required(VERSION 3.25)
project(ganalytics VERSION 1.0.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CMAKE_AUTOMOC ON)

find_package(Qt6 COMPONENTS Core Gui Network REQUIRED)
find_package(systeminfo REQUIRED)

set(ganalytics_SOURCES
src/ganalytics.cpp
src/ganalytics_worker.cpp
src/ganalytics_worker.h
include/ganalytics.h
)

add_library(ganalytics ${ganalytics_SOURCES})
set_target_properties(ganalytics PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(ganalytics::ganalytics ALIAS ganalytics)
target_link_libraries(ganalytics PUBLIC Qt6::Core Qt6::Gui Qt6::Network systeminfo::systeminfo)
target_include_directories(ganalytics
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/ganalytics>)

install(TARGETS ganalytics
    EXPORT ganalytics-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES include/ganalytics.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ganalytics)
install(EXPORT ganalytics-targets
    NAMESPACE ganalytics::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ganalytics)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ganalyticsConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/ganalytics-config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ganalytics)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/ganalytics-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/ganalytics-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/ganalytics-config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ganalytics)
