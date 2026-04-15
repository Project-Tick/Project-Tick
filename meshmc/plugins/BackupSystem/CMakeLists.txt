# BackupSystem — MeshMC plugin (.mmco)
# Creates instance backups (snapshots), restores, exports, imports.
#
# All Qt/MeshMC types are obtained through the SDK header (mmco_sdk.h).
# Plugin source files do NOT directly #include Qt or MeshMC headers.

set(BACKUP_SOURCES
    BackupPlugin.cpp
    BackupManager.h
    BackupManager.cpp
    BackupPage.h
    BackupPage.cpp
)

set(BACKUP_UIS
    BackupPage.ui
)

qt_wrap_ui(BACKUP_UI_HEADERS ${BACKUP_UIS})

add_library(BackupSystem MODULE ${BACKUP_SOURCES} ${BACKUP_UI_HEADERS})

target_link_libraries(BackupSystem PRIVATE
    MeshMC_logic            # in-tree: link against launcher for symbols
    MeshMC::SDK             # SDK: provides all include paths + Qt deps
)

target_include_directories(BackupSystem PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}   # for generated ui headers
)

# Output as .mmco into the staging directory
set_target_properties(BackupSystem PROPERTIES
    PREFIX ""
    SUFFIX ".mmco"
    LIBRARY_OUTPUT_DIRECTORY "${MESHMC_PLUGIN_STAGING_DIR}"
)

# Also copy to build dir on multi-config generators
foreach(CFG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${CFG}" CFG_UPPER)
    set_target_properties(BackupSystem PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY_${CFG_UPPER} "${MESHMC_PLUGIN_STAGING_DIR}"
    )
endforeach()

# Install the .mmco alongside the binary (bin/mmcmodules/)
install(TARGETS BackupSystem
    LIBRARY DESTINATION "${BINARY_DEST_DIR}/mmcmodules"
)
