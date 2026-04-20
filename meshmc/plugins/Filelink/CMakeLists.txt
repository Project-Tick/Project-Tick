# Filelink — MeshMC plugin (.mmco)
# Create desktop shortcuts (.desktop / .lnk) for instances.
#
# Supports Linux and Windows.  macOS is not supported.

set(FILELINK_SOURCES
    FilelinkPlugin.cpp
)

add_library(Filelink MODULE ${FILELINK_SOURCES})

target_link_libraries(Filelink PRIVATE
    MeshMC_logic
    MeshMC::SDK
)

# Output as .mmco into the staging directory
set_target_properties(Filelink PROPERTIES
    PREFIX ""
    SUFFIX ".mmco"
    LIBRARY_OUTPUT_DIRECTORY "${MESHMC_PLUGIN_STAGING_DIR}"
)

# Also copy to build dir on multi-config generators
foreach(CFG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${CFG}" CFG_UPPER)
    set_target_properties(Filelink PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY_${CFG_UPPER} "${MESHMC_PLUGIN_STAGING_DIR}"
    )
endforeach()

# Install the .mmco alongside the binary (bin/mmcmodules/)
install(TARGETS Filelink
    LIBRARY DESTINATION "${BINARY_DEST_DIR}/mmcmodules"
)
