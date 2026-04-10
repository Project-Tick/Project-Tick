cmake_minimum_required(VERSION 3.14)

project(DummyImport CXX)

include(FetchContent)
find_package(Git REQUIRED)

if(DEFINED nlohmann_json_source)
    get_filename_component(JSON_SOURCE_DIRECTORY "${nlohmann_json_source}" ABSOLUTE)
else()
    get_filename_component(JSON_SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
endif()

execute_process(
    COMMAND "${GIT_EXECUTABLE}" -C "${JSON_SOURCE_DIRECTORY}" rev-parse --show-toplevel
    OUTPUT_VARIABLE GIT_REPOSITORY_DIRECTORY
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE GIT_TOPLEVEL_RESULT
    ERROR_QUIET
)

if(NOT GIT_TOPLEVEL_RESULT EQUAL 0 OR NOT IS_DIRECTORY "${GIT_REPOSITORY_DIRECTORY}")
    set(GIT_REPOSITORY_DIRECTORY "${JSON_SOURCE_DIRECTORY}")
endif()

file(RELATIVE_PATH JSON_SOURCE_SUBDIR "${GIT_REPOSITORY_DIRECTORY}" "${JSON_SOURCE_DIRECTORY}")
set(FETCH_CONTENT_DECLARE_ARGS
    GIT_REPOSITORY "${GIT_REPOSITORY_DIRECTORY}"
    GIT_TAG HEAD
)

if(JSON_SOURCE_SUBDIR)
    list(APPEND FETCH_CONTENT_DECLARE_ARGS SOURCE_SUBDIR "${JSON_SOURCE_SUBDIR}")
endif()

FetchContent_Declare(json ${FETCH_CONTENT_DECLARE_ARGS})
FetchContent_MakeAvailable(json)

if(MSVC)
    add_compile_options(/EHsc)
endif()

add_executable(with_namespace_target main.cpp)
target_link_libraries(with_namespace_target nlohmann_json::nlohmann_json)

add_executable(without_namespace_target main.cpp)
target_link_libraries(without_namespace_target nlohmann_json)
