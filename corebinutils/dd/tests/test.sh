#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
DD_BIN=${DD_BIN:-"$ROOT/out/dd"}
DD_GEN=${DD_GEN:-"$ROOT/out/dd-gen"}
TMPDIR=${TMPDIR:-/tmp}
WORKDIR=$(mktemp -d "$TMPDIR/dd-test.XXXXXX")
fifo_writer=""
dd_pid=""
trap '
	if [ -n "$dd_pid" ]; then
		kill "$dd_pid" 2>/dev/null || true
		wait "$dd_pid" 2>/dev/null || true
	fi
	if [ -n "$fifo_writer" ]; then
		kill "$fifo_writer" 2>/dev/null || true
		wait "$fifo_writer" 2>/dev/null || true
	fi
	rm -rf "$WORKDIR"
' EXIT INT TERM

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

assert_file_eq() {
	name=$1
	expected=$2
	actual=$3
	cmp "$expected" "$actual" >/dev/null 2>&1 || fail "$name"
}

run_err() {
	"$DD_BIN" "$@" 2>&1 || true
}

[ -x "$DD_BIN" ] || fail "missing binary: $DD_BIN"
[ -x "$DD_GEN" ] || fail "missing generator: $DD_GEN"

export LC_ALL=C

usage_output=$(run_err bogus=1)
assert_match "unknown operand rejection" 'unknown operand bogus' "$usage_output"

missing_value=$(run_err bs=)
assert_match "missing operand value rejection" 'no value specified for bs' "$missing_value"

record_error=$(run_err cbs=8)
assert_match "cbs without record conversion rejection" 'cbs meaningless if not doing record operations' "$record_error"

files_error=$(run_err files=2 if=/dev/zero of=/dev/null count=1 status=none)
assert_match "files on non-tape rejected" 'files is not supported for non-tape devices' "$files_error"

printf 'abcdef' > "$WORKDIR/basic.in"
"$DD_BIN" if="$WORKDIR/basic.in" of="$WORKDIR/basic.out" bs=3 count=2 2>"$WORKDIR/basic.err"
assert_file_eq "basic copy" "$WORKDIR/basic.in" "$WORKDIR/basic.out"
assert_match "default summary emitted" '^2\+0 records in' "$(cat "$WORKDIR/basic.err")"

printf '0123456789' > "$WORKDIR/seek.in"
printf 'zzzzzzzzzz' > "$WORKDIR/seek.out"
"$DD_BIN" if="$WORKDIR/seek.in" of="$WORKDIR/seek.out" ibs=2 obs=2 skip=1 seek=2 count=2 conv=notrunc status=none 2>"$WORKDIR/seek.err"
assert_eq "skip/seek/notrunc" "zzzz2345zz" "$(cat "$WORKDIR/seek.out")"
[ ! -s "$WORKDIR/seek.err" ] || fail "status=none should suppress summary output"

"$DD_GEN" | "$DD_BIN" conv=swab status=none 2>/dev/null | hexdump -C > "$WORKDIR/swab.txt"
assert_file_eq "swab conversion reference" "$ROOT/ref.swab" "$WORKDIR/swab.txt"

"$DD_GEN" 189284 | "$DD_BIN" ibs=16 obs=8 conv=sparse of="$WORKDIR/sparse.out" status=none 2>/dev/null
hexdump -C "$WORKDIR/sparse.out" > "$WORKDIR/sparse.txt"
assert_file_eq "sparse content reference" "$ROOT/ref.obs_zeroes" "$WORKDIR/sparse.txt"

"$DD_GEN" 189284 > "$WORKDIR/regular.in"
"$DD_BIN" if="$WORKDIR/regular.in" of="$WORKDIR/regular.out" bs=16 status=none 2>/dev/null
sparse_blocks=$(stat -c '%b' "$WORKDIR/sparse.out")
regular_blocks=$(stat -c '%b' "$WORKDIR/regular.out")
[ "$sparse_blocks" -le "$regular_blocks" ] || fail "sparse output should not allocate more blocks than regular output"

mkfifo "$WORKDIR/fullblock.fifo"
(
	exec 3>"$WORKDIR/fullblock.fifo"
	printf 'ab' >&3
	sleep 1
	printf 'cd' >&3
	exec 3>&-
) &
fifo_writer=$!
"$DD_BIN" if="$WORKDIR/fullblock.fifo" of="$WORKDIR/fullblock-short.out" bs=4 count=1 status=none 2>/dev/null
wait "$fifo_writer" || true
fifo_writer=""
assert_eq "short read without fullblock" "ab" "$(cat "$WORKDIR/fullblock-short.out")"

rm -f "$WORKDIR/fullblock.fifo"
mkfifo "$WORKDIR/fullblock.fifo"
(
	exec 3>"$WORKDIR/fullblock.fifo"
	printf 'ab' >&3
	sleep 1
	printf 'cd' >&3
	exec 3>&-
) &
fifo_writer=$!
"$DD_BIN" if="$WORKDIR/fullblock.fifo" of="$WORKDIR/fullblock-full.out" bs=4 count=1 iflag=fullblock status=none 2>/dev/null
wait "$fifo_writer" || true
fifo_writer=""
assert_eq "fullblock fills requested block" "abcd" "$(cat "$WORKDIR/fullblock-full.out")"

mkfifo "$WORKDIR/signal-read.fifo"
(
	exec 3>"$WORKDIR/signal-read.fifo"
	sleep 30
) &
fifo_writer=$!
"$DD_BIN" if="$WORKDIR/signal-read.fifo" of=/dev/null 2>"$WORKDIR/signal-read.err" &
dd_pid=$!
sleep 1
kill -USR1 "$dd_pid"
sleep 1
kill -INT "$dd_pid"
read_rc=0
wait "$dd_pid" || read_rc=$?
dd_pid=""
kill "$fifo_writer" 2>/dev/null || true
wait "$fifo_writer" 2>/dev/null || true
fifo_writer=""
[ "$read_rc" -gt 128 ] || fail "SIGINT during blocked read should terminate via signal"
assert_match "SIGUSR1 summary output" 'records in' "$(cat "$WORKDIR/signal-read.err")"

mkfifo "$WORKDIR/signal-open.fifo"
"$DD_BIN" if="$WORKDIR/signal-open.fifo" of=/dev/null 2>"$WORKDIR/signal-open.err" &
dd_pid=$!
sleep 1
kill -INT "$dd_pid"
open_rc=0
wait "$dd_pid" || open_rc=$?
dd_pid=""
[ "$open_rc" -gt 128 ] || fail "SIGINT during blocked open should terminate via signal"
[ -s "$WORKDIR/signal-open.err" ] || fail "SIGINT during blocked open should emit diagnostics"

printf '%s\n' "PASS"
