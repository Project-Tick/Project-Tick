#
# Makefile for UNIX-like for create self-installing exe of MNV.
# 2025-10-03, Restorer, restorer@mail2k.ru
#


.SUFFIXES:
.PHONY: all makeinst prepare rename clean 

ifdef MNVSRC
MKNSISFLAGS := -D"MNVSRC=$(MNVSRC)"
endif

ifdef MNVRT
MKNSISFLAGS := $(MKNSISFLAGS) -D"MNVRT=$(MNVRT)"
endif

ifdef MNVTOOLS
MKNSISFLAGS := $(MKNSISFLAGS) -D"MNVTOOLS=$(MNVTOOLS)"
endif

ifdef GETTEXT
MKNSISFLAGS := $(MKNSISFLAGS) -D"GETTEXT=$(GETTEXT)"
endif

ifdef HAVE_UPX
MKNSISFLAGS := $(MKNSISFLAGS) -DHAVE_UPX=$(HAVE_UPX)
endif

ifdef HAVE_NLS
MKNSISFLAGS := $(MKNSISFLAGS) -DHAVE_NLS=$(HAVE_NLS)
endif

ifdef HAVE_MULTI_LANG
MKNSISFLAGS := $(MKNSISFLAGS) -DHAVE_MULTI_LANG=$(HAVE_MULTI_LANG)
endif

ifdef WIN64
MKNSISFLAGS := $(MKNSISFLAGS) -DWIN64=$(WIN64)
endif

ifdef ARM64
MKNSISFLAGS := $(MKNSISFLAGS) -DARM64=$(ARM64)
endif

ifdef INCLUDE_LIBGCC
MKNSISFLAGS := $(MKNSISFLAGS) -DINCLUDE_LIBGCC=$(INCLUDE_LIBGCC)
endif

ifdef X
XX := -X"$(X:;=" -X")"
endif

MKNSISFLAGS := -INPUTCHARSET UTF8 $(MKNSISFLAGS)

all: makeinst

makeinst: prepare
	makensis $(MKNSISFLAGS) gmnv.nsi $(XX)

prepare: unzipicons license rename

unzipicons: icons.zip
	if test -d `basename $? .zip` ; then rm -rf `basename $? .zip` ; fi
	unzip $?

license: ../lang/LICENSE.*.txt ../LICENSE
	for lic in $? ; do \
		bn=`basename $$lic .txt` ; \
		awk 'sub("$$", "\r")' < $$lic | \
		iconv -f UTF-8 -t UTF-16 > ../lang/$$bn.nsis.txt ; \
	done

rename:
	../tools/rename.bat "$(SRC)" "$(DST)"

clean:
	rm -f ../lang/LICENSE*.nsis.txt
	if test -d icons ; then rm -rf icons ; fi
	if test -f gmnv??.exe ; then rm -f gmnv??.exe ; fi

# mnv: set noet sw=8 ts=8 sts=0 wm=0 tw=0 ft=make:
