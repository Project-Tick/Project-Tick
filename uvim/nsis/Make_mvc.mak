#
# Makefile for MS Windows for create self-installing exe of MNV.
# 2025-10-03, Restorer, restorer@mail2k.ru
#


# included common tools
!INCLUDE ..\src\auto\nmake\tools.mak

.SUFFIXES :

!IFDEF PROGRAMW6432
ProgFiles=%%PROGRAMFILES(x86)%%
!ELSE
ProgFiles=$(PROGRAMFILES)
!ENDIF

!IFDEF MNVSRC
MKNSISFLAGS = /D"MNVSRC=$(MNVSRC)"
!ENDIF

!IFDEF MNVRT
MKNSISFLAGS = $(MKNSISFLAGS) /D"MNVRT=$(MNVRT)"
!ENDIF

!IFDEF MNVTOOLS
MKNSISFLAGS = $(MKNSISFLAGS) /D"MNVTOOLS=$(MNVTOOLS)"
!ENDIF

!IFDEF GETTEXT
MKNSISFLAGS = $(MKNSISFLAGS) /D"GETTEXT=$(GETTEXT)"
!ENDIF

!IFDEF HAVE_UPX
MKNSISFLAGS = $(MKNSISFLAGS) /DHAVE_UPX=$(HAVE_UPX)
!ENDIF

!IFDEF HAVE_NLS
MKNSISFLAGS = $(MKNSISFLAGS) /DHAVE_NLS=$(HAVE_NLS)
!ENDIF

!IFDEF HAVE_MULTI_LANG
MKNSISFLAGS = $(MKNSISFLAGS) /DHAVE_MULTI_LANG=$(HAVE_MULTI_LANG)
!ENDIF

!IFDEF WIN64
MKNSISFLAGS = $(MKNSISFLAGS) /DWIN64=$(WIN64)
!ENDIF

!IFDEF ARM64
MKNSISFLAGS = $(MKNSISFLAGS) /DARM64=$(ARM64)
!ENDIF

!IFDEF INCLUDE_LIBGCC
MKNSISFLAGS = $(MKNSISFLAGS) /DINCLUDE_LIBGCC=$(INCLUDE_LIBGCC)
!ENDIF

!IFDEF X
XX = /X"$(X:;=" /X")"
!ENDIF

# If necessary, correct the full path of the NSIS compiler in the next line.
# Please do not enclose the path in quotation marks.
MKNSIS = $(ProgFiles)\NSIS\makensis.exe

MKNSISFLAGS = /INPUTCHARSET UTF8 $(MKNSISFLAGS)


all : makeinst

makeinst : prepare
	^"$(MKNSIS)" $(MKNSISFLAGS) gmnv.nsi $(XX)

prepare : unzipicons license rename

unzipicons : icons.zip
	@ if exist %|fF\nul $(RD) %|fF
	@ $(PS) $(PSFLAGS) \
		Add-Type -AssemblyName 'System.IO.Compression.FileSystem'; \
		[System.IO.Compression.ZipFile]::ExtractToDirectory('$**', '.')

license : ..\lang\LICENSE.*.txt ..\LICENSE
	!@ $(PS) $(PSFLAGS) \
		Get-Content -Path '$**' -Encoding UTF8 ^| \
		Set-Content -Path '..\lang\$(**B).nsis.txt' -Enc Unicode -Force

rename :
	@ ..\tools\rename.bat "$(SRC)" "$(DST)" 1> nul

clean :
	@ if exist ..\lang\LICENSE*.nsis.txt $(RM) ..\lang\LICENSE*.nsis.txt
	@ if exist .\icons\nul $(RD) .\icons
	@ if exist .\gmnv??.exe $(RM) .\gmnv??.exe

# mnv: set noet sw=8 ts=8 sts=0 wm=0 tw=79 ft=make:
