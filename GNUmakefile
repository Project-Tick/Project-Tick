.DEFAULT_GOAL := all

CC ?= cc
CPPFLAGS ?=
CPPFLAGS += -D_GNU_SOURCE
CFLAGS ?= -O2
CFLAGS += -std=c17 -g -Wall -Wextra -Werror -Wno-unused-parameter
LDFLAGS ?=
LDLIBS ?= -lm

OBJDIR := $(CURDIR)/build
OUTDIR := $(CURDIR)/out
TARGET := $(OUTDIR)/ps

SRCS := ps.c keyword.c print.c fmt.c nlist.c
OBJS := $(SRCS:%.c=$(OBJDIR)/%.o)

.PHONY: all clean dirs test

all: $(TARGET)

dirs:
	@mkdir -p "$(OBJDIR)" "$(OUTDIR)"

$(TARGET): $(OBJS) | dirs
	$(CC) $(LDFLAGS) -o "$@" $(OBJS) $(LDLIBS)

$(OBJDIR)/%.o: $(CURDIR)/%.c | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$<" -o "$@"

test: $(TARGET)
	CC="$(CC)" PS_BIN="$(TARGET)" sh "$(CURDIR)/tests/test.sh"

clean:
	@rm -rf "$(OBJDIR)" "$(OUTDIR)"
