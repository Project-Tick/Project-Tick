cmake_minimum_required(VERSION 3.28)
# SPDX-FileCopyrightText: 2026 Project Tick
# SPDX-FileContributor: Project Tick
# SPDX-License-Identifier: GPL-3.0-or-later
project(MeshMC)

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

find_package(ECM NO_MODULE REQUIRED)
set(CMAKE_MODULE_PATH "${ECM_MODULE_PATH};${CMAKE_MODULE_PATH}")

include(CTest)
include(ECMAddTests)
if(BUILD_TESTING)
    enable_testing()
endif()

string(COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${CMAKE_BUILD_DIR}" IS_IN_SOURCE_BUILD)
if(IS_IN_SOURCE_BUILD)
    message(FATAL_ERROR "You are building MeshMC in-source. Please separate the build tree from the source tree.")
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(CMAKE_HOST_SYSTEM_VERSION MATCHES ".*[Mm]icrosoft.*" OR
        CMAKE_HOST_SYSTEM_VERSION MATCHES ".*WSL.*"
    )
        message(FATAL_ERROR "Building MeshMC is not supported in Linux-on-Windows distributions.")
    endif()
endif()

set(MeshMC_COMPILER_NAME ${CMAKE_CXX_COMPILER_ID})
set(MeshMC_COMPILER_VERSION ${CMAKE_CXX_COMPILER_VERSION})
set(MeshMC_COMPILER_TARGET_SYSTEM ${CMAKE_SYSTEM_NAME})
set(MeshMC_COMPILER_TARGET_SYSTEM_VERSION ${CMAKE_SYSTEM_VERSION})
set(MeshMC_COMPILER_TARGET_PROCESSOR ${CMAKE_SYSTEM_PROCESSOR})

##################################### Set CMake options #####################################
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/;${CMAKE_MODULE_PATH}")

# Output all executables and shared libs in the main build folder, not in subfolders.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
if(UNIX)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
endif()
set(CMAKE_JAVA_TARGET_OUTPUT_DIR ${PROJECT_BINARY_DIR}/jars)

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
if(MSVC)
    # FIXME: readd /WX flag
    set(CMAKE_CXX_FLAGS " /W4 ${CMAKE_CXX_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /DQT_NO_DEPRECATED_WARNINGS=Y")
else()
    # FIXME: readd -Werror flag
    set(CMAKE_CXX_FLAGS " -Wall -pedantic -Wno-deprecated-declarations -fstack-protector-strong --param=ssp-buffer-size=4 -O3 -D_FORTIFY_SOURCE=2 ${CMAKE_CXX_FLAGS}")
    if(UNIX AND APPLE)
        set(CMAKE_CXX_FLAGS " -stdlib=libc++ ${CMAKE_CXX_FLAGS}")
    endif()
    # FIXME: readd this: set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Werror=return-type")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DQT_NO_DEPRECATED_WARNINGS=Y")
endif()

include(ECMQtDeclareLoggingCategory)

option(ENABLE_LTO "Enable Link Time Optimization" off)
option(MeshMC_DISABLE_JAVA_DOWNLOADER "Disable the built-in Java downloader feature" OFF)

if(ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)

    if(ipo_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL TRUE)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)
        if(CMAKE_BUILD_TYPE)
            if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
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

################################ 3rd Party Libs ################################

# Find the required Qt parts
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    Concurrent
    Network
    NetworkAuth
    Test
    Xml
)

find_package(QuaZip-Qt6 REQUIRED)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

####################################### Branding #######################################

######## Firstly, please set version numbers ########
set(MeshMC_VERSION_MAJOR    7)
set(MeshMC_VERSION_MINOR    0)
set(MeshMC_VERSION_HOTFIX   0)

