cmake_minimum_required(VERSION 3.25)
project(xz-embedded VERSION 1.0.0 LANGUAGES C)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(XZ_BUILD_BCJ "Build xz-embedded with BCJ support (native binary optimization)" OFF)
option(XZ_BUILD_CRC64 "Build xz-embedded with CRC64 checksum support" ON)
option(XZ_BUILD_MINIDEC "Build a tiny utility that decompresses xz streams" OFF)

set(XZ_SOURCES
    src/xz_crc32.c
    src/xz_crc64.c
    src/xz_dec_lzma2.c
    src/xz_dec_stream.c
)
add_library(xz-embedded ${XZ_SOURCES})
add_library(xz-embedded::xz-embedded ALIAS xz-embedded)
target_include_directories(xz-embedded
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/xz-embedded>)
set_property(TARGET xz-embedded PROPERTY C_STANDARD 99)

install(TARGETS xz-embedded
    EXPORT xz-embedded-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/xz-embedded)
install(EXPORT xz-embedded-targets
    NAMESPACE xz-embedded::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/xz-embedded)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/xz-embeddedConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/xz-embedded-config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/xz-embedded)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/xz-embedded-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/xz-embedded-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/xz-embedded-config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/xz-embedded)

if(${XZ_BUILD_MINIDEC})
    add_executable(xzminidec xzminidec.c)
    target_link_libraries(xzminidec xz-embedded)
    set_property(TARGET xzminidec PROPERTY C_STANDARD 99)
endif()
