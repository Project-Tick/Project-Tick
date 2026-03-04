#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
STTY_BIN=${STTY_BIN:-"$ROOT/out/stty"}
CC=${CC:-cc}
TMPDIR=${TMPDIR:-/tmp}
WORKDIR=$(mktemp -d "$TMPDIR/stty-test.XXXXXX")
MKPTY_BIN="$WORKDIR/mkpty"
PROBE_BIN="$WORKDIR/termios_probe"
SLAVE_PATH_FILE="$WORKDIR/slave-path"
HOLD_FIFO="$WORKDIR/hold"
STDOUT_FILE="$WORKDIR/stdout"
STDERR_FILE="$WORKDIR/stderr"
PROBE_FILE="$WORKDIR/probe"
MKPTY_PID=""
HOLD_OPEN=0

export LC_ALL=C

cleanup() {
	if [ "$HOLD_OPEN" -eq 1 ]; then
		exec 3>&-
	fi
	if [ -n "$MKPTY_PID" ]; then
		wait "$MKPTY_PID" 2>/dev/null || true
	fi
	rm -rf "$WORKDIR"
}

trap cleanup EXIT INT TERM

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

assert_contains() {
	name=$1
	text=$2
	pattern=$3
	case $text in
		*"$pattern"*) ;;
		*) fail "$name" ;;
	esac
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

build_helpers() {
	"$CC" -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=700 -O2 -std=c17 -Wall -Wextra -Werror \
		"$ROOT/tests/mkpty.c" -o "$MKPTY_BIN" \
		|| fail "failed to build mkpty helper"
	"$CC" -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=700 -O2 -std=c17 -Wall -Wextra -Werror \
		"$ROOT/tests/termios_probe.c" -o "$PROBE_BIN" \
		|| fail "failed to build termios_probe helper"
}

start_pty() {
	mkfifo "$HOLD_FIFO"
	"$MKPTY_BIN" <"$HOLD_FIFO" >"$SLAVE_PATH_FILE" 2>"$WORKDIR/mkpty.err" &
	MKPTY_PID=$!
	exec 3>"$HOLD_FIFO"
	HOLD_OPEN=1

	i=0
	while [ ! -s "$SLAVE_PATH_FILE" ]; do
		if ! kill -0 "$MKPTY_PID" 2>/dev/null; then
			wait "$MKPTY_PID" || status=$?
			status=${status:-0}
			if [ "$status" -eq 77 ]; then
				return 77
			fi
			fail "mkpty helper failed: $(cat "$WORKDIR/mkpty.err")"
		fi
		i=$((i + 1))
		if [ "$i" -gt 100 ]; then
			fail "mkpty helper timed out"
		fi
		sleep 0.05
	done

	SLAVE_PATH=$(cat "$SLAVE_PATH_FILE")
	[ -n "$SLAVE_PATH" ] || fail "mkpty helper did not print a slave path"
}

probe_capture() {
	"$PROBE_BIN" "$SLAVE_PATH" >"$PROBE_FILE" 2>"$WORKDIR/probe.err" \
		|| fail "termios probe failed: $(cat "$WORKDIR/probe.err")"
	LAST_PROBE=$(cat "$PROBE_FILE")
}

probe_value() {
	key=$1
	value=$(sed -n "s/^$key=//p" "$PROBE_FILE")
	[ -n "$value" ] || fail "missing probe key: $key"
	printf '%s' "$value"
}

[ -x "$STTY_BIN" ] || fail "missing binary: $STTY_BIN"

run_capture "$STTY_BIN"
assert_status "stdin terminal check" 1 "$LAST_STATUS"
assert_eq "stdin terminal stderr" "stty: stdin is not a terminal" "$LAST_STDERR"
assert_eq "stdin terminal stdout" "" "$LAST_STDOUT"

run_capture "$STTY_BIN" -a -g
assert_status "mutually exclusive format options" 1 "$LAST_STATUS"
assert_contains "mutually exclusive usage" "$LAST_STDERR" \
	"usage: stty [-a | -e | -g] [-f file] [arguments]"

build_helpers
if ! start_pty; then
	printf '%s\n' "SKIP: PTY-dependent tests are unavailable in this environment" >&2
	printf '%s\n' "PASS"
	exit 0
fi

run_capture "$STTY_BIN" -f "$SLAVE_PATH"
assert_status "default output status" 0 "$LAST_STATUS"
assert_contains "default output speed" "$LAST_STDOUT" "speed "

run_capture "$STTY_BIN" -a -f "$SLAVE_PATH"
assert_status "posix output status" 0 "$LAST_STATUS"
assert_contains "posix output erase" "$LAST_STDOUT" "erase = "
assert_contains "posix output min" "$LAST_STDOUT" "min = "