set(MeshMC_RELEASE_VERSION_NAME "${MeshMC_VERSION_MAJOR}.${MeshMC_VERSION_MINOR}.${MeshMC_VERSION_HOTFIX}")
set(MeshMC_VERSION_NAME4 "${MeshMC_VERSION_MAJOR}.${MeshMC_VERSION_MINOR}.${MeshMC_VERSION_HOTFIX}.0")
set(MeshMC_VERSION_NAME4_COMMA "${MeshMC_VERSION_MAJOR},${MeshMC_VERSION_MINOR},${MeshMC_VERSION_HOTFIX},0")

######## Please define MeshMC attributes ########

set(MeshMC_CommonName "MeshMC")
set(MeshMC_AppBinaryName "meshmc")
set(MeshMC_AppID "org.projecttick.MeshMC")
set(MeshMC_Copyright "Project Tick")
set(MeshMC_Copyright_Mac "© 2026 Project Tick")
set(MeshMC_Domain "projecttick.org")
set(MeshMC_Name "${MeshMC_CommonName}")
set(MeshMC_DisplayName "${MeshMC_CommonName}")
set(MeshMC_UserAgent "${MeshMC_CommonName}/5.0")
set(MeshMC_ConfigFile "meshmc.cfg")
set(MeshMC_Git "https://github.com/Project-Tick/MeshMC")

set(MeshMC_SVGFileName "${MeshMC_AppID}.svg")
set(MeshMC_Branding_ICNS "branding/${MeshMC_AppID}.icns")
set(MeshMC_Branding_WindowsRC "branding/${MeshMC_AppBinaryName}.rc")
set(MeshMC_Branding_LogoQRC "branding/${MeshMC_AppBinaryName}.qrc")

configure_file(branding/${MeshMC_AppID}.desktop.in ${MeshMC_AppID}.desktop)
configure_file(branding/${MeshMC_AppID}.metainfo.xml.in ${MeshMC_AppID}.metainfo.xml)
configure_file(branding/${MeshMC_AppBinaryName}.rc.in ${MeshMC_AppBinaryName}.rc @ONLY)
configure_file(branding/${MeshMC_AppBinaryName}.qrc.in ${MeshMC_AppBinaryName}.qrc @ONLY)
configure_file(branding/${MeshMC_CommonName}.manifest.in ${MeshMC_AppBinaryName}.manifest @ONLY)
configure_file(branding/${MeshMC_AppID}.mime.xml ${MeshMC_AppID}.xml COPYONLY)
configure_file(branding/${MeshMC_SVGFileName} ${MeshMC_SVGFileName} COPYONLY)
configure_file(branding/${MeshMC_AppID}.ico ${MeshMC_AppID}.ico COPYONLY)
set(MeshMC_Portable_File "branding/portable.txt")
set(MeshMC_Branding_MAC_ICON "branding/${MeshMC_CommonName}.icon")

if(MSVC)
    set(MeshMC_MSVC_Redist_NSIS_Section [=[
!ifdef haveNScurl
Section "Visual Studio Runtime"
    Var /GLOBAL vc_redist_exe
    ${If} ${IsNativeARM64}
        StrCpy $vc_redist_exe "vc_redist.arm64.exe"
    ${Else}
        StrCpy $vc_redist_exe "vc_redist.x64.exe"
    ${EndIf}
    DetailPrint 'Downloading Microsoft Visual C++ Redistributable...'
    NScurl::http GET "https://aka.ms/vs/17/release/$vc_redist_exe" "$INSTDIR\vc_redist\$vc_redist_exe" /INSIST /CANCEL /Zone.Identifier /END
    Pop $0
    ${If} $0 == "OK"
        DetailPrint "Download successful"
        ExecWait "$INSTDIR\vc_redist\$vc_redist_exe /install /passive /norestart"
    ${Else}
        DetailPrint "Download failed with error $0"
    ${EndIf}
SectionEnd
!endif
]=])
endif()

configure_file(branding/win_install.nsi.in win_install.nsi @ONLY)

######## Please define URLs ########

