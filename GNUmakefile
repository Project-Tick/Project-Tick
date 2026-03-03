.DEFAULT_GOAL := all

CC ?= cc
CPPFLAGS ?=
CPPFLAGS += -D_POSIX_C_SOURCE=200809L
CFLAGS ?= -O2
CFLAGS += -std=c17 -g -Wall -Wextra -Werror
LDFLAGS ?=
LDLIBS ?=

OBJDIR := $(CURDIR)/build
OUTDIR := $(CURDIR)/out
TARGET := $(OUTDIR)/rmail
OBJS := $(OBJDIR)/rmail.o

HELPER_SRC := $(CURDIR)/tests/mock_sendmail.c
HELPER_BIN := $(OUTDIR)/mock_sendmail

.PHONY: all clean dirs status test

all: $(TARGET)

dirs:
	@mkdir -p "$(OBJDIR)" "$(OUTDIR)"

$(TARGET): $(OBJS) | dirs
	$(CC) $(LDFLAGS) -o "$@" $(OBJS) $(LDLIBS)

$(OBJDIR)/rmail.o: $(CURDIR)/rmail.c | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$(CURDIR)/rmail.c" -o "$@"

$(HELPER_BIN): $(HELPER_SRC) | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) "$<" -o "$@" $(LDLIBS)

test: $(TARGET) $(HELPER_BIN)
	RMAIL_BIN="$(TARGET)" MOCK_SENDMAIL_BIN="$(HELPER_BIN)" \
	  sh "$(CURDIR)/tests/test.sh"

status:
	@printf '%s\n' "$(TARGET)"

clean:
	@rm -rf "$(OBJDIR)" "$(OUTDIR)"
