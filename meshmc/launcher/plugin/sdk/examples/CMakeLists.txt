# Example CMakeLists.txt for an out-of-tree MeshMC plugin (.mmco)
#
# Prerequisites:
#   1. Install MeshMC with SDK headers:
#        cmake --install <build-dir> --prefix /usr/local
#   2. Install Qt6 development packages (Core, Widgets, Gui)
#
# Build:
#   cmake -B build -DCMAKE_PREFIX_PATH="/usr/local;/usr/lib/qt6"
#   cmake --build build
#
# Install:
#   cp build/MyPlugin.mmco ~/.local/lib/mmcmodules/

cmake_minimum_required(VERSION 3.21)
project(MyPlugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui)
find_package(MeshMC_SDK REQUIRED)

add_library(MyPlugin MODULE
    src/MyPlugin.cpp
)

target_link_libraries(MyPlugin PRIVATE
    MeshMC::SDK
)

set_target_properties(MyPlugin PROPERTIES
    PREFIX ""
    SUFFIX ".mmco"
)