set(MeshMC_NEWS_RSS_URL "https://projecttick.org/product/meshmc/feed.xml" CACHE STRING "URL to fetch MeshMC's news RSS feed from.")

# Build number
set(MeshMC_VERSION_BUILD -1 CACHE STRING "Build number. -1 for no build number.")

# Name to help updater identify valid artifacts
set(MeshMC_BUILD_ARTIFACT "" CACHE STRING "Artifact name to help the updater identify valid artifacts.")

# Build platform.
set(MeshMC_BUILD_PLATFORM "" CACHE STRING "A short string identifying the platform that this build was built for. Only used by the notification system and to display in the about dialog.")

# Channel list URL
set(MeshMC_UPDATER_BASE "" CACHE STRING "Base URL for the updater.")

# Notification URL
set(MeshMC_NOTIFICATION_URL "https://projecttick.org/" CACHE STRING "URL for checking for notifications.")

# The metadata server
set(MeshMC_META_URL "https://meta.projecttick.org/" CACHE STRING "URL to fetch MeshMC's meta files from.")

# Microsoft Client ID
set(MeshMC_MICROSOFT_CLIENT_ID "3035382c-8f73-493a-b579-d182905c2864")

# paste.ee API key
set(MeshMC_PASTE_EE_API_KEY "ay2miY3lWxUHEEzYucj5e4YShIuo1aaY43MfwsUAe" CACHE STRING "API key you can get from paste.ee when you register an account")

# Imgur API Client ID
set(MeshMC_IMGUR_CLIENT_ID "5b97b0713fba4a3" CACHE STRING "Client ID you can get from Imgur when you register an application")

# CurseForge API key
set(MeshMC_CURSEFORGE_API_KEY "$2a$10$S7KcKijbCj8mCHUQcn0tgOmtHg0kA8q9FI0niNJJ7knPq0INomzrG" CACHE STRING "API key for the CurseForge API (get one at https://console.curseforge.com/)")

# Google analytics ID
set(MeshMC_ANALYTICS_ID "G-91XWT2Q4LY" CACHE STRING "ID you can get from Google analytics")

# Bug tracker URL
set(MeshMC_BUG_TRACKER_URL "https://github.com/Project-Tick/MeshMC/issues" CACHE STRING "URL for the bug tracker.")

# Discord URL
set(MeshMC_DISCORD_URL "" CACHE STRING "URL for the Discord guild.")

# Subreddit URL
set(MeshMC_SUBREDDIT_URL "" CACHE STRING "URL for the subreddit.")

#### Check the current Git commit and branch
include(GetGitRevisionDescription)
get_git_head_revision(MeshMC_GIT_REFSPEC MeshMC_GIT_COMMIT)
git_get_exact_tag(MeshMC_GIT_TAG)

message(STATUS "Git commit: ${MeshMC_GIT_COMMIT}")
message(STATUS "Git refspec: ${MeshMC_GIT_REFSPEC}")
message(STATUS "Git tag: ${MeshMC_GIT_TAG}")

string(TIMESTAMP TODAY "%Y-%m-%d")
set(MeshMC_BUILD_TIMESTAMP "${TODAY}")

#### Custom target to just print the version.
add_custom_target(version echo "Version: ${MeshMC_RELEASE_VERSION_NAME}")
add_custom_target(tcversion echo "\\#\\#teamcity[setParameter name=\\'env.MESHMC_VERSION\\' value=\\'${MeshMC_RELEASE_VERSION_NAME}\\']")

####################################### Install layout #######################################

if(NOT (UNIX AND APPLE))
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/${MeshMC_Portable_File}" DESTINATION "." COMPONENT portable EXCLUDE_FROM_ALL)
endif()

