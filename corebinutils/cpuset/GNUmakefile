.DEFAULT_GOAL := all

CC ?= cc
CPPFLAGS += -D_GNU_SOURCE
CFLAGS ?= -O2
CFLAGS += -std=c17 -g -Wall -Wextra -Wno-unused-parameter
LDFLAGS ?=
LDLIBS ?=

OBJDIR := $(CURDIR)/build
OUTDIR := $(CURDIR)/out
TARGET := $(OUTDIR)/cpuset
OBJS := $(OBJDIR)/cpuset.o

.PHONY: all clean dirs test status

all: $(TARGET)

dirs:
	@mkdir -p "$(OBJDIR)" "$(OUTDIR)"

$(TARGET): $(OBJS) | dirs
	$(CC) $(LDFLAGS) -o "$@" $(OBJS) $(LDLIBS)

$(OBJDIR)/cpuset.o: $(CURDIR)/cpuset.c | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$(CURDIR)/cpuset.c" -o "$@"

test: $(TARGET)
	CPUSET_BIN="$(TARGET)" sh "$(CURDIR)/tests/test.sh"

status:
	@printf '%s\n' "$(TARGET)"

clean:
	@rm -rf "$(CURDIR)/build" "$(CURDIR)/out"
