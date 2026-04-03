# -*- coding: utf-8 -*-
# NSIS file to create a self-installing exe for MNV.
# It requires NSIS version 3.0 or later.
# Last Change:	2025-09-30
#

Unicode true  ; !include defaults to UTF-8 after Unicode True since 3.0 Alpha 2

# WARNING: if you make changes to this script, look out for $0 to be valid,
# because uninstall deletes most files in $0.

# Location of gmnv_ole.exe, mnvw32.exe, GmnvExt/*, etc.
!ifndef MNVSRC
  !define MNVSRC "..\src"
!endif

# Location of runtime files
!ifndef MNVRT
  !define MNVRT "..\runtime"
!endif

# Location of extra tools: diff.exe, winpty{32|64}.dll, winpty-agent.exe, etc.
!ifndef MNVTOOLS
  !define MNVTOOLS "..\.."
!endif

# Location of gettext.
# It must contain two directories: gettext32 and gettext64.
# See README.txt for detail.
!ifndef GETTEXT
  !define GETTEXT ${MNVTOOLS}
!endif

# If you have UPX, use the switch /DHAVE_UPX=1 on the command line makensis.exe.
# This property will be set to 1. Get it at https://upx.github.io/
!ifndef HAVE_UPX
  !define HAVE_UPX 0
!endif

# If you do not want to add Native Language Support, use the switch /DHAVE_NLS=0
# in the command line makensis.exe. This property will be set to 0.
!ifndef HAVE_NLS
  !define HAVE_NLS 1
!endif

# To create an English-only the installer, use the switch /DHAVE_MULTI_LANG=0 on
# the command line makensis.exe. This property will be set to 0.
!ifndef HAVE_MULTI_LANG
  !define HAVE_MULTI_LANG 1
!endif

# if you want to create a 64-bit the installer, use the switch /DWIN64=1 on
# the command line makensis.exe. This property will be set to 1.
!ifndef WIN64
  !define WIN64 0
!endif

# if you want to create the installer for ARM64, use the /DARM64=1 on
# the command line makensis.exe. This property will be set to 1.
!ifndef ARM64
  !define ARM64 0
!else
  !if ${ARM64} > 0
    !if ${WIN64} < 1
      !define /redef WIN64 1
    !endif
  !endif
!endif

# if you don't want to include libgcc_s_sjlj-1.dll in the package, use the
# switch /DINCLUDE_LIBGCC=0 on the command line makensis.exe.
!ifndef INCLUDE_LIBGCC
  !define INCLUDE_LIBGCC 1
!endif

# Get version numbers
!getdllversion "${MNVSRC}\gmnv_ole.exe" MNVVer_
!echo "MNV version MAJOR=${MNVVer_1} MINOR=${MNVVer_2} PATCHLEVEL=${MNVVer_3}"

!ifndef VER_MAJOR
  !define VER_MAJOR  ${MNVVer_1}
!endif
!ifndef VER_MINOR
  !define VER_MINOR  ${MNVVer_2}
!endif
!ifndef PATCHLEVEL
  !define PATCHLEVEL ${MNVVer_3}
!endif

# ----------- No configurable settings below this line -----------

##########################################################
# Installer Attributes, Including headers, Plugins and etc. 

CRCCheck force

SetCompressor /SOLID lzma
SetCompressorDictSize 64
SetDatablockOptimize on

!if ${HAVE_UPX}
  !packhdr temp.dat "upx.exe --best --compress-icons=1 temp.dat"
!endif

RequestExecutionLevel highest
ManifestDPIAware true
# https://github.com/NSIS-Dev/nsis/blob/691211035c2aaaebe8fbca48ee02d4de93594a52/Docs/src/attributes.but#L292
ManifestDPIAwareness "PerMonitorV2,System"
ManifestSupportedOS \
    {35138b9a-5d96-4fbd-8e2d-a2440225f93a} /* WinNT 6.1 */ \
    {4a2f28e3-53b9-4441-ba9c-d69d4a4a6e38} /* WinNT 6.2 */ \
    {1f676c76-80e1-4239-95bb-83d0f6d0da78} /* WinNT 6.3 */ \
    {8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a} /* WinNT 10/11 */

!define PRODUCT         "MNV ${VER_MAJOR}.${VER_MINOR}"
!define UNINST_REG_KEY  "Software\Microsoft\Windows\CurrentVersion\Uninstall"
!define UNINST_REG_KEY_MNV  "${UNINST_REG_KEY}\${PRODUCT}"

!if ${WIN64}
  !define BIT 64
# This adds '\MNV' to the user choice automagically.  The actual value is
# obtained below with CheckOldMNV.
  !define DEFAULT_INSTDIR "$PROGRAMFILES64\MNV"
  !if ${ARM64}
    Name "${PRODUCT} (ARM64)"
  !else
    Name "${PRODUCT} (x64)"
  !endif
!else
  !define BIT 32
  !define DEFAULT_INSTDIR "$PROGRAMFILES\MNV"
  Name "${PRODUCT}"
!endif

OutFile gmnv${VER_MAJOR}${VER_MINOR}.exe
InstallDir ${DEFAULT_INSTDIR}
BrandingText "MNV - the text editor"

# Types of installs we can perform:
InstType $(str_type_typical)
InstType $(str_type_minimal)
InstType $(str_type_full)

SilentInstall normal

##########################################################
# Version resources

VIFileVersion ${VER_MAJOR}.${VER_MINOR}.${PATCHLEVEL}.0
VIProductVersion ${VER_MAJOR}.${VER_MINOR}.${PATCHLEVEL}.0
VIAddVersionKey /LANG=0 "ProductName" "MNV"
VIAddVersionKey /LANG=0 "CompanyName" "The MNV Project"
VIAddVersionKey /LANG=0 "LegalTrademarks" "MNV"
VIAddVersionKey /LANG=0 "LegalCopyright" "Copyright (C) 1996"
VIAddVersionKey /LANG=0 "FileDescription" \
    "MNV is not Vim - A Text Editor"
VIAddVersionKey /LANG=0 "ProductVersion" \
    "${VER_MAJOR}.${VER_MINOR}.${PATCHLEVEL}.0"
VIAddVersionKey /LANG=0 "FileVersion" \
    "${VER_MAJOR}.${VER_MINOR}.${PATCHLEVEL}.0"

##########################################################
# including headers

!include "Library.nsh"		; for DLL install
!include "LogicLib.nsh"
!include "MUI2.nsh"		; new user interface
!include "nsDialogs.nsh"
!include "Sections.nsh"		; for section control
!include "x64.nsh"

!include .\auxiliary.nsh	; helper file

##########################################################
# MUI2 settings

!define MUI_ABORTWARNING
!define MUI_UNABORTWARNING

!define MUI_ICON   "icons\in_mnv_32bpp.ico"
!define MUI_UNICON "icons\un_mnv_32bpp.ico"

# Show all languages, despite user's codepage:
!define MUI_LANGDLL_ALLLANGUAGES
# Always show dialog choice language
#!define MUI_LANGDLL_ALWAYSSHOW
!define MUI_LANGDLL_REGISTRY_ROOT	"HKCU"
!define MUI_LANGDLL_REGISTRY_KEY	"Software\MNV"
!define MUI_LANGDLL_REGISTRY_VALUENAME  "Installer Language"

!define MUI_WELCOMEFINISHPAGE_BITMAP	"icons\in_welcome.bmp"
!define MUI_UNWELCOMEFINISHPAGE_BITMAP	"icons\un_welcome.bmp"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP		"icons\in_header.bmp"
!define MUI_HEADERIMAGE_UNBITMAP	"icons\un_header.bmp"

!define MUI_WELCOMEFINISHPAGE_BITMAP_STRETCH	"AspectFitHeight"
!define MUI_UNWELCOMEFINISHPAGE_BITMAP_STRETCH  "AspectFitHeight"
!define MUI_HEADERIMAGE_BITMAP_STRETCH		"AspectFitHeight"
!define MUI_HEADERIMAGE_UNBITMAP_STRETCH	"AspectFitHeight"

!define MUI_COMPONENTSPAGE_SMALLDESC
!define MUI_LICENSEPAGE_CHECKBOX
!define MUI_FINISHPAGE_SHOWREADME
!define MUI_FINISHPAGE_SHOWREADME_TEXT		$(str_show_readme)
!define MUI_FINISHPAGE_SHOWREADME_FUNCTION	LaunchApplication

# Installer pages:
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE $(page_lic_file)
!insertmacro MUI_PAGE_COMPONENTS
Page custom SetCustom ValidateCustom
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_NOREBOOTSUPPORT
!insertmacro MUI_PAGE_FINISH

# Uninstaller pages:
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_COMPONENTS
!insertmacro MUI_UNPAGE_INSTFILES
!define MUI_FINISHPAGE_NOREBOOTSUPPORT
!insertmacro MUI_UNPAGE_FINISH

##########################################################
# Languages Files

!insertmacro MUI_RESERVEFILE_LANGDLL
!include "lang\english.nsi"

# Include support for other languages:
!if ${HAVE_MULTI_LANG}
  !include "lang\danish.nsi"
  !include "lang\dutch.nsi"
  !include "lang\german.nsi"
  !include "lang\greek.nsi"
  !include "lang\italian.nsi"
  !include "lang\japanese.nsi"
  !include "lang\portuguesebr.nsi"
  !include "lang\russian.nsi"
  !include "lang\serbian.nsi"
  !include "lang\simpchinese.nsi"
  !include "lang\swedish.nsi"
  !include "lang\tradchinese.nsi"
  !include "lang\turkish.nsi"
!endif

##########################################################
# Global variables
Var mnv_dialog
Var mnv_nsd_compat
Var mnv_nsd_keymap
Var mnv_nsd_mouse
Var mnv_compat_stat
Var mnv_keymap_stat
Var mnv_mouse_stat

##########################################################
# Reserve files
ReserveFile ${MNVSRC}\installw32.exe

##########################################################
# Functions

# Check if MNV is already installed.
# return: Installed directory. If not found, it will be empty.
Function CheckOldMNV
  Push $0
  Push $R0
  Push $R1
  Push $R2

  ${If} ${RunningX64}
    SetRegView 64
  ${EndIf}

  ClearErrors
  StrCpy $0 ""   ; Installed directory
  StrCpy $R0 0   ; Sub-key index
  StrCpy $R1 ""  ; Sub-key
  ${Do}
    # Enumerate the sub-key:
    EnumRegKey $R1 HKLM ${UNINST_REG_KEY} $R0

    # Stop if no more sub-key:
    ${If} ${Errors}
    ${OrIf} $R1 == ""
      ${ExitDo}
    ${EndIf}

    # Move to the next sub-key:
    IntOp $R0 $R0 + 1

    # Check if the key is MNV uninstall key or not:
    StrCpy $R2 $R1 4
    ${If} $R2 S!= "MNV "
      ${Continue}
    ${EndIf}

    # Verifies required sub-keys:
    ReadRegStr $R2 HKLM "${UNINST_REG_KEY}\$R1" "DisplayName"
    ${If} ${Errors}
    ${OrIf} $R2 == ""
      ${Continue}
    ${EndIf}

    ReadRegStr $R2 HKLM "${UNINST_REG_KEY}\$R1" "UninstallString"
    ${If} ${Errors}
    ${OrIf} $R2 == ""
      ${Continue}
    ${EndIf}

    # Found
    Push $R2
    call GetParent
    call GetParent
    Pop $0  ; MNV directory
    ${ExitDo}

  ${Loop}

  ${If} ${RunningX64}
    SetRegView lastused
  ${EndIf}

  Pop $R2
  Pop $R1
  Pop $R0
  Exch $0  ; put $0 on top of stack, restore $0 to original value
