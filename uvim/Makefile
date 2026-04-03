# This Makefile has two purposes:
# 1. Starting the compilation of MNV for Unix.
# 2. Creating the various distribution files.


#########################################################################
# 1. Starting the compilation of MNV for Unix.
#
# Using this Makefile without an argument will compile MNV for Unix.
# "make install" is also possible.
#
# NOTE: If this doesn't work properly, first change directory to "src" and use
# the Makefile there:
#	cd src
#	make [arguments]
# Noticed on AIX systems when using this Makefile: Trying to run "cproto" or
# something else after MNV has been compiled.  Don't know why...
# Noticed on OS/390 Unix: Restarts configure.
#
# The first (default) target is "first".  This will result in running
# "make first", so that the target from "src/auto/config.mk" is picked
# up properly when config didn't run yet.  Doing "make all" before configure
# has run can result in compiling with $(CC) empty.

first:
	@if test ! -f src/auto/config.mk; then \
		cp src/config.mk.dist src/auto/config.mk; \
	fi
	@echo "Starting make in the src directory."
	@echo "If there are problems, cd to the src directory and run make there"
	cd src && $(MAKE) $@

# Some make programs use the last target for the $@ default; put the other
# targets separately to always let $@ expand to "first" by default.
all install uninstall tools config configure reconfig proto depend lint tags types test scripttests testtiny test_libvterm unittests testclean clean distclean:
	@if test ! -f src/auto/config.mk; then \
		cp src/config.mk.dist src/auto/config.mk; \
	fi
	@echo "Starting make in the src directory."
	@echo "If there are problems, cd to the src directory and run make there"
	cd src && $(MAKE) $@
	@# When the target is "test" also run the indent and syntax tests.
	@if test "$@" = "test" -o "$@" = "testtiny"; then \
		$(MAKE) indenttest; \
		$(MAKE) syntaxtest; \
	fi
	@# When the target is "clean" also clean for the indent and syntax tests.
	@if test "$@" = "clean" -o "$@" = "distclean" -o "$@" = "testclean"; then \
		(cd runtime/indent && $(MAKE) clean); \
		(cd runtime/syntax && $(MAKE) clean); \
	fi

# Executable used for running the indent tests.
MNV_FOR_INDENTTEST = ../../src/mnv

indenttest:
	cd runtime/indent && \
		$(MAKE) clean && \
		$(MAKE) test MNVPROG="$(MNV_FOR_INDENTTEST)"

# Executable used for running the syntax tests.
MNV_FOR_SYNTAXTEST = ../../src/mnv

# (For local testing only with GNU Make.)
MNV_SYNTAX_TEST_FILTER =
MNV_SYNTAX_TEST_WAIT_TIME =

syntaxtest:
	cd runtime/syntax && \
		$(MAKE) clean && \
		$(MAKE) test MNVPROG="$(MNV_FOR_SYNTAXTEST)"


#########################################################################
# 2. Creating the various distribution files.
#
# TARGET	PRODUCES		CONTAINS
# unixall	mnv-#.#.tar.bz2		All runtime files and sources, for Unix
#
# html		mnv##html.zip		HTML docs
#
# dossrc	mnv##src.zip		sources for MS-DOS
# dosrt		mnv##rt.zip		runtime for MS-DOS
# dosbin	mnv##w32.zip		binary for Win32
#		gmnv##.zip		binary for GUI Win32
#		gmnv##ole.zip		OLE exe for Win32 GUI
#
# OBSOLETE
# amisrc	mnv##src.tgz		sources for Amiga
# amirt		mnv##rt.tgz		runtime for Amiga
# amibin	mnv##bin.tgz		binary for Amiga
#
# farsi		farsi##.zip		Farsi fonts
#
#    All output files are created in the "dist" directory.  Existing files are
#    overwritten!
#    To do all this you need the Unix archive and compiled binaries.
#    Before creating an archive first delete all backup files, *.orig, etc.

MAJOR = 9
MINOR = 2

