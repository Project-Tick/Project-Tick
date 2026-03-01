#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
MV_BIN=${MV_BIN:-"$ROOT/out/mv"}
TMPDIR=${TMPDIR:-/tmp}
WORKDIR=$(mktemp -d "$TMPDIR/mv-test.XXXXXX")
STDOUT_FILE="$WORKDIR/stdout"
STDERR_FILE="$WORKDIR/stderr"
LAST_STATUS=0
LAST_STDOUT=
LAST_STDERR=
ALTROOT=

cleanup() {
	rm -rf "$WORKDIR"
	if [ -n "${ALTROOT:-}" ] && [ -d "$ALTROOT" ]; then
		rm -rf "$ALTROOT"
	fi
}
trap cleanup EXIT INT TERM

export LC_ALL=C

MV_USAGE=$(cat <<'EOF'
usage: mv [-f | -i | -n] [-hv] source target
       mv [-f | -i | -n] [-v] source ... directory
EOF
)

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
		printf '%s' "$expected" >&2
		printf '\n%s\n' "--- actual ---" >&2
		printf '%s' "$actual" >&2
		printf '\n' >&2
		exit 1
	fi
}

assert_contains() {
	name=$1
	text=$2
	pattern=$3
	case $text in
		*"$pattern"*) ;;
		*) fail "$name" ;;
	esac
}

assert_empty() {
	name=$1
	text=$2
	[ -z "$text" ] || fail "$name"
}

assert_exists() {
	path=$1
	[ -e "$path" ] || fail "missing path: $path"
}

assert_not_exists() {
	path=$1
	[ ! -e "$path" ] || fail "unexpected path: $path"
}

assert_status() {
	name=$1
	expected=$2
	actual=$3
	[ "$expected" -eq "$actual" ] || fail "$name"
}

assert_file_text() {
	expected=$1
	path=$2
	actual=$(cat "$path")
	[ "$actual" = "$expected" ] || fail "content mismatch: $path"
}

assert_mode() {
	expected=$1
	path=$2
	actual=$(stat -c '%a' "$path")
	[ "$actual" = "$expected" ] || fail "mode mismatch: $path"
}

assert_symlink_target() {
	expected=$1
	path=$2
	[ -L "$path" ] || fail "not a symlink: $path"
	actual=$(readlink "$path")
	[ "$actual" = "$expected" ] || fail "symlink mismatch: $path"
}

inode_key() {
	stat -c '%d:%i' "$1"
}

assert_same_inode() {
	path1=$1
	path2=$2
	[ "$(inode_key "$path1")" = "$(inode_key "$path2")" ] || \
		fail "inode mismatch: $path1 $path2"
}

assert_different_inode() {
	path1=$1
	path2=$2
	[ "$(inode_key "$path1")" != "$(inode_key "$path2")" ] || \
		fail "unexpected shared inode: $path1 $path2"
}

run_capture() {
	if "$@" >"$STDOUT_FILE" 2>"$STDERR_FILE"; then
		LAST_STATUS=0
	else
		LAST_STATUS=$?
	fi
	LAST_STDOUT=$(cat "$STDOUT_FILE")
	LAST_STDERR=$(cat "$STDERR_FILE")
}

find_alt_parent() {
	base_dev=$(stat -c '%d' "$WORKDIR")
	uid=$(id -u)

	for candidate in /dev/shm "$TMPDIR" /tmp /var/tmp "/run/user/$uid"; do
		[ -d "$candidate" ] || continue
		[ -w "$candidate" ] || continue
		probe=$(mktemp -d "$candidate/mv-test-probe.XXXXXX" 2>/dev/null) || continue
		probe_dev=$(stat -c '%d' "$probe" 2>/dev/null || true)
		rm -rf "$probe"
		[ -n "$probe_dev" ] || continue
		if [ "$probe_dev" != "$base_dev" ]; then
			printf '%s\n' "$candidate"
			return 0
		fi
	done
	return 1
}

setup_cross_device_root() {
	alt_parent=$(find_alt_parent) || return 1
	ALTROOT=$(mktemp -d "$alt_parent/mv-test-fs.XXXXXX")
	return 0
}