FunctionEnd

Function LaunchApplication
  SetOutPath $0
  !if ${HAVE_NLS}
    ShellExecAsUser::ShellExecAsUser "" "$0\gmnv.exe" \
	'-R "$0\$(mnv_readme_file)"'
  !else
    ShellExecAsUser::ShellExecAsUser "" "$0\gmnv.exe" '-R "$0\README.txt"'
  !endif
FunctionEnd

##########################################################
# Installer Functions and Sections

Section "$(str_section_old_ver)" id_section_old_ver
  SectionIn 1 2 3 RO

  # run the install program to check for already installed versions
  SetOutPath $TEMP
  File /oname=install.exe ${MNVSRC}\installw32.exe
  DetailPrint "$(str_msg_uninstalling)"
  ${Do}
    nsExec::Exec "$TEMP\install.exe -uninstall-check"
    Pop $3

    call CheckOldMNV
    Pop $3
    ${If} $3 == ""
      ${ExitDo}
    ${Else}
      # It seems that the old version is still remaining.
      # TODO: Should we show a warning and run the uninstaller again?

      ${ExitDo}  ; Just ignore for now.
    ${EndIf}
  ${Loop}
  Delete $TEMP\install.exe
  Delete $TEMP\mnvini.ini   ; install.exe creates this, but we don't need it.

  # We may have been put to the background when uninstall did something.
  BringToFront
SectionEnd

##########################################################
Section "$(str_section_exe)" id_section_exe
  SectionIn 1 2 3 RO

  # we need also this here if the user changes the instdir
  StrCpy $0 "$INSTDIR\mnv${VER_MAJOR}${VER_MINOR}"

  SetOutPath $0
  File /oname=gmnv.exe ${MNVSRC}\gmnv_ole.exe
  !if /FileExists "${MNVSRC}\mnv${BIT}.dll"
    File ${MNVSRC}\mnv${BIT}.dll
  !endif
  File /oname=install.exe ${MNVSRC}\installw32.exe
  File /oname=uninstall.exe ${MNVSRC}\uninstallw32.exe
  File ${MNVSRC}\mnvrun.exe
  File /oname=tee.exe ${MNVSRC}\teew32.exe
  File /oname=xxd.exe ${MNVSRC}\xxdw32.exe
  File ..\mnvtutor.bat
  File ..\README.txt
  File /oname=LICENSE.txt ..\LICENSE
  File ..\uninstall.txt
  File ${MNVRT}\*.mnv

  !if /FileExists "${MNVTOOLS}\diff.exe"
    File ${MNVTOOLS}\diff.exe
  !endif
  !if /FileExists "${MNVTOOLS}\winpty${BIT}.dll"
    File ${MNVTOOLS}\winpty${BIT}.dll
  !endif
  !if /FileExists "${MNVTOOLS}\winpty-agent.exe"
    File ${MNVTOOLS}\winpty-agent.exe
  !endif
  !if /FileExists "${MNVTOOLS}\libsodium.dll"
    File ${MNVTOOLS}\libsodium.dll
  !endif

  SetOutPath $0\colors
  File /r ${MNVRT}\colors\*.*

  SetOutPath $0\compiler
  File ${MNVRT}\compiler\*.*

  SetOutPath $0\doc
  File /x uganda.nsis.txt ${MNVRT}\doc\*.txt
  File ${MNVRT}\doc\tags

  SetOutPath $0\ftplugin
  File ${MNVRT}\ftplugin\*.*

  SetOutPath $0\indent
  File ${MNVRT}\indent\README.txt
  File ${MNVRT}\indent\*.mnv

  SetOutPath $0\keymap
  File ${MNVRT}\keymap\README.txt
  File ${MNVRT}\keymap\*.mnv

  SetOutPath $0\macros
  File /r /x *.info ${MNVRT}\macros\*.*

  SetOutPath $0\pack
  File /r ${MNVRT}\pack\*.*

  SetOutPath $0\plugin
  File ${MNVRT}\plugin\*.*

  SetOutPath $0\autoload
  File /r ${MNVRT}\autoload\*.*

  SetOutPath $0\import\dist
  File ${MNVRT}\import\dist\*.*

  SetOutPath $0\bitmaps
  File ${MNVSRC}\mnv.ico

  SetOutPath $0\syntax
  File /r /x testdir /x generator /x Makefile ${MNVRT}\syntax\*.*

  SetOutPath $0\spell
  File ${MNVRT}\spell\*.txt
  File ${MNVRT}\spell\*.mnv
  File ${MNVRT}\spell\*.spl
  File ${MNVRT}\spell\*.sug

  SetOutPath $0\tools
  File ${MNVRT}\tools\*.*

  SetOutPath $0\tutor
  File /r /x *.info ${MNVRT}\tutor\*.*
SectionEnd

##########################################################
Section "$(str_section_console)" id_section_console
  SectionIn 1 3

  SetOutPath $0
  File /oname=mnv.exe ${MNVSRC}\mnvw32.exe
  StrCpy $2 "$2 mnv view mnvdiff"
SectionEnd

##########################################################
Section "$(str_section_batch)" id_section_batch
  SectionIn 3

  StrCpy $1 "$1 -create-batfiles $2"
SectionEnd

##########################################################
SectionGroup $(str_group_icons) id_group_icons
  Section "$(str_section_desktop)" id_section_desktop
    SectionIn 1 3

    StrCpy $1 "$1 -install-icons"
  SectionEnd

  Section "$(str_section_start_menu)" id_section_startmenu
    SectionIn 1 3

    StrCpy $1 "$1 -add-start-menu"
  SectionEnd
SectionGroupEnd