# CHECKLIST for creating a new version:
#
# - Update MNV version number.  For a test version in: src/version.h,
#   READMEdir/Contents, MAJOR/MINOR above, MNVMAJOR and MNVMINOR in
#   src/Makefile, README.txt, README.md, src/README.md, READMEdir/README*.txt,
#   runtime/doc/*.txt.
#   For a minor/major version: src/GmnvExt/GmnvExt.reg, src/mnv.manifest.
# - Compile MNV with GTK, Perl, Python, Python3, TCL, Ruby, Lua, Cscope and
#   "huge" features.  Add MZscheme if you can make it work.
#   Use "make reconfig" after selecting the configure arguments.
# - With these features: "make proto" (requires cproto and Motif installed;
#   ignore warnings for missing include files, fix problems for syntax errors).
# - With these features: "make depend" (works best with gcc).
# - If you have a lint program: "make lint" and check the output (ignore GTK
#   warnings).
# - compile release versions using -DNDEBUG to disable assert()s
# - If you have valgrind, enable it in src/testdir/Makefile and run "make
#   test".  Enable EXITFREE, disable GUI, scheme and tcl to avoid false alarms.
#   Check the valgrind output.
# - Adjust the date and other info in src/version.h.
# - Correct included_patches[] in src/version.c.
# - Check for missing entries in runtime/makemenu.mnv (with checkmenu script).
# - Check for missing options in runtime/optwin.mnv et al. (with check.mnv).
# - Do "make menu" to update the runtime/synmenu.mnv file.
# - Add remarks for changes to runtime/doc/version9.txt.
# - Check that runtime/doc/help.txt doesn't contain entries in "LOCAL
#   ADDITIONS".
# - In runtime/doc run "make" and "make html" to check for errors.
# - Check if src/Makefile, src/testdir/Makefile and src/feature.h don't contain
#   any personal preferences or the changes mentioned above.
# - Check file protections to be "644" for text and "755" for executables (run
#   the "check" script).
# - Check compiling on Amiga, MS-DOS and MS-Windows.
# - Delete all *~, *.sw?, *.orig, *.rej files
# - "make unixall", "make html"
# - Make diff files against the previous release: "makediff7 7.1 7.2"
#
# Amiga: (OBSOLETE, Amiga files are no longer distributed)
# - "make amisrc", move the archive to the Amiga and compile:
#   "make -f Make_manx.mak" (will use "big" features by default).
# - Run the tests: "make -f Make_manx.mak test"
# - Place the executables MNV and Xxd in this directory (set the executable
#   flag).
# - "make amirt", "make amibin".
#
# MS-Windows:
# - Run make on Unix to update the ".mo" files.
# - Get 32 bit libintl-8.dll, libiconv-2.dll and libgcc_s_sjlj-1.dll. E.g. from
#   https://mlocati.github.io/gettext-iconv-windows/ .
#   Use the "shared-32.zip file and extract the archive to get the files.
#   Put them in the gettext32 directory, "make dosrt" uses them.
# - Get 64 bit libintl-8.dll and libiconv-2.dll. E.g. from
#   https://mlocati.github.io/gettext-iconv-windows/ .
#   Use the "shared-64.zip file and extract the archive to get the files.
#   Put them in the gettext64 directory, "make dosrt" uses them.
# - > make dossrc
#   > make dosrt
#   Unpack dist/mnv##rt.zip and dist/mnv##src.zip on an MS-Windows PC.
#   This creates the directory mnv/mnv91 and puts all files in there.
# Win32 console version build:
# - See src/INSTALLpc.txt for installing the compiler and SDK.
# - Set environment for Visual C++ 2015:
#   > cd src
#   > msvc-latest.bat
# - Build the console binary:
#   > nmake.exe -f Make_mvc.mak
# - Run the tests and check the output:
#   > nmake.exe -f Make_mvc.mak testclean
#   > nmake.exe -f Make_mvc.mak test
# - Rename (using ../tools/rename.bat):
#           mnv.exe to mnvw32.exe
#           tee/tee.exe to teew32.exe
#           xxd/xxd.exe to xxdw32.exe
#           mnv.pdb to mnvw32.pdb
#           install.exe to installw32.exe
#           uninstall.exe to uninstallw32.exe
# Win32 GUI version build:
# - > cd src
#   > nmake.exe -f Make_mvc.mak "GUI=yes"
# - Run the tests and check the output:
#   > nmake.exe -f Make_mvc.mak testclean
#   > nmake.exe -f Make_mvc.mak testgmnv
# - move "gmnv.exe" to here (otherwise the OLE version will overwrite it).
# - Move gmnv.pdb to here.
# - Copy "GmnvExt/gmnvext.dll" to here.
# - Delete mnvrun.exe, install.exe and uninstall.exe.
# Win32 GUI version with OLE, PERL, Ruby, TCL, PYTHON and dynamic IME:
# - Install the interfaces you want, see src/INSTALLpc.txt
#   Adjust bigmnv.bat to match the version of each interface you want.
# - Build:
#   > cd src
#   > bigmnv.bat
# - Run the tests:
#   > nmake.exe -f Make_mvc.mak testclean
#   > nmake.exe -f Make_mvc.mak testgmnv
#   - check the output.
# - Rename "gmnv.exe" to "gmnv_ole.exe".
# - Rename gmnv.pdb to "gmnv_ole.pdb".
# - Delete install.exe and uninstall.exe.
# Create the archives:
# - Copy all the "*.exe" files to where this Makefile is.
# - Copy all the "*.pdb" files to where this Makefile is.
# - in this directory:
#   > make dosbin
# NSIS self installing exe:
# - To get NSIS see http://nsis.sourceforge.net
# - Make sure gmnv_ole.exe, mnvw32.exe, installw32.exe,
#   uninstallw32.exe, teew32.exe and xxdw32.exe have been build as mentioned
#   above.
# - copy these files (get them from a binary archive or build them):
#	gmnvext.dll in src/GmnvExt
#	gmnvext64.dll in src/GmnvExt
#   gmnvext64.dll can be obtained from:
#   https://github.com/mnv/mnv-win32-installer/releases
#	It is part of gmnv_9.1.*_x64.zip as mnv/mnv91/GmnvExt/gmnvext64.dll.
# - Make sure there is a diff.exe two levels up (get it from a previous MNV
#   version).  Also put winpty32.dll and winpty-agent.exe there.
# - go to ../nsis and do:
#   > nmake.exe -f Make_mvc.mak all
#    (takes a few minutes).
#    See nsis/README.txt for details.
# - Copy gmnv##.exe to the dist directory.
#
# 64 bit builds (these are not in the normal distribution, the 32 bit build
# works just fine on 64 bit systems).
# Like the console and GUI version, but first run vcvars64.bat or
#   "..\VC\vcvarsall.bat x86_amd64".
# - Build the console version:
#   > nmake.exe -f Make_mvc.mak
# - Build the GUI version:
#   > nmake.exe -f Make_mvc.mak "GUI=yes"
# - Build the OLE version with interfaces:
#   > bigmnv64.bat
#
#
# OBSOLETE systems: You can build these if you have an appropriate system.
#
# 16 bit DOS version: You need to get a very old version of MNV, for several
# years even the tiny build is too big to fit in DOS memory.
#
# 32 bit DOS version: Support was removed in 7.4.1399.  When syncing to before
# that it probably won't build.
#
# Win32s GUI version: Support was removed in patch 7.4.1364.
#
# OS/2 support was removed in patch 7.4.1008.  If you want to give it a try
# sync to before that and check the old version of this Makefile for
# instructions.

