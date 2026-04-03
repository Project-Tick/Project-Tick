#
# Makefile for the MNV documentation on Unix
#
# If you get "don't know how to make scratch", first run make in the source
# directory.  Or remove the include below.

AWK = awk

MNVPROG = ../../src/mnv

# include the config.mk from the source directory.  It's only needed to set
# AWK, used for "make html".  Comment this out if the include gives problems.
include ../../src/auto/config.mk

# Common components
include Make_all.mak

.SUFFIXES:
.SUFFIXES: .c .o .txt .html
.PHONY: all mnvtags noerrors perlhtml clean test_urls

all: tags mnv.man emnv.man mnvdiff.man mnvtutor.man xxd.man $(CONVERTED)

# Use MNV to generate the tags file.  Can only be used when MNV has been
# compiled.  Supports multiple languages.
mnvtags: $(DOCS)
	@$(MNVPROG) --clean -esX -V1 -u doctags.mnv

# Use "doctags" to generate the tags file.  Only works for English!
tags: doctags $(DOCS)
	./doctags $(DOCS) | LANG=C LC_ALL=C sort >$@
	uniq -d -2 tags

doctags: doctags.c
	$(CC) doctags.c -o doctags

mnv.man: mnv.1
	nroff -c -man $? | sed -e s/.//g > $@

emnv.man: emnv.1
	nroff -c -man $? | sed -e s/.//g > $@

mnvdiff.man: mnvdiff.1
	nroff -c -man $? | sed -e s/.//g > $@

mnvtutor.man: mnvtutor.1
	nroff -c -man $? | sed -e s/.//g > $@

xxd.man: xxd.1
	nroff -c -man $? | sed -e s/.//g > $@

# Awk version of .txt to .html conversion.
html: noerrors tags $(HTMLS)
	@if test -f errors.log; then more errors.log; fi

noerrors:
	-rm -f errors.log

$(HTMLS): tags.ref

.txt.html:
	$(AWK) -f makehtml.awk $< >$@

# index.html is the starting point for HTML, but for the help files it is
# help.txt.  Therefore use mnvindex.html for index.txt.
index.html: help.txt
	$(AWK) -f makehtml.awk $? >$@

mnvindex.html: index.txt
	$(AWK) -f makehtml.awk $? >$@

tags.ref tags.html: tags
	$(AWK) -f maketags.awk tags >tags.html

# Perl version of .txt to .html conversion.
# There can't be two rules to produce a .html from a .txt file.
# Just run over all .txt files each time one changes.  It's fast anyway.
perlhtml: tags $(DOCS)
	./mnv2html.pl tags $(DOCS)

# Check URLs in the help with "curl".
test_urls:
	$(MNVPROG) --clean -S test_urls.mnv

clean:
	-rm -f doctags *.html tags.ref tags-* mnv-stylesheet.css

# These files are in the extra archive, skip if not present

arabic.txt:
	touch $@

farsi.txt:
	touch $@

hebrew.txt:
	touch $@

russian.txt:
	touch $@

gui_w32.txt:
	touch $@

if_ole.txt:
	touch $@

os_390.txt:
	touch $@

os_amiga.txt:
	touch $@

os_beos.txt:
	touch $@

os_dos.txt:
	touch $@

os_haiku.txt:
	touch $@

os_mac.txt:
	touch $@

os_mint.txt:
	touch $@

os_msdos.txt:
	touch $@

os_os2.txt:
	touch $@

os_qnx.txt:
	touch $@

os_risc.txt:
	touch $@

os_win32.txt:
	touch $@

pi_netrw.txt: ../pack/dist/opt/netrw/doc/netrw.txt
	cp ../pack/dist/opt/netrw/doc/netrw.txt $@.tmp
	sed -e '1s/$$/	*pi_netrw.txt*/' $@.tmp > $@ && \
	rm -f $@.tmp

vietnamese.txt:
	touch $@

# In *BSD, the variable '$<' is used in suffix-transformation rules (in GNU this
# is called "implicit rules", and in MS Windows it is called "inference rules").
# For code portability, it is better to use the '$?' variable for explicit rules.
# Especially since there is only one dependency and it should presumably always
# be newer than the target file.
mnv-da.UTF-8.1: mnv-da.1
	iconv -f latin1 -t UTF-8 $? >$@