##########################################################
Section "$(str_section_edit_with)" id_section_editwith
  SectionIn 1 3

  SetOutPath $0

  ${If} ${RunningX64}
    # Install 64-bit gmnvext.dll into the GmnvExt64 directory.
    SetOutPath $0\GmnvExt64
    ClearErrors
    !define LIBRARY_SHELL_EXTENSION
    !define LIBRARY_X64
    !insertmacro InstallLib DLL NOTSHARED REBOOT_NOTPROTECTED \
	"${MNVSRC}\GmnvExt\gmnvext64.dll" "$0\GmnvExt64\gmnvext.dll" "$0"
    !undef LIBRARY_X64
    !undef LIBRARY_SHELL_EXTENSION
  ${EndIf}

  # Install 32-bit gmnvext.dll into the GmnvExt32 directory.
  SetOutPath $0\GmnvExt32
  ClearErrors
  !define LIBRARY_SHELL_EXTENSION
  !insertmacro InstallLib DLL NOTSHARED REBOOT_NOTPROTECTED \
      "${MNVSRC}\GmnvExt\gmnvext.dll" "$0\GmnvExt32\gmnvext.dll" "$0"
  !undef LIBRARY_SHELL_EXTENSION

  # We don't have a separate entry for the "Open With..." menu, assume
  # the user wants either both or none.
  StrCpy $1 "$1 -install-popup -install-openwith"
SectionEnd

##########################################################
Section "$(str_section_mnv_rc)" id_section_mnvrc
  SectionIn 1 3

  StrCpy $1 "$1 -create-mnvrc"

  ${If} ${RunningX64}
    SetRegView 64
  ${EndIf}
  WriteRegStr HKLM "${UNINST_REG_KEY_MNV}" "mnv_compat"   "$mnv_compat_stat"
  WriteRegStr HKLM "${UNINST_REG_KEY_MNV}" "mnv_keyremap" "$mnv_keymap_stat"
  WriteRegStr HKLM "${UNINST_REG_KEY_MNV}" "mnv_mouse"    "$mnv_mouse_stat"
  ${If} ${RunningX64}
    SetRegView lastused
  ${EndIf}

  ${If} $mnv_compat_stat == "vi"
    StrCpy $1 "$1 -mnvrc-compat vi"
  ${ElseIf} $mnv_compat_stat == "mnv"
    StrCpy $1 "$1 -mnvrc-compat mnv"
  ${ElseIf} $mnv_compat_stat == "defaults"
    StrCpy $1 "$1 -mnvrc-compat defaults"
  ${Else}
    StrCpy $1 "$1 -mnvrc-compat all"
  ${EndIf}

  ${If} $mnv_keymap_stat == "default"
    StrCpy $1 "$1 -mnvrc-remap no"
  ${Else}
    StrCpy $1 "$1 -mnvrc-remap win"
  ${EndIf}

  ${If} $mnv_mouse_stat == "default"
    StrCpy $1 "$1 -mnvrc-behave default"
  ${ElseIf} $mnv_mouse_stat == "windows"
    StrCpy $1 "$1 -mnvrc-behave mswin"
  ${Else}
    StrCpy $1 "$1 -mnvrc-behave unix"
  ${EndIf}
SectionEnd

##########################################################
SectionGroup $(str_group_plugin) id_group_plugin
  Section "$(str_section_plugin_home)" id_section_pluginhome
    SectionIn 1 3
    # use ShellExecAsUser below instead
    # StrCpy $1 "$1 -create-directories home"
  SectionEnd

  Section "$(str_section_plugin_mnv)" id_section_pluginmnv
    SectionIn 3
    StrCpy $1 "$1 -create-directories mnv"
  SectionEnd
SectionGroupEnd

##########################################################
!if ${HAVE_NLS}
  Section "$(str_section_nls)" id_section_nls
    SectionIn 1 3

    SetOutPath $INSTDIR
    !if /FileExists "..\lang\README.*.txt"
      File ..\lang\README.*.txt
      CopyFiles /SILENT /FILESONLY $INSTDIR\README.$lng_usr.txt \
	  $INSTDIR\mnv${VER_MAJOR}${VER_MINOR}\README.$lng_usr.txt
      Delete $INSTDIR\README.*.txt
    !endif
    StrCpy $R7 0
    !if /FileExists "..\lang\LICENSE.??.txt"
      File ..\lang\LICENSE.??.txt
      IntOp $R7 $R7 + 1
    !endif
    !if /FileExists "..\lang\LICENSE.??_??.txt"
      File ..\lang\LICENSE.??_??.txt
      IntOp $R7 $R7 + 1
    !endif
    IntCmp $R7 0 notcpy notcpy cpy
    cpy:
    CopyFiles /SILENT /FILESONLY $INSTDIR\LICENSE.$lng_usr.txt \
	$INSTDIR\mnv${VER_MAJOR}${VER_MINOR}\LICENSE.$lng_usr.txt
    Delete $INSTDIR\LICENSE.*.txt
    notcpy:

    SetOutPath $0\lang
    File /r /x Makefile ${MNVRT}\lang\*.*
    SetOutPath $0
    !insertmacro InstallLib DLL NOTSHARED REBOOT_NOTPROTECTED \
	"${GETTEXT}\gettext${BIT}\libintl-8.dll" "$0\libintl-8.dll" "$0"
    !insertmacro InstallLib DLL NOTSHARED REBOOT_NOTPROTECTED \
	"${GETTEXT}\gettext${BIT}\libiconv-2.dll" "$0\libiconv-2.dll" "$0"
    # Install libgcc_s_sjlj-1.dll only if it is needed.
    !if ${INCLUDE_LIBGCC}
      !if /FileExists "${GETTEXT}\gettext${BIT}\libgcc_s_sjlj-1.dll"
	!insertmacro InstallLib DLL NOTSHARED REBOOT_NOTPROTECTED \
	    "${GETTEXT}\gettext${BIT}\libgcc_s_sjlj-1.dll" \
	    "$0\libgcc_s_sjlj-1.dll" "$0"
      !endif
    !endif

    ${If} ${SectionIsSelected} ${id_section_editwith}
      ${If} ${RunningX64}
	# Install DLLs for 64-bit gmnvext.dll into the GmnvExt64 directory.
	SetOutPath $0\GmnvExt64
	ClearErrors
	!define LIBRARY_X64
	!insertmacro InstallLib DLL NOTSHARED REBOOT_NOTPROTECTED \
	    "${GETTEXT}\gettext64\libintl-8.dll" \
	    "$0\GmnvExt64\libintl-8.dll" "$0\GmnvExt64"
	!insertmacro InstallLib DLL NOTSHARED REBOOT_NOTPROTECTED \
	    "${GETTEXT}\gettext64\libiconv-2.dll" \
	    "$0\GmnvExt64\libiconv-2.dll" "$0\GmnvExt64"
	!undef LIBRARY_X64
      ${EndIf}

      # Install DLLs for 32-bit gmnvext.dll into the GmnvExt32 directory.
      SetOutPath $0\GmnvExt32
      ClearErrors
      !insertmacro InstallLib DLL NOTSHARED REBOOT_NOTPROTECTED \
	  "${GETTEXT}\gettext32\libintl-8.dll" \
	  "$0\GmnvExt32\libintl-8.dll" "$0\GmnvExt32"
      !insertmacro InstallLib DLL NOTSHARED REBOOT_NOTPROTECTED \
	  "${GETTEXT}\gettext32\libiconv-2.dll" \
	  "$0\GmnvExt32\libiconv-2.dll" "$0\GmnvExt32"
      # Install libgcc_s_sjlj-1.dll only if it is needed.
      !if ${INCLUDE_LIBGCC}
	!if /FileExists "${GETTEXT}\gettext32\libgcc_s_sjlj-1.dll"
	    !insertmacro InstallLib DLL NOTSHARED REBOOT_NOTPROTECTED \
		"${GETTEXT}\gettext32\libgcc_s_sjlj-1.dll" \
		"$0\GmnvExt32\libgcc_s_sjlj-1.dll" "$0\GmnvExt32"
	!endif
      !endif
    ${EndIf}
  SectionEnd
