cmake_minimum_required(VERSION 3.25) # Required for features like `CMAKE_MSVC_DEBUG_INFORMATION_FORMAT`

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
if(POLICY CMP0177)
    cmake_policy(SET CMP0177 NEW)
endif()

project(Launcher)

# Force bzip2 to build static library (required for quazip)
set(ENABLE_STATIC_LIB ON CACHE BOOL "Enable static lib for bzip2" FORCE)

# Set default build type for single-config generators only.
if(CMAKE_CONFIGURATION_TYPES)
    if(DEFINED CMAKE_BUILD_TYPE)
        unset(CMAKE_BUILD_TYPE CACHE)
    endif()
else()
    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    endif()
endif()

string(COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${CMAKE_BUILD_DIR}" IS_IN_SOURCE_BUILD)
if(IS_IN_SOURCE_BUILD)
    message(FATAL_ERROR "You are building the Launcher in-source. Please separate the build tree from the source tree.")
endif()



##################################### Set CMake options ####################################
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/")

# Output layout
set(Launcher_OUTPUT_ROOT "${PROJECT_BINARY_DIR}/out" CACHE PATH "Root output directory for build artifacts")
if(DEFINED ENV{GITHUB_ACTIONS})
    file(TO_CMAKE_PATH "${CMAKE_SOURCE_DIR}" _projt_src_dir)
    file(TO_CMAKE_PATH "${PROJECT_BINARY_DIR}" _projt_bin_dir)
    string(REGEX REPLACE "([][+.*^$()\\\\|?])" "\\\\\\1" _projt_src_dir_esc "${_projt_src_dir}")
    string(REGEX REPLACE "([][+.*^$()\\\\|?])" "\\\\\\1" _projt_bin_dir_esc "${_projt_bin_dir}")
    if(NOT Launcher_OUTPUT_ROOT MATCHES "^${_projt_src_dir_esc}(/|$)"
       AND NOT Launcher_OUTPUT_ROOT MATCHES "^${_projt_bin_dir_esc}(/|$)")
        set(Launcher_OUTPUT_ROOT "${PROJECT_BINARY_DIR}/out" CACHE PATH "Root output directory for build artifacts" FORCE)
    endif()
    unset(_projt_src_dir)
    unset(_projt_bin_dir)
    unset(_projt_src_dir_esc)
    unset(_projt_bin_dir_esc)
endif()
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${Launcher_OUTPUT_ROOT}/root/$<CONFIG>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${Launcher_OUTPUT_ROOT}/root/$<CONFIG>")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${Launcher_OUTPUT_ROOT}/root/$<CONFIG>")
set(CMAKE_JAVA_TARGET_OUTPUT_DIR "${Launcher_OUTPUT_ROOT}/root/jars")

macro(projt_push_output_dirs name)
    set(_PROJT_PREV_RUNTIME "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    set(_PROJT_PREV_LIBRARY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
    set(_PROJT_PREV_ARCHIVE "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")
    set(_PROJT_PREV_JAVA "${CMAKE_JAVA_TARGET_OUTPUT_DIR}")
    set(_projt_dir "${Launcher_OUTPUT_ROOT}/${name}")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${_projt_dir}/$<CONFIG>")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${_projt_dir}/$<CONFIG>")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${_projt_dir}/$<CONFIG>")
    set(CMAKE_JAVA_TARGET_OUTPUT_DIR "${_projt_dir}/jars")
endmacro()

macro(projt_pop_output_dirs)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${_PROJT_PREV_RUNTIME}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${_PROJT_PREV_LIBRARY}")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${_PROJT_PREV_ARCHIVE}")
    set(CMAKE_JAVA_TARGET_OUTPUT_DIR "${_PROJT_PREV_JAVA}")
    unset(_projt_dir)
endmacro()

macro(projt_push_install_libdir new_libdir)
    set(_PROJT_PREV_INSTALL_LIBDIR "${CMAKE_INSTALL_LIBDIR}")
    set(_PROJT_PREV_INSTALL_FULL_LIBDIR "${CMAKE_INSTALL_FULL_LIBDIR}")
    set(CMAKE_INSTALL_LIBDIR "${new_libdir}")
    if(IS_ABSOLUTE "${new_libdir}")
        set(CMAKE_INSTALL_FULL_LIBDIR "${new_libdir}")
    else()
        set(CMAKE_INSTALL_FULL_LIBDIR "${CMAKE_INSTALL_PREFIX}/${new_libdir}")
    endif()
endmacro()

macro(projt_pop_install_libdir)
    set(CMAKE_INSTALL_LIBDIR "${_PROJT_PREV_INSTALL_LIBDIR}")
    set(CMAKE_INSTALL_FULL_LIBDIR "${_PROJT_PREV_INSTALL_FULL_LIBDIR}")
    unset(_PROJT_PREV_INSTALL_LIBDIR)
    unset(_PROJT_PREV_INSTALL_FULL_LIBDIR)
endmacro()

macro(projt_push_install_includedir new_includedir)
    set(_PROJT_PREV_INSTALL_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}")
    set(_PROJT_PREV_INSTALL_FULL_INCLUDEDIR "${CMAKE_INSTALL_FULL_INCLUDEDIR}")
    set(CMAKE_INSTALL_INCLUDEDIR "${new_includedir}")
    if(IS_ABSOLUTE "${new_includedir}")
        set(CMAKE_INSTALL_FULL_INCLUDEDIR "${new_includedir}")
    else()
        set(CMAKE_INSTALL_FULL_INCLUDEDIR "${CMAKE_INSTALL_PREFIX}/${new_includedir}")
    endif()
endmacro()

macro(projt_pop_install_includedir)
    set(CMAKE_INSTALL_INCLUDEDIR "${_PROJT_PREV_INSTALL_INCLUDEDIR}")
    set(CMAKE_INSTALL_FULL_INCLUDEDIR "${_PROJT_PREV_INSTALL_FULL_INCLUDEDIR}")
    unset(_PROJT_PREV_INSTALL_INCLUDEDIR)
    unset(_PROJT_PREV_INSTALL_FULL_INCLUDEDIR)
endmacro()

macro(projt_push_install_libexecdir new_libexecdir)
    set(_PROJT_PREV_INSTALL_LIBEXECDIR "${CMAKE_INSTALL_LIBEXECDIR}")
    set(_PROJT_PREV_INSTALL_FULL_LIBEXECDIR "${CMAKE_INSTALL_FULL_LIBEXECDIR}")
    set(CMAKE_INSTALL_LIBEXECDIR "${new_libexecdir}")
    if(IS_ABSOLUTE "${new_libexecdir}")
        set(CMAKE_INSTALL_FULL_LIBEXECDIR "${new_libexecdir}")
    else()
        set(CMAKE_INSTALL_FULL_LIBEXECDIR "${CMAKE_INSTALL_PREFIX}/${new_libexecdir}")
    endif()
endmacro()

macro(projt_pop_install_libexecdir)
    set(CMAKE_INSTALL_LIBEXECDIR "${_PROJT_PREV_INSTALL_LIBEXECDIR}")
    set(CMAKE_INSTALL_FULL_LIBEXECDIR "${_PROJT_PREV_INSTALL_FULL_LIBEXECDIR}")
    unset(_PROJT_PREV_INSTALL_LIBEXECDIR)
    unset(_PROJT_PREV_INSTALL_FULL_LIBEXECDIR)
endmacro()

macro(projt_push_autogen_disabled)
    set(_PROJT_PREV_AUTOMOC "${CMAKE_AUTOMOC}")
    set(_PROJT_PREV_AUTORCC "${CMAKE_AUTORCC}")
    set(_PROJT_PREV_AUTOUIC "${CMAKE_AUTOUIC}")
    set(CMAKE_AUTOMOC OFF)
    set(CMAKE_AUTORCC OFF)
    set(CMAKE_AUTOUIC OFF)
endmacro()

macro(projt_pop_autogen_disabled)
    set(CMAKE_AUTOMOC "${_PROJT_PREV_AUTOMOC}")
    set(CMAKE_AUTORCC "${_PROJT_PREV_AUTORCC}")
    set(CMAKE_AUTOUIC "${_PROJT_PREV_AUTOUIC}")
    unset(_PROJT_PREV_AUTOMOC)
    unset(_PROJT_PREV_AUTORCC)
    unset(_PROJT_PREV_AUTOUIC)
endmacro()

if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)
    set(_launcher_default_bundled_libdir "${CMAKE_INSTALL_LIBDIR}/projtlauncher")
    set(Launcher_BUNDLED_LIBDIR "${_launcher_default_bundled_libdir}" CACHE STRING "Install directory for bundled libraries (relative to prefix). Set empty to disable.")
    set(_launcher_default_bundled_includedir "include/projtlauncher")
    set(Launcher_BUNDLED_INCLUDEDIR "${_launcher_default_bundled_includedir}" CACHE STRING "Install directory for bundled headers (relative to prefix). Set empty to disable.")
    set(_launcher_default_bundled_libexecdir "libexec/projtlauncher")
    set(Launcher_BUNDLED_LIBEXECDIR "${_launcher_default_bundled_libexecdir}" CACHE STRING "Install directory for bundled libexec tools (relative to prefix). Set empty to disable.")
    set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME Runtime)
    unset(_launcher_default_bundled_libdir)
    unset(_launcher_default_bundled_includedir)
    unset(_launcher_default_bundled_libexecdir)
endif()

######## Set compiler flags ########
set(CMAKE_CXX_STANDARD_REQUIRED true)
set(CMAKE_C_STANDARD_REQUIRED true)
set(CMAKE_CXX_STANDARD 23)

if(MSVC)
set(CMAKE_C_STANDARD 11)
else()
set(CMAKE_C_STANDARD 23)
endif()

include(GenerateExportHeader)
add_compile_definitions(QT_WARN_DEPRECATED_UP_TO=0x060400)
add_compile_definitions(QT_DISABLE_DEPRECATED_UP_TO=0x060400)

if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    add_compile_options(
        # /GS Adds buffer security checks, default on but included anyway to mirror gcc's fstack-protector flag
        "$<$<COMPILE_LANGUAGE:C,CXX>:/GS>"

        # /Gw helps reduce binary size
        # /Gy allows the compiler to package individual functions
        # /guard:cf enables control flow guard
        "$<$<AND:$<CONFIG:Release,RelWithDebInfo>,$<COMPILE_LANGUAGE:C,CXX>>:/Gw;/Gy;/guard:cf>"
    )

    add_link_options(
        # /LTCG allows for linking wholy optimizated programs
        # /MANIFEST:NO disables generating a manifest file, we instead provide our own
        # /STACK sets the stack reserve size, ATL's pack list needs 3-4 MiB as of November 2022, provide 8 MiB
        "$<$<COMPILE_LANGUAGE:C,CXX>:/LTCG;/MANIFEST:NO;/STACK:8388608>"
    )

    # /GL enables whole program optimizations
    # NOTE: With Clang, this is implemented as regular LTO and only used during linking
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        add_compile_options("$<$<AND:$<CONFIG:Release,RelWithDebInfo>,$<COMPILE_LANGUAGE:C,CXX>>:/GL>")
    endif()

    # TODO(@YongDo-Hyun): Is sccache affected by this? Would be nice to use `ProgramDatabase`....
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>")

    # Use static runtime for MSVC to make the app portable (avoids missing VCRUNTIME140.dll etc.)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    if(CMAKE_MSVC_RUNTIME_LIBRARY STREQUAL "MultiThreadedDLL")
        set(CMAKE_MAP_IMPORTED_CONFIG_DEBUG Release "")
        set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release "")
    endif()
