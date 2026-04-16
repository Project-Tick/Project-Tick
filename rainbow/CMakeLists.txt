cmake_minimum_required(VERSION 3.25)
project(rainbow VERSION 1.0.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

find_package(Qt6 COMPONENTS Core Gui REQUIRED)

set(RAINBOW_SOURCES
src/rainbow.cpp
)

add_definitions(-DRAINBOW_LIBRARY)
add_library(rainbow SHARED ${RAINBOW_SOURCES})
target_include_directories(rainbow
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/rainbow>)
target_link_libraries(rainbow PUBLIC Qt6::Core Qt6::Gui)

install(TARGETS rainbow
    EXPORT rainbow-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/rainbow)
install(EXPORT rainbow-targets
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rainbow)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/rainbowConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/rainbow-config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rainbow)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/rainbow-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/rainbow-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/rainbow-config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rainbow)
