function(projt_add_ptlibzippy)
    set(_projt_prev_skip_install_all "${SKIP_INSTALL_ALL}")
    set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)

    # Fuzzing
    if(BUILD_FUZZERS)
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
        set(PTLIBZIPPY_BUILD_SHARED OFF CACHE BOOL "" FORCE)
        set(PTLIBZIPPY_BUILD_STATIC ON CACHE BOOL "" FORCE)
        message(STATUS "Fuzzing build: Building ptlibzippy as static library only")
    endif()

    # Save flags
    set(_SAVED_CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    set(_SAVED_CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    set(_SAVED_CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
    set(_SAVED_CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")

    # Strip coverage
    string(REGEX REPLACE "-fprofile-instr-generate|--coverage|-fprofile-generate|-fprofile-arcs|-ftest-coverage" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    string(REGEX REPLACE "-fprofile-instr-generate|--coverage|-fprofile-generate|-fprofile-arcs|-ftest-coverage" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

    # Shared/static logic
    if(UNIX AND NOT APPLE AND NOT BUILD_FUZZERS)
        set(PTLIBZIPPY_BUILD_SHARED ON CACHE BOOL "" FORCE)
        set(PTLIBZIPPY_BUILD_STATIC ON CACHE BOOL "" FORCE)
    endif()

    set(PTLIBZIPPY_PREFIX OFF CACHE BOOL "" FORCE)

    # OSS-Fuzz sanitizer strip
    if(DEFINED ENV{LIB_FUZZING_ENGINE})
        string(REGEX REPLACE "-fsanitize=[^ ]*" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
        string(REGEX REPLACE "-fsanitize-coverage=[^ ]*" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
        string(REGEX REPLACE "-fno-sanitize-recover=[^ ]*" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
        message(STATUS "OSS-Fuzz build: Stripping sanitizer flags from zlib")
    endif()

    # Install dirs
    if(UNIX AND NOT APPLE)
        if(DEFINED Launcher_BUNDLED_LIBDIR AND NOT Launcher_BUNDLED_LIBDIR STREQUAL "")
            projt_push_install_libdir("${Launcher_BUNDLED_LIBDIR}")
        endif()
        if(DEFINED Launcher_BUNDLED_INCLUDEDIR AND NOT Launcher_BUNDLED_INCLUDEDIR STREQUAL "")
            projt_push_install_includedir("${Launcher_BUNDLED_INCLUDEDIR}")
        endif()
    endif()

    projt_push_output_dirs("zlib")
    projt_push_autogen_disabled()

    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../ptlibzippy ptlibzippy)

    projt_pop_autogen_disabled()
    projt_pop_output_dirs()

    if(UNIX AND NOT APPLE)
        if(DEFINED Launcher_BUNDLED_INCLUDEDIR AND NOT Launcher_BUNDLED_INCLUDEDIR STREQUAL "")
            projt_pop_install_includedir()
        endif()
        if(DEFINED Launcher_BUNDLED_LIBDIR AND NOT Launcher_BUNDLED_LIBDIR STREQUAL "")
            projt_pop_install_libdir()
        endif()
    endif()

    # Restore flags
    set(CMAKE_C_FLAGS "${_SAVED_CMAKE_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${_SAVED_CMAKE_CXX_FLAGS}")
    set(CMAKE_C_FLAGS_DEBUG "${_SAVED_CMAKE_C_FLAGS_DEBUG}")
    set(CMAKE_C_FLAGS_RELEASE "${_SAVED_CMAKE_C_FLAGS_RELEASE}")

    # PIC fix
    if(TARGET ptlibzippystatic)
        set_target_properties(ptlibzippystatic PROPERTIES POSITION_INDEPENDENT_CODE ON)
    endif()
    if(TARGET ptlibzippy)
        set_target_properties(ptlibzippy PROPERTIES POSITION_INDEPENDENT_CODE ON)
    endif()

    # Export variables
    if(TARGET PTlibzippy::PTlibzippy)
        get_target_property(_projt_ptlibzippy_includes PTlibzippy::PTlibzippy INTERFACE_INCLUDE_DIRECTORIES)
    endif()

    if(_projt_ptlibzippy_includes)
        set(PTLIBZIPPY_INCLUDE_DIR "${_projt_ptlibzippy_includes}" PARENT_SCOPE)
    elseif(DEFINED PTlibzippy_SOURCE_DIR AND DEFINED PTlibzippy_BINARY_DIR)
        set(PTLIBZIPPY_INCLUDE_DIR "${PTlibzippy_SOURCE_DIR};${PTlibzippy_BINARY_DIR}" PARENT_SCOPE)
    endif()

    set(PTLIBZIPPY_LIBRARIES "PTlibzippy::PTlibzippy" PARENT_SCOPE)

    unset(_projt_ptlibzippy_includes)

    set(SKIP_INSTALL_ALL "${_projt_prev_skip_install_all}" CACHE BOOL "" FORCE)
endfunction()
