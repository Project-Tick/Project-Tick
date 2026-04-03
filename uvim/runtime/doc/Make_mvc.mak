#
# Makefile for the MNV documentation on Windows
#
# 2024-03-20, Restorer, <restorer@mail2k.ru>
#

# included common tools
!INCLUDE ..\..\src\auto\nmake\tools.mak

# Common components
!INCLUDE .\Make_all.mak


# TODO: to think about what to use instead of awk. PowerShell?
#AWK =

# Correct the following line for the where executable file MNV is installed.
# Please do not put the path in quotes.
MNVPROG = ..\..\src\mnv.exe

# Correct the following line for the directory where iconv installed.
# Please do not put the path in quotes.
ICONV_PATH = D:\Programs\GetText\bin

# In case some package like GnuWin32, UnixUtils, gettext
# or something similar is installed on the system.
# If the "iconv" program is installed on the system, but it is not registered
# in the %PATH% environment variable, then specify the full path to this file.
!IF EXIST ("iconv.exe")
ICONV = iconv.exe
!ELSEIF EXIST ("$(ICONV_PATH)\iconv.exe")
ICONV = "$(ICONV_PATH)\iconv.exe"
!ENDIF

.SUFFIXES :
.SUFFIXES : .c .o .txt .html


all : tags perlhtml $(CONVERTED)

