#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
LINUX_VERSION_BIN=${LINUX_VERSION_BIN:-"$ROOT_DIR/out/linux-version"}
TMPDIR=${TMPDIR:-/tmp}
WORKDIR=$(mktemp -d "$TMPDIR/linux-version-test.XXXXXX")
trap 'rm -rf "$WORKDIR"' EXIT INT TERM

fail() {
	printf '%s\n' "FAIL: $1" >&2
	exit 1
}

assert_eq() {
	name=$1
	expected=$2
	actual=$3
	if [ "$expected" != "$actual" ]; then
		printf '%s\n' "FAIL: $name" >&2
		printf '%s\n' "--- expected ---" >&2
		printf '%s\n' "$expected" >&2
		printf '%s\n' "--- actual ---" >&2
		printf '%s\n' "$actual" >&2
		exit 1
	fi
}

assert_match() {
	name=$1
	pattern=$2
	text=$3
	printf '%s\n' "$text" | grep -Eq "$pattern" || fail "$name"
}

write_os_release() {
	root=$1
	content=$2
	mkdir -p "$root/etc"
	printf '%s\n' "$content" >"$root/etc/os-release"
}

[ -x "$LINUX_VERSION_BIN" ] || fail "missing binary: $LINUX_VERSION_BIN"

usage_output=$("$LINUX_VERSION_BIN" -x 2>&1 || true)
assert_match "invalid option should print usage" '^usage: linux-version ' "$usage_output"

too_many_output=$("$LINUX_VERSION_BIN" arg1 arg2 2>&1 || true)
assert_match "extra operands should print usage" '^usage: linux-version ' "$too_many_output"

jail_output=$("$LINUX_VERSION_BIN" -j demo 2>&1 || true)
assert_eq "unsupported jail option" \
	"linux-version: option -j is not supported on Linux" "$jail_output"

root_default="$WORKDIR/root-default"
write_os_release "$root_default" 'NAME=Test Linux
VERSION_ID=42.3
PRETTY_NAME="Test Linux 42.3"'

default_output=$(ROOT="$root_default" "$LINUX_VERSION_BIN")
assert_eq "default output should be userland version" "42.3" "$default_output"

quoted_root="$WORKDIR/root-quoted"
write_os_release "$quoted_root" 'NAME=Quoted Linux
PRETTY_NAME="Quoted Linux 1.0 LTS"'

quoted_output=$(ROOT="$quoted_root" "$LINUX_VERSION_BIN" -u)
assert_eq "quoted PRETTY_NAME fallback" "Quoted Linux 1.0 LTS" "$quoted_output"

malformed_root="$WORKDIR/root-malformed"
write_os_release "$malformed_root" 'NAME=Broken Linux
VERSION_ID="unterminated'

malformed_output=$(ROOT="$malformed_root" "$LINUX_VERSION_BIN" -u 2>&1 || true)
assert_eq "malformed os-release should fail" \
	"linux-version: malformed os-release entry 'VERSION_ID' in $malformed_root/etc/os-release" \
	"$malformed_output"

missing_root="$WORKDIR/root-missing"
mkdir -p "$missing_root"
missing_output=$(ROOT="$missing_root" "$LINUX_VERSION_BIN" -u 2>&1 || true)
assert_eq "missing os-release should fail" \
	"linux-version: unable to locate os-release under '$missing_root'" "$missing_output"

running_expected=
if [ -r /proc/sys/kernel/osrelease ]; then
	IFS= read -r running_expected </proc/sys/kernel/osrelease || true
fi
if [ -z "$running_expected" ]; then
	running_expected=$(uname -r)
fi
running_output=$("$LINUX_VERSION_BIN" -r)
assert_eq "running kernel release" "$running_expected" "$running_output"

kernel_link_root="$WORKDIR/root-kernel-link"
mkdir -p "$kernel_link_root/boot" "$kernel_link_root/etc"
ln -s ../images/vmlinuz-6.9.7-port "$kernel_link_root/boot/vmlinuz"
write_os_release "$kernel_link_root" 'VERSION_ID=9.1'

kernel_link_output=$(ROOT="$kernel_link_root" "$LINUX_VERSION_BIN" -k)
assert_eq "kernel version from default symlink" "6.9.7-port" "$kernel_link_output"

kernel_modules_root="$WORKDIR/root-kernel-modules"
mkdir -p "$kernel_modules_root/lib/modules/6.8.12-demo/kernel" "$kernel_modules_root/etc"
: >"$kernel_modules_root/lib/modules/6.8.12-demo/modules.dep"
write_os_release "$kernel_modules_root" 'VERSION_ID=6.8-userland'

kernel_modules_output=$(ROOT="$kernel_modules_root" "$LINUX_VERSION_BIN" -k)
assert_eq "kernel version from unique modules tree" "6.8.12-demo" "$kernel_modules_output"

ambiguous_root="$WORKDIR/root-ambiguous"
mkdir -p "$ambiguous_root/lib/modules/6.1.1-a/kernel" "$ambiguous_root/lib/modules/6.1.2-b/kernel"
: >"$ambiguous_root/lib/modules/6.1.1-a/modules.dep"
: >"$ambiguous_root/lib/modules/6.1.2-b/modules.dep"

ambiguous_output=$(ROOT="$ambiguous_root" "$LINUX_VERSION_BIN" -k 2>&1 || true)
assert_match "ambiguous modules tree should fail" \
	"^linux-version: unable to determine installed kernel version under '$ambiguous_root': multiple kernel releases found \\(6\\.1\\.1-a, 6\\.1\\.2-b\\)$" \
	"$ambiguous_output"

combo_output=$(ROOT="$kernel_link_root" "$LINUX_VERSION_BIN" -kru)
combo_expected=$(printf '%s\n%s\n%s' "6.9.7-port" "$running_expected" "9.1")
assert_eq "combined output order" "$combo_expected" "$combo_output"

printf '%s\n' "PASS"
