# MeshMC In-Tree Plugin Registry
#
# Each subdirectory here builds a .mmco shared module.
# Built modules are placed in ${MESHMC_PLUGIN_STAGING_DIR} so the
# launcher can discover them at runtime from the app-relative
# mmcmodules/ directory.

set(MESHMC_PLUGIN_STAGING_DIR "${CMAKE_BINARY_DIR}/mmcmodules" CACHE PATH
    "Directory where built .mmco plugins are placed for runtime discovery")

file(MAKE_DIRECTORY "${MESHMC_PLUGIN_STAGING_DIR}")

#################################### Register plugins here ####################################

add_subdirectory(BackupSystem)