if(UNIX AND APPLE)
    set(BINARY_DEST_DIR "${MeshMC_Name}.app/Contents/MacOS")
    set(LIBRARY_DEST_DIR "${MeshMC_Name}.app/Contents/MacOS")
    set(PLUGIN_DEST_DIR "${MeshMC_Name}.app/Contents/MacOS")
    set(FRAMEWORK_DEST_DIR "${MeshMC_Name}.app/Contents/Frameworks")
    set(RESOURCES_DEST_DIR "${MeshMC_Name}.app/Contents/Resources")
    set(JARS_DEST_DIR "${MeshMC_Name}.app/Contents/MacOS/jars")
    set(PLUGIN_DEST_DIR "plugins")
    set(BUNDLE_DEST_DIR ".")
    set(RESOURCES_DEST_DIR ".")

    # Mac bundle settings
    set(MACOSX_BUNDLE_BUNDLE_NAME "${MeshMC_DisplayName}")
    set(MACOSX_BUNDLE_INFO_STRING "${MeshMC_DisplayName}: A custom launcher for Minecraft that allows you to easily manage multiple installations of Minecraft at once.")
    set(MACOSX_BUNDLE_GUI_IDENTIFIER "${MeshMC_AppID}")
    set(MACOSX_BUNDLE_BUNDLE_VERSION "${MeshMC_RELEASE_VERSION_NAME}")
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${MeshMC_RELEASE_VERSION_NAME}")
    set(MACOSX_BUNDLE_LONG_VERSION_STRING "${MeshMC_RELEASE_VERSION_NAME}")
    set(MACOSX_BUNDLE_ICON_FILE ${MeshMC_AppID}.icns)
    set(MACOSX_BUNDLE_COPYRIGHT "${MeshMC_Copyright_Mac}")
    set(MACOSX_SPARKLE_UPDATE_PUBLIC_KEY "C0eBoyDSoZbzgCMxQH9wH6kmjU2mPRmvhZZd9mHgqZQ=" CACHE STRING "Public key for Sparkle update feed")
    set(MACOSX_SPARKLE_UPDATE_FEED_URL "https://projecttick.org/product/meshmc/appcast.xml" CACHE STRING "URL for Sparkle update feed")

    set(MACOSX_SPARKLE_DOWNLOAD_URL "https://github.com/sparkle-project/Sparkle/releases/download/2.8.0/Sparkle-2.8.0.tar.xz" CACHE STRING "URL to Sparkle release archive")
    set(MACOSX_SPARKLE_SHA256 "fd5681ee92bf238aaac2d08214ceaf0cc8976e452d7f882d80bac1e61581f3b1" CACHE STRING "SHA256 checksum for Sparkle release archive")
    set(MACOSX_SPARKLE_DIR "${CMAKE_BINARY_DIR}/frameworks/Sparkle")

    # Add the icon
    install(FILES ${MeshMC_Branding_ICNS} DESTINATION ${RESOURCES_DEST_DIR} RENAME ${MeshMC_AppID}.icns)

    find_program(ACTOOL_EXE actool DOC "Path to the apple asset catalog compiler")
    if(ACTOOL_EXE)
        execute_process(
            COMMAND xcodebuild -version
            OUTPUT_VARIABLE XCODE_VERSION_OUTPUT
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        string(REGEX MATCH "Xcode ([0-9]+\.[0-9]+)" XCODE_VERSION_MATCH "${XCODE_VERSION_OUTPUT}")
        if(XCODE_VERSION_MATCH)
            set(XCODE_VERSION ${CMAKE_MATCH_1})
        else()
            set(XCODE_VERSION 0.0)
        endif()

        if(XCODE_VERSION VERSION_GREATER_EQUAL 26.0)
            set(ASSETS_OUT_DIR "${CMAKE_BINARY_DIR}/branding")
            set(GENERATED_ASSETS_CAR "${ASSETS_OUT_DIR}/Assets.car")
            set(ICON_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${MeshMC_Branding_MAC_ICON}")

            add_custom_command(
                OUTPUT "${GENERATED_ASSETS_CAR}"
                COMMAND ${ACTOOL_EXE} "${ICON_SOURCE}"
                        --compile "${ASSETS_OUT_DIR}"
                        --output-partial-info-plist /dev/null
                        --app-icon ${MeshMC_Name}
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


elseif(UNIX)
    include(KDEInstallDirs)

    set(BINARY_DEST_DIR "bin")
    set(LIBRARY_DEST_DIR "lib${LIB_SUFFIX}")
    set(JARS_DEST_DIR "share/${MeshMC_Name}")

    # Set RPATH
    SET(MeshMC_BINARY_RPATH "$ORIGIN/../lib${LIB_SUFFIX}")

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${MeshMC_AppID}.desktop DESTINATION ${KDE_INSTALL_APPDIR})
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${MeshMC_AppID}.metainfo.xml DESTINATION ${KDE_INSTALL_METAINFODIR})
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/branding/${MeshMC_AppID}.svg DESTINATION "${KDE_INSTALL_ICONDIR}/hicolor/scalable/apps")
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/branding/${MeshMC_AppID}_256.png DESTINATION "${KDE_INSTALL_ICONDIR}/hicolor/256x256/apps" RENAME "${MeshMC_AppID}.png")
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${MeshMC_AppID}.xml DESTINATION ${KDE_INSTALL_MIMEDIR})

    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/launcher/qtlogging.ini" DESTINATION "share/${MeshMC_Name}")

    set(PLUGIN_DEST_DIR "plugins")
    set(BUNDLE_DEST_DIR ".")
    set(RESOURCES_DEST_DIR ".")

    if(MeshMC_ManPage)
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${MeshMC_ManPage} DESTINATION "${KDE_INSTALL_MANDIR}/man6")
    endif()

    # Install basic runner script if component "portable" is selected
    configure_file(launcher/MeshMC.in "${CMAKE_CURRENT_BINARY_DIR}/MeshMCScript" @ONLY)
    install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/MeshMCScript" DESTINATION "." RENAME ${MeshMC_Name} COMPONENT portable EXCLUDE_FROM_ALL)