run_capture "$STTY_BIN" -e -f "$SLAVE_PATH"
assert_status "bsd output status" 0 "$LAST_STATUS"
assert_contains "bsd output lflags" "$LAST_STDOUT" "lflags:"
assert_contains "bsd output iflags" "$LAST_STDOUT" "iflags:"

run_capture "$STTY_BIN" -g -f "$SLAVE_PATH"
assert_status "gfmt output status" 0 "$LAST_STATUS"
assert_contains "gfmt output prefix" "$LAST_STDOUT" "gfmt1:"
SAVED_STATE=$LAST_STDOUT

run_capture "$STTY_BIN" -f "$SLAVE_PATH" rows 40 columns 120
assert_status "rows columns status" 0 "$LAST_STATUS"
probe_capture
assert_eq "rows after set" "40" "$(probe_value row)"
assert_eq "columns after set" "120" "$(probe_value col)"

run_capture "$STTY_BIN" -f "$SLAVE_PATH" size
assert_status "size status" 0 "$LAST_STATUS"
assert_eq "size output" "40 120" "$LAST_STDOUT"

run_capture "$STTY_BIN" -f "$SLAVE_PATH" speed 9600
assert_status "speed command status" 0 "$LAST_STATUS"
probe_capture
assert_eq "speed command ispeed" "9600" "$(probe_value ispeed)"
assert_eq "speed command ospeed" "9600" "$(probe_value ospeed)"

run_capture "$STTY_BIN" -f "$SLAVE_PATH" ospeed 19200
assert_status "ospeed command status" 0 "$LAST_STATUS"
probe_capture
assert_eq "ospeed command ispeed unchanged" "9600" "$(probe_value ispeed)"
assert_eq "ospeed command ospeed" "19200" "$(probe_value ospeed)"

run_capture "$STTY_BIN" -f "$SLAVE_PATH" ispeed 4800
assert_status "ispeed command status" 0 "$LAST_STATUS"
probe_capture
assert_eq "ispeed command ispeed" "4800" "$(probe_value ispeed)"
assert_eq "ispeed command ospeed unchanged" "19200" "$(probe_value ospeed)"

run_capture "$STTY_BIN" -f "$SLAVE_PATH" 38400
assert_status "bare speed status" 0 "$LAST_STATUS"
probe_capture
assert_eq "bare speed ispeed" "38400" "$(probe_value ispeed)"
assert_eq "bare speed ospeed" "38400" "$(probe_value ospeed)"

run_capture "$STTY_BIN" -f "$SLAVE_PATH" intr '^A' min 7 time 9 -echo -icanon
assert_status "control char and flag status" 0 "$LAST_STATUS"
probe_capture
assert_eq "intr control char" "1" "$(probe_value intr)"
assert_eq "min control char" "7" "$(probe_value min)"
assert_eq "time control char" "9" "$(probe_value time)"
assert_eq "echo disabled" "0" "$(probe_value echo)"
assert_eq "icanon disabled" "0" "$(probe_value icanon)"

run_capture "$STTY_BIN" -f "$SLAVE_PATH" raw
assert_status "raw status" 0 "$LAST_STATUS"
probe_capture
assert_eq "raw echo" "0" "$(probe_value echo)"
assert_eq "raw icanon" "0" "$(probe_value icanon)"
assert_eq "raw isig" "0" "$(probe_value isig)"
assert_eq "raw iexten" "0" "$(probe_value iexten)"
assert_eq "raw opost" "0" "$(probe_value opost)"
assert_eq "raw csize" "8" "$(probe_value csize)"

run_capture "$STTY_BIN" -f "$SLAVE_PATH" -raw
assert_status "negative raw status" 0 "$LAST_STATUS"
probe_capture
assert_eq "sane echo" "1" "$(probe_value echo)"
assert_eq "sane icanon" "1" "$(probe_value icanon)"
assert_eq "sane isig" "1" "$(probe_value isig)"
assert_eq "sane iexten" "1" "$(probe_value iexten)"
assert_eq "sane opost" "1" "$(probe_value opost)"

run_capture "$STTY_BIN" -f "$SLAVE_PATH" extproc
assert_status "extproc enable status" 0 "$LAST_STATUS"
probe_capture
assert_eq "extproc enabled" "1" "$(probe_value extproc)"

run_capture "$STTY_BIN" -f "$SLAVE_PATH" -extproc
assert_status "extproc disable status" 0 "$LAST_STATUS"
probe_capture
assert_eq "extproc disabled" "0" "$(probe_value extproc)"

run_capture "$STTY_BIN" -f "$SLAVE_PATH" dec
assert_status "dec status" 0 "$LAST_STATUS"
probe_capture
assert_eq "dec intr" "3" "$(probe_value intr)"
assert_eq "dec ixany" "0" "$(probe_value ixany)"

