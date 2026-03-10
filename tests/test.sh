#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TEST_BIN=${TEST_BIN:-"$ROOT/out/test"}
BRACKET_BIN=${BRACKET_BIN:-"$ROOT/out/["}
FD_HELPER_BIN=${FD_HELPER_BIN:-"$ROOT/build/fd_helper"}

TMPDIR=${TMPDIR:-/tmp}
WORKDIR=$(mktemp -d "$TMPDIR/test-test.XXXXXX")
STDOUT_FILE="$WORKDIR/stdout"
STDERR_FILE="$WORKDIR/stderr"
LAST_STATUS=0
LAST_STDOUT=
LAST_STDERR=

export LC_ALL=C
export TZ=UTC

cleanup() {
	rm -rf "$WORKDIR"
}

trap cleanup EXIT INT TERM HUP

fail() {
	printf 'FAIL: %s\n' "$1" >&2
	exit 1
}

assert_status() {
	name=$1
	expected=$2
	actual=$3

	if [ "$expected" -ne "$actual" ]; then
		printf 'FAIL: %s\n' "$name" >&2
		printf 'expected status: %s\n' "$expected" >&2
		printf 'actual status: %s\n' "$actual" >&2
		exit 1
	fi
}

assert_eq() {
	name=$1
	expected=$2
	actual=$3

	if [ "$expected" != "$actual" ]; then
		printf 'FAIL: %s\n' "$name" >&2
		printf '%s\n' '--- expected ---' >&2
		printf '%s\n' "$expected" >&2
		printf '%s\n' '--- actual ---' >&2
		printf '%s\n' "$actual" >&2
		exit 1
	fi
}

assert_empty() {
	name=$1
	value=$2

	if [ -n "$value" ]; then
		printf 'FAIL: %s\n' "$name" >&2
		printf '%s\n' '--- expected empty ---' >&2
		printf '%s\n' '--- actual ---' >&2
		printf '%s\n' "$value" >&2
		exit 1
	fi
}

assert_contains() {
	name=$1
	value=$2
	pattern=$3

	case $value in
		*"$pattern"*) ;;
		*) fail "$name" ;;
	esac
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

run_in_shell() {
	if sh -c "$1" >"$STDOUT_FILE" 2>"$STDERR_FILE"; then
		LAST_STATUS=0
	else
		LAST_STATUS=$?
	fi
	LAST_STDOUT=$(cat "$STDOUT_FILE")
	LAST_STDERR=$(cat "$STDERR_FILE")
}

find_socket_path() {
	for candidate in \
		/run/* /run/*/* /run/*/*/* \
		/var/run/* /var/run/*/* /var/run/*/*/* \
		/tmp/* /tmp/*/* /tmp/*/*/*; do
		if [ -S "$candidate" ]; then
			printf '%s\n' "$candidate"
			return 0
		fi
	done
	return 1
}

skip() {
	printf 'SKIP: %s\n' "$1"
}

[ -x "$TEST_BIN" ] || fail "missing binary: $TEST_BIN"
[ -x "$BRACKET_BIN" ] || fail "missing bracket binary: $BRACKET_BIN"
[ -x "$FD_HELPER_BIN" ] || fail "missing fd helper: $FD_HELPER_BIN"

run_capture "$TEST_BIN"
assert_status "no expression status" 1 "$LAST_STATUS"
assert_empty "no expression stdout" "$LAST_STDOUT"
assert_empty "no expression stderr" "$LAST_STDERR"

run_capture "$TEST_BIN" ""
assert_status "empty operand status" 1 "$LAST_STATUS"
assert_empty "empty operand stdout" "$LAST_STDOUT"
assert_empty "empty operand stderr" "$LAST_STDERR"

run_capture "$TEST_BIN" value
assert_status "single operand status" 0 "$LAST_STATUS"
assert_empty "single operand stdout" "$LAST_STDOUT"
assert_empty "single operand stderr" "$LAST_STDERR"

