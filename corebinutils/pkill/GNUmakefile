.DEFAULT_GOAL := all

CC ?= cc
CPPFLAGS ?=
CPPFLAGS += -D_GNU_SOURCE
CFLAGS ?= -O2
CFLAGS += -std=c17 -g -Wall -Wextra -Werror
LDFLAGS ?=
LDLIBS ?=

OBJDIR := $(CURDIR)/build
OUTDIR := $(CURDIR)/out
TARGET := $(OUTDIR)/pkill
PGREP  := $(OUTDIR)/pgrep
OBJS   := $(OBJDIR)/pkill.o

HELPER_SRC := $(CURDIR)/tests/spin_helper.c
HELPER_BIN := $(OUTDIR)/spin_helper

.PHONY: all clean dirs status test

all: $(TARGET) $(PGREP) $(HELPER_BIN)

dirs:
	@mkdir -p "$(OBJDIR)" "$(OUTDIR)"

$(TARGET): $(OBJS) | dirs
	$(CC) $(LDFLAGS) -o "$@" $(OBJS) $(LDLIBS)

$(PGREP): $(TARGET) | dirs
	ln -sf pkill "$@"

$(OBJDIR)/pkill.o: $(CURDIR)/pkill.c | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$(CURDIR)/pkill.c" -o "$@"

$(HELPER_BIN): $(HELPER_SRC) | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) "$<" -o "$@" $(LDLIBS)

test: $(TARGET) $(PGREP) $(HELPER_BIN)
	CC="$(CC)" PKILL_BIN="$(TARGET)" PGREP_BIN="$(PGREP)" \
	  HELPER_BIN="$(HELPER_BIN)" sh "$(CURDIR)/tests/test.sh"

status:
	@printf '%s\n' "$(TARGET)"

clean:
	@rm -rf "$(OBJDIR)" "$(OUTDIR)"