# Use "doctags" to generate the tags file.  Only works for English!
tags : doctags $(DOCS)
	doctags.exe $(DOCS) | sort /L C /O tags
	$(PS) $(PSFLAGS) \
		(Get-Content -Raw tags ^| Get-Unique ^| %%{$$_ \
		-replace \"`r\", \"\"}) \
		^| New-Item -Path . -Name tags -ItemType file -Force

doctags : doctags.c
	$(CC) doctags.c


# Use MNV to generate the tags file.  Can only be used when MNV has been
# compiled and installed.  Supports multiple languages.
mnvtags : $(DOCS)
	@ "$(MNVPROG)" --clean -esX -V1 -u doctags.mnv

# TODO:
#html: noerrors tags $(HTMLS)
#	if exist errors.log (more errors.log)

# TODO:
#noerrors:
#	$(RM) errors.log

# TODO:
#.txt.html:


# TODO:
#index.html: help.txt


# TODO:
#mnvindex.html: index.txt


# TODO:
#tags.ref tags.html: tags

# Perl version of .txt to .html conversion.
# There can't be two rules to produce a .html from a .txt file.
# Just run over all .txt files each time one changes.  It's fast anyway.
perlhtml : tags $(DOCS)
	mnv2html.pl tags $(DOCS)

# Check URLs in the help with "curl" or "powershell".
test_urls :
	"$(MNVPROG)" --clean -S test_urls.mnv

clean :
	- $(RM) doctags.exe doctags.obj
	- $(RM) *.html mnv-stylesheet.css


arabic.txt :
	@ <<touch.bat $@
@$(TOUCH)
<<

farsi.txt :
	@ <<touch.bat $@
@$(TOUCH)
<<

hebrew.txt :
	@ <<touch.bat $@
@$(TOUCH)
<<

russian.txt :
	@ <<touch.bat $@
@$(TOUCH)
<<

gui_w32.txt :
	@ <<touch.bat $@
@$(TOUCH)
<<

if_ole.txt :
	@ <<touch.bat $@
@$(TOUCH)
<<

os_390.txt :
	@ <<touch.bat $@
@$(TOUCH)
<<

os_amiga.txt :
	@ <<touch.bat $@
@$(TOUCH)
<<

os_beos.txt :
	@ <<touch.bat $@
@$(TOUCH)
<<

os_dos.txt :
	@ <<touch.bat $@
@$(TOUCH)
<<

os_haiku.txt :
	@ <<touch.bat $@
@$(TOUCH)
<<

os_mac.txt :
	@ <<touch.bat $@
@$(TOUCH)
<<

os_mint.txt :
	@ <<touch.bat $@
@$(TOUCH)
<<

os_msdos.txt :
	@ <<touch.bat $@
@$(TOUCH)
<<

os_os2.txt :
	@ <<touch.bat $@
@$(TOUCH)
<<

os_qnx.txt :
	@ <<touch.bat $@
@$(TOUCH)
<<

os_risc.txt :
	@ <<touch.bat $@
@$(TOUCH)
<<

os_win32.txt :
	@ <<touch.bat $@
@$(TOUCH)
<<

convert-all : $(CONVERTED)

mnv-da.UTF-8.1 : mnv-da.1
!IF DEFINED (ICONV)
	$(ICONV) -f ISO-8859-1 -t UTF-8 $? >$@
!ELSE
# Conversion to UTF-8 encoding without BOM and with UNIX-like line ending
	$(PS) $(PSFLAGS) \
		[IO.File]::ReadAllText(\"$?\", \
		[Text.Encoding]::GetEncoding(28591)) ^| \
		1>nul New-Item -Path . -Name $@ -ItemType file -Force
!ENDIF

mnvdiff-da.UTF-8.1 : mnvdiff-da.1
!IF DEFINED (ICONV)
	$(ICONV) -f ISO-8859-1 -t UTF-8 $? >$@
!ELSE
# Conversion to UTF-8 encoding without BOM and with UNIX-like line ending
	$(PS) $(PSFLAGS) \
		[IO.File]::ReadAllText(\"$?\", \
		[Text.Encoding]::GetEncoding(28591)) ^| \
		1>nul New-Item -Path . -Name $@ -ItemType file -Force
!ENDIF

mnvtutor-da.UTF-8.1 : mnvtutor-da.1
!IF DEFINED (ICONV)
	$(ICONV) -f ISO-8859-1 -t UTF-8 $? >$@
!ELSE
# Conversion to UTF-8 encoding without BOM and with UNIX-like line ending
	$(PS) $(PSFLAGS) \
		[IO.File]::ReadAllText(\"$?\", \
		[Text.Encoding]::GetEncoding(28591)) ^| \
		1>nul New-Item -Path . -Name $@ -ItemType file -Force
!ENDIF

mnv-de.UTF-8.1 : mnv-de.1
!IF DEFINED (ICONV)
	$(ICONV) -f ISO-8859-1 -t UTF-8 $? >$@
!ELSE
# Conversion to UTF-8 encoding without BOM and with UNIX-like line ending
	$(PS) $(PSFLAGS) \
		[IO.File]::ReadAllText(\"$?\", \
		[Text.Encoding]::GetEncoding(28591)) ^| \
		1>nul New-Item -Path . -Name $@ -ItemType file -Force
!ENDIF

emnv-fr.UTF-8.1 : emnv-fr.1
!IF DEFINED (ICONV)
	$(ICONV) -f ISO-8859-1 -t UTF-8 $? >$@
!ELSE
# Conversion to UTF-8 encoding without BOM and with UNIX-like line ending
	$(PS) $(PSFLAGS) \
		[IO.File]::ReadAllText(\"$?\", \
		[Text.Encoding]::GetEncoding(28591)) ^| \
		1>nul New-Item -Path . -Name $@ -ItemType file -Force
!ENDIF

mnv-fr.UTF-8.1 : mnv-fr.1
!IF DEFINED (ICONV)
	$(ICONV) -f ISO-8859-1 -t UTF-8 $? >$@
!ELSE
# Conversion to UTF-8 encoding without BOM and with UNIX-like line ending
	$(PS) $(PSFLAGS) \
		[IO.File]::ReadAllText(\"$?\", \
		[Text.Encoding]::GetEncoding(28591)) ^| \
		1>nul New-Item -Path . -Name $@ -ItemType file -Force
!ENDIF

mnvdiff-fr.UTF-8.1 : mnvdiff-fr.1
!IF DEFINED (ICONV)
	$(ICONV) -f ISO-8859-1 -t UTF-8 $? >$@
!ELSE
# Conversion to UTF-8 encoding without BOM and with UNIX-like line ending
	$(PS) $(PSFLAGS) \
		[IO.File]::ReadAllText(\"$?\", \
		[Text.Encoding]::GetEncoding(28591)) ^| \
		1>nul New-Item -Path . -Name $@ -ItemType file -Force
!ENDIF

mnvtutor-fr.UTF-8.1 : mnvtutor-fr.1
!IF DEFINED (ICONV)
	$(ICONV) -f ISO-8859-1 -t utf-8 $? >$@
!ELSE
# Conversion to UTF-8 encoding without BOM and with UNIX-like line ending
	$(PS) $(PSFLAGS) \
		[IO.File]::ReadAllText(\"$?\", \
		[Text.Encoding]::GetEncoding(28591)) ^| \
		1>nul New-Item -Path . -Name $@ -ItemType file -Force
!ENDIF

xxd-fr.UTF-8.1 : xxd-fr.1
!IF DEFINED (ICONV)
	$(ICONV) -f ISO-8859-1 -t UTF-8 $? >$@
!ELSE
# Conversion to UTF-8 encoding without BOM and with UNIX-like line ending
	$(PS) $(PSFLAGS) \
		[IO.File]::ReadAllText(\"$?\", \
		[Text.Encoding]::GetEncoding(28591)) ^| \
		1>nul New-Item -Path . -Name $@ -ItemType file -Force
!ENDIF

emnv-it.UTF-8.1 : emnv-it.1
!IF DEFINED (ICONV)
	$(ICONV) -f ISO-8859-1 -t UTF-8 $? >$@
!ELSE
# Conversion to UTF-8 encoding without BOM and with UNIX-like line ending
	$(PS) $(PSFLAGS) \
		[IO.File]::ReadAllText(\"$?\", \
		[Text.Encoding]::GetEncoding(28591)) ^| \
		1>nul New-Item -Path . -Name $@ -ItemType file -Force
!ENDIF

mnv-it.UTF-8.1 : mnv-it.1
!IF DEFINED (ICONV)
	$(ICONV) -f ISO-8859-1 -t UTF-8 $? >$@
!ELSE
# Conversion to UTF-8 encoding without BOM and with UNIX-like line ending
	$(PS) $(PSFLAGS) \
		[IO.File]::ReadAllText(\"$?\", \
		[Text.Encoding]::GetEncoding(28591)) ^| \
		1>nul New-Item -Path . -Name $@ -ItemType file -Force
!ENDIF

mnvdiff-it.UTF-8.1 : mnvdiff-it.1
!IF DEFINED (ICONV)
	$(ICONV) -f ISO-8859-1 -t UTF-8 $? >$@
!ELSE
# Conversion to UTF-8 encoding without BOM and with UNIX-like line ending
	$(PS) $(PSFLAGS) \
		[IO.File]::ReadAllText(\"$?\", \
		[Text.Encoding]::GetEncoding(28591)) ^| \
		1>nul New-Item -Path . -Name $@ -ItemType file -Force
!ENDIF

mnvtutor-it.UTF-8.1 : mnvtutor-it.1
!IF DEFINED (ICONV)
	$(ICONV) -f ISO-8859-1 -t UTF-8 $? >$@
!ELSE
# Conversion to UTF-8 encoding without BOM and with UNIX-like line ending
	$(PS) $(PSFLAGS) \
		[IO.File]::ReadAllText(\"$?\", \
		[Text.Encoding]::GetEncoding(28591)) ^| \
		1>nul New-Item -Path . -Name $@ -ItemType file -Force
!ENDIF

xxd-it.UTF-8.1 : xxd-it.1
!IF DEFINED (ICONV)
	$(ICONV) -f ISO-8859-1 -t UTF-8 $? >$@
!ELSE
# Conversion to UTF-8 encoding without BOM and with UNIX-like line ending
	$(PS) $(PSFLAGS) \
		[IO.File]::ReadAllText(\"$?\", \
		[Text.Encoding]::GetEncoding(28591)) ^| \
		1>nul New-Item -Path . -Name $@ -ItemType file -Force
!ENDIF

emnv-pl.UTF-8.1 : emnv-pl.1
!IF DEFINED (ICONV)
	$(ICONV) -f ISO-8859-2 -t UTF-8 $? >$@
!ELSE
# Conversion to UTF-8 encoding without BOM and with UNIX-like line ending
	$(PS) $(PSFLAGS) \
		[IO.File]::ReadAllText(\"$?\", \
		[Text.Encoding]::GetEncoding(28592)) ^| \
		1>nul New-Item -Path . -Name $@ -ItemType file -Force
!ENDIF

mnv-pl.UTF-8.1 : mnv-pl.1
!IF DEFINED (ICONV)
	$(ICONV) -f ISO-8859-2 -t UTF-8 $? >$@
!ELSE
# Conversion to UTF-8 encoding without BOM and with UNIX-like line ending
	$(PS) $(PSFLAGS) \
		[IO.File]::ReadAllText(\"$?\", \
		[Text.Encoding]::GetEncoding(28592)) ^| \
		1>nul New-Item -Path . -Name $@ -ItemType file -Force
!ENDIF

mnvdiff-pl.UTF-8.1 : mnvdiff-pl.1
!IF DEFINED (ICONV)
	$(ICONV) -f ISO-8859-2 -t UTF-8 $? >$@
!ELSE
# Conversion to UTF-8 encoding without BOM and with UNIX-like line ending
	$(PS) $(PSFLAGS) \
		[IO.File]::ReadAllText(\"$?\", \
		[Text.Encoding]::GetEncoding(28592)) ^| \
		1>nul New-Item -Path . -Name $@ -ItemType file -Force
!ENDIF

mnvtutor-pl.UTF-8.1 : mnvtutor-pl.1
!IF DEFINED (ICONV)
	$(ICONV) -f ISO-8859-2 -t UTF-8 $? >$@
!ELSE
# Conversion to UTF-8 encoding without BOM and with UNIX-like line ending
	$(PS) $(PSFLAGS) \
		[IO.File]::ReadAllText(\"$?\", \
		[Text.Encoding]::GetEncoding(28592)) ^| \
		1>nul New-Item -Path . -Name $@ -ItemType file -Force
!ENDIF

xxd-pl.UTF-8.1 : xxd-pl.1
!IF DEFINED (ICONV)
	$(ICONV) -f ISO-8859-2 -t UTF-8 $? >$@
!ELSE
# Conversion to UTF-8 encoding without BOM and with UNIX-like line ending
	$(PS) $(PSFLAGS) \
		[IO.File]::ReadAllText(\"$?\", \
		[Text.Encoding]::GetEncoding(28592)) ^| \
		1>nul New-Item -Path . -Name $@ -ItemType file -Force
!ENDIF

emnv-ru.UTF-8.1 : emnv-ru.1
!IF DEFINED (ICONV)
	$(ICONV) -f KOI8-R -t UTF-8 $? >$@
!ELSE
# Conversion to UTF-8 encoding without BOM and with UNIX-like line ending
	$(PS) $(PSFLAGS) \
		[IO.File]::ReadAllText(\"$?\", \
		[Text.Encoding]::GetEncoding(20866)) ^| \
		1>nul New-Item -Path . -Name $@ -ItemType file -Force
!ENDIF

mnv-ru.UTF-8.1 : mnv-ru.1
!IF DEFINED (ICONV)
	$(ICONV) -f KOI8-R -t UTF-8 $? >$@
!ELSE
# Conversion to UTF-8 encoding without BOM and with UNIX-like line ending
	$(PS) $(PSFLAGS) \
		[IO.File]::ReadAllText(\"$?\", \
		[Text.Encoding]::GetEncoding(20866)) ^| \
		1>nul New-Item -Path . -Name $@ -ItemType file -Force
!ENDIF

mnvdiff-ru.UTF-8.1 : mnvdiff-ru.1
!IF DEFINED (ICONV)
	$(ICONV) -f KOI8-R -t UTF-8 $? >$@
!ELSE
# Conversion to UTF-8 encoding without BOM and with UNIX-like line ending
	$(PS) $(PSFLAGS) \
		[IO.File]::ReadAllText(\"$?\", \
		[Text.Encoding]::GetEncoding(20866)) ^| \
		1>nul New-Item -Path . -Name $@ -ItemType file -Force
!ENDIF

mnvtutor-ru.UTF-8.1 : mnvtutor-ru.1
!IF DEFINED (ICONV)
	$(ICONV) -f KOI8-R -t UTF-8 $? >$@
!ELSE
# Conversion to UTF-8 encoding without BOM and with UNIX-like line ending
	$(PS) $(PSFLAGS) \
		[IO.File]::ReadAllText(\"$?\", \
		[Text.Encoding]::GetEncoding(20866)) ^| \
		1>nul New-Item -Path . -Name $@ -ItemType file -Force
!ENDIF

xxd-ru.UTF-8.1 : xxd-ru.1
!IF DEFINED (ICONV)
	$(ICONV) -f KOI8-R -t UTF-8 $? >$@
!ELSE
# Conversion to UTF-8 encoding without BOM and with UNIX-like line ending
	$(PS) $(PSFLAGS) \
		[IO.File]::ReadAllText(\"$?\", \
		[Text.Encoding]::GetEncoding(20866)) ^| \
		1>nul New-Item -Path . -Name $@ -ItemType file -Force
!ENDIF

emnv-sv.UTF-8.1 : emnv-sv.1
!IF DEFINED (ICONV)
	$(ICONV) -f ISO-8859-1 -t UTF-8 $? >$@
!ELSE
# Conversion to UTF-8 encoding without BOM and with UNIX-like line ending
	$(PS) $(PSFLAGS) \
		[IO.File]::ReadAllText(\"$?\", \
		[Text.Encoding]::GetEncoding(28599)) ^| \
		1>nul New-Item -Path . -Name $@ -ItemType file -Force
!ENDIF

mnv-sv.UTF-8.1 : mnv-sv.1
!IF DEFINED (ICONV)
	$(ICONV) -f ISO-8859-1 -t UTF-8 $? >$@
!ELSE
# Conversion to UTF-8 encoding without BOM and with UNIX-like line ending
	$(PS) $(PSFLAGS) \
		[IO.File]::ReadAllText(\"$?\", \
		[Text.Encoding]::GetEncoding(28599)) ^| \
		1>nul New-Item -Path . -Name $@ -ItemType file -Force
!ENDIF

mnvdiff-sv.UTF-8.1 : mnvdiff-sv.1
!IF DEFINED (ICONV)
	$(ICONV) -f ISO-8859-1 -t UTF-8 $? >$@
!ELSE
# Conversion to UTF-8 encoding without BOM and with UNIX-like line ending
	$(PS) $(PSFLAGS) \
		[IO.File]::ReadAllText(\"$?\", \
		[Text.Encoding]::GetEncoding(28599)) ^| \
		1>nul New-Item -Path . -Name $@ -ItemType file -Force
!ENDIF

mnvtutor-sv.UTF-8.1 : mnvtutor-sv.1
!IF DEFINED (ICONV)
	$(ICONV) -f ISO-8859-1 -t UTF-8 $? >$@
!ELSE
# Conversion to UTF-8 encoding without BOM and with UNIX-like line ending
	$(PS) $(PSFLAGS) \
		[IO.File]::ReadAllText(\"$?\", \
		[Text.Encoding]::GetEncoding(28599)) ^| \
		1>nul New-Item -Path . -Name $@ -ItemType file -Force
!ENDIF

xxd-sv.UTF-8.1 : xxd-sv.1
!IF DEFINED (ICONV)
	$(ICONV) -f ISO-8859-1 -t UTF-8 $? >$@
!ELSE
# Conversion to UTF-8 encoding without BOM and with UNIX-like line ending
	$(PS) $(PSFLAGS) \
		[IO.File]::ReadAllText(\"$?\", \
		[Text.Encoding]::GetEncoding(28599)) ^| \
		1>nul New-Item -Path . -Name $@ -ItemType file -Force
!ENDIF

emnv-tr.UTF-8.1 : emnv-tr.1
!IF DEFINED (ICONV)
	$(ICONV) -f ISO-8859-9 -t UTF-8 $? >$@
!ELSE
# Conversion to UTF-8 encoding without BOM and with UNIX-like line ending
	$(PS) $(PSFLAGS) \
		[IO.File]::ReadAllText(\"$?\", \
		[Text.Encoding]::GetEncoding(28599)) ^| \
		1>nul New-Item -Path . -Name $@ -ItemType file -Force
!ENDIF

mnv-tr.UTF-8.1 : mnv-tr.1
!IF DEFINED (ICONV)
	$(ICONV) -f ISO-8859-9 -t UTF-8 $? >$@
!ELSE
# Conversion to UTF-8 encoding without BOM and with UNIX-like line ending
	$(PS) $(PSFLAGS) \
		[IO.File]::ReadAllText(\"$?\", \
		[Text.Encoding]::GetEncoding(28599)) ^| \
		1>nul New-Item -Path . -Name $@ -ItemType file -Force
!ENDIF

mnvdiff-tr.UTF-8.1 : mnvdiff-tr.1
!IF DEFINED (ICONV)
	$(ICONV) -f ISO-8859-9 -t UTF-8 $? >$@
!ELSE
# Conversion to UTF-8 encoding without BOM and with UNIX-like line ending
	$(PS) $(PSFLAGS) \
		[IO.File]::ReadAllText(\"$?\", \
		[Text.Encoding]::GetEncoding(28599)) ^| \
		1>nul New-Item -Path . -Name $@ -ItemType file -Force
!ENDIF

mnvtutor-tr.UTF-8.1 : mnvtutor-tr.1
!IF DEFINED (ICONV)
	$(ICONV) -f ISO-8859-9 -t UTF-8 $? >$@
!ELSE
# Conversion to UTF-8 encoding without BOM and with UNIX-like line ending
	$(PS) $(PSFLAGS) \
		[IO.File]::ReadAllText(\"$?\", \
		[Text.Encoding]::GetEncoding(28599)) ^| \
		1>nul New-Item -Path . -Name $@ -ItemType file -Force
!ENDIF

# mnv: set noet sw=8 ts=8 sts=0 wm=0 tw=79 ft=make:
