.DEFAULT_GOAL := all

ifeq ($(origin CC), default)
CC := /usr/bin/cc
endif
ifeq ($(origin CC), environment)
CC := /usr/bin/cc
endif

AR ?= ar
AWK ?= awk
RANLIB ?= ranlib
SH ?= sh

CPPFLAGS += -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=700 -D_PATH_TCSHELL='"/bin/csh"' -UNLS_CATALOGS
CFLAGS ?= -O2
CFLAGS += -g -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare
LDFLAGS ?=

CC_MACHINE := $(strip $(shell $(CC) -dumpmachine 2>/dev/null))
IS_MUSL := $(if $(findstring musl,$(CC) $(CC_MACHINE)),1,0)

ifeq ($(IS_MUSL),1)
CRYPTO_LIBS ?=
else
CRYPTO_LIBS ?= -lcrypt
endif
LDLIBS ?= $(CRYPTO_LIBS)

TCSHDIR := $(CURDIR)/tcsh
NCURSESDIR := $(CURDIR)/ncurses
NCINCDIR := $(NCURSESDIR)/include
NCSRCDIR := $(NCURSESDIR)/ncurses
TINFOBASEDIR := $(NCSRCDIR)/base
TINFODIR := $(NCSRCDIR)/tinfo
TINFOTTYDIR := $(NCSRCDIR)/tty
TINFOTRACEDIR := $(NCSRCDIR)/trace

OBJDIR := $(CURDIR)/build/obj
TINFOOBJDIR := $(CURDIR)/build/obj-tinfo
GENDIR := $(CURDIR)/build/gen
TINFOGENDIR := $(CURDIR)/build/gen-tinfo
OUTDIR := $(CURDIR)/out
TARGET := $(OUTDIR)/csh
TINFO_LIB := $(OUTDIR)/libtinfow.a

CONFIG_HEADERS := $(CURDIR)/config.h $(CURDIR)/config_p.h $(TCSHDIR)/config_f.h
GEN_HEADERS := $(GENDIR)/sh.err.h $(GENDIR)/ed.defns.h $(GENDIR)/tc.const.h
GEN_SOURCES := $(GENDIR)/tc.defs.c

TINFO_CPPFLAGS := $(CPPFLAGS) -DTERMIOS -I"$(TINFOGENDIR)" -I"$(NCURSESDIR)" -I"$(NCSRCDIR)" -I"$(NCINCDIR)"
TINFO_CONFIG := $(NCURSESDIR)/ncurses_cfg.h $(NCURSESDIR)/pathnames.h

NCURSES_MAJOR := 6
NCURSES_MINOR := 6
NCURSES_PATCH := 20251230
NCURSES_CH_T := cchar_t
NCURSES_CONST := const
NCURSES_INLINE := inline
NCURSES_LIBUTF8 := 0
NCURSES_MBSTATE_T := 0
NCURSES_MOUSE_VERSION := 2
NCURSES_OK_WCHAR_T := 1
NCURSES_OPAQUE := 0
NCURSES_OPAQUE_FORM := 0
NCURSES_OPAQUE_MENU := 0
NCURSES_OPAQUE_PANEL := 0
NCURSES_OSPEED := short
NCURSES_RGB_COLORS := 0
NCURSES_SBOOL := char
NCURSES_SIZE_T := short
NCURSES_TPARM_ARG := intptr_t
NCURSES_TPARM_VARARGS := 1
NCURSES_WATTR_MACROS := 1
NCURSES_WCHAR_T := 0
NCURSES_WCWIDTH_GRAPHICS := 1
NCURSES_WINT_T := 0
NCURSES_EXT_COLORS := 1
NCURSES_EXT_FUNCS := 1
NCURSES_INTEROP_FUNCS := 1
NCURSES_SP_FUNCS := 1
NCURSES_XNAMES := 1
BROKEN_LINKER := 0
BUILTIN_BOOL := 1
ENABLE_LP64 := 1
ENABLE_OPAQUE := NCURSES_OPAQUE
ENABLE_REENTRANT := 0
ENABLE_SIGWINCH := 1
HAVE_STDBOOL_H := 1
HAVE_STDINT_H := 1
HAVE_STDNORETURN_H := 0
HAVE_TCGETATTR := 1
HAVE_TERMIOS_H := 1
HAVE_TERMIO_H := 0
HAVE_VSSCANF := 1
NEED_WCHAR_H := 1
ONEUL := 1U
TYPEOF_CHTYPE := uint32_t
TYPEOF_MMASK_T := uint32_t
TYPE_OF_BOOL := unsigned char
USE_BIG_STRINGS := 1
USE_CXX_BOOL := defined(__cplusplus)

