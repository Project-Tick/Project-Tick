#!/bin/sh
set -eu

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
LN_BIN=${LN_BIN:-"$ROOT/out/ln"}
LINK_BIN=${LINK_BIN:-"$ROOT/out/link"}
TMPDIR=${TMPDIR:-/tmp}
WORKDIR=$(mktemp -d "$TMPDIR/ln-test.XXXXXX")
STDOUT_FILE="$WORKDIR/stdout"
STDERR_FILE="$WORKDIR/stderr"
LAST_STATUS=0
LAST_STDOUT=
LAST_STDERR=
trap 'rm -rf "$WORKDIR"' EXIT INT TERM

export LC_ALL=C

LN_USAGE=$(cat <<'EOF'
usage: ln [-s [-F] | -L | -P] [-f | -i] [-hnvw] source_file [target_file]
       ln [-s [-F] | -L | -P] [-f | -i] [-hnvw] source_file ... target_dir
EOF
)

LINK_USAGE=$(cat <<'EOF'
usage: link source_file target_file
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
	if [ -n "$text" ]; then
		printf '%s\n' "FAIL: $name" >&2
		printf '%s\n' "--- expected empty ---" >&2
		printf '%s\n' "--- actual ---" >&2
		printf '%s' "$text" >&2
		printf '\n' >&2
		exit 1
	fi
}

assert_status() {
	name=$1
	expected=$2
	actual=$3
	if [ "$expected" -ne "$actual" ]; then
		printf '%s\n' "FAIL: $name" >&2
		printf '%s\n' "expected status: $expected" >&2
		printf '%s\n' "actual status: $actual" >&2
		exit 1
	fi
}

assert_same_inode() {
	path1=$1
	path2=$2
	inode1=$(stat -c '%d:%i' "$path1")
	inode2=$(stat -c '%d:%i' "$path2")
	[ "$inode1" = "$inode2" ] || fail "$path1 and $path2 differ"
}

