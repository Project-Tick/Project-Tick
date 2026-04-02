.DEFAULT_GOAL := all

CC ?= cc
CPPFLAGS ?=
CPPFLAGS += -D_POSIX_C_SOURCE=200809L
CFLAGS ?= -O2
CFLAGS += -std=c17 -g -Wall -Wextra -Werror
LDFLAGS ?=
LDLIBS ?=

OBJDIR := $(CURDIR)/build
OUTDIR := $(CURDIR)/out
TARGET := $(OUTDIR)/ln
LINK_TARGET := $(OUTDIR)/link
OBJS := $(OBJDIR)/ln.o

.PHONY: all clean dirs status test

all: $(TARGET) $(LINK_TARGET)

dirs:
	@mkdir -p "$(OBJDIR)" "$(OUTDIR)"

$(TARGET): $(OBJS) | dirs
	$(CC) $(LDFLAGS) -o "$@" $(OBJS) $(LDLIBS)

$(LINK_TARGET): $(TARGET) | dirs
	ln -sf "ln" "$@"

$(OBJDIR)/ln.o: $(CURDIR)/ln.c | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$(CURDIR)/ln.c" -o "$@"

test: $(TARGET) $(LINK_TARGET)
	LN_BIN="$(TARGET)" LINK_BIN="$(LINK_TARGET)" sh "$(CURDIR)/tests/test.sh"

status:
	@printf '%s\n' "$(TARGET)"

clean:
	@rm -rf "$(OBJDIR)" "$(OUTDIR)"