elseif(WIN32)
    set(BINARY_DEST_DIR ".")
    set(LIBRARY_DEST_DIR ".")
    set(PLUGIN_DEST_DIR ".")
    set(RESOURCES_DEST_DIR ".")
    set(JARS_DEST_DIR "jars")
else()
    message(FATAL_ERROR "Platform not supported")
endif()

################################ Included Libs ################################

include(ExternalProject)
set_directory_properties(PROPERTIES EP_BASE External)

option(NBT_BUILD_SHARED "Build NBT shared library" ON)
option(NBT_USE_ZLIB "Build NBT library with zlib support" OFF)
option(NBT_BUILD_TESTS "Build NBT library tests" OFF) #FIXME: fix unit tests.
set(NBT_NAME MeshMC_nbt++)
set(NBT_DEST_DIR ${LIBRARY_DEST_DIR})
add_subdirectory(libraries/libnbtplusplus)

add_subdirectory(libraries/ganalytics) # google analytics library
add_subdirectory(libraries/systeminfo) # system information library
add_subdirectory(libraries/hoedown) # markdown parser
add_subdirectory(libraries/launcher) # java based launcher part for Minecraft
add_subdirectory(libraries/javacheck) # java compatibility checker
add_subdirectory(libraries/xz-embedded) # xz compression
add_subdirectory(libraries/rainbow) # Qt extension for colors
add_subdirectory(libraries/iconfix) # fork of Qt's QIcon loader
add_subdirectory(libraries/LocalPeer) # fork of a library from Qt solutions
add_subdirectory(libraries/classparser) # google analytics library
add_subdirectory(libraries/optional-bare)
add_subdirectory(libraries/tomlc99) # toml parser
add_subdirectory(libraries/katabasis) # An OAuth2 library that tried to do too much

############################### Built Artifacts ###############################

add_subdirectory(buildconfig)

# NOTE: this must always be last to appease the CMake deity of quirky install command evaluation order.
add_subdirectory(launcher)
