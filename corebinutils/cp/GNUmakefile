.DEFAULT_GOAL := all

CC ?= cc
CPPFLAGS += -D_GNU_SOURCE -I$(CURDIR)
CFLAGS ?= -O2
CFLAGS += -g -Wall -Wextra -Wno-unused-parameter
LDFLAGS ?=
LDLIBS ?=

OBJDIR := $(CURDIR)/build
OUTDIR := $(CURDIR)/out
TARGET := $(OUTDIR)/cp
OBJS := $(OBJDIR)/cp.o $(OBJDIR)/utils.o $(OBJDIR)/fts.o

.PHONY: all clean dirs test status

all: $(TARGET)

dirs:
	@mkdir -p "$(OBJDIR)" "$(OUTDIR)"

$(TARGET): $(OBJS) | dirs
	$(CC) $(LDFLAGS) -o "$@" $(OBJS) $(LDLIBS)

$(OBJDIR)/cp.o: $(CURDIR)/cp.c $(CURDIR)/extern.h $(CURDIR)/fts.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$(CURDIR)/cp.c" -o "$@"

$(OBJDIR)/utils.o: $(CURDIR)/utils.c $(CURDIR)/extern.h $(CURDIR)/fts.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$(CURDIR)/utils.c" -o "$@"

$(OBJDIR)/fts.o: $(CURDIR)/fts.c $(CURDIR)/fts.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$(CURDIR)/fts.c" -o "$@"

test: $(TARGET)
	CP_BIN="$(TARGET)" sh "$(CURDIR)/tests/test.sh"

status:
	@printf '%s\n' "$(TARGET)"

clean:
	@rm -rf "$(CURDIR)/build" "$(CURDIR)/out"