!endif

##########################################################
Section -call_install_exe
  SetOutPath $0
  DetailPrint "$(str_msg_registering)"
  nsExec::Exec "$0\install.exe $1"
  Pop $3

  ${If} ${SectionIsSelected} ${id_section_pluginhome}
    ReadEnvStr $3 "COMSPEC"
    Call GetHomeDir
    Pop $4
    ShellExecAsUser::ShellExecAsUser "" "$3" '/c "cd /d "$4" & mkdir mnvfiles \
	& cd mnvfiles & mkdir colors compiler doc ftdetect ftplugin indent \
	keymap plugin syntax"' SW_HIDE
  ${EndIf}
SectionEnd

##########################################################
Section -post
  # Get estimated install size
  SectionGetSize ${id_section_exe} $3
  ${If} ${SectionIsSelected} ${id_section_console}
    SectionGetSize ${id_section_console} $4
    IntOp $3 $3 + $4
  ${EndIf}
  ${If} ${SectionIsSelected} ${id_section_editwith}
    SectionGetSize ${id_section_editwith} $4
    IntOp $3 $3 + $4
  ${EndIf}
  !if ${HAVE_NLS}
    ${If} ${SectionIsSelected} ${id_section_nls}
      SectionGetSize ${id_section_nls} $4
      IntOp $3 $3 + $4
    ${EndIf}
  !endif

  # Register EstimatedSize and AllowSilent.
  # Other information will be set by the install.exe (dosinst.c).
  ${If} ${RunningX64}
    SetRegView 64
  ${EndIf}
  WriteRegDWORD HKLM "${UNINST_REG_KEY_MNV}" "EstimatedSize" $3
  WriteRegDWORD HKLM "${UNINST_REG_KEY_MNV}" "AllowSilent" 1
  ${If} ${RunningX64}
    SetRegView lastused
  ${EndIf}

  # Store the selections to the registry.
  ${If} ${RunningX64}
    SetRegView 64
  ${EndIf}
  !insertmacro SaveSectionSelection ${id_section_console}    "select_console"
  !insertmacro SaveSectionSelection ${id_section_batch}      "select_batch"
  !insertmacro SaveSectionSelection ${id_section_desktop}    "select_desktop"
  !insertmacro SaveSectionSelection ${id_section_startmenu}  "select_startmenu"
  !insertmacro SaveSectionSelection ${id_section_editwith}   "select_editwith"
  !insertmacro SaveSectionSelection ${id_section_mnvrc}      "select_mnvrc"
  !insertmacro SaveSectionSelection ${id_section_pluginhome} \
      "select_pluginhome"
  !insertmacro SaveSectionSelection ${id_section_pluginmnv}  "select_pluginmnv"
  !if ${HAVE_NLS}
    !insertmacro SaveSectionSelection ${id_section_nls}      "select_nls"
  !endif
  ${If} ${RunningX64}
    SetRegView lastused
  ${EndIf}

  BringToFront
SectionEnd

