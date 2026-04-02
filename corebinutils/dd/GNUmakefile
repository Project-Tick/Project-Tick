.DEFAULT_GOAL := all

CC ?= cc
CPPFLAGS += -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -I$(CURDIR)
CFLAGS ?= -O2
CFLAGS += -std=c17 -g -Wall -Wextra -Wno-unused-parameter
LDFLAGS ?=
LDLIBS ?=

OBJDIR := $(CURDIR)/build
OUTDIR := $(CURDIR)/out
TARGET := $(OUTDIR)/dd
GEN := $(OUTDIR)/dd-gen
OBJS := \
	$(OBJDIR)/args.o \
	$(OBJDIR)/conv.o \
	$(OBJDIR)/conv_tab.o \
	$(OBJDIR)/dd.o \
	$(OBJDIR)/misc.o \
	$(OBJDIR)/position.o

.PHONY: all clean dirs status test

all: $(TARGET)

dirs:
	@mkdir -p "$(OBJDIR)" "$(OUTDIR)"

$(TARGET): $(OBJS) | dirs
	$(CC) $(LDFLAGS) -o "$@" $(OBJS) $(LDLIBS)

$(GEN): $(OBJDIR)/gen.o | dirs
	$(CC) $(LDFLAGS) -o "$@" "$(OBJDIR)/gen.o" $(LDLIBS)

$(OBJDIR)/args.o: $(CURDIR)/args.c $(CURDIR)/dd.h $(CURDIR)/extern.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$(CURDIR)/args.c" -o "$@"

$(OBJDIR)/conv.o: $(CURDIR)/conv.c $(CURDIR)/dd.h $(CURDIR)/extern.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$(CURDIR)/conv.c" -o "$@"

$(OBJDIR)/conv_tab.o: $(CURDIR)/conv_tab.c $(CURDIR)/dd.h $(CURDIR)/extern.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$(CURDIR)/conv_tab.c" -o "$@"

$(OBJDIR)/dd.o: $(CURDIR)/dd.c $(CURDIR)/dd.h $(CURDIR)/extern.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$(CURDIR)/dd.c" -o "$@"

$(OBJDIR)/misc.o: $(CURDIR)/misc.c $(CURDIR)/dd.h $(CURDIR)/extern.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$(CURDIR)/misc.c" -o "$@"

$(OBJDIR)/position.o: $(CURDIR)/position.c $(CURDIR)/dd.h $(CURDIR)/extern.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$(CURDIR)/position.c" -o "$@"

$(OBJDIR)/gen.o: $(CURDIR)/gen.c | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$(CURDIR)/gen.c" -o "$@"

test: $(TARGET) $(GEN)
	DD_BIN="$(TARGET)" DD_GEN="$(GEN)" sh "$(CURDIR)/tests/test.sh"

status:
	@printf '%s\n' "$(TARGET)"

clean:
	@rm -rf "$(OBJDIR)" "$(OUTDIR)"
