function(projt_add_libpng)
    # Ensure libpng install rules are enabled even if other deps toggled them.
    set(SKIP_INSTALL_ALL OFF CACHE BOOL "" FORCE)
    set(SKIP_INSTALL_LIBRARIES OFF CACHE BOOL "" FORCE)
    set(PNG_SHARED ON CACHE BOOL "" FORCE)
    set(PNG_STATIC OFF CACHE BOOL "" FORCE)
    # Avoid loading both libpng16d and system libpng16 in Debug builds.
    set(PNG_DEBUG_POSTFIX "" CACHE STRING "" FORCE)
    set(PNG_TESTS OFF CACHE BOOL "" FORCE)
    set(PNG_TOOLS OFF CACHE BOOL "" FORCE)
    set(PNG_EXECUTABLES OFF CACHE BOOL "" FORCE)
    if(UNIX AND NOT APPLE)
        if(DEFINED Launcher_BUNDLED_LIBDIR AND NOT Launcher_BUNDLED_LIBDIR STREQUAL "")
            projt_push_install_libdir("${Launcher_BUNDLED_LIBDIR}")
        endif()
        if(DEFINED Launcher_BUNDLED_INCLUDEDIR AND NOT Launcher_BUNDLED_INCLUDEDIR STREQUAL "")
            projt_push_install_includedir("${Launcher_BUNDLED_INCLUDEDIR}")
        endif()
    endif()
    projt_push_output_dirs("libpng")
    projt_push_autogen_disabled()
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../libpng libpng)
    projt_pop_autogen_disabled()
    projt_pop_output_dirs()
    foreach(_projt_png_target png_shared png_static png_framework)
        if(TARGET ${_projt_png_target})
            target_compile_definitions(
                ${_projt_png_target}
                PRIVATE adler32=ptpng_adler32
                        crc32=ptpng_crc32
                        deflate=ptpng_deflate
                        deflateInit2_=ptpng_deflateInit2_
                        deflateReset=ptpng_deflateReset
                        inflate=ptpng_inflate
                        inflateInit2_=ptpng_inflateInit2_
                        inflateReset=ptpng_inflateReset
                        inflateReset2=ptpng_inflateReset2)
        endif()
    endforeach()
    if(UNIX AND NOT APPLE)
        if(DEFINED Launcher_BUNDLED_INCLUDEDIR AND NOT Launcher_BUNDLED_INCLUDEDIR STREQUAL "")
            projt_pop_install_includedir()
        endif()
        if(DEFINED Launcher_BUNDLED_LIBDIR AND NOT Launcher_BUNDLED_LIBDIR STREQUAL "")
            projt_pop_install_libdir()
        endif()
    endif()
endfunction()