mnvdiff-da.UTF-8.1: mnvdiff-da.1
	iconv -f latin1 -t UTF-8 $? >$@

mnvtutor-da.UTF-8.1: mnvtutor-da.1
	iconv -f latin1 -t UTF-8 $? >$@

mnv-de.UTF-8.1: mnv-de.1
	iconv -f latin1 -t UTF-8 $? >$@

mnv-fr.UTF-8.1: mnv-fr.1
	iconv -f latin1 -t UTF-8 $? >$@

emnv-fr.UTF-8.1: emnv-fr.1
	iconv -f latin1 -t UTF-8 $? >$@

mnvdiff-fr.UTF-8.1: mnvdiff-fr.1
	iconv -f latin1 -t UTF-8 $? >$@

mnvtutor-fr.UTF-8.1: mnvtutor-fr.1
	iconv -f latin1 -t UTF-8 $? >$@

xxd-fr.UTF-8.1: xxd-fr.1
	iconv -f latin1 -t UTF-8 $? >$@

mnv-it.UTF-8.1: mnv-it.1
	iconv -f latin1 -t UTF-8 $? >$@

emnv-it.UTF-8.1: emnv-it.1
	iconv -f latin1 -t UTF-8 $? >$@

mnvdiff-it.UTF-8.1: mnvdiff-it.1
	iconv -f latin1 -t UTF-8 $? >$@

mnvtutor-it.UTF-8.1: mnvtutor-it.1
	iconv -f latin1 -t UTF-8 $? >$@

xxd-it.UTF-8.1: xxd-it.1
	iconv -f latin1 -t UTF-8 $? >$@

mnv-pl.UTF-8.1: mnv-pl.1
	iconv -f latin2 -t UTF-8 $? >$@

emnv-pl.UTF-8.1: emnv-pl.1
	iconv -f latin2 -t UTF-8 $? >$@

mnvdiff-pl.UTF-8.1: mnvdiff-pl.1
	iconv -f latin2 -t UTF-8 $? >$@

mnvtutor-pl.UTF-8.1: mnvtutor-pl.1
	iconv -f latin2 -t UTF-8 $? >$@

xxd-pl.UTF-8.1: xxd-pl.1
	iconv -f latin2 -t UTF-8 $? >$@

mnv-ru.UTF-8.1: mnv-ru.1
	iconv -f KOI8-R -t UTF-8 $? >$@

emnv-ru.UTF-8.1: emnv-ru.1
	iconv -f KOI8-R -t UTF-8 $? >$@

mnvdiff-ru.UTF-8.1: mnvdiff-ru.1
	iconv -f KOI8-R -t UTF-8 $? >$@

mnvtutor-ru.UTF-8.1: mnvtutor-ru.1
	iconv -f KOI8-R -t UTF-8 $? >$@

xxd-ru.UTF-8.1: xxd-ru.1
	iconv -f KOI8-R -t UTF-8 $? >$@

mnv-tr.UTF-8.1: mnv-tr.1
	iconv -f latin5 -t UTF-8 $? >$@

emnv-tr.UTF-8.1: emnv-tr.1
	iconv -f latin5 -t UTF-8 $? >$@

mnvdiff-tr.UTF-8.1: mnvdiff-tr.1
	iconv -f latin5 -t UTF-8 $? >$@

mnvtutor-tr.UTF-8.1: mnvtutor-tr.1
	iconv -f latin5 -t UTF-8 $? >$@

mnv-sv.UTF-8.1: mnv-sv.1
	iconv -f latin1 -t UTF-8 $? >$@

emnv-sv.UTF-8.1: emnv-sv.1
	iconv -f latin1 -t UTF-8 $? >$@

mnvdiff-sv.UTF-8.1: mnvdiff-sv.1
	iconv -f latin1 -t UTF-8 $? >$@

mnvtutor-sv.UTF-8.1: mnvtutor-sv.1
	iconv -f latin1 -t UTF-8 $? >$@

xxd-sv.UTF-8.1: xxd-sv.1
	iconv -f latin1 -t UTF-8 $? >$@
