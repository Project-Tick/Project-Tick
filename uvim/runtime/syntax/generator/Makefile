#
# Makefile for generate runtime/syntax/mnv.mnv
#
MNV_SRCDIR := ../../../src
RUN_MNVPROG := $(MNV_SRCDIR)/mnv -N -u NONE -i NONE -n
TARGET := ../mnv.mnv
CHECK_HELP_DOC := 0

SRC :=	$(MNV_SRCDIR)/eval.c $(MNV_SRCDIR)/ex_cmds.h $(MNV_SRCDIR)/ex_docmd.c \
		$(MNV_SRCDIR)/fileio.c $(MNV_SRCDIR)/option.c $(MNV_SRCDIR)/syntax.c

export MNV_SRCDIR
export CHECK_HELP_DOC

.PHONY: generate check_doc clean
all: generate

generate: $(TARGET)

check_doc: CHECK_HELP_DOC := 1
check_doc: clean $(TARGET)

clean:
	rm -f mnv.mnv.rc $(TARGET)
	rm -f sanity_check.err generator.err

$(TARGET): mnv.mnv.rc update_date.mnv
	@echo "Generating $(TARGET) ..."
	@cp -f mnv.mnv.rc $(TARGET)
	@$(RUN_MNVPROG) -S update_date.mnv $(TARGET)
	@echo "done."

mnv.mnv.rc: gen_syntax_mnv.mnv mnv.mnv.base $(SRC)
	@echo "Generating mnv.mnv.rc ..."
	@rm -f sanity_check.err generator.err
	@$(RUN_MNVPROG) -S gen_syntax_mnv.mnv $(TARGET)
	@if test -f sanity_check.err ; then \
		echo ; \
		echo "Sanity errors:" ; \
		cat sanity_check.err ; \
	fi
	@if test -f generator.err ; then \
		echo ; \
		echo "Generator errors:" ; \
		cat generator.err ; \
	fi
	@if test -f sanity_check.err || test -f generator.err ; then \
		exit 1 ; \
	fi
	@echo "done."
