.DEFAULT_GOAL := all

CC ?= cc
CPPFLAGS ?=
CFLAGS ?= -O2
CFLAGS += -std=c17 -g -Wall -Wextra -Werror
LDFLAGS ?=
LDLIBS ?=

OBJDIR := $(CURDIR)/build
OUTDIR := $(CURDIR)/out
TARGET := $(OUTDIR)/uuidgen
OBJS := $(OBJDIR)/uuidgen.o

.PHONY: all clean dirs status test

all: $(TARGET)

dirs:
	@mkdir -p "$(OBJDIR)" "$(OUTDIR)"

$(TARGET): $(OBJS) | dirs
	$(CC) $(LDFLAGS) -o "$@" $(OBJS) $(LDLIBS)

$(OBJDIR)/uuidgen.o: $(CURDIR)/uuidgen.c | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$(CURDIR)/uuidgen.c" -o "$@"

test: $(TARGET)
	CC="$(CC)" UUIDGEN_BIN="$(TARGET)" sh "$(CURDIR)/tests/test.sh"

status:
	@printf '%s\n' "$(TARGET)"

clean:
	@rm -rf "$(OBJDIR)" "$(OUTDIR)"