run_capture "$TEST_BIN" -n ""
assert_status "string -n status" 1 "$LAST_STATUS"
assert_empty "string -n stdout" "$LAST_STDOUT"
assert_empty "string -n stderr" "$LAST_STDERR"

run_capture "$TEST_BIN" -z ""
assert_status "string -z status" 0 "$LAST_STATUS"
assert_empty "string -z stdout" "$LAST_STDOUT"
assert_empty "string -z stderr" "$LAST_STDERR"

run_capture "$TEST_BIN" -h = -h
assert_status "operator-like operand status" 0 "$LAST_STATUS"
assert_empty "operator-like operand stdout" "$LAST_STDOUT"
assert_empty "operator-like operand stderr" "$LAST_STDERR"

run_capture "$TEST_BIN" alpha = alpha
assert_status "string equality status" 0 "$LAST_STATUS"
assert_empty "string equality stdout" "$LAST_STDOUT"
assert_empty "string equality stderr" "$LAST_STDERR"

run_capture "$TEST_BIN" alpha == alpha
assert_status "string double equals status" 0 "$LAST_STATUS"
assert_empty "string double equals stdout" "$LAST_STDOUT"
assert_empty "string double equals stderr" "$LAST_STDERR"

run_capture "$TEST_BIN" alpha '!=' beta
assert_status "string inequality status" 0 "$LAST_STATUS"
assert_empty "string inequality stdout" "$LAST_STDOUT"
assert_empty "string inequality stderr" "$LAST_STDERR"

run_capture "$TEST_BIN" alpha '<' beta
assert_status "string less-than status" 0 "$LAST_STATUS"
assert_empty "string less-than stdout" "$LAST_STDOUT"
assert_empty "string less-than stderr" "$LAST_STDERR"

run_capture "$TEST_BIN" beta '>' alpha
assert_status "string greater-than status" 0 "$LAST_STATUS"
assert_empty "string greater-than stdout" "$LAST_STDOUT"
assert_empty "string greater-than stderr" "$LAST_STDERR"

run_capture "$TEST_BIN" 100 -eq 100
assert_status "numeric eq status" 0 "$LAST_STATUS"
assert_empty "numeric eq stdout" "$LAST_STDOUT"
assert_empty "numeric eq stderr" "$LAST_STDERR"

run_capture "$TEST_BIN" -5 -lt 5
assert_status "numeric lt status" 0 "$LAST_STATUS"
assert_empty "numeric lt stdout" "$LAST_STDOUT"
assert_empty "numeric lt stderr" "$LAST_STDERR"

run_capture "$TEST_BIN" 7 -ge 8
assert_status "numeric false status" 1 "$LAST_STATUS"
assert_empty "numeric false stdout" "$LAST_STDOUT"
assert_empty "numeric false stderr" "$LAST_STDERR"

run_capture "$TEST_BIN" abc -eq 1
assert_status "bad number status" 2 "$LAST_STATUS"
assert_empty "bad number stdout" "$LAST_STDOUT"
assert_eq "bad number stderr" "test: abc: bad number" "$LAST_STDERR"

run_capture "$TEST_BIN" 999999999999999999999999999999 -eq 1
assert_status "out of range status" 2 "$LAST_STATUS"
assert_empty "out of range stdout" "$LAST_STDOUT"
assert_eq "out of range stderr" "test: 999999999999999999999999999999: out of range" "$LAST_STDERR"

run_capture "$TEST_BIN" '!' ""
assert_status "bang empty status" 0 "$LAST_STATUS"
assert_empty "bang empty stdout" "$LAST_STDOUT"
assert_empty "bang empty stderr" "$LAST_STDERR"

run_capture "$TEST_BIN" '!'
assert_status "bare bang status" 0 "$LAST_STATUS"
assert_empty "bare bang stdout" "$LAST_STDOUT"
assert_empty "bare bang stderr" "$LAST_STDERR"

run_capture "$TEST_BIN" '(' 0 -eq 0 ')' -a '(' 2 -gt 1 ')'
assert_status "paren and status" 0 "$LAST_STATUS"
assert_empty "paren and stdout" "$LAST_STDOUT"
assert_empty "paren and stderr" "$LAST_STDERR"