SRCS := \
	$(TCSHDIR)/sh.c \
	$(TCSHDIR)/sh.dir.c \
	$(TCSHDIR)/sh.dol.c \
	$(TCSHDIR)/sh.err.c \
	$(TCSHDIR)/sh.exec.c \
	$(TCSHDIR)/sh.char.c \
	$(TCSHDIR)/sh.exp.c \
	$(TCSHDIR)/sh.file.c \
	$(TCSHDIR)/sh.func.c \
	$(TCSHDIR)/sh.glob.c \
	$(TCSHDIR)/sh.hist.c \
	$(TCSHDIR)/sh.init.c \
	$(TCSHDIR)/sh.lex.c \
	$(TCSHDIR)/sh.misc.c \
	$(TCSHDIR)/sh.parse.c \
	$(TCSHDIR)/sh.print.c \
	$(TCSHDIR)/sh.proc.c \
	$(TCSHDIR)/sh.sem.c \
	$(TCSHDIR)/sh.set.c \
	$(TCSHDIR)/sh.time.c \
	$(TCSHDIR)/glob.c \
	$(TCSHDIR)/tw.help.c \
	$(TCSHDIR)/tw.init.c \
	$(TCSHDIR)/tw.parse.c \
	$(TCSHDIR)/tw.spell.c \
	$(TCSHDIR)/tw.comp.c \
	$(TCSHDIR)/tw.color.c \
	$(TCSHDIR)/ed.chared.c \
	$(TCSHDIR)/ed.defns.c \
	$(TCSHDIR)/ed.init.c \
	$(TCSHDIR)/ed.inputl.c \
	$(TCSHDIR)/ed.refresh.c \
	$(TCSHDIR)/ed.screen.c \
	$(TCSHDIR)/ed.xmap.c \
	$(TCSHDIR)/ed.term.c \
	$(TCSHDIR)/tc.alloc.c \
	$(TCSHDIR)/tc.bind.c \
	$(TCSHDIR)/tc.const.c \
	$(TCSHDIR)/tc.disc.c \
	$(TCSHDIR)/tc.func.c \
	$(TCSHDIR)/tc.nls.c \
	$(TCSHDIR)/tc.os.c \
	$(TCSHDIR)/tc.printf.c \
	$(TCSHDIR)/tc.prompt.c \
	$(TCSHDIR)/tc.sched.c \
	$(TCSHDIR)/tc.sig.c \
	$(TCSHDIR)/tc.str.c \
	$(TCSHDIR)/tc.vers.c \
	$(TCSHDIR)/tc.who.c \
	$(TCSHDIR)/dotlock.c \
	$(GEN_SOURCES)

TCSH_OBJS := $(addprefix $(OBJDIR)/,$(notdir $(SRCS:.c=.o)))

TINFO_BASE_SRCS := define_key.c key_defined.c keybound.c keyok.c tries.c version.c
TINFO_CORE_SRCS := \
	access.c \
	add_tries.c \
	alloc_entry.c \
	alloc_ttype.c \
	captoinfo.c \
	comp_captab.c \
	comp_error.c \
	comp_expand.c \
	comp_hash.c \
	comp_parse.c \
	comp_scan.c \
	comp_userdefs.c \
	db_iterator.c \
	doalloc.c \
	entries.c \
	free_ttype.c \
	getenv_num.c \
	hashed_db.c \
	home_terminfo.c \
	init_keytry.c \
	lib_acs.c \
	lib_baudrate.c \
	lib_cur_term.c \
	lib_data.c \
	lib_has_cap.c \
	lib_kernel.c \
	lib_longname.c \
	lib_napms.c \
	lib_options.c \
	lib_raw.c \
	lib_setup.c \
	lib_termcap.c \
	lib_termname.c \
	lib_tgoto.c \
	lib_ti.c \
	lib_tparm.c \
	lib_tputs.c \
	lib_ttyflags.c \
	name_match.c \
	obsolete.c \
	parse_entry.c \
	read_entry.c \
	read_termcap.c \
	strings.c \
	trim_sgr0.c \
	write_entry.c