else()
    add_compile_options("$<$<COMPILE_LANGUAGE:C,CXX>:-Wall;-pedantic;-fstack-protector-strong;--param=ssp-buffer-size=4>")

    # ATL's pack list needs more than the default 1 Mib stack on windows
    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        add_link_options("$<$<COMPILE_LANGUAGE:C,CXX>:-Wl,--stack,8388608>")

        # Emit PDBs for WinDbg, etc.
        if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
            set(CMAKE_EXE_LINKER_FLAGS "-Wl,--pdb= ${CMAKE_EXE_LINKER_FLAGS}")

            # TODO: Look into -gc-sections to further reduce binary size
            add_compile_options("$<$<AND:$<CONFIG:Release,RelWithDebInfo>,$<COMPILE_LANGUAGE:C,CXX>>:-ffunction-sections;-fdata-sections;-mguard=cf>")
        endif()

        # -ffunction-sections and -fdata-sections help reduce binary size
        # -mguard=cf enables Control Flow Guard when the MinGW frontend supports it
        # -Wl,--gc-sections removes unused sections during linking
        foreach(lang C CXX)
            set(_projt_release_flags "-ffunction-sections -fdata-sections")
            if(CMAKE_${lang}_COMPILER_ID STREQUAL "Clang")
                string(APPEND _projt_release_flags " -mguard=cf")
            endif()
            set("CMAKE_${lang}_FLAGS_RELEASE" "${_projt_release_flags}")
            unset(_projt_release_flags)
        endforeach()
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-Wl,--gc-sections ${CMAKE_EXE_LINKER_FLAGS_RELEASE}")
        set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "-Wl,--gc-sections ${CMAKE_SHARED_LINKER_FLAGS_RELEASE}")
    elseif(APPLE)
        # macOS specific dead code stripping
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-Wl,-dead_strip ${CMAKE_EXE_LINKER_FLAGS_RELEASE}")
        set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "-Wl,-dead_strip ${CMAKE_SHARED_LINKER_FLAGS_RELEASE}")
    elseif(UNIX)
        # Linux/BSD dead code stripping
        foreach(lang C CXX)
            set("CMAKE_${lang}_FLAGS_RELEASE" "-ffunction-sections -fdata-sections")
        endforeach()
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-Wl,--gc-sections ${CMAKE_EXE_LINKER_FLAGS_RELEASE}")
        set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "-Wl,--gc-sections ${CMAKE_SHARED_LINKER_FLAGS_RELEASE}")
    endif()
endif()

# Fix aarch64 build for toml++
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DTOML_ENABLE_FLOAT16=0")

# set release flags for build targets
set(CMAKE_C_FLAGS_RELEASE "-O2 -D_FORTIFY_SOURCE=2 ${CMAKE_C_FLAGS_RELEASE}")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -D_FORTIFY_SOURCE=2 ${CMAKE_CXX_FLAGS_RELEASE}")

# Export compile commands for debug builds if we can (useful in LSPs like clangd)
# https://cmake.org/cmake/help/v3.31/variable/CMAKE_EXPORT_COMPILE_COMMANDS.html
if(CMAKE_GENERATOR STREQUAL "Unix Makefiles" OR CMAKE_GENERATOR MATCHES "^Ninja")
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

if(UNIX AND NOT APPLE)
    # Keep install RPATHs relative so RPM check-rpaths doesn't reject build-root paths.
    set(_launcher_rpath_entries "")
    if(DEFINED Launcher_BUNDLED_LIBDIR AND NOT Launcher_BUNDLED_LIBDIR STREQUAL "")
        list(APPEND _launcher_rpath_entries "$ORIGIN/../${Launcher_BUNDLED_LIBDIR}")
    endif()
    list(APPEND _launcher_rpath_entries "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}" "$ORIGIN")
    string(JOIN ":" CMAKE_INSTALL_RPATH ${_launcher_rpath_entries})
    unset(_launcher_rpath_entries)
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
endif()

option(DEBUG_ADDRESS_SANITIZER "Enable Address Sanitizer in Debug builds" OFF)

