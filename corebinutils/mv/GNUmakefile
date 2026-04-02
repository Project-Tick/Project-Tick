.DEFAULT_GOAL := all

CC ?= cc
CCACHE_DISABLE ?= 1
CPPFLAGS += -D_POSIX_C_SOURCE=200809L -D_FILE_OFFSET_BITS=64
CFLAGS ?= -O2
CFLAGS += -std=c17 -g -Wall -Wextra -Werror
LDFLAGS ?=
LDLIBS ?=

OBJDIR := $(CURDIR)/build
OUTDIR := $(CURDIR)/out
TARGET := $(OUTDIR)/mv
OBJS := $(OBJDIR)/mv.o

.PHONY: all clean dirs status test

all: $(TARGET)

dirs:
	@mkdir -p "$(OBJDIR)" "$(OUTDIR)"

$(TARGET): $(OBJS) | dirs
	env CCACHE_DISABLE="$(CCACHE_DISABLE)" $(CC) $(LDFLAGS) -o "$@" $(OBJS) $(LDLIBS)

$(OBJDIR)/mv.o: $(CURDIR)/mv.c | dirs
	env CCACHE_DISABLE="$(CCACHE_DISABLE)" $(CC) $(CPPFLAGS) $(CFLAGS) -c "$(CURDIR)/mv.c" -o "$@"

test: $(TARGET)
	MV_BIN="$(TARGET)" sh "$(CURDIR)/tests/test.sh"

status:
	@printf '%s\n' "$(TARGET)"

clean:
	@rm -rf "$(OBJDIR)" "$(OUTDIR)"