TINFO_TTY_SRCS := lib_twait.c
TINFO_TRACE_SRCS := lib_trace.c visbuf.c
TINFO_GEN_SRCS := names.c codes.c lib_keyname.c unctrl.c fallback.c comp_captab.c comp_userdefs.c
TINFO_ALL_SRCS := $(TINFO_BASE_SRCS) $(TINFO_CORE_SRCS) $(TINFO_TTY_SRCS) $(TINFO_TRACE_SRCS) $(TINFO_GEN_SRCS)
TINFO_OBJS := $(addprefix $(TINFOOBJDIR)/,$(TINFO_ALL_SRCS:.c=.o))

TINFO_BASE_HEADERS := \
	$(TINFOGENDIR)/curses.h \
	$(TINFOGENDIR)/hashsize.h \
	$(TINFOGENDIR)/ncurses_def.h \
	$(TINFOGENDIR)/ncurses_dll.h \
	$(TINFOGENDIR)/parametrized.h \
	$(TINFOGENDIR)/term.h \
	$(TINFOGENDIR)/termcap.h \
	$(TINFOGENDIR)/unctrl.h

TINFO_HEADERS := $(TINFO_BASE_HEADERS) $(TINFOGENDIR)/init_keytry.h $(TINFOGENDIR)/nomacros.h

TINFO_INTERMEDIATES := \
	$(TINFOGENDIR)/MKterm.h.awk \
	$(TINFOGENDIR)/codes.c \
	$(TINFOGENDIR)/comp_captab.c \
	$(TINFOGENDIR)/comp_userdefs.c \
	$(TINFOGENDIR)/curses.head \
	$(TINFOGENDIR)/fallback.c \
	$(TINFOGENDIR)/keys.list \
	$(TINFOGENDIR)/lib_keyname.c \
	$(TINFOGENDIR)/names.c \
	$(TINFOGENDIR)/unctrl.c

vpath %.c $(TCSHDIR) $(TINFOBASEDIR) $(TINFODIR) $(TINFOTTYDIR) $(TINFOTRACEDIR) $(GENDIR) $(TINFOGENDIR)

.PHONY: all clean dirs status test

all: $(TARGET)

dirs:
	@mkdir -p "$(OBJDIR)" "$(TINFOOBJDIR)" "$(GENDIR)" "$(TINFOGENDIR)" "$(OUTDIR)"

$(TARGET): $(TCSH_OBJS) $(TINFO_LIB) | dirs
	$(CC) $(LDFLAGS) -o "$@" $(TCSH_OBJS) $(TINFO_LIB) $(LDLIBS)

$(TINFO_LIB): $(TINFO_OBJS) | dirs
	$(AR) rcs "$@" $(TINFO_OBJS)
	$(RANLIB) "$@"

$(OBJDIR)/%.o: %.c $(GEN_HEADERS) $(CONFIG_HEADERS) | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -I"$(GENDIR)" -I"$(CURDIR)" -I"$(TCSHDIR)" -c "$<" -o "$@"

$(TINFOOBJDIR)/%.o: %.c $(TINFO_HEADERS) $(TINFO_INTERMEDIATES) $(TINFO_CONFIG) | dirs
	$(CC) $(TINFO_CPPFLAGS) $(CFLAGS) -c "$<" -o "$@"

$(GENDIR)/gethost: $(TCSHDIR)/gethost.c $(GENDIR)/sh.err.h $(CONFIG_HEADERS) | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -D_h_tc_const -I"$(GENDIR)" -I"$(CURDIR)" -I"$(TCSHDIR)" "$<" -o "$@"

$(GENDIR)/tc.defs.c: $(GENDIR)/gethost $(TCSHDIR)/host.defs | dirs
	{ \
		printf '/* Do not edit this file, make creates it */\n'; \
		"$(GENDIR)/gethost" "$(TCSHDIR)/host.defs"; \
	} > "$@"

