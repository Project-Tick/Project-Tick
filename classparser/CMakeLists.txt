cmake_minimum_required(VERSION 3.25)
project(classparser VERSION 1.0.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CMAKE_AUTOMOC ON)

######## Check endianness ########
include(TestBigEndian)
test_big_endian(BIGENDIAN)
if(${BIGENDIAN})
    add_definitions(-DMULTIMC_BIG_ENDIAN)
endif(${BIGENDIAN})

find_package(Qt6 COMPONENTS Core REQUIRED)
find_package(LibArchive REQUIRED)

set(CLASSPARSER_HEADERS
include/classparser_config.h
include/classparser.h
src/annotations.h
src/classfile.h
src/constants.h
src/errors.h
src/javaendian.h
src/membuffer.h
)

set(CLASSPARSER_SOURCES
src/classparser.cpp
src/annotations.cpp
)

add_definitions(-DCLASSPARSER_LIBRARY)

add_library(classparser ${CLASSPARSER_SOURCES} ${CLASSPARSER_HEADERS})
set_target_properties(classparser PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(classparser
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/classparser>)
target_link_libraries(classparser PRIVATE LibArchive::LibArchive Qt6::Core)

install(TARGETS classparser
    EXPORT classparser-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/classparser)
install(EXPORT classparser-targets
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/classparser)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/classparserConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/classparser-config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/classparser)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/classparser-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/classparser-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/classparser-config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/classparser)