MNVVER	= mnv-$(MAJOR).$(MINOR)
VERSION = $(MAJOR)$(MINOR)
VDOT	= $(MAJOR).$(MINOR)
MNVRTDIR = mnv$(VERSION)

# How to include Filelist depends on the version of "make" you have.
# If the current choice doesn't work, try the other one.

include Filelist
#.include "Filelist"


# All output is put in the "dist" directory.
dist:
	mkdir dist

# Clean up some files to avoid they are included.
# Copy README files to the top directory.
prepare:
	if test -f lang/LICENSE.nsis.txt; then \
		rm -f lang/LICENSE*.nsis.txt; fi
	for name in $(IN_README_DIR); do \
	  cp READMEdir/"$$name" .; \
	  done

# For the zip files we need to create a file with the comment line
dist/comment:
	mkdir dist/comment

COMMENT_RT = comment/$(VERSION)-rt
COMMENT_W32 = comment/$(VERSION)-bin-w32
COMMENT_GMNV = comment/$(VERSION)-bin-gmnv
COMMENT_OLE = comment/$(VERSION)-bin-ole
COMMENT_SRC = comment/$(VERSION)-src
COMMENT_HTML = comment/$(VERSION)-html
COMMENT_FARSI = comment/$(VERSION)-farsi

dist/$(COMMENT_RT): dist/comment
	echo "MNV - MNV is not Vim - v$(VDOT) runtime files for MS-DOS and MS-Windows" > dist/$(COMMENT_RT)