# If this is a Debug build turn on address sanitiser
if ((CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo") AND DEBUG_ADDRESS_SANITIZER)
    message(STATUS "Address Sanitizer enabled for Debug builds, Turn it off with -DDEBUG_ADDRESS_SANITIZER=off")
    if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
        if (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
            # using clang with clang-cl front end
            message(STATUS "Address Sanitizer available on Clang MSVC frontend")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fsanitize=address /Oy-")
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /fsanitize=address /Oy-")
        else()
            # AppleClang and Clang
            message(STATUS "Address Sanitizer available on Clang")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer -fsanitize=undefined -fno-sanitize-recover=null")
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer -fsanitize=undefined -fno-sanitize-recover=null")
        endif()
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        # GCC
        message(STATUS "Address Sanitizer available on GCC")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer -fsanitize=undefined -fno-sanitize-recover")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer -fsanitize=undefined -fno-sanitize-recover")
        link_libraries("asan")
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
        message(STATUS "Address Sanitizer available on MSVC")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fsanitize=address /Oy-")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /fsanitize=address /Oy-")
    else()
        message(STATUS "Address Sanitizer not available on compiler ${CMAKE_CXX_COMPILER_ID}")
    endif()
endif()


option(ENABLE_LTO "Enable Link Time Optimization" off)

if(ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)

    if(ipo_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL TRUE)
        if(CMAKE_BUILD_TYPE)
            if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
                message(STATUS "IPO / LTO enabled")
            else()
                message(STATUS "Not enabling IPO / LTO on debug builds")
            endif()
        else()
            message(STATUS "IPO / LTO will only be enabled for release builds")
        endif()
    else()
        message(STATUS "IPO / LTO not supported: <${ipo_error}>")
    endif()
endif()

option(BUILD_TESTING "Build the testing tree." ON)
option(BUILD_FUZZERS "Build fuzzing targets." OFF)
option(Launcher_ALLOW_FETCHCONTENT "Allow FetchContent fallback for tomlplusplus" OFF)
option(Launcher_ALLOW_ECM_FETCHCONTENT "Allow FetchContent fallback for ECM" OFF)
option(Launcher_SHOW_CMAKE_WARNINGS "Show CMake developer/deprecated warnings from dependencies" ON)

if(Launcher_SHOW_CMAKE_WARNINGS)
    set(CMAKE_WARN_DEPRECATED ON)
    set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS OFF)
endif()

macro(launcher_fetchcontent_makeavailable content_name)
    if(Launcher_SHOW_CMAKE_WARNINGS)
        FetchContent_MakeAvailable(${content_name})
    else()
        set(launcher_warn_deprecated_set FALSE)
        if(DEFINED CMAKE_WARN_DEPRECATED)
            set(launcher_warn_deprecated_value "${CMAKE_WARN_DEPRECATED}")
            set(launcher_warn_deprecated_set TRUE)
        endif()
        set(launcher_suppress_dev_set FALSE)
        if(DEFINED CMAKE_SUPPRESS_DEVELOPER_WARNINGS)
            set(launcher_suppress_dev_value "${CMAKE_SUPPRESS_DEVELOPER_WARNINGS}")
            set(launcher_suppress_dev_set TRUE)
        endif()

        set(CMAKE_WARN_DEPRECATED OFF)
        set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS ON)
        FetchContent_MakeAvailable(${content_name})

        if(launcher_warn_deprecated_set)
            set(CMAKE_WARN_DEPRECATED "${launcher_warn_deprecated_value}")
        else()
            unset(CMAKE_WARN_DEPRECATED)
        endif()
        if(launcher_suppress_dev_set)
            set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS "${launcher_suppress_dev_value}")
        else()
            unset(CMAKE_SUPPRESS_DEVELOPER_WARNINGS)
        endif()
        unset(launcher_warn_deprecated_set)
        unset(launcher_warn_deprecated_value)
        unset(launcher_suppress_dev_set)
        unset(launcher_suppress_dev_value)
    endif()
endmacro()

