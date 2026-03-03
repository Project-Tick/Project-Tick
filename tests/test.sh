#!/bin/sh
set -eu

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
RMDIR_BIN=${RMDIR_BIN:-"$ROOT/out/rmdir"}
TMPDIR=${TMPDIR:-/tmp}
WORKDIR=$(mktemp -d "$TMPDIR/rmdir-test.XXXXXX")
STDOUT_FILE="$WORKDIR/stdout"
STDERR_FILE="$WORKDIR/stderr"
LAST_STATUS=0
LAST_STDOUT=
LAST_STDERR=
trap 'chmod -R u+rwx "$WORKDIR" 2>/dev/null || true; rm -rf "$WORKDIR"' EXIT INT TERM

export LC_ALL=C

USAGE_TEXT='usage: rmdir [-pv] directory ...'

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

run_capture() {
	if "$@" >"$STDOUT_FILE" 2>"$STDERR_FILE"; then
		LAST_STATUS=0
	else
		LAST_STATUS=$?
	fi
	LAST_STDOUT=$(cat "$STDOUT_FILE")
	LAST_STDERR=$(cat "$STDERR_FILE")
}

[ -x "$RMDIR_BIN" ] || fail "missing binary: $RMDIR_BIN"

run_capture "$RMDIR_BIN"
assert_status "usage status" 2 "$LAST_STATUS"
assert_empty "usage stdout" "$LAST_STDOUT"
assert_eq "usage stderr" "$USAGE_TEXT" "$LAST_STDERR"

run_capture "$RMDIR_BIN" -z
assert_status "invalid option status" 2 "$LAST_STATUS"
assert_empty "invalid option stdout" "$LAST_STDOUT"
assert_eq "invalid option stderr" "$USAGE_TEXT" "$LAST_STDERR"

run_capture "$RMDIR_BIN" --
assert_status "dash-dash without operands status" 2 "$LAST_STATUS"
assert_empty "dash-dash without operands stdout" "$LAST_STDOUT"
assert_eq "dash-dash without operands stderr" "$USAGE_TEXT" "$LAST_STDERR"

mkdir "$WORKDIR/basic"
run_capture "$RMDIR_BIN" "$WORKDIR/basic"
assert_status "basic remove status" 0 "$LAST_STATUS"
assert_empty "basic remove stdout" "$LAST_STDOUT"
assert_empty "basic remove stderr" "$LAST_STDERR"
[ ! -e "$WORKDIR/basic" ] || fail "basic directory still exists"

mkdir "$WORKDIR/verbose"
run_capture "$RMDIR_BIN" -v "$WORKDIR/verbose"
assert_status "verbose remove status" 0 "$LAST_STATUS"
assert_eq "verbose remove stdout" "$WORKDIR/verbose" "$LAST_STDOUT"
assert_empty "verbose remove stderr" "$LAST_STDERR"
[ ! -e "$WORKDIR/verbose" ] || fail "verbose directory still exists"

mkdir "$WORKDIR/non-empty"
printf 'payload\n' >"$WORKDIR/non-empty/file"
run_capture "$RMDIR_BIN" "$WORKDIR/non-empty"
assert_status "non-empty status" 1 "$LAST_STATUS"
assert_empty "non-empty stdout" "$LAST_STDOUT"
assert_contains "non-empty stderr path" "$LAST_STDERR" "$WORKDIR/non-empty"
[ -d "$WORKDIR/non-empty" ] || fail "non-empty directory removed unexpectedly"

run_capture "$RMDIR_BIN" "$WORKDIR/missing"
assert_status "missing status" 1 "$LAST_STATUS"
assert_empty "missing stdout" "$LAST_STDOUT"
assert_contains "missing stderr path" "$LAST_STDERR" "$WORKDIR/missing"

printf 'plain\n' >"$WORKDIR/plain-file"
run_capture "$RMDIR_BIN" "$WORKDIR/plain-file"
assert_status "file operand status" 1 "$LAST_STATUS"
assert_empty "file operand stdout" "$LAST_STDOUT"
assert_contains "file operand stderr path" "$LAST_STDERR" "$WORKDIR/plain-file"
[ -f "$WORKDIR/plain-file" ] || fail "file operand changed unexpectedly"

