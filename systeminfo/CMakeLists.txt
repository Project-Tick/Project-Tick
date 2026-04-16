cmake_minimum_required(VERSION 3.25)
project(systeminfo VERSION 1.0.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

find_package(Qt6 COMPONENTS Core Gui Network REQUIRED)

set(systeminfo_SOURCES
include/sys.h
include/distroutils.h
src/distroutils.cpp
)

if (WIN32)
    list(APPEND systeminfo_SOURCES src/sys_win32.cpp)
elseif (UNIX)
    if(APPLE)
        list(APPEND systeminfo_SOURCES src/sys_apple.cpp)
    else()
        list(APPEND systeminfo_SOURCES src/sys_unix.cpp)
    endif()
endif()

add_library(systeminfo ${systeminfo_SOURCES})
add_library(systeminfo::systeminfo ALIAS systeminfo)
set_target_properties(systeminfo PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(systeminfo PUBLIC Qt6::Core Qt6::Gui Qt6::Network)
target_include_directories(systeminfo
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/systeminfo>)

install(TARGETS systeminfo
    EXPORT systeminfo-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/systeminfo)
install(EXPORT systeminfo-targets
    NAMESPACE systeminfo::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/systeminfo)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/systeminfoConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/systeminfo-config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/systeminfo)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/systeminfo-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/systeminfo-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/systeminfo-config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/systeminfo)

if(PROJECT_IS_TOP_LEVEL AND BUILD_TESTING)
    include(CTest)
    include(ECMAddTests)
    enable_testing()
    add_unit_test(sys
        SOURCES src/sys_test.cpp
        LIBS systeminfo
    )
endif()