##########################################################
Function .onInit
  !if ${HAVE_MULTI_LANG}
    # Select a language (or read from the registry).
    !insertmacro MUI_LANGDLL_DISPLAY
  !endif

  !if ${HAVE_NLS}
    call GetUserLocale
  !endif

  ${If} $INSTDIR == ${DEFAULT_INSTDIR}
    # Check $MNV
    ReadEnvStr $3 "MNV"
    ${If} $3 != ""
      StrCpy $INSTDIR $3
    ${EndIf}
  ${EndIf}

  call CheckOldMNV
  Pop $3
  ${If} $3 == ""
    # No old versions of MNV found. Unselect and hide the section.
    !insertmacro UnselectSection ${id_section_old_ver}
    SectionSetInstTypes ${id_section_old_ver} 0
    SectionSetText ${id_section_old_ver} ""
  ${Else}
    ${If} $INSTDIR == ${DEFAULT_INSTDIR}
      StrCpy $INSTDIR $3
    ${EndIf}
  ${EndIf}

  ${If} ${RunningX64}
    SetRegView 64
  ${EndIf}
  # Load the selections from the registry (if any).
  !insertmacro LoadSectionSelection ${id_section_console}    "select_console"
  !insertmacro LoadSectionSelection ${id_section_batch}      "select_batch"
  !insertmacro LoadSectionSelection ${id_section_desktop}    "select_desktop"
  !insertmacro LoadSectionSelection ${id_section_startmenu}  "select_startmenu"
  !insertmacro LoadSectionSelection ${id_section_editwith}   "select_editwith"
  !insertmacro LoadSectionSelection ${id_section_mnvrc}      "select_mnvrc"
  !insertmacro LoadSectionSelection ${id_section_pluginhome} \
      "select_pluginhome"
  !insertmacro LoadSectionSelection ${id_section_pluginmnv}  "select_pluginmnv"
  !if ${HAVE_NLS}
    !insertmacro LoadSectionSelection ${id_section_nls}      "select_nls"
  !endif
  # Load the default _mnvrc settings from the registry (if any).
  !insertmacro LoadDefaultMNVrc $mnv_compat_stat "mnv_compat"   "all"
  !insertmacro LoadDefaultMNVrc $mnv_keymap_stat "mnv_keyremap" "default"
  !insertmacro LoadDefaultMNVrc $mnv_mouse_stat  "mnv_mouse"    "default"
  ${If} ${RunningX64}
    SetRegView lastused
  ${EndIf}

  # User variables:
  # $0 - holds the directory the executables are installed to
  # $1 - holds the parameters to be passed to install.exe.  Starts with OLE
  #      registration (since a non-OLE gmnv will not complain, and we want to
  #      always register an OLE gmnv).
  # $2 - holds the names to create batch files for
  StrCpy $0 "$INSTDIR\mnv${VER_MAJOR}${VER_MINOR}"
  StrCpy $1 "-register-OLE"
  StrCpy $2 "gmnv emnv gview gmnvdiff mnvtutor"
FunctionEnd

Function .onInstSuccess
  WriteUninstaller mnv${VER_MAJOR}${VER_MINOR}\uninstall-gui.exe
FunctionEnd

Function .onInstFailed
  MessageBox MB_OK|MB_ICONEXCLAMATION "$(str_msg_install_fail)" /SD IDOK
FunctionEnd

##########################################################
Function SetCustom
  # Display the _mnvrc setting dialog using nsDialogs.

  # Check if a _mnvrc should be created
  ${IfNot} ${SectionIsSelected} ${id_section_mnvrc}
    Abort
  ${EndIf}

  !insertmacro MUI_HEADER_TEXT \
      $(str_mnvrc_page_title) $(str_mnvrc_page_subtitle)

  nsDialogs::Create 1018
  Pop $mnv_dialog

  ${If} $mnv_dialog == error
    Abort
  ${EndIf}

  ${If} ${RunningX64}
    SetRegView 64
  ${EndIf}

  GetFunctionAddress $3 ValidateCustom
  nsDialogs::OnBack $3

  # 1st group - Compatibility
  ${NSD_CreateGroupBox} 0u 0u 296u 44u $(str_msg_compat_title)
  Pop $3

  ${NSD_CreateLabel} 16u 14u 269u 10u $(str_msg_compat_desc)
  Pop $3
  ${NSD_CreateDropList} 42u 26u 237u 13u ""
  Pop $mnv_nsd_compat
  ${NSD_CB_AddString} $mnv_nsd_compat $(str_msg_compat_vi)
  ${NSD_CB_AddString} $mnv_nsd_compat $(str_msg_compat_mnv)
  ${NSD_CB_AddString} $mnv_nsd_compat $(str_msg_compat_defaults)
  ${NSD_CB_AddString} $mnv_nsd_compat $(str_msg_compat_all)

  ${If} $mnv_compat_stat == "defaults"
    StrCpy $4 2
  ${ElseIf} $mnv_compat_stat == "mnv"
    StrCpy $4 1
  ${ElseIf} $mnv_compat_stat == "vi"
    StrCpy $4 0
  ${Else} ; default
    StrCpy $4 3
  ${EndIf}
  ${NSD_CB_SetSelectionIndex} $mnv_nsd_compat $4

  # 2nd group - Key remapping
  ${NSD_CreateGroupBox} 0u 48u 296u 44u $(str_msg_keymap_title)
  Pop $3

  ${NSD_CreateLabel} 16u 62u 269u 10u $(str_msg_keymap_desc)
  Pop $3
  ${NSD_CreateDropList} 42u 74u 236u 13u ""
  Pop $mnv_nsd_keymap
  ${NSD_CB_AddString} $mnv_nsd_keymap $(str_msg_keymap_default)
  ${NSD_CB_AddString} $mnv_nsd_keymap $(str_msg_keymap_windows)

  ${If} $mnv_keymap_stat == "windows"
    StrCpy $4 1
  ${Else} ; default
    StrCpy $4 0
  ${EndIf}
  ${NSD_CB_SetSelectionIndex} $mnv_nsd_keymap $4

  # 3rd group - Mouse behavior
  ${NSD_CreateGroupBox} 0u 95u 296u 44u $(str_msg_mouse_title)
  Pop $3

  ${NSD_CreateLabel} 16u 108u 269u 10u $(str_msg_mouse_desc)
  Pop $3
  ${NSD_CreateDropList} 42u 121u 237u 13u ""
  Pop $mnv_nsd_mouse
  ${NSD_CB_AddString} $mnv_nsd_mouse $(str_msg_mouse_default)
  ${NSD_CB_AddString} $mnv_nsd_mouse $(str_msg_mouse_windows)
  ${NSD_CB_AddString} $mnv_nsd_mouse $(str_msg_mouse_unix)

  ${If} $mnv_mouse_stat == "xterm"
    StrCpy $4 2
  ${ElseIf} $mnv_mouse_stat == "windows"
    StrCpy $4 1
  ${Else} ; default
    StrCpy $4 0
  ${EndIf}
  ${NSD_CB_SetSelectionIndex} $mnv_nsd_mouse $4

  ${If} ${RunningX64}
    SetRegView lastused
  ${EndIf}

  nsDialogs::Show
