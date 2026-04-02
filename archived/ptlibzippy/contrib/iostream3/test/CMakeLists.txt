# if we are built from with PTlibzippy, use this path's)
if(DEFINED PTLIBZIPPY_BUILD_IOSTREAM3)
    set(WORK_DIR ${PTlibzippy_BINARY_DIR})
    set(inst_setup ptlibzippy_install)
else(DEFINED PTLIBZIPPY_BUILD_IOSTREAM3)
    set(WORK_DIR ${iostreamV3_BINARY_DIR})
    set(inst_setup ptlibzippy_iostream3_install)

    add_test(
        NAME ptlibzippy_iostream3_install
        COMMAND ${CMAKE_COMMAND} --install ${iostreamV3_BINARY_DIR} --prefix
                ${CMAKE_CURRENT_BINARY_DIR}/test_install --config $<CONFIG>
        WORKING_DIRECTORY ${iostreamV3_BINARY_DIR})

    set_tests_properties(ptlibzippy_iostream3_install
        PROPERTIES
            FIXTURES_SETUP ptlibzippy_iostream3_install)
endif(DEFINED PTLIBZIPPY_BUILD_IOSTREAM3)

set(IOSTREAM3_TEST_INSTALL_DIR "${WORK_DIR}/test/test_install")
set(IOSTREAM3_TEST_PACKAGE_DIR
    "${IOSTREAM3_TEST_INSTALL_DIR}/${CMAKE_INSTALL_LIBDIR}/cmake/iostreamv3")
if(DEFINED PTLIBZIPPY_BUILD_IOSTREAM3)
    set(PTLIBZIPPY_TEST_PACKAGE_DIR
        "${IOSTREAM3_TEST_INSTALL_DIR}/${CMAKE_INSTALL_LIBDIR}/cmake/ptlibzippy")
else()
    set(PTLIBZIPPY_TEST_PACKAGE_DIR "${PTlibzippy_DIR}")
endif()
set(IOSTREAM3_FIND_PACKAGE_ARGS
    "-Diostreamv3_DIR=${IOSTREAM3_TEST_PACKAGE_DIR}"
    "-DPTlibzippy_DIR=${PTLIBZIPPY_TEST_PACKAGE_DIR}")

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/findpackage_test)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/add_subdirectory_test)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/add_subdirectory_exclude_test)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/find_package_test.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/findpackage_test/CMakeLists.txt @ONLY)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/add_subdirectory_test.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/add_subdirectory_test/CMakeLists.txt @ONLY)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/add_subdirectory_exclude_test.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/add_subdirectory_exclude_test/CMakeLists.txt
    @ONLY)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/find_package_no_components_test.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/findpackage_no_components_test/CMakeLists.txt
    @ONLY)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/find_package_wrong_components_test.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/findpackage_wrong_components_test/CMakeLists.txt
    @ONLY)

# CMAKE_GENERATOR_PLATFORM doesn't work in the if
set(GENERATOR ${CMAKE_GENERATOR_PLATFORM})

if(GENERATOR)
    set(PLATFORM "-A ${GENERATOR}")
endif(GENERATOR)

#
# findpackage_test
#
add_test(
    NAME ptlibzippy_iostream3_find_package_configure
    COMMAND
        ${CMAKE_COMMAND}
        ${PLATFORM}
        -B${CMAKE_CURRENT_BINARY_DIR}/findpackage_test_build
        -DCMAKE_BUILD_TYPE=$<CONFIG>
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
        -DCMAKE_INSTALL_PREFIX=${IOSTREAM3_TEST_INSTALL_DIR}
        ${IOSTREAM3_FIND_PACKAGE_ARGS}
        --fresh
        -G "${CMAKE_GENERATOR}"
        -S${CMAKE_CURRENT_BINARY_DIR}/findpackage_test)

add_test(
    NAME ptlibzippy_iostream3_find_package_build
    COMMAND ${CMAKE_COMMAND} --build . --config $<CONFIG>
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/findpackage_test_build)

set_tests_properties(ptlibzippy_iostream3_find_package_configure
    PROPERTIES
        FIXTURES_REQUIRED ${inst_setup}
        FIXTURES_SETUP iostream3_fp_config)

set_tests_properties(ptlibzippy_iostream3_find_package_build
    PROPERTIES
        FIXTURES_REQUIRED iostream3_fp_config)

#
# add_subdirectory_test
#
add_test(
    NAME ptlibzippy_iostream3_add_subdirectory_configure
    COMMAND
        ${CMAKE_COMMAND}
        ${PLATFORM}
        -B${CMAKE_CURRENT_BINARY_DIR}/add_subdirectory_test_build
        -DCMAKE_BUILD_TYPE=$<CONFIG>
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
        -DCMAKE_INSTALL_PREFIX=${IOSTREAM3_TEST_INSTALL_DIR}
        ${IOSTREAM3_FIND_PACKAGE_ARGS}
        --fresh
        -G "${CMAKE_GENERATOR}"
        -S${CMAKE_CURRENT_BINARY_DIR}/add_subdirectory_test)

