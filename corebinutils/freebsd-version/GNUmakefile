.DEFAULT_GOAL := all

SED ?= sed
SH ?= sh

OBJDIR := $(CURDIR)/build
OUTDIR := $(CURDIR)/out
GENERATED := $(OBJDIR)/linux-version
TARGET := $(OUTDIR)/linux-version

.PHONY: all clean dirs status test

all: $(TARGET)

dirs:
	@mkdir -p "$(OBJDIR)" "$(OUTDIR)"

$(GENERATED): $(CURDIR)/linux-version.sh.in | dirs
	$(SED) \
		-e 's|@@OS_RELEASE_PRIMARY@@|/etc/os-release|g' \
		-e 's|@@OS_RELEASE_FALLBACK@@|/usr/lib/os-release|g' \
		-e 's|@@PROC_OSRELEASE@@|/proc/sys/kernel/osrelease|g' \
		"$<" >"$@"
	@chmod +x "$@"

$(TARGET): $(GENERATED) | dirs
	cp "$(GENERATED)" "$(TARGET)"
	@chmod +x "$(TARGET)"

test: $(TARGET)
	LINUX_VERSION_BIN="$(TARGET)" $(SH) "$(CURDIR)/tests/test.sh"

status:
	@printf '%s\n' "$(TARGET)"

clean:
	@rm -rf "$(OBJDIR)" "$(OUTDIR)"