[ -x "$MV_BIN" ] || fail "missing binary: $MV_BIN"

run_capture "$MV_BIN"
assert_status "usage status" 2 "$LAST_STATUS"
assert_empty "usage stdout" "$LAST_STDOUT"
assert_eq "usage stderr" "$MV_USAGE" "$LAST_STDERR"

run_capture "$MV_BIN" -h a b c
assert_status "invalid -h usage status" 2 "$LAST_STATUS"
assert_empty "invalid -h usage stdout" "$LAST_STDOUT"
assert_eq "invalid -h usage stderr" "$MV_USAGE" "$LAST_STDERR"

mkdir "$WORKDIR/basic"
printf 'hello\n' >"$WORKDIR/basic/src"
run_capture "$MV_BIN" "$WORKDIR/basic/src" "$WORKDIR/basic/dst"
assert_status "basic rename status" 0 "$LAST_STATUS"
assert_empty "basic rename stdout" "$LAST_STDOUT"
assert_empty "basic rename stderr" "$LAST_STDERR"
assert_file_text "hello" "$WORKDIR/basic/dst"
assert_not_exists "$WORKDIR/basic/src"

mkdir "$WORKDIR/into-dir"
printf 'alpha\n' >"$WORKDIR/into-dir/src"
mkdir "$WORKDIR/into-dir/out"
run_capture "$MV_BIN" "$WORKDIR/into-dir/src" "$WORKDIR/into-dir/out"
assert_status "move into dir status" 0 "$LAST_STATUS"
assert_file_text "alpha" "$WORKDIR/into-dir/out/src"
assert_not_exists "$WORKDIR/into-dir/src"

mkdir "$WORKDIR/hflag"
mkdir "$WORKDIR/hflag/actual"
ln -s actual "$WORKDIR/hflag/linkdir"
printf 'first\n' >"$WORKDIR/hflag/src1"
run_capture "$MV_BIN" "$WORKDIR/hflag/src1" "$WORKDIR/hflag/linkdir"
assert_status "default symlink-dir target status" 0 "$LAST_STATUS"
assert_file_text "first" "$WORKDIR/hflag/actual/src1"
printf 'second\n' >"$WORKDIR/hflag/src2"
run_capture "$MV_BIN" -h "$WORKDIR/hflag/src2" "$WORKDIR/hflag/linkdir"
assert_status "h replace symlink status" 0 "$LAST_STATUS"
assert_file_text "second" "$WORKDIR/hflag/linkdir"
[ ! -L "$WORKDIR/hflag/linkdir" ] || fail "-h did not replace symlink target"

mkdir "$WORKDIR/nflag"
printf 'source\n' >"$WORKDIR/nflag/src"
printf 'target\n' >"$WORKDIR/nflag/dst"
run_capture "$MV_BIN" -n "$WORKDIR/nflag/src" "$WORKDIR/nflag/dst"
assert_status "n status" 0 "$LAST_STATUS"
assert_empty "n stdout" "$LAST_STDOUT"
assert_empty "n stderr" "$LAST_STDERR"
assert_file_text "source" "$WORKDIR/nflag/src"
assert_file_text "target" "$WORKDIR/nflag/dst"

printf 'source\n' >"$WORKDIR/nflag/src2"
printf 'target\n' >"$WORKDIR/nflag/dst2"
run_capture "$MV_BIN" -vn "$WORKDIR/nflag/src2" "$WORKDIR/nflag/dst2"
assert_status "vn status" 0 "$LAST_STATUS"
assert_eq "vn stdout" "$WORKDIR/nflag/dst2 not overwritten" "$LAST_STDOUT"
assert_empty "vn stderr" "$LAST_STDERR"
assert_file_text "source" "$WORKDIR/nflag/src2"
assert_file_text "target" "$WORKDIR/nflag/dst2"

mkdir "$WORKDIR/interactive-no"
printf 'source\n' >"$WORKDIR/interactive-no/src"
printf 'target\n' >"$WORKDIR/interactive-no/dst"
if printf 'n\n' | "$MV_BIN" -i "$WORKDIR/interactive-no/src" \
	"$WORKDIR/interactive-no/dst" >"$STDOUT_FILE" 2>"$STDERR_FILE"; then
	LAST_STATUS=0