add_test(
    NAME ptlibzippy_iostream3_add_subdirectory_build
    COMMAND ${CMAKE_COMMAND} --build . --config $<CONFIG>
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/add_subdirectory_test_build)

set_tests_properties(ptlibzippy_iostream3_add_subdirectory_configure
    PROPERTIES
        FIXTURES_REQUIRED ${inst_setup}
        FIXTURES_SETUP iostream3_as_config)

set_tests_properties(ptlibzippy_iostream3_add_subdirectory_build
    PROPERTIES
        FIXTURES_REQUIRED iostream3_as_config)

#
# add_subdirectory_exclude_test
#
add_test(
    NAME ptlibzippy_iostream3_add_subdirectory_exclude_configure
    COMMAND
        ${CMAKE_COMMAND} ${PLATFORM}
        -B${CMAKE_CURRENT_BINARY_DIR}/add_subdirectory_exclude_test_build
        -DCMAKE_BUILD_TYPE=$<CONFIG> -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
        -DCMAKE_INSTALL_PREFIX=${IOSTREAM3_TEST_INSTALL_DIR}
        ${IOSTREAM3_FIND_PACKAGE_ARGS}
        --fresh
        -G "${CMAKE_GENERATOR}"
        -S${CMAKE_CURRENT_BINARY_DIR}/add_subdirectory_exclude_test)

add_test(
    NAME ptlibzippy_iostream3_add_subdirectory_exclude_build
    COMMAND ${CMAKE_COMMAND} --build . --config $<CONFIG>
    WORKING_DIRECTORY
        ${CMAKE_CURRENT_BINARY_DIR}/add_subdirectory_exclude_test_build)

set_tests_properties(ptlibzippy_iostream3_add_subdirectory_exclude_configure
    PROPERTIES
        FIXTURES_REQUIRED ${inst_setup}
        FIXTURES_SETUP iostream3_asx_config)

set_tests_properties(ptlibzippy_iostream3_add_subdirectory_exclude_build
    PROPERTIES
        FIXTURES_REQUIRED iostream3_asx_config)

#
# findpackage_no_components_test
#
add_test(
    NAME ptlibzippy_iostream3_find_package_no_components_configure
    COMMAND
        ${CMAKE_COMMAND} ${PLATFORM}
        -B${CMAKE_CURRENT_BINARY_DIR}/findpackage_no_components_test_build
        -DCMAKE_BUILD_TYPE=$<CONFIG> -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
        -DCMAKE_INSTALL_PREFIX=${IOSTREAM3_TEST_INSTALL_DIR}
        ${IOSTREAM3_FIND_PACKAGE_ARGS}
        --fresh
        -G "${CMAKE_GENERATOR}"
        -S${CMAKE_CURRENT_BINARY_DIR}/findpackage_no_components_test)

set_tests_properties(ptlibzippy_iostream3_find_package_no_components_configure
    PROPERTIES
        FIXTURES_REQUIRED ${inst_setup})

if(NOT PTLIBZIPPY_IOSTREAM3_BUILD_SHARED OR NOT PTLIBZIPPY_IOSTREAM3_BUILD_STATIC)
    set_tests_properties(ptlibzippy_iostream3_find_package_no_components_configure
        PROPERTIES
            WILL_FAIL TRUE)
endif(NOT PTLIBZIPPY_IOSTREAM3_BUILD_SHARED OR NOT PTLIBZIPPY_IOSTREAM3_BUILD_STATIC)

#
# findpackage_no_components_test
#
add_test(
    NAME ptlibzippy_iostream3_find_package_wrong_components_configure
    COMMAND
        ${CMAKE_COMMAND} ${PLATFORM}
        -B${CMAKE_CURRENT_BINARY_DIR}/findpackage_wrong_components_test_build
        -DCMAKE_BUILD_TYPE=$<CONFIG> -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
        -DCMAKE_INSTALL_PREFIX=${IOSTREAM3_TEST_INSTALL_DIR}
        ${IOSTREAM3_FIND_PACKAGE_ARGS}
        --fresh
        -G "${CMAKE_GENERATOR}"
        -S${CMAKE_CURRENT_BINARY_DIR}/findpackage_wrong_components_test)

set_tests_properties(ptlibzippy_iostream3_find_package_wrong_components_configure
    PROPERTIES
        FIXTURES_REQUIRED ${inst_setup}
        WILL_FAIL TRUE)
