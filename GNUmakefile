.DEFAULT_GOAL := all

CC ?= cc
CPPFLAGS += -D_GNU_SOURCE -I$(CURDIR)
CFLAGS ?= -O2
CFLAGS += -g -Wall -Wextra -Wno-unused-parameter
LDFLAGS ?=
LDLIBS ?=

OBJDIR := $(CURDIR)/build
OUTDIR := $(CURDIR)/out
TARGET := $(OUTDIR)/chflags
OBJS := $(OBJDIR)/chflags.o $(OBJDIR)/flags.o $(OBJDIR)/fts.o

.PHONY: all clean dirs test status

all: $(TARGET)

dirs:
	@mkdir -p "$(OBJDIR)" "$(OUTDIR)"

$(TARGET): $(OBJS) | dirs
	$(CC) $(LDFLAGS) -o "$@" $(OBJS) $(LDLIBS)

$(OBJDIR)/chflags.o: $(CURDIR)/chflags.c $(CURDIR)/flags.h $(CURDIR)/fts.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$(CURDIR)/chflags.c" -o "$@"

$(OBJDIR)/flags.o: $(CURDIR)/flags.c $(CURDIR)/flags.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$(CURDIR)/flags.c" -o "$@"

$(OBJDIR)/fts.o: $(CURDIR)/fts.c $(CURDIR)/fts.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$(CURDIR)/fts.c" -o "$@"

test: $(TARGET)
	CHFLAGS_BIN="$(TARGET)" sh "$(CURDIR)/tests/test.sh"

status:
	@printf '%s\n' "$(TARGET)"

clean:
	@rm -rf "$(CURDIR)/build" "$(CURDIR)/out"
