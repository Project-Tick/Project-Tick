.DEFAULT_GOAL := all

export CCACHE_DISABLE ?= 1
export LC_ALL := C
export LANG := C

CC ?= cc
AWK ?= awk
CPPFLAGS += -D_GNU_SOURCE -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=700 -DNO_HISTORY \
	-I"$(CURDIR)" -I"$(CURDIR)/build/gen" -I"$(CURDIR)/../kill" \
	-I"$(CURDIR)/../../usr.bin/printf" -I"$(CURDIR)/../../bin/test"
CFLAGS ?= -O2
CFLAGS += -g -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare
LDFLAGS ?=
LDLIBS ?=

OBJDIR := $(CURDIR)/build/obj
TOOLDIR := $(CURDIR)/build/tools
GENDIR := $(CURDIR)/build/gen
OUTDIR := $(CURDIR)/out
TARGET := $(OUTDIR)/sh
TEST_RUNNER := $(TOOLDIR)/run-default-sigpipe

KILLDIR := $(CURDIR)/../kill
TESTDIR := $(CURDIR)/../../bin/test
PRINTFDIR := $(CURDIR)/../../usr.bin/printf

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
	trap.c \
	var.c
BUILTIN_SRCS := bltin/echo.c
EXTERNAL_SRCS := $(KILLDIR)/kill.c $(TESTDIR)/test.c $(PRINTFDIR)/printf.c
GEN_SRCS := $(GENDIR)/builtins.c $(GENDIR)/nodes.c $(GENDIR)/syntax.c
GEN_HDRS := $(GENDIR)/builtins.h $(GENDIR)/nodes.h $(GENDIR)/syntax.h $(GENDIR)/token.h
SRCS := $(LOCAL_SRCS) $(BUILTIN_SRCS) $(EXTERNAL_SRCS) $(GEN_SRCS)
OBJS := $(addprefix $(OBJDIR)/,$(notdir $(SRCS:.c=.o)))

vpath %.c $(CURDIR) $(CURDIR)/bltin $(KILLDIR) $(TESTDIR) $(PRINTFDIR) $(GENDIR)

.PHONY: all clean dirs test

all: $(TARGET)

dirs:
	@mkdir -p "$(OBJDIR)" "$(TOOLDIR)" "$(GENDIR)" "$(OUTDIR)"

$(TARGET): $(OBJS) | dirs
	$(CC) $(LDFLAGS) -o "$@" $(OBJS) $(LDLIBS)

$(OBJDIR)/%.o: %.c $(GEN_HDRS) | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -DSHELL -c "$<" -o "$@"

$(OBJDIR)/compat.o: $(CURDIR)/compat.c | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$<" -o "$@"

$(OBJDIR)/mode.o: $(CURDIR)/mode.c $(CURDIR)/mode.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$<" -o "$@"

$(OBJDIR)/signames.o: $(CURDIR)/signames.c $(CURDIR)/signames.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$<" -o "$@"

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

test: $(TARGET) $(TEST_RUNNER)
	SH_BIN="$(TARGET)" SH_RUNNER="$(TEST_RUNNER)" sh "$(CURDIR)/tests/test.sh"

clean:
	@rm -rf "$(CURDIR)/build" "$(CURDIR)/out"