message(STATUS "Using bundled extra-cmake-modules")
set(ECM_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../extra-cmake-modules/modules")
set(ECM_MODULE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../extra-cmake-modules/modules")
# Add kde-modules for KDEInstallDirs and other KDE modules
list(APPEND ECM_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../extra-cmake-modules/kde-modules")
set(CMAKE_MODULE_PATH "${ECM_MODULE_PATH};${CMAKE_MODULE_PATH}")

include(CTest)
include(ECMAddTests)

if(BUILD_TESTING)
    enable_testing()
endif()

##################################### Set Application options #####################################

######## Set URLs ########
set(Launcher_NEWS_RSS_URL "https://projecttick.org/product/projt-launcher/feed.xml" CACHE STRING "URL to fetch ProjT Launcher's news RSS feed from.")
set(Launcher_NEWS_OPEN_URL "https://projecttick.org/product/projt-launcher/news" CACHE STRING "URL that gets opened when the user clicks 'More News'")
set(Launcher_WIKI_URL "https://projecttick.org/handbook/" CACHE STRING "URL that gets opened when the user clicks 'Launcher Help'")
set(Launcher_HELP_URL "https://projecttick.org/handbook/help-pages/%1" CACHE STRING "URL (with arg %1 to be substituted with page-id) that gets opened when the user requests help in a dialog window")
set(Launcher_HUB_HOME_URL "https://projecttick.org/p/projt-launcher/" CACHE STRING "Launcher Hub home URL")
set(Launcher_HUB_COMMUNITY_URL "https://projecttick.org/projtlauncher/discord" CACHE STRING "Launcher Hub community URL")
set(Launcher_HUB_SEARCH_URL "https://www.google.com/search?q=%1" CACHE STRING "Search URL (with %1 placeholder) used by the Launcher Hub address bar")
set(Launcher_LOGIN_CALLBACK_URL "https://projecttick.org/p/projt-launcher" CACHE STRING "URL that gets opened when the user successfully logins.")
set(Launcher_FMLLIBS_BASE_URL "https://files.projecttick.org/fmllibs/" CACHE STRING "URL for FML Libraries.")

######## Set version numbers ########
set(Launcher_VERSION_MAJOR 0)
set(Launcher_VERSION_MINOR 0)
set(Launcher_VERSION_PATCH 5)
set(Launcher_VERSION_TWEAK 1)

set(Launcher_VERSION_NAME "${Launcher_VERSION_MAJOR}.${Launcher_VERSION_MINOR}.${Launcher_VERSION_PATCH}.${Launcher_VERSION_TWEAK}")
set(Launcher_VERSION_NAME4 "${Launcher_VERSION_MAJOR}.${Launcher_VERSION_MINOR}.${Launcher_VERSION_PATCH}.${Launcher_VERSION_TWEAK}")
set(Launcher_VERSION_NAME4_COMMA "${Launcher_VERSION_MAJOR},${Launcher_VERSION_MINOR},${Launcher_VERSION_PATCH},${Launcher_VERSION_TWEAK}")

# Build platform.
set(Launcher_BUILD_PLATFORM "unknown" CACHE STRING "A short string identifying the platform that this build was built for. Only used to display in the about dialog.")

# CI fuzz builds use a reduced target set; skip heavy components and installers when enabled.
option(LAUNCHER_FUZZ_ONLY "Build only fuzzing targets (skips installers, Java launcher, and desktop integration)" OFF)

# Release feed URL for updater
set(Launcher_UPDATER_RELEASES_URL "https://projecttick.org/product/projt-launcher/feed.xml" CACHE STRING "URL for the updater release feed manifest.")

# Name to help updater identify valid artifacts
set(Launcher_BUILD_ARTIFACT "" CACHE STRING "Artifact name to help the updater identify valid artifacts.")

# The metadata server
set(Launcher_META_URL "https://meta.projecttick.org/" CACHE STRING "URL to fetch Launcher's meta files from.")

# Imgur API Client ID
set(Launcher_IMGUR_CLIENT_ID "5b97b0713fba4a3" CACHE STRING "Client ID you can get from Imgur when you register an application")

# Bug tracker URL
set(Launcher_BUG_TRACKER_URL "https://github.com/Project-Tick/ProjT-Launcher/issues" CACHE STRING "URL for the bug tracker.")

# Translations Platform URL
set(Launcher_TRANSLATIONS_URL "https://crowdin.com/project/projtlauncher" CACHE STRING "URL for the translations platform.")
set(Launcher_TRANSLATION_FILES_URL "https://i18n.projecttick.org/" CACHE STRING "URL for the translations files.")

# Matrix Space
set(Launcher_MATRIX_URL "https://projecttick.org/projtlauncher/matrix" CACHE STRING "URL to the Matrix Space")

# Discord URL
set(Launcher_DISCORD_URL "https://projecttick.org/projtlauncher/discord" CACHE STRING "URL for the Discord guild.")

# Subreddit URL
set(Launcher_SUBREDDIT_URL "https:/projecttick.org/projtlauncher/reddit" CACHE STRING "URL for the subreddit.")

# Builds
set(Launcher_QT_VERSION_MAJOR "6" CACHE STRING "Major Qt version to build against")

# Java downloader
set(Launcher_ENABLE_JAVA_DOWNLOADER_DEFAULT ON)

# Although we recommend enabling this, we cannot guarantee binary compatibility on
# differing Linux/BSD/etc distributions. Downstream packagers should be explicitly opt-ing into this
# feature if they know it will work with their distribution.
if(UNIX AND NOT APPLE)
    set(Launcher_ENABLE_JAVA_DOWNLOADER_DEFAULT OFF)
endif()

# Java downloader
option(Launcher_ENABLE_JAVA_DOWNLOADER "Build the java downloader feature" ${Launcher_ENABLE_JAVA_DOWNLOADER_DEFAULT})

# Native libraries
if(UNIX AND APPLE)
    set(Launcher_GLFW_LIBRARY_NAME "libglfw.dylib" CACHE STRING "Name of native glfw library")
    set(Launcher_OPENAL_LIBRARY_NAME "libopenal.dylib" CACHE STRING "Name of native openal library")
elseif(UNIX)
    set(Launcher_GLFW_LIBRARY_NAME "libglfw.so" CACHE STRING "Name of native glfw library")
    set(Launcher_OPENAL_LIBRARY_NAME "libopenal.so" CACHE STRING "Name of native openal library")
elseif(WIN32)
    set(Launcher_GLFW_LIBRARY_NAME "glfw.dll" CACHE STRING "Name of native glfw library")
    set(Launcher_OPENAL_LIBRARY_NAME "OpenAL.dll" CACHE STRING "Name of native openal library")
endif()

# API Keys
# NOTE: These API keys are here for convenience. If you rebrand this software or intend to break the terms of service
# of these platforms, please change these API keys beforehand.
# Be aware that if you were to use these API keys for malicious purposes they might get revoked, which might cause
# breakage to thousands of users.
# If you don't plan to use these features of this software, you can just remove these values.

# By using this key in your builds you accept the terms of use laid down in
# https://docs.microsoft.com/en-us/legal/microsoft-identity-platform/terms-of-use
set(Launcher_MSA_CLIENT_ID "3035382c-8f73-493a-b579-d182905c2864" CACHE STRING "Client ID you can get from Microsoft Identity Platform when you register an application")

# By using this key in your builds you accept the terms and conditions laid down in
# https://support.curseforge.com/en/support/solutions/articles/9000207405-curse-forge-3rd-party-api-terms-and-conditions
# NOTE: CurseForge requires you to change this if you make any kind of derivative work.
# This key was issued specifically for ProjT Launcher
set(Launcher_CURSEFORGE_API_KEY "$2a$10$S7KcKijbCj8mCHUQcn0tgOmtHg0kA8q9FI0niNJJ7knPq0INomzrG" CACHE STRING "API key for the CurseForge platform")

set(Launcher_COMPILER_NAME ${CMAKE_CXX_COMPILER_ID})
set(Launcher_COMPILER_VERSION ${CMAKE_CXX_COMPILER_VERSION})
set(Launcher_COMPILER_TARGET_SYSTEM ${CMAKE_SYSTEM_NAME})
set(Launcher_COMPILER_TARGET_SYSTEM_VERSION ${CMAKE_SYSTEM_VERSION})
set(Launcher_COMPILER_TARGET_PROCESSOR ${CMAKE_SYSTEM_PROCESSOR})

#### Check the current Git commit and branch
include(GetGitRevisionDescription)
git_get_exact_tag(Launcher_GIT_TAG)
get_git_head_revision(Launcher_GIT_REFSPEC Launcher_GIT_COMMIT)

message(STATUS "Git commit: ${Launcher_GIT_COMMIT}")
message(STATUS "Git tag: ${Launcher_GIT_TAG}")
message(STATUS "Git refspec: ${Launcher_GIT_REFSPEC}")

string(TIMESTAMP TODAY "%Y-%m-%d")
set(Launcher_BUILD_TIMESTAMP "${TODAY}")

################################ 3rd Party Libs ################################

if(WIN32 AND NOT MINGW)
    # Detect NuGet architecture
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|arm64")
        set(NUGET_ARCH "arm64")
        set(OSSL_ARCH "arm64")
    else()
        set(NUGET_ARCH "x64")
        set(OSSL_ARCH "x64")
    endif()

    include(findOpenSSLbyNuget)
    projt_find_openssl_by_nuget()

    list(APPEND CMAKE_PREFIX_PATH "${PTLIBZIPPY_ROOT}" "${OPENSSL_ROOT_DIR}")
    list(APPEND CMAKE_INCLUDE_PATH "${CMAKE_SOURCE_DIR}/dependencies")
endif()

include(usePTlibzippy)
projt_add_ptlibzippy()

if(DEFINED PTlibzippy_SOURCE_DIR AND DEFINED PTlibzippy_BINARY_DIR)
    # Make PTlibzippy headers available to libpng config generation.
    set(ZLIB_INCLUDE_DIRS "${PTlibzippy_SOURCE_DIR};${PTlibzippy_BINARY_DIR}")
elseif(PTLIBZIPPY_INCLUDE_DIRS)
    set(ZLIB_INCLUDE_DIRS "${PTLIBZIPPY_INCLUDE_DIRS}")
endif()

if(NOT LAUNCHER_FUZZ_ONLY AND UNIX AND NOT APPLE)
    include(useLibpng)
    projt_add_libpng()
endif()

if(NOT LAUNCHER_FUZZ_ONLY)
    if(NOT OpenSSL_FOUND)
        find_package(OpenSSL REQUIRED)
    endif()

    set(LAUNCHER_WITH_WEBENGINE ON)

    if(MINGW)
        set(LAUNCHER_WITH_WEBENGINE OFF)
    endif()
    if(UNIX AND NOT APPLE)
        set(LAUNCHER_WITH_WEBENGINE OFF)
    endif()
    if(MSVC AND (CMAKE_VS_PLATFORM_NAME STREQUAL "ARM64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64"))
        set(LAUNCHER_WITH_WEBENGINE OFF)
    endif()

    set(QT_COMPONENTS Core CoreTools Widgets Concurrent Network Test Xml NetworkAuth OpenGL)
    if(LAUNCHER_WITH_WEBENGINE)
        list(APPEND QT_COMPONENTS WebEngineWidgets WebChannel)
    endif()

    find_package(Qt6 REQUIRED COMPONENTS ${QT_COMPONENTS})
    if(DEFINED Qt6Core_VERSION)
        set(_projt_qt_version "${Qt6Core_VERSION}")
    elseif(DEFINED Qt6_VERSION)
        set(_projt_qt_version "${Qt6_VERSION}")
    endif()
    if(DEFINED _projt_qt_version AND _projt_qt_version VERSION_LESS "6.8.0")
        message(FATAL_ERROR "Qt 6.8.0 or newer is required. Detected: ${_projt_qt_version}")
    endif()
    unset(_projt_qt_version)
    find_package(Qt6 COMPONENTS DBus)
    list(APPEND Launcher_QT_DBUS Qt6::DBus)
endif()
# Note: Qt6 removed from fuzzer builds - fuzz_qjson_parse disabled due to glib dependency issues

# System / Package Manager Provided Libs - Using bundled versions
if(NOT LAUNCHER_FUZZ_ONLY)
    include(useBZip2)
    projt_add_bzip2()

    if(UNIX AND NOT APPLE)
        include(useMinizip)
        projt_add_minizip()
    endif()

    include(useQuazip)
    projt_add_quazip()

    include(useTomlplusplus)
    projt_add_tomlplusplus()
endif()

if(NOT LAUNCHER_FUZZ_ONLY)
    include(useCMark)
    projt_add_cmark()
endif()

if(NOT LAUNCHER_FUZZ_ONLY)
    include(useLibqrencode)
    projt_add_libqrencode()
endif()

if(NOT LAUNCHER_FUZZ_ONLY)
    include(ECMQtDeclareLoggingCategory)
endif()

####################################### Program Info #######################################

if(NOT LAUNCHER_FUZZ_ONLY)
    projt_push_output_dirs("program_info")
    add_subdirectory(program_info)
    projt_pop_output_dirs()
endif()

####################################### Install layout #######################################

set(Launcher_ENABLE_UPDATER NO)
set(Launcher_BUILD_UPDATER NO)

if (NOT APPLE AND (NOT Launcher_UPDATER_RELEASES_URL STREQUAL "" AND NOT Launcher_BUILD_ARTIFACT STREQUAL ""))
    set(Launcher_BUILD_UPDATER YES)
endif()

if(NOT LAUNCHER_FUZZ_ONLY)
    if(NOT (UNIX AND APPLE))
        # Install "portable.txt" if selected component is "portable"
        install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/${Launcher_Portable_File}" DESTINATION "." COMPONENT portable EXCLUDE_FROM_ALL)
    endif()
endif()

if(NOT LAUNCHER_FUZZ_ONLY AND UNIX AND APPLE)
    set(BINARY_DEST_DIR "${Launcher_Name}.app/Contents/MacOS")
    set(LIBRARY_DEST_DIR "${Launcher_Name}.app/Contents/MacOS")
    set(PLUGIN_DEST_DIR "${Launcher_Name}.app/Contents/MacOS")
    set(FRAMEWORK_DEST_DIR "${Launcher_Name}.app/Contents/Frameworks")
    set(RESOURCES_DEST_DIR "${Launcher_Name}.app/Contents/Resources")
    set(JARS_DEST_DIR "${Launcher_Name}.app/Contents/MacOS/jars")

    # Mac bundle settings
    set(MACOSX_BUNDLE_BUNDLE_NAME "${Launcher_DisplayName}")
    set(MACOSX_BUNDLE_INFO_STRING "${Launcher_DisplayName}: A custom launcher for Minecraft that allows you to easily manage multiple installations of Minecraft at once.")
    set(MACOSX_BUNDLE_GUI_IDENTIFIER "org.projecttick.${Launcher_Name}")
    set(MACOSX_BUNDLE_BUNDLE_VERSION "${Launcher_VERSION_NAME}")
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${Launcher_VERSION_NAME}")
    set(MACOSX_BUNDLE_LONG_VERSION_STRING "${Launcher_VERSION_NAME}")
    set(MACOSX_BUNDLE_ICON_FILE ${Launcher_Name}.icns)
    set(MACOSX_BUNDLE_COPYRIGHT "${Launcher_Copyright_Mac}")
    set(MACOSX_SPARKLE_UPDATE_PUBLIC_KEY "Lju8CtOHHDmev7KkEl3ks/6wWkG1mlZ0G89XIG8hNLQ=" CACHE STRING "Public key for Sparkle update feed")
    set(MACOSX_SPARKLE_UPDATE_FEED_URL "https://projecttick.org/product/projt-launcher/feed/appcast.xml" CACHE STRING "URL for Sparkle update feed")
    set(MACOSX_BOOTSTRAP_FEED_URL "https://projecttick.org/product/projt-launcher/appcast.xml" CACHE STRING "URL for bootstrap update feed")
    set(MACOSX_BOOTSTRAP_URL_TEMPLATE "https://github.com/Project-Tick/ProjT-Launcher/releases/download/%s/ProjTLauncher-macOS-%s.zip" CACHE STRING "URL template for bootstrap download")

    set(MACOSX_SPARKLE_DOWNLOAD_URL "https://github.com/sparkle-project/Sparkle/releases/download/2.8.0/Sparkle-2.8.0.tar.xz" CACHE STRING "URL to Sparkle release archive")
    set(MACOSX_SPARKLE_SHA256 "fd5681ee92bf238aaac2d08214ceaf0cc8976e452d7f882d80bac1e61581f3b1" CACHE STRING "SHA256 checksum for Sparkle release archive")
    set(MACOSX_SPARKLE_DIR "${CMAKE_BINARY_DIR}/frameworks/Sparkle")

    if(NOT MACOSX_SPARKLE_UPDATE_PUBLIC_KEY STREQUAL "" AND NOT MACOSX_SPARKLE_UPDATE_FEED_URL STREQUAL "")
        set(Launcher_ENABLE_UPDATER YES)
    endif()

    # Add the icon
    install(FILES ${Launcher_Branding_ICNS} DESTINATION ${RESOURCES_DEST_DIR} RENAME ${Launcher_Name}.icns)

    if(NOT LAUNCHER_FUZZ_ONLY)
        add_subdirectory(bootstrap/macos)
        find_program(ACTOOL_EXE actool DOC "Path to the apple asset catalog compiler")
        if(ACTOOL_EXE)
            execute_process(
                COMMAND xcodebuild -version
                OUTPUT_VARIABLE XCODE_VERSION_OUTPUT
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )

            string(REGEX MATCH "Xcode ([0-9]+\\.[0-9]+)" XCODE_VERSION_MATCH "${XCODE_VERSION_OUTPUT}")
            if(XCODE_VERSION_MATCH)
                set(XCODE_VERSION ${CMAKE_MATCH_1})
            else()
                set(XCODE_VERSION 0.0)
            endif()

            if(XCODE_VERSION VERSION_GREATER_EQUAL 26.0)
                set(ASSETS_OUT_DIR "${CMAKE_BINARY_DIR}/program_info")
                set(GENERATED_ASSETS_CAR "${ASSETS_OUT_DIR}/Assets.car")
                set(ICON_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${Launcher_Branding_MAC_ICON}")

                add_custom_command(
                    OUTPUT "${GENERATED_ASSETS_CAR}"
                    COMMAND ${ACTOOL_EXE} "${ICON_SOURCE}"
                            --compile "${ASSETS_OUT_DIR}"
                            --output-partial-info-plist /dev/null
                            --app-icon ${Launcher_Name}
                            --enable-on-demand-resources NO
                            --target-device mac
                            --minimum-deployment-target ${CMAKE_OSX_DEPLOYMENT_TARGET}
                            --platform macosx
                    DEPENDS "${ICON_SOURCE}"
                    COMMENT "Compiling asset catalog (${ICON_SOURCE})"
                    VERBATIM
                )
                add_custom_target(compile_assets ALL DEPENDS "${GENERATED_ASSETS_CAR}")
                install(FILES "${GENERATED_ASSETS_CAR}" DESTINATION "${RESOURCES_DEST_DIR}")
            else()
                message(WARNING "Xcode ${XCODE_VERSION} is too old. Minimum required version is 26.0. Not compiling liquid glass icons.")
            endif()

        else()
            message(WARNING "actool not found. Cannot compile macOS app icons.\n"
                    "Install Xcode command line tools: 'xcode-select --install'")
        endif()
    endif()

elseif(NOT LAUNCHER_FUZZ_ONLY AND UNIX)
    foreach(_launcher_install_dir IN ITEMS
        BINDIR
        SBINDIR
        LIBDIR
        LIBEXECDIR
        INCLUDEDIR
        LOCALSTATEDIR
        SHAREDSTATEDIR
        DATAROOTDIR
        DATADIR
        LOCALEDIR
        MANDIR
        INFODIR
        SYSCONFDIR
        RUNSTATEDIR
        DOCDIR)
        if(NOT DEFINED KDE_INSTALL_${_launcher_install_dir}
            AND DEFINED CMAKE_INSTALL_${_launcher_install_dir}
            AND NOT CMAKE_INSTALL_${_launcher_install_dir} STREQUAL "")
            set(KDE_INSTALL_${_launcher_install_dir} "${CMAKE_INSTALL_${_launcher_install_dir}}" CACHE PATH "" FORCE)
        endif()
    endforeach()
    unset(_launcher_install_dir)
    include(KDEInstallDirs)

    set(BINARY_DEST_DIR "bin")
    set(LIBRARY_DEST_DIR "lib${LIB_SUFFIX}")
    set(JARS_DEST_DIR "share/${Launcher_Name}")

    # Set RPATH (include bundled lib dir if configured)
    set(_launcher_rpath_entries "")
    if(DEFINED Launcher_BUNDLED_LIBDIR AND NOT Launcher_BUNDLED_LIBDIR STREQUAL "")
        list(APPEND _launcher_rpath_entries "$ORIGIN/../${Launcher_BUNDLED_LIBDIR}")
    endif()
    list(APPEND _launcher_rpath_entries "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}" "$ORIGIN")
    string(JOIN ":" Launcher_BINARY_RPATH ${_launcher_rpath_entries})
    unset(_launcher_rpath_entries)

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${Launcher_Desktop} DESTINATION ${KDE_INSTALL_APPDIR})
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${Launcher_MetaInfo} DESTINATION ${KDE_INSTALL_METAINFODIR})
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/${Launcher_SVG} DESTINATION "${KDE_INSTALL_ICONDIR}/hicolor/scalable/apps")
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${Launcher_MIMEInfo} DESTINATION ${KDE_INSTALL_MIMEDIR})

    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/launcher/qtlogging.ini" DESTINATION "share/${Launcher_Name}")

    set(PLUGIN_DEST_DIR "plugins")
    set(BUNDLE_DEST_DIR ".")
    set(RESOURCES_DEST_DIR ".")

    if(Launcher_ManPage)
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${Launcher_ManPage} DESTINATION "${KDE_INSTALL_MANDIR}/man6")
    endif()

    # Install basic runner script if component "portable" is selected
    configure_file(launcher/Launcher.in "${CMAKE_CURRENT_BINARY_DIR}/LauncherScript" @ONLY)
    install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/LauncherScript" DESTINATION "." RENAME ${Launcher_Name} COMPONENT portable EXCLUDE_FROM_ALL)