$(GENDIR)/ed.defns.h: $(TCSHDIR)/ed.defns.c | dirs
	{ \
		printf '/* Do not edit this file, make creates it. */\n'; \
		printf '#ifndef _h_ed_defns\n#define _h_ed_defns\n'; \
		LC_ALL=C grep '[FV]_' "$<" | LC_ALL=C grep '^#define'; \
		printf '#endif /* _h_ed_defns */\n'; \
	} > "$@"

$(GENDIR)/sh.err.h: $(TCSHDIR)/sh.err.c | dirs
	{ \
		printf '/* Do not edit this file, make creates it. */\n'; \
		printf '#ifndef _h_sh_err\n#define _h_sh_err\n'; \
		LC_ALL=C grep 'ERR_' "$<" | LC_ALL=C grep '^#define'; \
		printf '#endif /* _h_sh_err */\n'; \
	} > "$@"

$(GENDIR)/tc.const.h: $(TCSHDIR)/tc.const.c | dirs
	{ \
		printf '/* Do not edit this file, make creates it. */\n'; \
		printf '#ifndef _h_tc_const\n#define _h_tc_const\n'; \
		LC_ALL=C awk '/^Char STR/ { sub(/\[\].*/, "[];"); print "extern " $$0 }' "$<"; \
		printf '#endif /* _h_tc_const */\n'; \
	} > "$@"

$(TINFOGENDIR)/ncurses_def.h: $(NCINCDIR)/ncurses_defs | dirs
	AWK="$(AWK)" "$(SH)" "$(NCINCDIR)/MKncurses_def.sh" "$<" > "$@"

$(TINFOGENDIR)/ncurses_dll.h: $(NCINCDIR)/ncurses_dll.h.in | dirs
	sed < "$<" \
		-e 's%@NCURSES_WRAP_PREFIX@%_nc_%g' > "$@"

$(TINFOGENDIR)/termcap.h: $(NCINCDIR)/termcap.h.in | dirs
	sed < "$<" \
		-e '/@NCURSES_MAJOR@/s%%$(NCURSES_MAJOR)%' \
		-e '/@NCURSES_MINOR@/s%%$(NCURSES_MINOR)%' \
		-e '/@NCURSES_CONST@/s%%$(NCURSES_CONST)%' \
		-e '/@NCURSES_OSPEED@/s%%$(NCURSES_OSPEED)%' > "$@"

$(TINFOGENDIR)/unctrl.h: $(NCINCDIR)/unctrl.h.in | dirs
	sed < "$<" \
		-e 's%@NCURSES_SP_FUNCS@%$(NCURSES_SP_FUNCS)%g' \
		-e '/@NCURSES_MAJOR@/s%%$(NCURSES_MAJOR)%' \
		-e '/@NCURSES_MINOR@/s%%$(NCURSES_MINOR)%' > "$@"

$(TINFOGENDIR)/hashsize.h: $(NCINCDIR)/Caps $(NCINCDIR)/Caps-ncurses | dirs
	"$(SH)" "$(NCINCDIR)/MKhashsize.sh" "$(NCINCDIR)/Caps" "$(NCINCDIR)/Caps-ncurses" > "$@"

$(TINFOGENDIR)/parametrized.h: $(NCINCDIR)/Caps $(NCINCDIR)/Caps-ncurses | dirs
	AWK="$(AWK)" "$(SH)" "$(NCINCDIR)/MKparametrized.sh" "$(NCINCDIR)/Caps" "$(NCINCDIR)/Caps-ncurses" > "$@"

$(TINFOGENDIR)/MKterm.h.awk: $(NCINCDIR)/MKterm.h.awk.in | dirs
	sed < "$<" \
		-e '/@BROKEN_LINKER@/s%%$(BROKEN_LINKER)%' \
		-e 's%@NCURSES_USE_DATABASE@%1%g' \
		-e 's%@NCURSES_USE_TERMCAP@%1%g' \
		-e '/@NCURSES_MAJOR@/s%%$(NCURSES_MAJOR)%' \
		-e '/@NCURSES_MINOR@/s%%$(NCURSES_MINOR)%' \
		-e '/@NCURSES_CONST@/s%%$(NCURSES_CONST)%' \
		-e '/@NCURSES_TPARM_VARARGS@/s%%$(NCURSES_TPARM_VARARGS)%' \
		-e '/@NCURSES_SBOOL@/s%%$(NCURSES_SBOOL)%' \
		-e '/@NCURSES_XNAMES@/s%%$(NCURSES_XNAMES)%' \
		-e '/@NCURSES_EXT_COLORS@/s%%$(NCURSES_EXT_COLORS)%' \
		-e '/@HAVE_TERMIOS_H@/s%%$(HAVE_TERMIOS_H)%' \
		-e '/@HAVE_TERMIO_H@/s%%$(HAVE_TERMIO_H)%' \
		-e '/@HAVE_TCGETATTR@/s%%$(HAVE_TCGETATTR)%' \
		-e 's%@cf_cv_enable_reentrant@%$(ENABLE_REENTRANT)%g' \
		-e 's%@NCURSES_SP_FUNCS@%1%' \
		-e 's%@NCURSES_PATCH@%$(NCURSES_PATCH)%' > "$@"

$(TINFOGENDIR)/term.h: $(TINFOGENDIR)/MKterm.h.awk $(NCINCDIR)/Caps $(NCINCDIR)/Caps-ncurses $(NCINCDIR)/edit_cfg.sh $(NCURSESDIR)/ncurses_cfg.h | dirs
	"$(AWK)" -f "$(TINFOGENDIR)/MKterm.h.awk" "$(NCINCDIR)/Caps" "$(NCINCDIR)/Caps-ncurses" > "$@.tmp"
	"$(SH)" "$(NCINCDIR)/edit_cfg.sh" "$(NCURSESDIR)/ncurses_cfg.h" "$@.tmp"
	mv -f "$@.tmp" "$@"

$(TINFOGENDIR)/curses.head: $(NCINCDIR)/curses.h.in | dirs
	sed < "$<" \
		-e '/@BROKEN_LINKER@/s%%$(BROKEN_LINKER)%' \
		-e '/@HAVE_VSSCANF@/s%%$(HAVE_VSSCANF)%' \
		-e '/@HAVE_STDINT_H@/s%%$(HAVE_STDINT_H)%' \
		-e '/@HAVE_STDNORETURN_H@/s%%$(HAVE_STDNORETURN_H)%' \
		-e '/@NCURSES_CH_T@/s%%$(NCURSES_CH_T)%' \
		-e '/@NCURSES_CONST@/s%%$(NCURSES_CONST)%' \
		-e '/@NCURSES_EXT_COLORS@/s%%$(NCURSES_EXT_COLORS)%' \
		-e '/@NCURSES_EXT_FUNCS@/s%%$(NCURSES_EXT_FUNCS)%' \
		-e '/@NCURSES_INLINE@/s%%$(NCURSES_INLINE)%' \
		-e '/@NCURSES_LIBUTF8@/s%%$(NCURSES_LIBUTF8)%' \
		-e '/@NCURSES_MAJOR@/s%%$(NCURSES_MAJOR)%' \
		-e '/@NCURSES_MBSTATE_T@/s%%$(NCURSES_MBSTATE_T)%' \
		-e '/@NCURSES_MINOR@/s%%$(NCURSES_MINOR)%' \
		-e '/@NCURSES_MOUSE_VERSION@/s%%$(NCURSES_MOUSE_VERSION)%' \
		-e '/@NCURSES_XNAMES@/s%%$(NCURSES_XNAMES)%' \
		-e '/@NCURSES_OK_WCHAR_T@/s%%$(NCURSES_OK_WCHAR_T)%' \
		-e '/@NCURSES_OPAQUE@/s%%$(NCURSES_OPAQUE)%' \
		-e '/@NCURSES_OPAQUE_FORM@/s%%$(NCURSES_OPAQUE_FORM)%' \
		-e '/@NCURSES_OPAQUE_MENU@/s%%$(NCURSES_OPAQUE_MENU)%' \
		-e '/@NCURSES_OPAQUE_PANEL@/s%%$(NCURSES_OPAQUE_PANEL)%' \
		-e '/@NCURSES_PATCH@/s%%$(NCURSES_PATCH)%' \
		-e '/@NCURSES_RGB_COLORS@/s%%$(NCURSES_RGB_COLORS)%' \
		-e '/@NCURSES_SIZE_T@/s%%$(NCURSES_SIZE_T)%' \
		-e '/@NCURSES_TPARM_VARARGS@/s%%$(NCURSES_TPARM_VARARGS)%' \
		-e '/@NCURSES_WATTR_MACROS@/s%%$(NCURSES_WATTR_MACROS)%' \
		-e '/@NCURSES_WCHAR_T@/s%%$(NCURSES_WCHAR_T)%' \
		-e '/@NCURSES_WCWIDTH_GRAPHICS@/s%%$(NCURSES_WCWIDTH_GRAPHICS)%g' \
		-e '/@NCURSES_WINT_T@/s%%$(NCURSES_WINT_T)%' \
		-e '/@NCURSES_INTEROP_FUNCS@/s%%$(NCURSES_INTEROP_FUNCS)%g' \
		-e '/@NCURSES_TPARM_ARG@/s%%$(NCURSES_TPARM_ARG)%g' \
		-e '/@NCURSES_SP_FUNCS@/s%%$(NCURSES_SP_FUNCS)%g' \
		-e '/@NEED_WCHAR_H@/s%%$(NEED_WCHAR_H)%' \
		-e '/@USE_CXX_BOOL@/s%%$(USE_CXX_BOOL)%' \
		-e '/@USE_STDBOOL_H@/s%%$(HAVE_STDBOOL_H)%' \
		-e '/@GENERATED_EXT_FUNCS@/s%%generated%g' \
		-e 's%@NCURSES_CCHARW_MAX@%5%g' \
		-e 's%@cf_cv_1UL@%$(ONEUL)%g' \
		-e 's%@USE_BUILTIN_BOOL@%$(BUILTIN_BOOL)%g' \
		-e 's%@cf_cv_enable_lp64@%$(ENABLE_LP64)%g' \
		-e 's%@cf_cv_enable_opaque@%$(ENABLE_OPAQUE)%g' \
		-e 's%@cf_cv_enable_reentrant@%$(ENABLE_REENTRANT)%g' \
		-e 's%@cf_cv_enable_sigwinch@%$(ENABLE_SIGWINCH)%g' \
		-e 's%@cf_cv_type_of_bool@%$(TYPE_OF_BOOL)%g' \
		-e 's%@cf_cv_typeof_chtype@%$(TYPEOF_CHTYPE)%g' \
		-e 's%@cf_cv_typeof_mmask_t@%$(TYPEOF_MMASK_T)%g' \
		-e 's/ _WCHAR_T/ __wchar_t/g' \
		-e 's/ _WINT_T/ __wint_t/g' > "$@"

