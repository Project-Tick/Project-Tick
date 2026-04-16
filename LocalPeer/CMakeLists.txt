cmake_minimum_required(VERSION 3.25)
project(LocalPeer VERSION 1.0.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CMAKE_AUTOMOC ON)

find_package(Qt6 COMPONENTS Core Network REQUIRED)

set(SINGLE_SOURCES
src/LocalPeer.cpp
src/LockedFile.cpp
src/LockedFile.h
include/LocalPeer.h
)

if(UNIX)
    list(APPEND SINGLE_SOURCES
        src/LockedFile_unix.cpp
    )
endif()

if(WIN32)
    list(APPEND SINGLE_SOURCES
        src/LockedFile_win.cpp
    )
endif()

add_library(LocalPeer ${SINGLE_SOURCES})
set_target_properties(LocalPeer PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(LocalPeer::LocalPeer ALIAS LocalPeer)
target_include_directories(LocalPeer
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/LocalPeer>)
target_link_libraries(LocalPeer PUBLIC Qt6::Core Qt6::Network)

install(TARGETS LocalPeer
    EXPORT LocalPeer-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES include/LocalPeer.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/LocalPeer)
install(EXPORT LocalPeer-targets
    NAMESPACE LocalPeer::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LocalPeer)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/LocalPeerConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/LocalPeerConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LocalPeer)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/LocalPeerConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/LocalPeerConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/LocalPeerConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LocalPeer)