else
	LAST_STATUS=$?
fi
LAST_STDOUT=$(cat "$STDOUT_FILE")
LAST_STDERR=$(cat "$STDERR_FILE")
assert_status "interactive no status" 0 "$LAST_STATUS"
assert_empty "interactive no stdout" "$LAST_STDOUT"
assert_contains "interactive no prompt" "$LAST_STDERR" \
	"overwrite $WORKDIR/interactive-no/dst? (y/n [n]) "
assert_contains "interactive no rejection" "$LAST_STDERR" "not overwritten"
assert_file_text "source" "$WORKDIR/interactive-no/src"
assert_file_text "target" "$WORKDIR/interactive-no/dst"

mkdir "$WORKDIR/interactive-yes"
printf 'source\n' >"$WORKDIR/interactive-yes/src"
printf 'target\n' >"$WORKDIR/interactive-yes/dst"
if printf 'y\n' | "$MV_BIN" -i "$WORKDIR/interactive-yes/src" \
	"$WORKDIR/interactive-yes/dst" >"$STDOUT_FILE" 2>"$STDERR_FILE"; then
	LAST_STATUS=0
else
	LAST_STATUS=$?
fi
LAST_STDOUT=$(cat "$STDOUT_FILE")
LAST_STDERR=$(cat "$STDERR_FILE")
assert_status "interactive yes status" 0 "$LAST_STATUS"
assert_empty "interactive yes stdout" "$LAST_STDOUT"
assert_contains "interactive yes prompt" "$LAST_STDERR" \
	"overwrite $WORKDIR/interactive-yes/dst? (y/n [n]) "
assert_not_exists "$WORKDIR/interactive-yes/src"
assert_file_text "source" "$WORKDIR/interactive-yes/dst"

mkdir "$WORKDIR/f-precedence"
printf 'source\n' >"$WORKDIR/f-precedence/src"
printf 'target\n' >"$WORKDIR/f-precedence/dst"
run_capture "$MV_BIN" -i -n -f "$WORKDIR/f-precedence/src" "$WORKDIR/f-precedence/dst"
assert_status "force precedence status" 0 "$LAST_STATUS"
assert_empty "force precedence stderr" "$LAST_STDERR"
assert_file_text "source" "$WORKDIR/f-precedence/dst"
assert_not_exists "$WORKDIR/f-precedence/src"

mkdir "$WORKDIR/dir-to-file"
mkdir "$WORKDIR/dir-to-file/src"
printf 'payload\n' >"$WORKDIR/dir-to-file/src/file"
printf 'target\n' >"$WORKDIR/dir-to-file/dst"
run_capture "$MV_BIN" "$WORKDIR/dir-to-file/src" "$WORKDIR/dir-to-file/dst"
assert_status "dir-to-file status" 1 "$LAST_STATUS"
assert_contains "dir-to-file stderr" "$LAST_STDERR" "Not a directory"
assert_exists "$WORKDIR/dir-to-file/src/file"

mkdir "$WORKDIR/file-to-dir"
printf 'payload\n' >"$WORKDIR/file-to-dir/src"
mkdir -p "$WORKDIR/file-to-dir/out/src"
run_capture "$MV_BIN" "$WORKDIR/file-to-dir/src" "$WORKDIR/file-to-dir/out"
assert_status "file-to-dir final dir status" 1 "$LAST_STATUS"
assert_contains "file-to-dir stderr" "$LAST_STDERR" "Is a directory"
assert_exists "$WORKDIR/file-to-dir/src"

mkdir "$WORKDIR/verbose"
printf 'payload\n' >"$WORKDIR/verbose/src"
mkdir "$WORKDIR/verbose/out"
run_capture "$MV_BIN" -v "$WORKDIR/verbose/src" "$WORKDIR/verbose/out"
assert_status "verbose status" 0 "$LAST_STATUS"
assert_eq "verbose stdout" \
	"$WORKDIR/verbose/src -> $WORKDIR/verbose/out/src" "$LAST_STDOUT"
assert_empty "verbose stderr" "$LAST_STDERR"

