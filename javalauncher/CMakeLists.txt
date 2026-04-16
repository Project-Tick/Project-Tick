cmake_minimum_required(VERSION 3.25)
project(NewLaunch VERSION 1.0.0)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

find_package(Java 1.7 REQUIRED COMPONENTS Development)

include(UseJava)
set(CMAKE_JAVA_JAR_ENTRY_POINT org.projecttick.EntryPoint)
set(CMAKE_JAVA_COMPILE_FLAGS --release 7 -Xlint:deprecation -Xlint:unchecked -Xlint:-options)

set(SRC
    org/projecttick/EntryPoint.java
    org/projecttick/MeshMC.java
    org/projecttick/LegacyFrame.java
    org/projecttick/NotFoundException.java
    org/projecttick/ParamBucket.java
    org/projecttick/ParseException.java
    org/projecttick/Utils.java
    org/projecttick/onesix/OneSixLauncher.java
    org/projecttick/modern/ModernLauncher.java
    net/minecraft/MeshMC.java
)
add_jar(NewLaunch ${SRC})

install_jar(NewLaunch "${CMAKE_INSTALL_DATADIR}/NewLaunch")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/NewLaunch.jar"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/NewLaunch")

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/NewLaunchConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/NewLaunchConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NewLaunch)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/NewLaunchConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/NewLaunchConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/NewLaunchConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NewLaunch)