FunctionEnd

Function ValidateCustom
  ${NSD_CB_GetSelectionIndex} $mnv_nsd_compat $3
  ${If} $3 = 0
    StrCpy $mnv_compat_stat "vi"
  ${ElseIf} $3 = 1
    StrCpy $mnv_compat_stat "mnv"
  ${ElseIf} $3 = 2
    StrCpy $mnv_compat_stat "defaults"
  ${Else}
    StrCpy $mnv_compat_stat "all"
  ${EndIf}

  ${NSD_CB_GetSelectionIndex} $mnv_nsd_keymap $3
  ${If} $3 = 0
    StrCpy $mnv_keymap_stat "default"
  ${Else}
    StrCpy $mnv_keymap_stat "windows"
  ${EndIf}

  ${NSD_CB_GetSelectionIndex} $mnv_nsd_mouse $3
  ${If} $3 = 0
    StrCpy $mnv_mouse_stat "default"
  ${ElseIf} $3 = 1
    StrCpy $mnv_mouse_stat "windows"
  ${Else}
    StrCpy $mnv_mouse_stat "xterm"
  ${EndIf}
FunctionEnd

##########################################################
# Description for Installer Sections

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${id_section_old_ver}   $(str_desc_old_ver)
  !insertmacro MUI_DESCRIPTION_TEXT ${id_section_exe}	    $(str_desc_exe)
  !insertmacro MUI_DESCRIPTION_TEXT ${id_section_console}   $(str_desc_console)
  !insertmacro MUI_DESCRIPTION_TEXT ${id_section_batch}	    $(str_desc_batch)
  !insertmacro MUI_DESCRIPTION_TEXT ${id_group_icons}	    $(str_desc_icons)
  !insertmacro MUI_DESCRIPTION_TEXT ${id_section_desktop}   $(str_desc_desktop)
  !insertmacro MUI_DESCRIPTION_TEXT ${id_section_startmenu} \
      $(str_desc_start_menu)
  !insertmacro MUI_DESCRIPTION_TEXT ${id_section_editwith}  \
      $(str_desc_edit_with)
  !insertmacro MUI_DESCRIPTION_TEXT ${id_section_mnvrc}	    $(str_desc_mnv_rc)
  !insertmacro MUI_DESCRIPTION_TEXT ${id_group_plugin}	    $(str_desc_plugin)
  !insertmacro MUI_DESCRIPTION_TEXT ${id_section_pluginhome} \
      $(str_desc_plugin_home)
  !insertmacro MUI_DESCRIPTION_TEXT ${id_section_pluginmnv} \
      $(str_desc_plugin_mnv)
  !if ${HAVE_NLS}
    !insertmacro MUI_DESCRIPTION_TEXT ${id_section_nls}	    $(str_desc_nls)
  !endif
!insertmacro MUI_FUNCTION_DESCRIPTION_END


##########################################################
# Uninstaller Functions and Sections

Function un.onInit
  !if ${HAVE_MULTI_LANG}
    # Get the language from the registry.
    !insertmacro MUI_UNGETLANGUAGE
  !endif
FunctionEnd

Section "un.$(str_unsection_register)" id_unsection_register
  SectionIn RO

  # Apparently $INSTDIR is set to the directory where the uninstaller is
  # created.  Thus the "mnv91" directory is included in it.
  StrCpy $0 "$INSTDIR"

  # delete the context menu entry and batch files
  DetailPrint "$(str_msg_unregistering)"
  nsExec::Exec "$0\uninstall.exe -nsis"
  Pop $3

  # We may have been put to the background when uninstall did something.
  BringToFront

  # Delete the installer language setting.
  DeleteRegKey ${MUI_LANGDLL_REGISTRY_ROOT} ${MUI_LANGDLL_REGISTRY_KEY}
SectionEnd

