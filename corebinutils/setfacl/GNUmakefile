.DEFAULT_GOAL := all

CC ?= cc
CPPFLAGS += -D_POSIX_C_SOURCE=200809L
CFLAGS ?= -O2
CFLAGS += -std=c17 -g -Wall -Wextra -Wpedantic
LDFLAGS ?=
LDLIBS ?=

OBJDIR := $(CURDIR)/build
OUTDIR := $(CURDIR)/out
TARGET := $(OUTDIR)/setfacl
OBJS := $(OBJDIR)/setfacl.o

.PHONY: all clean dirs test status

all: $(TARGET)

dirs:
	@mkdir -p "$(OBJDIR)" "$(OUTDIR)"

$(TARGET): $(OBJS) | dirs
	$(CC) $(LDFLAGS) -o "$@" $(OBJS) $(LDLIBS)

$(OBJDIR)/setfacl.o: $(CURDIR)/setfacl.c | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$(CURDIR)/setfacl.c" -o "$@"

test: $(TARGET)
	SETFACL_BIN="$(TARGET)" sh "$(CURDIR)/tests/test.sh"

status:
	@printf '%s\n' "$(TARGET)"

clean:
	@rm -rf "$(CURDIR)/build" "$(CURDIR)/out" "$(CURDIR)/.tmp-tests"