mkdir "$WORKDIR/fifo"
mkfifo "$WORKDIR/fifo/src"
run_capture "$MV_BIN" "$WORKDIR/fifo/src" "$WORKDIR/fifo/dst"
assert_status "fifo rename status" 0 "$LAST_STATUS"
[ -p "$WORKDIR/fifo/dst" ] || fail "fifo move lost fifo type"
assert_not_exists "$WORKDIR/fifo/src"

if setup_cross_device_root; then
	mkdir "$WORKDIR/cross-file"
	printf 'cross-data\n' >"$WORKDIR/cross-file/src"
	chmod 754 "$WORKDIR/cross-file/src"
	touch -t 202001020304.05 "$WORKDIR/cross-file/src"
	src_mtime=$(stat -c '%Y' "$WORKDIR/cross-file/src")
	run_capture "$MV_BIN" "$WORKDIR/cross-file/src" "$ALTROOT/regular"
	assert_status "cross regular status" 0 "$LAST_STATUS"
	assert_file_text "cross-data" "$ALTROOT/regular"
	assert_mode "754" "$ALTROOT/regular"
	[ "$(stat -c '%Y' "$ALTROOT/regular")" = "$src_mtime" ] || fail "mtime mismatch after cross-device regular move"
	assert_not_exists "$WORKDIR/cross-file/src"

	mkdir "$WORKDIR/cross-link"
	printf 'target\n' >"$WORKDIR/cross-link/data"
	ln -s data "$WORKDIR/cross-link/src"
	run_capture "$MV_BIN" "$WORKDIR/cross-link/src" "$ALTROOT/link"
	assert_status "cross symlink status" 0 "$LAST_STATUS"
	assert_symlink_target "data" "$ALTROOT/link"
	assert_not_exists "$WORKDIR/cross-link/src"

	mkdir "$WORKDIR/cross-tree"
	mkdir -p "$WORKDIR/cross-tree/src/sub"
	printf 'nested\n' >"$WORKDIR/cross-tree/src/sub/file"
	ln -s sub/file "$WORKDIR/cross-tree/src/link"
	mkfifo "$WORKDIR/cross-tree/src/pipe"
	chmod 711 "$WORKDIR/cross-tree/src/sub"
	run_capture "$MV_BIN" "$WORKDIR/cross-tree/src" "$ALTROOT"
	assert_status "cross tree status" 0 "$LAST_STATUS"
	assert_file_text "nested" "$ALTROOT/src/sub/file"
	assert_symlink_target "sub/file" "$ALTROOT/src/link"
	[ -p "$ALTROOT/src/pipe" ] || fail "cross tree fifo missing"
	assert_mode "711" "$ALTROOT/src/sub"
	assert_not_exists "$WORKDIR/cross-tree/src"

	mkdir "$WORKDIR/cross-empty"
	mkdir -p "$WORKDIR/cross-empty/src/child"
	printf 'replacement\n' >"$WORKDIR/cross-empty/src/child/file"
	mkdir -p "$ALTROOT/replace-empty/src"
	run_capture "$MV_BIN" "$WORKDIR/cross-empty/src" "$ALTROOT/replace-empty"
	assert_status "cross replace empty dir status" 0 "$LAST_STATUS"
	assert_file_text "replacement" "$ALTROOT/replace-empty/src/child/file"
	assert_not_exists "$WORKDIR/cross-empty/src"

	mkdir "$WORKDIR/cross-nonempty"
	mkdir -p "$WORKDIR/cross-nonempty/src/child"
	printf 'source\n' >"$WORKDIR/cross-nonempty/src/child/file"
	mkdir -p "$ALTROOT/replace-nonempty/src"
	printf 'keep\n' >"$ALTROOT/replace-nonempty/src/existing"
	run_capture "$MV_BIN" "$WORKDIR/cross-nonempty/src" "$ALTROOT/replace-nonempty"
	assert_status "cross replace nonempty dir status" 1 "$LAST_STATUS"
	assert_contains "cross replace nonempty dir stderr" "$LAST_STDERR" \
		"Directory not empty"
	assert_exists "$WORKDIR/cross-nonempty/src/child/file"
	assert_file_text "keep" "$ALTROOT/replace-nonempty/src/existing"

	mkdir "$WORKDIR/cross-fifo"
	mkfifo "$WORKDIR/cross-fifo/src"
	run_capture "$MV_BIN" "$WORKDIR/cross-fifo/src" "$ALTROOT/fifo"
	assert_status "cross fifo status" 0 "$LAST_STATUS"
	[ -p "$ALTROOT/fifo" ] || fail "cross-device fifo missing"
	assert_not_exists "$WORKDIR/cross-fifo/src"

	mkdir "$WORKDIR/cross-hardlinks"
	mkdir "$WORKDIR/cross-hardlinks/hardlinks-src"
	printf 'shared\n' >"$WORKDIR/cross-hardlinks/hardlinks-src/a"
	ln "$WORKDIR/cross-hardlinks/hardlinks-src/a" \
		"$WORKDIR/cross-hardlinks/hardlinks-src/b"
	assert_same_inode "$WORKDIR/cross-hardlinks/hardlinks-src/a" \
		"$WORKDIR/cross-hardlinks/hardlinks-src/b"
	run_capture "$MV_BIN" "$WORKDIR/cross-hardlinks/hardlinks-src" "$ALTROOT"
	assert_status "cross hardlink tree status" 0 "$LAST_STATUS"
	assert_file_text "shared" "$ALTROOT/hardlinks-src/a"
	assert_file_text "shared" "$ALTROOT/hardlinks-src/b"
	assert_different_inode "$ALTROOT/hardlinks-src/a" \
		"$ALTROOT/hardlinks-src/b"
	assert_not_exists "$WORKDIR/cross-hardlinks/hardlinks-src"

	alt_avail_kb=$(df -Pk "$ALTROOT" | awk 'NR==2 { print $4 }')
	case $alt_avail_kb in
		''|*[!0-9]*) alt_avail_kb=0 ;;
	esac
	if [ "$alt_avail_kb" -ge 4096 ] && [ "$alt_avail_kb" -le 131072 ]; then
		mkdir "$WORKDIR/cross-partial"
		dd if=/dev/zero of="$WORKDIR/cross-partial/src" bs=1024 count=2048 \
			>"$STDOUT_FILE" 2>"$STDERR_FILE" || fail "partial source creation failed"
		fill_kb=$((alt_avail_kb - 1024))
		if [ "$fill_kb" -gt 0 ] && dd if=/dev/zero of="$ALTROOT/.fill-enospc" \
			bs=1024 count="$fill_kb" >"$STDOUT_FILE" 2>"$STDERR_FILE"; then
			run_capture "$MV_BIN" "$WORKDIR/cross-partial/src" \
				"$ALTROOT/partial-target"
			[ "$LAST_STATUS" -ne 0 ] || fail "cross partial failure status"
			assert_contains "cross partial failure stderr" "$LAST_STDERR" \
				"No space left on device"
			assert_exists "$WORKDIR/cross-partial/src"
			assert_not_exists "$ALTROOT/partial-target"
		fi
	fi

	if command -v setfattr >/dev/null 2>&1 && command -v getfattr >/dev/null 2>&1; then
		printf 'probe\n' >"$WORKDIR/xattr-probe"
		printf 'probe\n' >"$ALTROOT/xattr-probe"
		if setfattr -n user.project_tick -v probe "$WORKDIR/xattr-probe" 2>/dev/null &&
		    setfattr -n user.project_tick -v probe "$ALTROOT/xattr-probe" 2>/dev/null; then
			printf 'payload\n' >"$WORKDIR/xattr-src"
			setfattr -n user.project_tick -v value "$WORKDIR/xattr-src"
			run_capture "$MV_BIN" "$WORKDIR/xattr-src" "$ALTROOT/xattr-dst"
			assert_status "cross xattr status" 0 "$LAST_STATUS"
			xattr_value=$(getfattr --only-values -n user.project_tick "$ALTROOT/xattr-dst" 2>/dev/null)
			assert_eq "cross xattr value" "value" "$xattr_value"
			assert_not_exists "$WORKDIR/xattr-src"
		fi
	fi
fi

printf '%s\n' "PASS"
