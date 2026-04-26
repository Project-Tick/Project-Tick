# LinuxPerf — MeshMC plugin (.mmco)
# MangoHud FPS overlay and GameMode performance integration for Minecraft.
#
# Linux-only (UNIX && NOT APPLE).
# Prepends mangohud and/or gamemoderun wrapper commands before launch.
#
# Vendor headers:
#   vendor/gamemode_client.h  — from FeralInteractive/gamemode (BSD-3-Clause)
#   vendor/mangohud_detect.h  — authored for this plugin (MIT)

set(LINUXPERF_SOURCES
    LinuxPerfPlugin.cpp
)

add_library(LinuxPerf MODULE ${LINUXPERF_SOURCES})

target_include_directories(LinuxPerf PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor"
)

target_link_libraries(LinuxPerf PRIVATE
    MeshMC_logic
    MeshMC::SDK
    ${CMAKE_DL_LIBS}   # -ldl for dlopen/dlsym used by gamemode_client.h
)

set_target_properties(LinuxPerf PROPERTIES
    PREFIX ""
    SUFFIX ".mmco"
    LIBRARY_OUTPUT_DIRECTORY "${MESHMC_PLUGIN_STAGING_DIR}"
)

foreach(CFG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${CFG}" CFG_UPPER)
    set_target_properties(LinuxPerf PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY_${CFG_UPPER} "${MESHMC_PLUGIN_STAGING_DIR}"
    )
endforeach()

install(TARGETS LinuxPerf
    LIBRARY DESTINATION "${BINARY_DEST_DIR}/mmcmodules"
)
