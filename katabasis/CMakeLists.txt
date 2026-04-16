cmake_minimum_required(VERSION 3.25)
project(Katabasis VERSION 1.0.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD_REQUIRED true)
set(CMAKE_C_STANDARD_REQUIRED true)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD 11)

find_package(Qt6 COMPONENTS Core Network REQUIRED)

set( katabasis_PRIVATE
    src/DeviceFlow.cpp
    src/JsonResponse.cpp
    src/JsonResponse.h
    src/PollServer.cpp
    src/Reply.cpp
)

set( katabasis_PUBLIC
    include/katabasis/DeviceFlow.h
    include/katabasis/Globals.h
    include/katabasis/PollServer.h
    include/katabasis/Reply.h
    include/katabasis/RequestParameter.h
)

add_library(Katabasis ${katabasis_PRIVATE} ${katabasis_PUBLIC})
add_library(Katabasis::Katabasis ALIAS Katabasis)
set_target_properties(Katabasis PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(Katabasis PUBLIC Qt6::Core Qt6::Network)
set_target_properties(Katabasis PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
target_include_directories(Katabasis
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE src include/katabasis)

install(TARGETS Katabasis
    EXPORT Katabasis-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/katabasis DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT Katabasis-targets
    NAMESPACE Katabasis::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Katabasis)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/KatabasisConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/KatabasisConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Katabasis)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/KatabasisConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/KatabasisConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/KatabasisConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Katabasis)