run_capture "$STTY_BIN" -f "$SLAVE_PATH" cbreak
assert_status "cbreak status" 0 "$LAST_STATUS"
probe_capture
assert_eq "cbreak icanon" "0" "$(probe_value icanon)"
assert_eq "cbreak isig" "1" "$(probe_value isig)"

run_capture "$STTY_BIN" -f "$SLAVE_PATH" cooked
assert_status "cooked status" 0 "$LAST_STATUS"
probe_capture
assert_eq "cooked echo" "1" "$(probe_value echo)"
assert_eq "cooked icanon" "1" "$(probe_value icanon)"

run_capture "$STTY_BIN" -f "$SLAVE_PATH" tty
assert_status "tty status" 0 "$LAST_STATUS"
probe_capture
assert_eq "tty line discipline" "0" "$(probe_value ldisc)"

run_capture "$STTY_BIN" -f "$SLAVE_PATH" all
assert_status "all compatibility status" 0 "$LAST_STATUS"
assert_contains "all compatibility output" "$LAST_STDOUT" "lflags:"

run_capture "$STTY_BIN" -f "$SLAVE_PATH" raw -echo min 5 time 8
assert_status "prepare for gfmt restore" 0 "$LAST_STATUS"
run_capture "$STTY_BIN" -f "$SLAVE_PATH" "$SAVED_STATE"
assert_status "gfmt restore status" 0 "$LAST_STATUS"
run_capture "$STTY_BIN" -g -f "$SLAVE_PATH"
assert_status "gfmt restore verify status" 0 "$LAST_STATUS"
assert_eq "gfmt round trip" "$SAVED_STATE" "$LAST_STDOUT"

run_capture "$STTY_BIN" -f "$SLAVE_PATH" rows nope
assert_status "invalid rows status" 1 "$LAST_STATUS"
assert_eq "invalid rows stderr" "stty: rows requires a number between 0 and 65535" \
	"$LAST_STDERR"

run_capture "$STTY_BIN" -f "$SLAVE_PATH" min 999
assert_status "invalid min status" 1 "$LAST_STATUS"
assert_eq "invalid min stderr" "stty: min requires a number between 0 and 255" \
	"$LAST_STDERR"

run_capture "$STTY_BIN" -f "$SLAVE_PATH" speed 12345
assert_status "unsupported speed status" 1 "$LAST_STATUS"
assert_eq "unsupported speed stderr" \
	"stty: unsupported baud rate on Linux termios API: 12345" "$LAST_STDERR"

run_capture "$STTY_BIN" -f "$SLAVE_PATH" speed
assert_status "missing speed argument status" 1 "$LAST_STATUS"
assert_eq "missing speed argument stderr" "stty: speed requires an argument" \
	"$LAST_STDERR"

run_capture "$STTY_BIN" -f "$SLAVE_PATH" intr ab
assert_status "invalid control char status" 1 "$LAST_STATUS"
assert_eq "invalid control char stderr" \
	"stty: control character intr requires a single character, ^X, ^?, ^-, or undef" \
	"$LAST_STDERR"

run_capture "$STTY_BIN" -f "$SLAVE_PATH" erase2 x
assert_status "unsupported erase2 status" 1 "$LAST_STATUS"
assert_eq "unsupported erase2 stderr" \
	"stty: argument 'erase2' is not supported on Linux: Linux termios has no VERASE2 control character" \
	"$LAST_STDERR"

run_capture "$STTY_BIN" -f "$SLAVE_PATH" status '^T'
assert_status "unsupported status control char status" 1 "$LAST_STATUS"
assert_eq "unsupported status control char stderr" \
	"stty: argument 'status' is not supported on Linux: Linux termios has no VSTATUS control character" \
	"$LAST_STDERR"

run_capture "$STTY_BIN" -f "$SLAVE_PATH" altwerase
assert_status "unsupported altwerase status" 1 "$LAST_STATUS"
assert_eq "unsupported altwerase stderr" \
	"stty: argument 'altwerase' is not supported on Linux: Linux termios has no ALTWERASE local mode" \
	"$LAST_STDERR"

run_capture "$STTY_BIN" -f "$SLAVE_PATH" kerninfo
assert_status "unsupported kerninfo status" 1 "$LAST_STATUS"
assert_eq "unsupported kerninfo stderr" \
	"stty: argument 'kerninfo' is not supported on Linux: Linux has no STATUS control character or kernel tty status line" \
	"$LAST_STDERR"

run_capture "$STTY_BIN" -f "$SLAVE_PATH" rtsdtr
assert_status "unsupported rtsdtr status" 1 "$LAST_STATUS"
assert_eq "unsupported rtsdtr stderr" \
	"stty: argument 'rtsdtr' is not supported on Linux: Linux termios cannot control BSD RTS/DTR-on-open semantics" \
	"$LAST_STDERR"

printf '%s\n' "PASS"