dist/$(COMMENT_W32): dist/comment
	echo "MNV - MNV is not Vim - v$(VDOT) binaries for MS-Windows" > dist/$(COMMENT_W32)

dist/$(COMMENT_GMNV): dist/comment
	echo "MNV - MNV is not Vim - v$(VDOT) GUI binaries for MS-Windows" > dist/$(COMMENT_GMNV)

dist/$(COMMENT_OLE): dist/comment
	echo "MNV - MNV is not Vim - v$(VDOT) MS-Windows GUI binaries with OLE support" > dist/$(COMMENT_OLE)

dist/$(COMMENT_SRC): dist/comment
	echo "MNV - MNV is not Vim - v$(VDOT) sources for MS-DOS and MS-Windows" > dist/$(COMMENT_SRC)

dist/$(COMMENT_HTML): dist/comment
	echo "MNV - MNV is not Vim - v$(VDOT) documentation in HTML" > dist/$(COMMENT_HTML)

dist/$(COMMENT_FARSI): dist/comment
	echo "MNV - MNV is not Vim - v$(VDOT) Farsi language files" > dist/$(COMMENT_FARSI)

unixall: dist prepare
	-rm -f dist/$(MNVVER).tar.bz2
	-rm -rf dist/$(MNVRTDIR)
	mkdir dist/$(MNVRTDIR)
	tar cf - \
		$(RT_ALL) \
		$(RT_ALL_BIN) \
		$(RT_UNIX) \
		$(RT_UNIX_DOS_BIN) \
		$(RT_SCRIPTS) \
		$(LANG_GEN) \
		$(LANG_GEN_BIN) \
		$(SRC_ALL) \
		$(SRC_UNIX) \
		$(SRC_DOS_UNIX) \
		$(EXTRA) \
		$(LANG_SRC) \
		| (cd dist/$(MNVRTDIR); tar xf -)
	-rm $(IN_README_DIR)
# Need to use a "distclean" config.mk file
# Note: this file is not included in the repository to avoid problems, but it's
# OK to put it in the archive.
	cp -f src/config.mk.dist dist/$(MNVRTDIR)/src/auto/config.mk
# Create an empty config.h file, make dependencies require it
	touch dist/$(MNVRTDIR)/src/auto/config.h
# Make sure configure is newer than config.mk to force it to be generated
	touch dist/$(MNVRTDIR)/src/configure
# Make sure ja.sjis.po is newer than ja.po to avoid it being regenerated.
# Same for cs.cp1250.po, pl.cp1250.po and sk.cp1250.po.
	touch dist/$(MNVRTDIR)/src/po/ja.sjis.po
	touch dist/$(MNVRTDIR)/src/po/cs.cp1250.po
	touch dist/$(MNVRTDIR)/src/po/pl.cp1250.po
	touch dist/$(MNVRTDIR)/src/po/sk.cp1250.po
	touch dist/$(MNVRTDIR)/src/po/zh_CN.cp936.po
	touch dist/$(MNVRTDIR)/src/po/ru.cp1251.po
	touch dist/$(MNVRTDIR)/src/po/uk.cp1251.po
# Create the archive.
	cd dist && tar cf $(MNVVER).tar $(MNVRTDIR)
	bzip2 dist/$(MNVVER).tar

