# xxd - Hex dump utility

add_executable(xxd xxd.c)

target_compile_definitions(xxd PRIVATE UNIX)

install(TARGETS xxd
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install man page
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/xxd.1")
    install(FILES xxd.1
        DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
    )
endif()
