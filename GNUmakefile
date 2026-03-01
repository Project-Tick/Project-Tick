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
TARGET := $(OUTDIR)/pwait
OBJS   := $(OBJDIR)/pwait.o

.PHONY: all clean dirs status test

all: $(TARGET)

dirs:
	@mkdir -p "$(OBJDIR)" "$(OUTDIR)"

$(TARGET): $(OBJS) | dirs
	$(CC) $(LDFLAGS) -o "$@" $(OBJS) $(LDLIBS)

$(OBJDIR)/pwait.o: $(CURDIR)/pwait.c | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$(CURDIR)/pwait.c" -o "$@"

test: $(TARGET)
	CC="$(CC)" PWAIT_BIN="$(TARGET)" \
	  sh "$(CURDIR)/tests/test.sh"

status:
	@printf '%s\n' "$(TARGET)"

clean:
	@rm -rf "$(OBJDIR)" "$(OUTDIR)"
