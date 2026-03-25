cmake_minimum_required(VERSION 3.25)
project(launcher Java)
find_package(Java 1.7 REQUIRED COMPONENTS Development)

include(UseJava)
set(CMAKE_JAVA_JAR_ENTRY_POINT org.projecttick.EntryPoint)
set(CMAKE_JAVA_COMPILE_FLAGS -target 7 -source 7 -Xlint:deprecation -Xlint:unchecked)

set(SRC
    org/projecttick/EntryPoint.java
    org/projecttick/Launcher.java
    org/projecttick/LegacyFrame.java
    org/projecttick/NotFoundException.java
    org/projecttick/ParamBucket.java
    org/projecttick/ParseException.java
    org/projecttick/Utils.java
    org/projecttick/onesix/OneSixLauncher.java
    org/projecttick/modern/ModernLauncher.java
    net/minecraft/Launcher.java
)
add_jar(NewLaunch ${SRC})
install_jar(NewLaunch "${JARS_DEST_DIR}")