$(TINFOGENDIR)/keys.list: $(TINFODIR)/MKkeys_list.sh $(NCINCDIR)/Caps $(NCINCDIR)/Caps-ncurses | dirs
	AWK="$(AWK)" USE_SIGWINCH="$(ENABLE_SIGWINCH)" "$(SH)" "$(TINFODIR)/MKkeys_list.sh" \
		"$(NCINCDIR)/Caps" "$(NCINCDIR)/Caps-ncurses" | LC_ALL=C sort > "$@"

$(TINFOGENDIR)/names.c: $(TINFODIR)/MKnames.awk $(NCINCDIR)/Caps | dirs
	"$(AWK)" -f "$<" bigstrings="$(USE_BIG_STRINGS)" "$(NCINCDIR)/Caps" > "$@"

$(TINFOGENDIR)/codes.c: $(TINFODIR)/MKcodes.awk $(NCINCDIR)/Caps | dirs
	"$(AWK)" -f "$<" bigstrings="$(USE_BIG_STRINGS)" "$(NCINCDIR)/Caps" > "$@"

$(TINFOGENDIR)/lib_keyname.c: $(TINFOGENDIR)/keys.list $(TINFOBASEDIR)/MKkeyname.awk | dirs
	"$(AWK)" -f "$(TINFOBASEDIR)/MKkeyname.awk" use_sigwinch="$(ENABLE_SIGWINCH)" \
		bigstrings="$(USE_BIG_STRINGS)" "$(TINFOGENDIR)/keys.list" > "$@"

$(TINFOGENDIR)/unctrl.c: $(TINFOBASEDIR)/MKunctrl.awk | dirs
	printf '\n' | "$(AWK)" -f "$<" bigstrings="$(USE_BIG_STRINGS)" > "$@"