elseif(WIN32)
    set(BINARY_DEST_DIR ".")
    set(LIBRARY_DEST_DIR ".")
    set(PLUGIN_DEST_DIR ".")
    set(RESOURCES_DEST_DIR ".")
    set(JARS_DEST_DIR "jars")
elseif(LAUNCHER_FUZZ_ONLY)
    # For fuzz-only builds on other platforms, skip installer/layout specifics.
    set(BINARY_DEST_DIR ".")
    set(LIBRARY_DEST_DIR ".")
    set(PLUGIN_DEST_DIR ".")
    set(RESOURCES_DEST_DIR ".")
    set(JARS_DEST_DIR "jars")
else()
    message(FATAL_ERROR "Platform not supported")
endif()

######################################## Packaging ########################################

if(NOT LAUNCHER_FUZZ_ONLY AND UNIX AND NOT APPLE)
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/LauncherPackaging.cmake)
endif()



################################ Included Libs ################################

include(ExternalProject)
set_directory_properties(PROPERTIES EP_BASE External)

include(useMinimalLibs)
projt_add_internallibs()

############################### Built Artifacts ###############################

if(NOT LAUNCHER_FUZZ_ONLY)
    projt_push_output_dirs("buildconfig")
    add_subdirectory(buildconfig)
    projt_pop_output_dirs()
endif()

if(BUILD_TESTING)
    projt_push_output_dirs("tests")
    add_subdirectory(tests)
    projt_pop_output_dirs()
endif()
if(BUILD_FUZZERS)
    projt_push_output_dirs("fuzz")
    add_subdirectory(fuzz EXCLUDE_FROM_ALL)
    projt_pop_output_dirs()
endif()
# NOTE: this must always be last to appease the CMake deity of quirky install command evaluation order.
if(NOT LAUNCHER_FUZZ_ONLY)
    projt_push_output_dirs("launcher")
    add_subdirectory(launcher)
    projt_pop_output_dirs()
endif()

# Ensure test executables pick up CEF runtime dependencies once they exist.
if(BUILD_TESTING AND TARGET projt_cef_runtime_deps)
    get_property(_projt_launcher_test_targets GLOBAL PROPERTY PROJT_LAUNCHER_TEST_TARGETS)
    foreach(_projt_launcher_test_target IN LISTS _projt_launcher_test_targets)
        if(TARGET "${_projt_launcher_test_target}")
            target_link_libraries(${_projt_launcher_test_target} projt_cef_runtime_deps)
        endif()
    endforeach()
    unset(_projt_launcher_test_targets)
    unset(_projt_launcher_test_target)
endif()
