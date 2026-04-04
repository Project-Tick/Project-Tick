# libvterm - Terminal emulation library
# Built as a static library for MNV's :terminal feature

add_library(vterm STATIC
    src/encoding.c
    src/keyboard.c
    src/mouse.c
    src/parser.c
    src/pen.c
    src/screen.c
    src/state.c
    src/unicode.c
    src/vterm.c
)

target_include_directories(vterm
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(vterm PRIVATE
    INLINE=
)

target_compile_options(vterm PRIVATE
    -std=c99
    -Wall
    -Wpedantic
)

if(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
    target_compile_definitions(vterm PRIVATE
        __EXTENSIONS__
        _XPG6
        __XOPEN_OR_POSIX
    )
endif()

if(MNV_DEBUG)
    target_compile_definitions(vterm PRIVATE DEBUG)
    target_compile_options(vterm PRIVATE -ggdb)
endif()

# Test harness for libvterm (optional)
if(MNV_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/t/harness.c")
    add_executable(vterm_test_harness t/harness.c)
    target_link_libraries(vterm_test_harness PRIVATE vterm)

    add_test(
        NAME libvterm_tests
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run_libvterm_tests.sh
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()
