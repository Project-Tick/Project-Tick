# NVIDIAPrime — MeshMC plugin (.mmco)
# Discrete GPU offload via NVIDIA Prime Render Offload.
#
# Forces Minecraft to use the NVIDIA GPU on Optimus laptops.
# Flatpak: uses prime-run wrapper. Non-Flatpak: sets env vars.

set(NVIDIA_PRIME_SOURCES
    NVIDIAPrimePlugin.cpp
)

add_library(NVIDIAPrime MODULE ${NVIDIA_PRIME_SOURCES})

target_link_libraries(NVIDIAPrime PRIVATE
    MeshMC_logic
    MeshMC::SDK
)

set_target_properties(NVIDIAPrime PROPERTIES
    PREFIX ""
    SUFFIX ".mmco"
    LIBRARY_OUTPUT_DIRECTORY "${MESHMC_PLUGIN_STAGING_DIR}"
)

foreach(CFG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${CFG}" CFG_UPPER)
    set_target_properties(NVIDIAPrime PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY_${CFG_UPPER} "${MESHMC_PLUGIN_STAGING_DIR}"
    )
endforeach()

install(TARGETS NVIDIAPrime
    LIBRARY DESTINATION "${BINARY_DEST_DIR}/mmcmodules"
)