# Amiga runtime - OBSOLETE
amirt: dist prepare
	-rm -f dist/mnv$(VERSION)rt.tar.gz
	-rm -rf dist/MNV
	mkdir dist/MNV
	mkdir dist/MNV/$(MNVRTDIR)
	tar cf - \
		$(ROOT_AMI) \
		$(RT_ALL) \
		$(RT_ALL_BIN) \
		$(RT_SCRIPTS) \
		$(RT_AMI) \
		$(RT_NO_UNIX) \
		$(RT_AMI_DOS) \
		| (cd dist/MNV/$(MNVRTDIR); tar xf -)
	-rm $(IN_README_DIR)
	mv dist/MNV/$(MNVRTDIR)/mnvdir.info dist/MNV.info
	mv dist/MNV/$(MNVRTDIR)/runtime.info dist/MNV/$(MNVRTDIR).info
	mv dist/MNV/$(MNVRTDIR)/runtime/* dist/MNV/$(MNVRTDIR)
	rmdir dist/MNV/$(MNVRTDIR)/runtime
	cd dist && tar cf mnv$(VERSION)rt.tar MNV MNV.info
	gzip -9 dist/mnv$(VERSION)rt.tar
	mv dist/mnv$(VERSION)rt.tar.gz dist/mnv$(VERSION)rt.tgz

# Amiga binaries - OBSOLETE
amibin: dist prepare
	-rm -f dist/mnv$(VERSION)bin.tar.gz
	-rm -rf dist/MNV
	mkdir dist/MNV
	mkdir dist/MNV/$(MNVRTDIR)
	tar cf - \
		$(ROOT_AMI) \
		$(BIN_AMI) \
		MNV \
		Xxd \
		| (cd dist/MNV/$(MNVRTDIR); tar xf -)
	-rm $(IN_README_DIR)
	mv dist/MNV/$(MNVRTDIR)/mnvdir.info dist/MNV.info
	mv dist/MNV/$(MNVRTDIR)/runtime.info dist/MNV/$(MNVRTDIR).info
	cd dist && tar cf mnv$(VERSION)bin.tar MNV MNV.info
	gzip -9 dist/mnv$(VERSION)bin.tar
	mv dist/mnv$(VERSION)bin.tar.gz dist/mnv$(VERSION)bin.tgz

# Amiga sources - OBSOLETE
amisrc: dist prepare
	-rm -f dist/mnv$(VERSION)src.tar.gz
	-rm -rf dist/MNV
	mkdir dist/MNV
	mkdir dist/MNV/$(MNVRTDIR)
	tar cf - \
		$(ROOT_AMI) \
		$(SRC_ALL) \
		$(SRC_AMI) \
		$(SRC_AMI_DOS) \
		| (cd dist/MNV/$(MNVRTDIR); tar xf -)
	-rm $(IN_README_DIR)
	mv dist/MNV/$(MNVRTDIR)/mnvdir.info dist/MNV.info
	mv dist/MNV/$(MNVRTDIR)/runtime.info dist/MNV/$(MNVRTDIR).info
	cd dist && tar cf mnv$(VERSION)src.tar MNV MNV.info
	gzip -9 dist/mnv$(VERSION)src.tar
	mv dist/mnv$(VERSION)src.tar.gz dist/mnv$(VERSION)src.tgz

# MS-DOS sources
dossrc: dist dist/$(COMMENT_SRC) license
	-rm -rf dist/mnv$(VERSION)src.zip
	-rm -rf dist/mnv
	mkdir dist/mnv
	mkdir dist/mnv/$(MNVRTDIR)
	tar cf - \
		$(SRC_ALL) \
		$(SRC_DOS) \
		$(SRC_DOS_BIN) \
		$(SRC_AMI_DOS) \
		$(SRC_DOS_UNIX) \
		lang/LICENSE.*.txt \
		lang/README.*.txt \
		| (cd dist/mnv/$(MNVRTDIR); tar xf -)
	mv dist/mnv/$(MNVRTDIR)/runtime/* dist/mnv/$(MNVRTDIR)
	rmdir dist/mnv/$(MNVRTDIR)/runtime
	cd dist && zip -9 -rD -z mnv$(VERSION)src.zip mnv <$(COMMENT_SRC)

license:
	cd nsis && $(MAKE) -f Makefile $@

dosrt: dist dist/$(COMMENT_RT) dosrt_files
	-rm -rf dist/mnv$(VERSION)rt.zip
	cd dist && zip -9 -rD -z mnv$(VERSION)rt.zip mnv <$(COMMENT_RT)

# Split in two parts to avoid an "argument list too long" error.
# We no longer convert the files from unix to dos fileformat.
dosrt_files: dist prepare
	-rm -rf dist/mnv
	mkdir dist/mnv
	mkdir dist/mnv/$(MNVRTDIR)
	mkdir dist/mnv/$(MNVRTDIR)/lang
	cd src && MAKEMO=yes $(MAKE) languages
	tar cf - \
		$(RT_ALL) \
		| (cd dist/mnv/$(MNVRTDIR); tar xf -)
	tar cf - \
		$(RT_SCRIPTS) \
		$(RT_DOS) \
		$(RT_NO_UNIX) \
		$(RT_AMI_DOS) \
		$(LANG_GEN) \
		| (cd dist/mnv/$(MNVRTDIR); tar xf -)
	tar cf - \
		$(RT_UNIX_DOS_BIN) \
		$(RT_ALL_BIN) \
		$(RT_DOS_BIN) \
		$(LANG_GEN_BIN) \
		| (cd dist/mnv/$(MNVRTDIR); tar xf -)
	-rm $(IN_README_DIR)
	mv dist/mnv/$(MNVRTDIR)/lang/ dist/mnv/$(MNVRTDIR)/readme/
	mv dist/mnv/$(MNVRTDIR)/runtime/* dist/mnv/$(MNVRTDIR)
	rmdir dist/mnv/$(MNVRTDIR)/runtime
# Add the message translations.  Trick: skip ja.mo/ja.euc-jp.mo and use
# ja.sjis.mo instead.  Same for cs.mo / cs.cp1250.mo, pl.mo / pl.cp1250.mo,
# sk.mo / sk.cp1250.mo, zh_CN.mo / zh_CN.cp936.mo, uk.mo / uk.cp1251.mo and
# ru.mo / ru.cp1251.mo.
	for i in $(LANG_DOS); do \
	      if test "$$i" != "src/po/ja.mo" -a "$$i" != "src/po/ja.euc-jp.mo" -a "$$i" != "src/po/pl.mo" -a "$$i" != "src/po/cs.mo" -a "$$i" != "src/po/sk.mo" -a "$$i" != "src/po/zh_CN.mo" -a "$$i" != "src/po/ru.mo" -a "$$i" != "src/po/uk.mo"; then \
		n=`echo $$i | sed -e "s+src/po/\([-a-zA-Z0-9_]*\(.UTF-8\)*\)\(.sjis\)*\(.cp1250\)*\(.cp1251\)*\(.cp936\)*.mo+\1+"`; \
		mkdir dist/mnv/$(MNVRTDIR)/lang/$$n; \
		mkdir dist/mnv/$(MNVRTDIR)/lang/$$n/LC_MESSAGES; \
		cp $$i dist/mnv/$(MNVRTDIR)/lang/$$n/LC_MESSAGES/mnv.mo; \
	      fi \
	    done
	mkdir dist/mnv/$(MNVRTDIR)/gettext32
	cp gettext32/libintl-8.dll dist/mnv/$(MNVRTDIR)/gettext32/
	cp gettext32/libiconv-2.dll dist/mnv/$(MNVRTDIR)/gettext32/
	cp gettext32/libgcc_s_sjlj-1.dll dist/mnv/$(MNVRTDIR)/gettext32/
	mkdir dist/mnv/$(MNVRTDIR)/gettext64
	cp gettext64/libintl-8.dll dist/mnv/$(MNVRTDIR)/gettext64/
	cp gettext64/libiconv-2.dll dist/mnv/$(MNVRTDIR)/gettext64/


# Used before uploading.  Don't delete the AAPDIR/sign files!
runtime_unix2dos: dosrt_files
	-rm -rf `find runtime/dos -type f -print | sed -e /AAPDIR/d`
	cd dist/mnv/$(MNVRTDIR); tar cf - * \
		| (cd ../../../runtime/dos; tar xf -)

dosbin: prepare dosbin_gmnv dosbin_w32 dosbin_ole $(DOSBIN_S)
	-rm $(IN_README_DIR)

# make Win32 gmnv
dosbin_gmnv: dist dist/$(COMMENT_GMNV)
	-rm -rf dist/gmnv$(VERSION).zip
	-rm -rf dist/mnv
	mkdir dist/mnv
	mkdir dist/mnv/$(MNVRTDIR)
	tar cf - \
		$(BIN_DOS) \
		| (cd dist/mnv/$(MNVRTDIR); tar xf -)
	cp gmnv.exe dist/mnv/$(MNVRTDIR)/gmnv.exe
	cp teew32.exe dist/mnv/$(MNVRTDIR)/tee.exe
	cp xxdw32.exe dist/mnv/$(MNVRTDIR)/xxd.exe
	cp mnvrun.exe dist/mnv/$(MNVRTDIR)/mnvrun.exe
	cp installw32.exe dist/mnv/$(MNVRTDIR)/install.exe
	cp uninstallw32.exe dist/mnv/$(MNVRTDIR)/uninstall.exe
	mkdir dist/mnv/$(MNVRTDIR)/GmnvExt32
	cp gmnvext.dll dist/mnv/$(MNVRTDIR)/GmnvExt32/gmnvext.dll
	mkdir dist/mnv/$(MNVRTDIR)/GmnvExt64
	cp gmnvext64.dll dist/mnv/$(MNVRTDIR)/GmnvExt64/gmnvext.dll
	cd dist && zip -9 -rD -z gmnv$(VERSION).zip mnv <$(COMMENT_GMNV)
	cp gmnv.pdb dist/gmnv$(VERSION).pdb

# make Win32 console
dosbin_w32: dist dist/$(COMMENT_W32)
	-rm -rf dist/mnv$(VERSION)w32.zip
	-rm -rf dist/mnv
	mkdir dist/mnv
	mkdir dist/mnv/$(MNVRTDIR)
	tar cf - \
		$(BIN_DOS) \
		| (cd dist/mnv/$(MNVRTDIR); tar xf -)
	cp mnvw32.exe dist/mnv/$(MNVRTDIR)/mnv.exe
	cp teew32.exe dist/mnv/$(MNVRTDIR)/tee.exe
	cp xxdw32.exe dist/mnv/$(MNVRTDIR)/xxd.exe
	cp installw32.exe dist/mnv/$(MNVRTDIR)/install.exe
	cp uninstallw32.exe dist/mnv/$(MNVRTDIR)/uninstall.exe
	cd dist && zip -9 -rD -z mnv$(VERSION)w32.zip mnv <$(COMMENT_W32)
	cp mnvw32.pdb dist/mnv$(VERSION)w32.pdb

# make Win32 gmnv with OLE
dosbin_ole: dist dist/$(COMMENT_OLE)
	-rm -rf dist/gmnv$(VERSION)ole.zip
	-rm -rf dist/mnv
	mkdir dist/mnv
	mkdir dist/mnv/$(MNVRTDIR)
	tar cf - \
		$(BIN_DOS) \
		| (cd dist/mnv/$(MNVRTDIR); tar xf -)
	cp gmnv_ole.exe dist/mnv/$(MNVRTDIR)/gmnv.exe
	cp teew32.exe dist/mnv/$(MNVRTDIR)/tee.exe
	cp xxdw32.exe dist/mnv/$(MNVRTDIR)/xxd.exe
	cp mnvrun.exe dist/mnv/$(MNVRTDIR)/mnvrun.exe
	cp installw32.exe dist/mnv/$(MNVRTDIR)/install.exe
	cp uninstallw32.exe dist/mnv/$(MNVRTDIR)/uninstall.exe
	cp gmnvext.dll dist/mnv/$(MNVRTDIR)/gmnvext.dll
	cp README_ole.txt dist/mnv/$(MNVRTDIR)
	cd dist && zip -9 -rD -z gmnv$(VERSION)ole.zip mnv <$(COMMENT_OLE)
	cp gmnv_ole.pdb dist/gmnv$(VERSION)ole.pdb

html: dist dist/$(COMMENT_HTML)
	-rm -rf dist/mnv$(VERSION)html.zip
	cd runtime/doc && zip -9 -z ../../dist/mnv$(VERSION)html.zip *.html <../../dist/$(COMMENT_HTML)

farsi: dist dist/$(COMMENT_FARSI)
	-rm -f dist/farsi$(VERSION).zip
	zip -9 -rD -z dist/farsi$(VERSION).zip farsi < dist/$(COMMENT_FARSI)
