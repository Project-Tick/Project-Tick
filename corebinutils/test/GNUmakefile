.DEFAULT_GOAL := all

CC ?= cc
CPPFLAGS ?=
CPPFLAGS += -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE
CFLAGS ?= -O2
CFLAGS += -std=c17 -g -Wall -Wextra -Werror -Wpedantic
LDFLAGS ?=
LDLIBS ?=

OBJDIR := $(CURDIR)/build
OUTDIR := $(CURDIR)/out
TARGET := $(OUTDIR)/test
BRACKET_TARGET := $(OUTDIR)/[
FD_HELPER := $(OBJDIR)/fd_helper
OBJS := $(OBJDIR)/test.o

.PHONY: all clean dirs status test

all: $(TARGET) $(BRACKET_TARGET)

dirs:
	@mkdir -p "$(OBJDIR)" "$(OUTDIR)"

$(TARGET): $(OBJS) | dirs
	$(CC) $(LDFLAGS) -o "$@" $(OBJS) $(LDLIBS)

$(BRACKET_TARGET): $(TARGET) | dirs
	ln -sf "test" "$@"

$(OBJDIR)/test.o: $(CURDIR)/test.c | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$(CURDIR)/test.c" -o "$@"

$(FD_HELPER): $(CURDIR)/tests/fd_helper.c | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) "$(CURDIR)/tests/fd_helper.c" -o "$@" $(LDFLAGS) $(LDLIBS)

test: $(TARGET) $(BRACKET_TARGET) $(FD_HELPER)
	TEST_BIN="$(TARGET)" BRACKET_BIN="$(BRACKET_TARGET)" \
	  FD_HELPER_BIN="$(FD_HELPER)" \
	  sh "$(CURDIR)/tests/test.sh"

status:
	@printf '%s\n' "$(TARGET)"

clean:
	@rm -rf "$(OBJDIR)" "$(OUTDIR)"
