.DEFAULT_GOAL := all

export CCACHE_DISABLE ?= 1
export LC_ALL := C
export LANG := C

CC ?= cc
AWK ?= awk
CPPFLAGS += -D_GNU_SOURCE -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=700 \
	-I"$(CURDIR)" -I"$(CURDIR)/build/gen" -I"$(CURDIR)/../kill" \
	-I"$(CURDIR)/../../usr.bin/printf" -I"$(CURDIR)/../../bin/test"
CFLAGS ?= -O2
CFLAGS += -g -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare
LDFLAGS ?=
LDLIBS ?=
EDITLINE_LIBS ?=
EDITLINE_CPPFLAGS ?=

OBJDIR := $(CURDIR)/build/obj
TOOLDIR := $(CURDIR)/build/tools
GENDIR := $(CURDIR)/build/gen
OUTDIR := $(CURDIR)/out
TARGET := $(OUTDIR)/sh
TEST_RUNNER := $(TOOLDIR)/run-default-sigpipe
LIBEDITDIR := $(CURDIR)/../../contrib/libedit
LIBEDIT_GENDIR := $(GENDIR)/libedit
VISDIR := $(CURDIR)/../../contrib/libc-vis

KILLDIR := $(CURDIR)/../kill
TESTDIR := $(CURDIR)/../../bin/test
PRINTFDIR := $(CURDIR)/../../usr.bin/printf

CPPFLAGS += -I"$(LIBEDIT_GENDIR)" -I"$(LIBEDITDIR)"
LIBEDIT_CPPFLAGS := $(CPPFLAGS) $(EDITLINE_CPPFLAGS) -include "$(CURDIR)/compat.h" -I"$(VISDIR)"

LOCAL_SRCS := \
	alias.c \
	arith_yacc.c \
	arith_yylex.c \
	cd.c \
	compat.c \
	error.c \
	eval.c \
	exec.c \
	expand.c \
	histedit.c \
	input.c \
	jobs.c \
	mail.c \
	main.c \
	memalloc.c \
	miscbltin.c \
	mode.c \
	mystring.c \
	options.c \
	output.c \
	parser.c \
	redir.c \
	show.c \
	signames.c \
	termcap.c \
	trap.c \
	var.c
BUILTIN_SRCS := bltin/echo.c testcmd.c
EXTERNAL_SRCS := $(KILLDIR)/kill.c $(PRINTFDIR)/printf.c
GEN_SRCS := $(GENDIR)/builtins.c $(GENDIR)/nodes.c $(GENDIR)/syntax.c
GEN_HDRS := $(GENDIR)/builtins.h $(GENDIR)/nodes.h $(GENDIR)/syntax.h $(GENDIR)/token.h
LIBEDIT_SRCS := \
	chared.c \
	chartype.c \
	common.c \
	el.c \
	eln.c \
	emacs.c \
	filecomplete.c \
	hist.c \
	history.c \
	historyn.c \
	keymacro.c \
	literal.c \
	map.c \
	parse.c \
	prompt.c \
	read.c \
	refresh.c \
	search.c \
	sig.c \
	terminal.c \
	tokenizer.c \
	tokenizern.c \
	tty.c \
	vi.c
LIBEDIT_GEN_HDRS := $(LIBEDIT_GENDIR)/vi.h $(LIBEDIT_GENDIR)/emacs.h \
	$(LIBEDIT_GENDIR)/common.h $(LIBEDIT_GENDIR)/fcns.h \
	$(LIBEDIT_GENDIR)/func.h $(LIBEDIT_GENDIR)/help.h
VIS_SRCS := vis.c unvis.c
SRCS := $(LOCAL_SRCS) $(BUILTIN_SRCS) $(EXTERNAL_SRCS) $(GEN_SRCS)
OBJS := $(addprefix $(OBJDIR)/,$(notdir $(SRCS:.c=.o)))
LIBEDIT_OBJS := $(addprefix $(OBJDIR)/libedit-,$(LIBEDIT_SRCS:.c=.o))
VIS_OBJS := $(addprefix $(OBJDIR)/vis-,$(VIS_SRCS:.c=.o))

vpath %.c $(CURDIR) $(CURDIR)/bltin $(KILLDIR) $(TESTDIR) $(PRINTFDIR) $(GENDIR)

.PHONY: all clean dirs test

all: $(TARGET)

dirs:
	@mkdir -p "$(OBJDIR)" "$(TOOLDIR)" "$(GENDIR)" "$(OUTDIR)" "$(LIBEDIT_GENDIR)"

$(TARGET): $(OBJS) $(LIBEDIT_OBJS) $(VIS_OBJS) | dirs
	$(CC) $(LDFLAGS) -o "$@" $(OBJS) $(LIBEDIT_OBJS) $(VIS_OBJS) $(LDLIBS) $(EDITLINE_LIBS)

$(OBJDIR)/%.o: %.c $(GEN_HDRS) | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -DSHELL -c "$<" -o "$@"

$(OBJDIR)/compat.o: $(CURDIR)/compat.c | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$<" -o "$@"

$(OBJDIR)/mode.o: $(CURDIR)/mode.c $(CURDIR)/mode.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$<" -o "$@"

$(OBJDIR)/signames.o: $(CURDIR)/signames.c $(CURDIR)/signames.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$<" -o "$@"

$(OBJDIR)/libedit-%.o: $(LIBEDITDIR)/%.c $(LIBEDIT_GEN_HDRS) $(LIBEDITDIR)/histedit.h $(LIBEDITDIR)/filecomplete.h | dirs
	$(CC) $(LIBEDIT_CPPFLAGS) $(CFLAGS) -c "$<" -o "$@"

$(OBJDIR)/vis-%.o: $(VISDIR)/%.c $(CURDIR)/namespace.h $(VISDIR)/vis.h | dirs
	$(CC) $(LIBEDIT_CPPFLAGS) $(CFLAGS) -c "$<" -o "$@"

$(TOOLDIR)/mknodes: $(CURDIR)/mknodes.c | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) "$<" -o "$@"

$(TOOLDIR)/mksyntax: $(CURDIR)/mksyntax.c $(CURDIR)/parser.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) "$<" -o "$@"

$(TEST_RUNNER): $(CURDIR)/tests/run-default-sigpipe.c | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) "$<" -o "$@"

$(GENDIR)/builtins.c $(GENDIR)/builtins.h: $(CURDIR)/mkbuiltins $(CURDIR)/builtins.def | dirs
	cd "$(GENDIR)" && sh "$(CURDIR)/mkbuiltins" "$(CURDIR)"

$(GENDIR)/nodes.c $(GENDIR)/nodes.h: $(TOOLDIR)/mknodes $(CURDIR)/nodetypes $(CURDIR)/nodes.c.pat | dirs
	cd "$(GENDIR)" && "$(TOOLDIR)/mknodes" "$(CURDIR)/nodetypes" "$(CURDIR)/nodes.c.pat"

$(GENDIR)/syntax.c $(GENDIR)/syntax.h: $(TOOLDIR)/mksyntax $(CURDIR)/parser.h | dirs
	cd "$(GENDIR)" && "$(TOOLDIR)/mksyntax"

$(GENDIR)/token.h: $(CURDIR)/mktokens | dirs
	cd "$(GENDIR)" && sh "$(CURDIR)/mktokens"

$(LIBEDIT_GENDIR)/vi.h: $(LIBEDITDIR)/vi.c $(LIBEDITDIR)/makelist | dirs
	cd "$(LIBEDIT_GENDIR)" && sh "$(LIBEDITDIR)/makelist" -h "$(LIBEDITDIR)/vi.c" > vi.h

$(LIBEDIT_GENDIR)/emacs.h: $(LIBEDITDIR)/emacs.c $(LIBEDITDIR)/makelist | dirs
	cd "$(LIBEDIT_GENDIR)" && sh "$(LIBEDITDIR)/makelist" -h "$(LIBEDITDIR)/emacs.c" > emacs.h

$(LIBEDIT_GENDIR)/common.h: $(LIBEDITDIR)/common.c $(LIBEDITDIR)/makelist | dirs
	cd "$(LIBEDIT_GENDIR)" && sh "$(LIBEDITDIR)/makelist" -h "$(LIBEDITDIR)/common.c" > common.h

$(LIBEDIT_GENDIR)/fcns.h: $(LIBEDIT_GENDIR)/vi.h $(LIBEDIT_GENDIR)/emacs.h $(LIBEDIT_GENDIR)/common.h $(LIBEDITDIR)/makelist | dirs
	cd "$(LIBEDIT_GENDIR)" && sh "$(LIBEDITDIR)/makelist" -fh vi.h emacs.h common.h > fcns.h

$(LIBEDIT_GENDIR)/func.h: $(LIBEDIT_GENDIR)/vi.h $(LIBEDIT_GENDIR)/emacs.h $(LIBEDIT_GENDIR)/common.h $(LIBEDITDIR)/makelist | dirs
	cd "$(LIBEDIT_GENDIR)" && sh "$(LIBEDITDIR)/makelist" -fc vi.h emacs.h common.h > func.h

$(LIBEDIT_GENDIR)/help.h: $(LIBEDITDIR)/vi.c $(LIBEDITDIR)/emacs.c $(LIBEDITDIR)/common.c $(LIBEDITDIR)/makelist | dirs
	cd "$(LIBEDIT_GENDIR)" && sh "$(LIBEDITDIR)/makelist" -bh "$(LIBEDITDIR)/vi.c" "$(LIBEDITDIR)/emacs.c" "$(LIBEDITDIR)/common.c" > help.h

test: $(TARGET) $(TEST_RUNNER)
	SH_BIN="$(TARGET)" SH_RUNNER="$(TEST_RUNNER)" sh "$(CURDIR)/tests/test.sh"

clean:
	@rm -rf "$(CURDIR)/build" "$(CURDIR)/out"
