.DEFAULT_GOAL := all

CC ?= cc
CCACHE_DISABLE ?= 1
CPPFLAGS += -include $(CURDIR)/compat.h -I$(CURDIR) -I$(CURDIR)/../cp
CFLAGS ?= -O2
CFLAGS += -std=c11 -g -Wall -Wextra -Wno-unused-parameter
LDFLAGS ?=
LDLIBS ?=

OBJDIR := $(CURDIR)/build
OUTDIR := $(CURDIR)/out
TARGET := $(OUTDIR)/pax
PAX_SRCS := ar_io.c ar_subs.c buf_subs.c cache.c cpio.c file_subs.c \
	ftree.c gen_subs.c getoldopt.c linux_attrs.c options.c pat_rep.c pax.c sel_subs.c \
	tables.c tar.c tty_subs.c compat.c
PAX_OBJS := $(addprefix $(OBJDIR)/,$(PAX_SRCS:.c=.o))
FTS_OBJ := $(OBJDIR)/fts.o
OBJS := $(PAX_OBJS) $(FTS_OBJ)

.PHONY: all clean dirs test status

all: $(TARGET)

dirs:
	@mkdir -p "$(OBJDIR)" "$(OUTDIR)"

$(TARGET): $(OBJS) | dirs
	env CCACHE_DISABLE=$(CCACHE_DISABLE) $(CC) $(LDFLAGS) -o "$@" $(OBJS) $(LDLIBS)

$(OBJDIR)/%.o: $(CURDIR)/%.c | dirs
	env CCACHE_DISABLE=$(CCACHE_DISABLE) $(CC) $(CPPFLAGS) $(CFLAGS) -c "$<" -o "$@"

$(FTS_OBJ): $(CURDIR)/../cp/fts.c $(CURDIR)/../cp/fts.h | dirs
	env CCACHE_DISABLE=$(CCACHE_DISABLE) $(CC) $(CPPFLAGS) $(CFLAGS) -c "$(CURDIR)/../cp/fts.c" -o "$@"

test: $(TARGET)
	PAX_BIN="$(TARGET)" sh "$(CURDIR)/tests/test.sh"

status:
	@printf '%s\n' "$(TARGET)"

clean:
	@rm -rf "$(CURDIR)/build" "$(CURDIR)/out"