run_capture "$TEST_BIN" 1 -eq 0 -o a = a -a 1 -eq 0 -o a = aa
assert_status "precedence status" 1 "$LAST_STATUS"
assert_empty "precedence stdout" "$LAST_STDOUT"
assert_empty "precedence stderr" "$LAST_STDERR"

run_capture "$TEST_BIN" '(' 1 = 1
assert_status "closing paren error status" 2 "$LAST_STATUS"
assert_empty "closing paren error stdout" "$LAST_STDOUT"
assert_eq "closing paren error stderr" "test: closing paren expected" "$LAST_STDERR"

run_capture "$TEST_BIN" 1 -eq
assert_status "argument expected status" 2 "$LAST_STATUS"
assert_empty "argument expected stdout" "$LAST_STDOUT"
assert_eq "argument expected stderr" "test: -eq: argument expected" "$LAST_STDERR"

run_capture "$TEST_BIN" one two
assert_status "unexpected operator status" 2 "$LAST_STATUS"
assert_empty "unexpected operator stdout" "$LAST_STDOUT"
assert_eq "unexpected operator stderr" "test: two: unexpected operator" "$LAST_STDERR"

REGULAR_FILE="$WORKDIR/regular"
EMPTY_FILE="$WORKDIR/empty"
EXECUTABLE_FILE="$WORKDIR/executable"
PERM_FILE="$WORKDIR/no-perm"
FIFO_PATH="$WORKDIR/fifo"
LINK_PATH="$WORKDIR/link"
HARDLINK_PATH="$WORKDIR/hardlink"
DIR_PATH="$WORKDIR/dir"
STICKY_DIR="$WORKDIR/sticky"
OLDER_FILE="$WORKDIR/older"
NEWER_FILE="$WORKDIR/newer"
MODE_FILE="$WORKDIR/mode"

printf 'payload\n' >"$REGULAR_FILE"
: >"$EMPTY_FILE"
printf '#!/bin/sh\nexit 0\n' >"$EXECUTABLE_FILE"
chmod 0755 "$EXECUTABLE_FILE"
: >"$PERM_FILE"
chmod 0000 "$PERM_FILE"
mkfifo "$FIFO_PATH"
ln -s "$REGULAR_FILE" "$LINK_PATH"
ln "$REGULAR_FILE" "$HARDLINK_PATH"
mkdir "$DIR_PATH"
mkdir "$STICKY_DIR"
chmod 1777 "$STICKY_DIR"
: >"$MODE_FILE"
chmod 6755 "$MODE_FILE"
: >"$OLDER_FILE"
sleep 1
: >"$NEWER_FILE"

run_capture "$TEST_BIN" -e "$REGULAR_FILE"
assert_status "file exists status" 0 "$LAST_STATUS"
assert_empty "file exists stdout" "$LAST_STDOUT"
assert_empty "file exists stderr" "$LAST_STDERR"

run_capture "$TEST_BIN" -f "$REGULAR_FILE"
assert_status "regular file status" 0 "$LAST_STATUS"
assert_empty "regular file stdout" "$LAST_STDOUT"
assert_empty "regular file stderr" "$LAST_STDERR"

run_capture "$TEST_BIN" -d "$DIR_PATH"
assert_status "directory status" 0 "$LAST_STATUS"
assert_empty "directory stdout" "$LAST_STDOUT"
assert_empty "directory stderr" "$LAST_STDERR"

run_capture "$TEST_BIN" -s "$REGULAR_FILE"
assert_status "size greater than zero status" 0 "$LAST_STATUS"
assert_empty "size greater than zero stdout" "$LAST_STDOUT"
assert_empty "size greater than zero stderr" "$LAST_STDERR"

run_capture "$TEST_BIN" -s "$EMPTY_FILE"
assert_status "size zero status" 1 "$LAST_STATUS"
assert_empty "size zero stdout" "$LAST_STDOUT"
assert_empty "size zero stderr" "$LAST_STDERR"

