cmake_minimum_required(VERSION 3.25)
project(optional-bare VERSION 1.0.0)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

add_library(optional-bare INTERFACE)
add_library(optional-bare::optional-bare ALIAS optional-bare)
target_include_directories(optional-bare
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/optional-bare>)

install(TARGETS optional-bare
    EXPORT optional-bare-targets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/optional-bare)
install(EXPORT optional-bare-targets
    NAMESPACE optional-bare::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/optional-bare)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/optional-bareConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/optional-bare-config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/optional-bare)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/optional-bare-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/optional-bare-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/optional-bare-config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/optional-bare)