Section "un.$(str_unsection_exe)" id_unsection_exe
  StrCpy $0 "$INSTDIR"

  # Delete gettext and iconv DLLs
  ${If} ${FileExists} "$0\libiconv-2.dll"
    !insertmacro UninstallLib DLL NOTSHARED REBOOT_NOTPROTECTED \
	"$0\libiconv-2.dll"
  ${EndIf}
  ${If} ${FileExists} "$0\libintl-8.dll"
    !insertmacro UninstallLib DLL NOTSHARED REBOOT_NOTPROTECTED \
	"$0\libintl-8.dll"
  ${EndIf}
  ${If} ${FileExists} "$0\libgcc_s_sjlj-1.dll"
    !insertmacro UninstallLib DLL NOTSHARED REBOOT_NOTPROTECTED \
	"$0\libgcc_s_sjlj-1.dll"
  ${EndIf}

  # Delete other DLLs
  Delete /REBOOTOK $0\*.dll

  # Delete 64-bit GmnvExt
  ${If} ${RunningX64}
    !define LIBRARY_X64
    ${If} ${FileExists} "$0\GmnvExt64\gmnvext.dll"
      !insertmacro UninstallLib DLL NOTSHARED REBOOT_NOTPROTECTED \
	  "$0\GmnvExt64\gmnvext.dll"
    ${EndIf}
    ${If} ${FileExists} "$0\GmnvExt64\libiconv-2.dll"
      !insertmacro UninstallLib DLL NOTSHARED REBOOT_NOTPROTECTED \
	  "$0\GmnvExt64\libiconv-2.dll"
    ${EndIf}
    ${If} ${FileExists} "$0\GmnvExt64\libintl-8.dll"
      !insertmacro UninstallLib DLL NOTSHARED REBOOT_NOTPROTECTED \
	  "$0\GmnvExt64\libintl-8.dll"
    ${EndIf}
    ${If} ${FileExists} "$0\GmnvExt64\libwinpthread-1.dll"
      !insertmacro UninstallLib DLL NOTSHARED REBOOT_NOTPROTECTED \
	  "$0\GmnvExt64\libwinpthread-1.dll"
    ${EndIf}
    !undef LIBRARY_X64
    RMDir /r $0\GmnvExt64
  ${EndIf}

  # Delete 32-bit GmnvExt
  ${If} ${FileExists} "$0\GmnvExt32\gmnvext.dll"
    !insertmacro UninstallLib DLL NOTSHARED REBOOT_NOTPROTECTED \
	"$0\GmnvExt32\gmnvext.dll"
  ${EndIf}
  ${If} ${FileExists} "$0\GmnvExt32\libiconv-2.dll"
    !insertmacro UninstallLib DLL NOTSHARED REBOOT_NOTPROTECTED \
	"$0\GmnvExt32\libiconv-2.dll"
  ${EndIf}
  ${If} ${FileExists} "$0\GmnvExt32\libintl-8.dll"
    !insertmacro UninstallLib DLL NOTSHARED REBOOT_NOTPROTECTED \
	"$0\GmnvExt32\libintl-8.dll"
  ${EndIf}
  ${If} ${FileExists} "$0\GmnvExt32\libgcc_s_sjlj-1.dll"
    !insertmacro UninstallLib DLL NOTSHARED REBOOT_NOTPROTECTED \
	"$0\GmnvExt32\libgcc_s_sjlj-1.dll"
  ${EndIf}
  RMDir /r $0\GmnvExt32

  ClearErrors
  # Remove everything but *.dll files.  Avoids that
  # a lot remains when gmnvext.dll cannot be deleted.
  RMDir /r $0\autoload
  RMDir /r $0\colors
  RMDir /r $0\compiler
  RMDir /r $0\doc
  RMDir /r $0\ftplugin
  RMDir /r $0\import
  RMDir /r $0\indent
  RMDir /r $0\macros
  RMDir /r $0\pack
  RMDir /r $0\plugin
  RMDir /r $0\spell
  RMDir /r $0\syntax
  RMDir /r $0\tools
  RMDir /r $0\tutor
  RMDir /r $0\lang
  RMDir /r $0\keymap
  RMDir /r $0\bitmaps
  Delete $0\*.exe
  Delete $0\*.bat
  Delete $0\*.mnv
  Delete $0\*.txt

  ${If} ${Errors}
    MessageBox MB_OK|MB_ICONEXCLAMATION $(str_msg_rm_exe_fail) /SD IDOK
  ${EndIf}

  # No error message if the "mnv91" directory can't be removed, the
  # gmnvext.dll may still be there.
  RMDir $0
SectionEnd

# Remove "mnvfiles" directory under the specified directory.
!macro RemoveMNVfiles dir
  ${If} ${FileExists} ${dir}\_mnvinfo
    Delete ${dir}\_mnvinfo
  ${EndIf}
  ${If} ${DirExists} ${dir}\mnvfiles
    RMDir ${dir}\mnvfiles\colors
    RMDir ${dir}\mnvfiles\compiler
    RMDir ${dir}\mnvfiles\doc
    RMDir ${dir}\mnvfiles\ftdetect
    RMDir ${dir}\mnvfiles\ftplugin
    RMDir ${dir}\mnvfiles\indent
    RMDir ${dir}\mnvfiles\keymap
    RMDir ${dir}\mnvfiles\plugin
    RMDir ${dir}\mnvfiles\syntax
    ${If} ${FileExists} ${dir}\mnvfiles\.netrwhist*
      Delete ${dir}\mnvfiles\.netrwhist*
    ${EndIf}
    RMDir ${dir}\mnvfiles
  ${EndIf}
!macroend

SectionGroup "un.$(str_ungroup_plugin)" id_ungroup_plugin
  Section /o "un.$(str_unsection_plugin_home)" id_unsection_plugin_home
  # get the home dir
    Call un.GetHomeDir
    Pop $0

    ${If} $0 != ""
      !insertmacro RemoveMNVfiles $0
    ${EndIf}
  SectionEnd

  Section "un.$(str_unsection_plugin_mnv)" id_unsection_plugin_mnv
    # get the parent dir of the installation
    Push $INSTDIR
    Call un.GetParent
    Pop $0

    # if a plugin dir was created at installation remove it
    !insertmacro RemoveMNVfiles $0
  SectionEnd
SectionGroupEnd

Section "un.$(str_unsection_rootdir)" id_unsection_rootdir
# get the parent dir of the installation
  Push $INSTDIR
  Call un.GetParent
  Pop $0

  ${IfNot} ${Silent}
    Delete $0\_mnvrc
  ${Endif}
  RMDir $0
SectionEnd

##########################################################
# Description for Uninstaller Sections

!insertmacro MUI_UNFUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${id_unsection_register}	\
      $(str_desc_unregister)
  !insertmacro MUI_DESCRIPTION_TEXT ${id_unsection_exe}  $(str_desc_rm_exe)
  !insertmacro MUI_DESCRIPTION_TEXT ${id_ungroup_plugin} $(str_desc_rm_plugin)
  !insertmacro MUI_DESCRIPTION_TEXT ${id_unsection_plugin_home} \
      $(str_desc_rm_plugin_home)
  !insertmacro MUI_DESCRIPTION_TEXT ${id_unsection_plugin_mnv}	\
      $(str_desc_rm_plugin_mnv)
  !insertmacro MUI_DESCRIPTION_TEXT ${id_unsection_rootdir}	\
      $(str_desc_rm_rootdir)
!insertmacro MUI_UNFUNCTION_DESCRIPTION_END

# vi:set ts=8 sw=2 sts=2 tw=79 wm=0 ft=nsis:
