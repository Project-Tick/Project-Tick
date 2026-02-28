.DEFAULT_GOAL := all

CC ?= cc
CPPFLAGS += -D_GNU_SOURCE
CFLAGS ?= -O2
CFLAGS += -g -Wall -Wextra -Wno-unused-parameter
LDFLAGS ?=
LDLIBS ?=

OBJDIR := $(CURDIR)/build
OUTDIR := $(CURDIR)/out
TARGET := $(OUTDIR)/cat
OBJS := $(OBJDIR)/cat.o

.PHONY: all clean dirs test

all: $(TARGET)

dirs:
	@mkdir -p "$(OBJDIR)" "$(OUTDIR)"

$(TARGET): $(OBJS) | dirs
	$(CC) $(LDFLAGS) -o "$@" $(OBJS) $(LDLIBS)

$(OBJDIR)/cat.o: $(CURDIR)/cat.c | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$(CURDIR)/cat.c" -o "$@"

test: $(TARGET)
	CAT_BIN="$(TARGET)" sh "$(CURDIR)/tests/test.sh"

clean:
	@rm -rf "$(OBJDIR)" "$(OUTDIR)"
