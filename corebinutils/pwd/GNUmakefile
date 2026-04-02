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
TARGET := $(OUTDIR)/pwd
OBJS   := $(OBJDIR)/pwd.o

DEEP_HELPER := $(OBJDIR)/deep_helper

.PHONY: all clean dirs status test

all: $(TARGET)

dirs:
	@mkdir -p "$(OBJDIR)" "$(OUTDIR)"

$(TARGET): $(OBJS) | dirs
	$(CC) $(LDFLAGS) -o "$@" $(OBJS) $(LDLIBS)

$(OBJDIR)/pwd.o: $(CURDIR)/pwd.c | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$(CURDIR)/pwd.c" -o "$@"

$(DEEP_HELPER): $(CURDIR)/tests/deep_helper.c | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$(CURDIR)/tests/deep_helper.c" -o "$(OBJDIR)/deep_helper.o"
	$(CC) $(LDFLAGS) -o "$@" "$(OBJDIR)/deep_helper.o" $(LDLIBS)

test: $(TARGET) $(DEEP_HELPER)
	CC="$(CC)" PWD_BIN="$(TARGET)" DEEP_HELPER_BIN="$(DEEP_HELPER)" \
	  LC_ALL=C sh "$(CURDIR)/tests/test.sh"

status:
	@printf '%s\n' "$(TARGET)"

clean:
	@rm -rf "$(OBJDIR)" "$(OUTDIR)"