mkdir -p "$WORKDIR/prune/a/b"
run_capture sh -c 'cd "$1" && "$2" -p prune/a/b' sh "$WORKDIR" "$RMDIR_BIN"
assert_status "parent prune status" 0 "$LAST_STATUS"
assert_empty "parent prune stdout" "$LAST_STDOUT"
assert_empty "parent prune stderr" "$LAST_STDERR"
[ ! -e "$WORKDIR/prune/a/b" ] || fail "leaf directory still exists"
[ ! -e "$WORKDIR/prune/a" ] || fail "parent directory still exists"
[ ! -e "$WORKDIR/prune" ] || fail "root prune directory still exists"

mkdir -p "$WORKDIR/verbose-prune/one/two"
run_capture sh -c 'cd "$1" && "$2" -pv verbose-prune/one/two' sh "$WORKDIR" "$RMDIR_BIN"
assert_status "verbose prune status" 0 "$LAST_STATUS"
assert_eq "verbose prune stdout" "verbose-prune/one/two
verbose-prune/one
verbose-prune" "$LAST_STDOUT"
assert_empty "verbose prune stderr" "$LAST_STDERR"
[ ! -e "$WORKDIR/verbose-prune" ] || fail "verbose prune root still exists"

mkdir -p "$WORKDIR/stop/a/b"
mkdir "$WORKDIR/stop/a/keep"
run_capture sh -c 'cd "$1" && "$2" -p stop/a/b' sh "$WORKDIR" "$RMDIR_BIN"
assert_status "parent stop status" 1 "$LAST_STATUS"
assert_empty "parent stop stdout" "$LAST_STDOUT"
assert_contains "parent stop stderr path" "$LAST_STDERR" "stop/a"
[ ! -e "$WORKDIR/stop/a/b" ] || fail "parent stop leaf still exists"
[ -d "$WORKDIR/stop/a" ] || fail "parent stop parent removed unexpectedly"
[ -d "$WORKDIR/stop" ] || fail "parent stop root removed unexpectedly"

mkdir -p "$WORKDIR/slashy/a/b"
run_capture sh -c 'cd "$1" && "$2" -pv slashy//a///b///' sh "$WORKDIR" "$RMDIR_BIN"
assert_status "slashy prune status" 0 "$LAST_STATUS"
assert_eq "slashy prune stdout" "slashy//a///b///
slashy//a
slashy" "$LAST_STDOUT"
assert_empty "slashy prune stderr" "$LAST_STDERR"
[ ! -e "$WORKDIR/slashy" ] || fail "slashy root still exists"

mkdir "$WORKDIR/-dash"
run_capture "$RMDIR_BIN" -- "$WORKDIR/-dash"
assert_status "dash operand status" 0 "$LAST_STATUS"
assert_empty "dash operand stdout" "$LAST_STDOUT"
assert_empty "dash operand stderr" "$LAST_STDERR"
[ ! -e "$WORKDIR/-dash" ] || fail "dash operand still exists"

mkdir "$WORKDIR/multi-ok"
run_capture "$RMDIR_BIN" "$WORKDIR/multi-ok" "$WORKDIR/multi-missing"
assert_status "multi operand partial failure status" 1 "$LAST_STATUS"
assert_empty "multi operand partial failure stdout" "$LAST_STDOUT"
assert_contains "multi operand partial failure stderr" "$LAST_STDERR" "$WORKDIR/multi-missing"
[ ! -e "$WORKDIR/multi-ok" ] || fail "multi operand successful removal failed"

run_capture "$RMDIR_BIN" /
assert_status "root operand status" 1 "$LAST_STATUS"
assert_empty "root operand stdout" "$LAST_STDOUT"
assert_contains "root operand stderr path" "$LAST_STDERR" "/"

printf '%s\n' "PASS"