$(TINFOGENDIR)/fallback.c: $(TINFODIR)/MKfallback.sh | dirs
	"$(SH)" "$<" "" "" "" "" > "$@.tmp"
	mv -f "$@.tmp" "$@"

$(TINFOGENDIR)/make_hash: $(TINFODIR)/make_hash.c $(TINFOGENDIR)/hashsize.h $(TINFOGENDIR)/ncurses_def.h $(TINFO_BASE_HEADERS) | dirs
	$(CC) $(TINFO_CPPFLAGS) $(CFLAGS) -DMAIN_PROGRAM "$<" -o "$@"

$(TINFOGENDIR)/make_keys: $(TINFODIR)/make_keys.c $(TINFOGENDIR)/names.c $(TINFOGENDIR)/ncurses_def.h $(TINFO_BASE_HEADERS) | dirs
	$(CC) $(TINFO_CPPFLAGS) $(CFLAGS) "$<" -o "$@"

$(TINFOGENDIR)/comp_captab.c: $(TINFODIR)/MKcaptab.sh $(TINFODIR)/MKcaptab.awk $(NCINCDIR)/Caps $(NCINCDIR)/Caps-ncurses $(TINFOGENDIR)/make_hash | dirs
	cd "$(TINFOGENDIR)" && env PATH="$(TINFOGENDIR):$$PATH" "$(SH)" "$(TINFODIR)/MKcaptab.sh" "$(AWK)" "$(USE_BIG_STRINGS)" \
		"$(TINFODIR)/MKcaptab.awk" "$(NCINCDIR)/Caps" "$(NCINCDIR)/Caps-ncurses" > "$@"

$(TINFOGENDIR)/comp_userdefs.c: $(TINFODIR)/MKuserdefs.sh $(NCINCDIR)/Caps $(NCINCDIR)/Caps-ncurses $(TINFOGENDIR)/make_hash | dirs
	cd "$(TINFOGENDIR)" && env PATH="$(TINFOGENDIR):$$PATH" "$(SH)" "$(TINFODIR)/MKuserdefs.sh" "$(AWK)" "$(USE_BIG_STRINGS)" \
		"$(NCINCDIR)/Caps" "$(NCINCDIR)/Caps-ncurses" > "$@"

$(TINFOGENDIR)/init_keytry.h: $(TINFOGENDIR)/keys.list $(TINFOGENDIR)/make_keys | dirs
	"$(TINFOGENDIR)/make_keys" "$(TINFOGENDIR)/keys.list" > "$@"

$(TINFOGENDIR)/curses.h: $(TINFOGENDIR)/curses.head $(NCINCDIR)/Caps $(NCINCDIR)/Caps-ncurses $(NCINCDIR)/curses.wide $(NCINCDIR)/curses.tail $(NCINCDIR)/MKkey_defs.sh | dirs
	cp "$(TINFOGENDIR)/curses.head" "$@.tmp"
	AWK="$(AWK)" _POSIX2_VERSION=199209 "$(SH)" "$(NCINCDIR)/MKkey_defs.sh" \
		"$(NCINCDIR)/Caps" "$(NCINCDIR)/Caps-ncurses" >> "$@.tmp"
	cat "$(NCINCDIR)/curses.wide" >> "$@.tmp"
	cat "$(NCINCDIR)/curses.tail" >> "$@.tmp"
	mv -f "$@.tmp" "$@"

$(TINFOGENDIR)/nomacros.h: $(TINFOBASEDIR)/MKlib_gen.sh $(TINFOGENDIR)/curses.h | dirs
	LC_ALL=C "$(SH)" "$(TINFOBASEDIR)/MKlib_gen.sh" "$(CC) -E $(CFLAGS) $(TINFO_CPPFLAGS)" \
		"$(AWK)" generated < "$(TINFOGENDIR)/curses.h" | grep -F undef > "$@"

test: $(TARGET)
	LC_ALL=C CSH_BIN="$(TARGET)" sh "$(CURDIR)/tests/test.sh"

status:
	@printf '%s\n' "$(TARGET)"

clean:
	@rm -rf "$(CURDIR)/build" "$(CURDIR)/out"