assert_symlink_target() {
	expected=$1
	path=$2
	[ -L "$path" ] || fail "$path is not a symlink"
	actual=$(readlink "$path")
	[ "$actual" = "$expected" ] || fail "$path target expected $expected got $actual"
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

[ -x "$LN_BIN" ] || fail "missing binary: $LN_BIN"
[ -x "$LINK_BIN" ] || fail "missing binary: $LINK_BIN"

run_capture "$LN_BIN"
assert_status "ln usage status" 2 "$LAST_STATUS"
assert_empty "ln usage stdout" "$LAST_STDOUT"
assert_eq "ln usage stderr" "$LN_USAGE" "$LAST_STDERR"

run_capture "$LINK_BIN"
assert_status "link usage status" 2 "$LAST_STATUS"
assert_empty "link usage stdout" "$LAST_STDOUT"
assert_eq "link usage stderr" "$LINK_USAGE" "$LAST_STDERR"

mkdir "$WORKDIR/basic"
printf 'payload\n' >"$WORKDIR/basic/source"
"$LN_BIN" "$WORKDIR/basic/source" "$WORKDIR/basic/target"
assert_same_inode "$WORKDIR/basic/source" "$WORKDIR/basic/target"

mkdir "$WORKDIR/symlink"
"$LN_BIN" -s relative-target "$WORKDIR/symlink/link"
assert_symlink_target "relative-target" "$WORKDIR/symlink/link"

mkdir "$WORKDIR/warn"
run_capture "$LN_BIN" -sw missing "$WORKDIR/warn/link"
assert_status "warn missing status" 0 "$LAST_STATUS"
assert_empty "warn missing stdout" "$LAST_STDOUT"
assert_contains "warn missing stderr" "$LAST_STDERR" "warning: missing"
assert_symlink_target "missing" "$WORKDIR/warn/link"

mkdir "$WORKDIR/follow"
printf 'x\n' >"$WORKDIR/follow/file"
ln -s file "$WORKDIR/follow/source-link"
"$LN_BIN" "$WORKDIR/follow/source-link" "$WORKDIR/follow/hard-default"
assert_same_inode "$WORKDIR/follow/file" "$WORKDIR/follow/hard-default"
[ ! -L "$WORKDIR/follow/hard-default" ] || fail "default hard link did not follow source symlink"
"$LN_BIN" -L "$WORKDIR/follow/source-link" "$WORKDIR/follow/hard-follow"
assert_same_inode "$WORKDIR/follow/file" "$WORKDIR/follow/hard-follow"
[ ! -L "$WORKDIR/follow/hard-follow" ] || fail "-L produced a symlink"

mkdir "$WORKDIR/nofollow"
printf 'x\n' >"$WORKDIR/nofollow/file"
ln -s file "$WORKDIR/nofollow/source-link"
"$LN_BIN" -P "$WORKDIR/nofollow/source-link" "$WORKDIR/nofollow/hard-link-to-symlink"
assert_same_inode "$WORKDIR/nofollow/source-link" "$WORKDIR/nofollow/hard-link-to-symlink"
assert_symlink_target "file" "$WORKDIR/nofollow/hard-link-to-symlink"

mkdir "$WORKDIR/force"
printf 'old\n' >"$WORKDIR/force/dst"
"$LN_BIN" -f "$WORKDIR/basic/source" "$WORKDIR/force/dst"
assert_same_inode "$WORKDIR/basic/source" "$WORKDIR/force/dst"

mkdir "$WORKDIR/interactive-yes"
printf 'src\n' >"$WORKDIR/interactive-yes/src"
printf 'dst\n' >"$WORKDIR/interactive-yes/dst"
if ! printf 'y\n' | "$LN_BIN" -i "$WORKDIR/interactive-yes/src" \
	"$WORKDIR/interactive-yes/dst" >"$STDOUT_FILE" 2>"$STDERR_FILE"; then
	fail "interactive yes failed"
fi
LAST_STDOUT=$(cat "$STDOUT_FILE")
LAST_STDERR=$(cat "$STDERR_FILE")
assert_empty "interactive yes stdout" "$LAST_STDOUT"
assert_contains "interactive yes prompt" "$LAST_STDERR" \
	"replace $WORKDIR/interactive-yes/dst?"
assert_same_inode "$WORKDIR/interactive-yes/src" "$WORKDIR/interactive-yes/dst"

mkdir "$WORKDIR/interactive-no"
printf 'src\n' >"$WORKDIR/interactive-no/src"
printf 'dst\n' >"$WORKDIR/interactive-no/dst"
if printf 'n\n' | "$LN_BIN" -i "$WORKDIR/interactive-no/src" \
	"$WORKDIR/interactive-no/dst" >"$STDOUT_FILE" 2>"$STDERR_FILE"; then
	fail "interactive no unexpectedly succeeded"
else
	LAST_STATUS=$?
fi
LAST_STDOUT=$(cat "$STDOUT_FILE")
LAST_STDERR=$(cat "$STDERR_FILE")
assert_status "interactive no status" 1 "$LAST_STATUS"
assert_empty "interactive no stdout" "$LAST_STDOUT"
assert_contains "interactive no prompt" "$LAST_STDERR" \
	"replace $WORKDIR/interactive-no/dst?"
assert_contains "interactive no not replaced" "$LAST_STDERR" "not replaced"
[ "$(cat "$WORKDIR/interactive-no/dst")" = "dst" ] || fail "interactive no replaced target"

mkdir "$WORKDIR/interactive-eof"
printf 'src\n' >"$WORKDIR/interactive-eof/src"
printf 'dst\n' >"$WORKDIR/interactive-eof/dst"
if : | "$LN_BIN" -i "$WORKDIR/interactive-eof/src" \
	"$WORKDIR/interactive-eof/dst" >"$STDOUT_FILE" 2>"$STDERR_FILE"; then
	fail "interactive eof unexpectedly succeeded"
else
	LAST_STATUS=$?
fi
LAST_STDOUT=$(cat "$STDOUT_FILE")
LAST_STDERR=$(cat "$STDERR_FILE")
assert_status "interactive eof status" 1 "$LAST_STATUS"
assert_empty "interactive eof stdout" "$LAST_STDOUT"
assert_contains "interactive eof prompt" "$LAST_STDERR" \
	"replace $WORKDIR/interactive-eof/dst?"
assert_contains "interactive eof not replaced" "$LAST_STDERR" "not replaced"
[ "$(cat "$WORKDIR/interactive-eof/dst")" = "dst" ] || fail "interactive eof replaced target"

mkdir "$WORKDIR/replace-dir"
mkdir "$WORKDIR/replace-dir/source-dir"
mkdir "$WORKDIR/replace-dir/empty-dir"
"$LN_BIN" -sF "$WORKDIR/replace-dir/source-dir" "$WORKDIR/replace-dir/empty-dir"
assert_symlink_target "$WORKDIR/replace-dir/source-dir" "$WORKDIR/replace-dir/empty-dir"

mkdir "$WORKDIR/nonempty"
mkdir "$WORKDIR/nonempty/source-dir"
mkdir "$WORKDIR/nonempty/dir"
printf 'keep\n' >"$WORKDIR/nonempty/dir/file"
run_capture "$LN_BIN" -sF "$WORKDIR/nonempty/source-dir" "$WORKDIR/nonempty/dir"
assert_status "non-empty dir replace status" 1 "$LAST_STATUS"
assert_empty "non-empty dir replace stdout" "$LAST_STDOUT"
assert_contains "non-empty dir replace stderr" "$LAST_STDERR" \
	"$WORKDIR/nonempty/dir"
[ -d "$WORKDIR/nonempty/dir" ] || fail "non-empty directory was removed"

mkdir "$WORKDIR/no-follow-target"
mkdir "$WORKDIR/no-follow-target/actual-dir"
ln -s actual-dir "$WORKDIR/no-follow-target/linkdir"
"$LN_BIN" -snf replacement "$WORKDIR/no-follow-target/linkdir"
assert_symlink_target "replacement" "$WORKDIR/no-follow-target/linkdir"
[ ! -e "$WORKDIR/no-follow-target/actual-dir/replacement" ] || \
	fail "-snf followed symlink target"

mkdir "$WORKDIR/follow-target"
mkdir "$WORKDIR/follow-target/actual-dir"
ln -s actual-dir "$WORKDIR/follow-target/linkdir"
"$LN_BIN" -s replacement "$WORKDIR/follow-target/linkdir"
assert_symlink_target "replacement" "$WORKDIR/follow-target/actual-dir/replacement"

mkdir "$WORKDIR/force-append"
mkdir "$WORKDIR/force-append/srcdir"
mkdir "$WORKDIR/force-append/out"
"$LN_BIN" -sF "$WORKDIR/force-append/srcdir" "$WORKDIR/force-append/out/"
assert_symlink_target "$WORKDIR/force-append/srcdir" \
	"$WORKDIR/force-append/out/srcdir"

mkdir "$WORKDIR/force-dot"
mkdir "$WORKDIR/force-dot/srcdir"
mkdir "$WORKDIR/force-dot/out"
(
	cd "$WORKDIR/force-dot/out"
	"$LN_BIN" -sF ../srcdir .
)
assert_symlink_target "../srcdir" "$WORKDIR/force-dot/out/srcdir"

mkdir "$WORKDIR/no-target-follow-error"
mkdir "$WORKDIR/no-target-follow-error/actual-dir"
ln -s actual-dir "$WORKDIR/no-target-follow-error/linkdir"
run_capture "$LN_BIN" -h "$WORKDIR/basic/source" \
	"$WORKDIR/force/dst" "$WORKDIR/no-target-follow-error/linkdir"
assert_status "target symlink with -h status" 1 "$LAST_STATUS"
assert_empty "target symlink with -h stdout" "$LAST_STDOUT"
assert_contains "target symlink with -h stderr" "$LAST_STDERR" \
	"$WORKDIR/no-target-follow-error/linkdir"

mkdir "$WORKDIR/multi"
printf 'a\n' >"$WORKDIR/multi/a"
printf 'b\n' >"$WORKDIR/multi/b"
mkdir "$WORKDIR/multi/out"
"$LN_BIN" "$WORKDIR/multi/a" "$WORKDIR/multi/b" "$WORKDIR/multi/out"
assert_same_inode "$WORKDIR/multi/a" "$WORKDIR/multi/out/a"
assert_same_inode "$WORKDIR/multi/b" "$WORKDIR/multi/out/b"

mkdir "$WORKDIR/verbose"
printf 'v\n' >"$WORKDIR/verbose/src"
run_capture "$LN_BIN" -sv "$WORKDIR/verbose/src" "$WORKDIR/verbose/link"
assert_status "verbose symbolic status" 0 "$LAST_STATUS"
assert_contains "verbose symbolic stdout" "$LAST_STDOUT" \
	"$WORKDIR/verbose/link -> $WORKDIR/verbose/src"
assert_empty "verbose symbolic stderr" "$LAST_STDERR"

mkdir "$WORKDIR/errors"
run_capture "$LN_BIN" "$WORKDIR/errors/missing" "$WORKDIR/errors/dst"
assert_status "missing hard source status" 1 "$LAST_STATUS"
assert_empty "missing hard source stdout" "$LAST_STDOUT"
assert_contains "missing hard source stderr" "$LAST_STDERR" \
	"$WORKDIR/errors/missing"

mkdir "$WORKDIR/errors/dir-source"
run_capture "$LN_BIN" "$WORKDIR/errors/dir-source" "$WORKDIR/errors/dir-link"
assert_status "dir source status" 1 "$LAST_STATUS"
assert_empty "dir source stdout" "$LAST_STDOUT"
assert_contains "dir source stderr" "$LAST_STDERR" "Is a directory"

mkdir "$WORKDIR/same-entry"
printf 'same\n' >"$WORKDIR/same-entry/file"
run_capture "$LN_BIN" "$WORKDIR/same-entry/file" "$WORKDIR/same-entry/./file"
assert_status "same entry status" 1 "$LAST_STATUS"
assert_empty "same entry stdout" "$LAST_STDOUT"
assert_contains "same entry stderr" "$LAST_STDERR" "same directory entry"

mkdir "$WORKDIR/link-mode"
printf 'link\n' >"$WORKDIR/link-mode/source"
"$LINK_BIN" "$WORKDIR/link-mode/source" "$WORKDIR/link-mode/target"
assert_same_inode "$WORKDIR/link-mode/source" "$WORKDIR/link-mode/target"

run_capture "$LINK_BIN" -s "$WORKDIR/link-mode/source" "$WORKDIR/link-mode/other"
assert_status "link mode rejects options" 2 "$LAST_STATUS"
assert_empty "link mode rejects options stdout" "$LAST_STDOUT"
assert_eq "link mode rejects options stderr" "$LINK_USAGE" "$LAST_STDERR"

printf '%s\n' "PASS"