run_capture "$TEST_BIN" -p "$FIFO_PATH"
assert_status "fifo status" 0 "$LAST_STATUS"
assert_empty "fifo stdout" "$LAST_STDOUT"
assert_empty "fifo stderr" "$LAST_STDERR"

run_capture "$TEST_BIN" -L "$LINK_PATH"
assert_status "symlink L status" 0 "$LAST_STATUS"
assert_empty "symlink L stdout" "$LAST_STDOUT"
assert_empty "symlink L stderr" "$LAST_STDERR"

run_capture "$TEST_BIN" -h "$LINK_PATH"
assert_status "symlink h status" 0 "$LAST_STATUS"
assert_empty "symlink h stdout" "$LAST_STDOUT"
assert_empty "symlink h stderr" "$LAST_STDERR"

SOCKET_PATH=$(find_socket_path || true)
if [ -n "$SOCKET_PATH" ]; then
	run_capture "$TEST_BIN" -S "$SOCKET_PATH"
	assert_status "socket status" 0 "$LAST_STATUS"
	assert_empty "socket stdout" "$LAST_STDOUT"
	assert_empty "socket stderr" "$LAST_STDERR"
else
	skip "socket positive test skipped because no UNIX socket path is visible"
fi

run_capture "$TEST_BIN" -O "$REGULAR_FILE"
assert_status "owner matches euid status" 0 "$LAST_STATUS"
assert_empty "owner matches euid stdout" "$LAST_STDOUT"
assert_empty "owner matches euid stderr" "$LAST_STDERR"

run_capture "$TEST_BIN" -G "$REGULAR_FILE"
assert_status "group matches egid status" 0 "$LAST_STATUS"
assert_empty "group matches egid stdout" "$LAST_STDOUT"
assert_empty "group matches egid stderr" "$LAST_STDERR"

run_capture "$TEST_BIN" -u "$MODE_FILE"
assert_status "setuid bit status" 0 "$LAST_STATUS"
assert_empty "setuid bit stdout" "$LAST_STDOUT"
assert_empty "setuid bit stderr" "$LAST_STDERR"

run_capture "$TEST_BIN" -g "$MODE_FILE"
assert_status "setgid bit status" 0 "$LAST_STATUS"
assert_empty "setgid bit stdout" "$LAST_STDOUT"
assert_empty "setgid bit stderr" "$LAST_STDERR"

run_capture "$TEST_BIN" -k "$STICKY_DIR"
assert_status "sticky bit status" 0 "$LAST_STATUS"
assert_empty "sticky bit stdout" "$LAST_STDOUT"
assert_empty "sticky bit stderr" "$LAST_STDERR"

run_capture "$TEST_BIN" -r "$REGULAR_FILE"
assert_status "readable status" 0 "$LAST_STATUS"
assert_empty "readable stdout" "$LAST_STDOUT"
assert_empty "readable stderr" "$LAST_STDERR"

run_capture "$TEST_BIN" -w "$REGULAR_FILE"
assert_status "writable status" 0 "$LAST_STATUS"
assert_empty "writable stdout" "$LAST_STDOUT"
assert_empty "writable stderr" "$LAST_STDERR"

run_capture "$TEST_BIN" -x "$EXECUTABLE_FILE"
assert_status "executable status" 0 "$LAST_STATUS"
assert_empty "executable stdout" "$LAST_STDOUT"
assert_empty "executable stderr" "$LAST_STDERR"

if [ "$(id -u)" -ne 0 ]; then
	run_capture "$TEST_BIN" -r "$PERM_FILE"
	assert_status "unreadable status" 1 "$LAST_STATUS"
	assert_empty "unreadable stdout" "$LAST_STDOUT"
	assert_empty "unreadable stderr" "$LAST_STDERR"

	run_capture "$TEST_BIN" -w "$PERM_FILE"
	assert_status "unwritable status" 1 "$LAST_STATUS"
	assert_empty "unwritable stdout" "$LAST_STDOUT"
	assert_empty "unwritable stderr" "$LAST_STDERR"
