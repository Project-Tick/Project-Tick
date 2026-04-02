if(UNIX)
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_search_module(SCDOC scdoc)
        if(SCDOC_FOUND)
            pkg_get_variable(SCDOC_SCDOC scdoc scdoc)
        endif()
    endif()
endif()

set(Launcher_AppBinaryName "projtlauncher")
set(Launcher_AppBinaryName "${Launcher_AppBinaryName}" PARENT_SCOPE)
set(Launcher_CommonName "ProjTLauncher")
set(Launcher_OrganizationName "Project-Tick")
set(Launcher_DisplayName "ProjT Launcher")
set(Launcher_AppID "org.projecttick.ProjTLauncher")
set(Launcher_Domain "projecttick.org" PARENT_SCOPE)
set(Launcher_Git "https://github.com/Project-Tick/Project-Tick" PARENT_SCOPE)

set(Launcher_Name "${Launcher_CommonName}" PARENT_SCOPE)
set(Launcher_DisplayName "${Launcher_DisplayName}" PARENT_SCOPE)
set(Launcher_Domain "${Launcher_Domain}" PARENT_SCOPE)
set(Launcher_Git "${Launcher_Git}" PARENT_SCOPE)

set(Launcher_SVGFileName "${Launcher_AppID}.svg")
set(Launcher_Copyright "© 2025-2026 Project Tick\\n© 2022-2025 Prism Launcher Contributors\\n© 2021-2022 PolyMC Contributors\\n© 2012-2021 MultiMC Contributors")
set(Launcher_Copyright_Mac "© 2025-2026 Project Tick, © 2022-2025 Prism Launcher Contributors, © 2021-2022 PolyMC Contributors and © 2012-2021 MultiMC Contributors" PARENT_SCOPE)
set(Launcher_Copyright "${Launcher_Copyright}" PARENT_SCOPE)
set(Launcher_UserAgent "${Launcher_CommonName}/${Launcher_VERSION_NAME}" PARENT_SCOPE)
set(Launcher_ConfigFile "${Launcher_AppBinaryName}.cfg" PARENT_SCOPE)
set(Launcher_AppID "${Launcher_AppID}" PARENT_SCOPE)
set(Launcher_SVGFileName "${Launcher_SVGFileName}" PARENT_SCOPE)

set(Launcher_Desktop "program_info/${Launcher_AppID}.desktop" PARENT_SCOPE)
set(Launcher_MIMEInfo "program_info/${Launcher_AppID}.xml" PARENT_SCOPE)
set(Launcher_MetaInfo "program_info/${Launcher_AppID}.metainfo.xml" PARENT_SCOPE)
set(Launcher_PNG_256 "program_info/${Launcher_AppID}_256.png" PARENT_SCOPE)
set(Launcher_SVG "program_info/${Launcher_SVGFileName}" PARENT_SCOPE)
set(Launcher_Branding_ICNS "program_info/${Launcher_AppBinaryName}.icns" PARENT_SCOPE)
set(Launcher_Branding_MAC_ICON "program_info/${Launcher_CommonName}.icon" PARENT_SCOPE)
set(Launcher_Branding_ICO "program_info/${Launcher_AppBinaryName}.ico")
set(Launcher_Branding_ICO "${Launcher_Branding_ICO}" PARENT_SCOPE)
set(Launcher_Branding_WindowsRC "program_info/${Launcher_AppBinaryName}.rc" PARENT_SCOPE)
set(Launcher_Branding_LogoQRC "program_info/${Launcher_AppBinaryName}.qrc" PARENT_SCOPE)
set(Launcher_Authors "MultiMC & Prism Launcher Contributors")

set(Launcher_Portable_File "program_info/portable.txt" PARENT_SCOPE)

configure_file(${Launcher_AppID}.desktop.in ${Launcher_AppID}.desktop)
configure_file(${Launcher_AppID}.metainfo.xml.in ${Launcher_AppID}.metainfo.xml)
configure_file(${Launcher_AppBinaryName}.rc.in ${Launcher_AppBinaryName}.rc @ONLY)
configure_file(${Launcher_AppBinaryName}.qrc.in ${Launcher_AppBinaryName}.qrc @ONLY)
configure_file(${Launcher_AppBinaryName}.manifest.in ${Launcher_AppBinaryName}.manifest @ONLY)
configure_file(${Launcher_AppBinaryName}.ico ${Launcher_AppBinaryName}.ico COPYONLY)
configure_file(${Launcher_SVGFileName} ${Launcher_SVGFileName} COPYONLY)
configure_file(gplv3-127x51.png gplv3-127x51.png COPYONLY)
configure_file(${Launcher_AppID}.mime.xml ${Launcher_AppID}.xml COPYONLY)
file(TO_CMAKE_PATH "${PROJECT_SOURCE_DIR}/COPYING.md" Launcher_License_File)

if(MSVC)
    set(Launcher_MSVC_Redist_NSIS_Section [=[
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

configure_file(win_install.nsi.in win_install.nsi @ONLY)

if(SCDOC_FOUND)
    configure_file(${Launcher_AppBinaryName}.6.scd.in ${Launcher_AppBinaryName}.6.scd @ONLY)

    set(in_scd "${CMAKE_CURRENT_BINARY_DIR}/${Launcher_AppBinaryName}.6.scd")
    set(out_man "${CMAKE_CURRENT_BINARY_DIR}/${Launcher_AppBinaryName}.6")
    add_custom_command(
        DEPENDS "${in_scd}"
        OUTPUT "${out_man}"
        COMMAND ${SCDOC_SCDOC} < "${in_scd}" > "${out_man}"
    )
    add_custom_target(man ALL DEPENDS ${out_man})
    set(Launcher_ManPage "program_info/${Launcher_AppBinaryName}.6" PARENT_SCOPE)
endif()