else
	skip "permission-negative tests skipped for euid 0"
fi

run_capture "$TEST_BIN" "$NEWER_FILE" -nt "$OLDER_FILE"
assert_status "newer-than status" 0 "$LAST_STATUS"
assert_empty "newer-than stdout" "$LAST_STDOUT"
assert_empty "newer-than stderr" "$LAST_STDERR"

run_capture "$TEST_BIN" "$OLDER_FILE" -ot "$NEWER_FILE"
assert_status "older-than status" 0 "$LAST_STATUS"
assert_empty "older-than stdout" "$LAST_STDOUT"
assert_empty "older-than stderr" "$LAST_STDERR"

run_capture "$TEST_BIN" "$REGULAR_FILE" -ef "$HARDLINK_PATH"
assert_status "same file status" 0 "$LAST_STATUS"
assert_empty "same file stdout" "$LAST_STDOUT"
assert_empty "same file stderr" "$LAST_STDERR"

run_capture "$TEST_BIN" -c /dev/null
assert_status "character device status" 0 "$LAST_STATUS"
assert_empty "character device stdout" "$LAST_STDOUT"
assert_empty "character device stderr" "$LAST_STDERR"

BLOCK_DEVICE_FOUND=
for candidate in /dev/* /dev/*/*; do
	if [ -b "$candidate" ]; then
		BLOCK_DEVICE_FOUND=$candidate
		break
	fi
done
if [ -n "${BLOCK_DEVICE_FOUND:-}" ]; then
	run_capture "$TEST_BIN" -b "$BLOCK_DEVICE_FOUND"
	assert_status "block device status" 0 "$LAST_STATUS"
	assert_empty "block device stdout" "$LAST_STDOUT"
	assert_empty "block device stderr" "$LAST_STDERR"
else
	skip "block-device positive test skipped because no block device is visible"
fi

run_capture "$TEST_BIN" -t 99
assert_status "closed fd tty status" 1 "$LAST_STATUS"
assert_empty "closed fd tty stdout" "$LAST_STDOUT"
assert_empty "closed fd tty stderr" "$LAST_STDERR"

run_capture "$FD_HELPER_BIN" 9 "$TEST_BIN" -t 9
if [ "$LAST_STATUS" -eq 126 ]; then
	assert_contains "pty helper skip reason" "$LAST_STDERR" "posix_openpt"
	skip "pty-backed -t positive test skipped because PTY allocation is blocked"
else
	assert_status "pty fd tty status" 0 "$LAST_STATUS"
	assert_empty "pty fd tty stdout" "$LAST_STDOUT"
	assert_empty "pty fd tty stderr" "$LAST_STDERR"
fi

run_capture "$BRACKET_BIN" alpha = alpha ']'
assert_status "bracket true status" 0 "$LAST_STATUS"
assert_empty "bracket true stdout" "$LAST_STDOUT"
assert_empty "bracket true stderr" "$LAST_STDERR"

run_capture "$BRACKET_BIN" ']'
assert_status "bracket empty expression status" 1 "$LAST_STATUS"
assert_empty "bracket empty expression stdout" "$LAST_STDOUT"
assert_empty "bracket empty expression stderr" "$LAST_STDERR"

run_capture "$BRACKET_BIN" alpha = alpha
assert_status "missing closing bracket status" 2 "$LAST_STATUS"
assert_empty "missing closing bracket stdout" "$LAST_STDOUT"
assert_eq "missing closing bracket stderr" "[: missing ']'" "$LAST_STDERR"

run_in_shell "TEST_BIN='$TEST_BIN' sh '$ROOT/tests/legacy_test.sh'"
assert_status "legacy suite status" 0 "$LAST_STATUS"
assert_contains "legacy suite stdout" "$LAST_STDOUT" "1..130"
assert_contains "legacy suite stdout" "$LAST_STDOUT" "PASS"
assert_empty "legacy suite stderr" "$LAST_STDERR"

printf '%s\n' "PASS"
