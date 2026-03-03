#!/bin/sh
set -eu

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
RMAIL_BIN=${RMAIL_BIN:-"$ROOT/out/rmail"}
MOCK_SENDMAIL_BIN=${MOCK_SENDMAIL_BIN:-"$ROOT/out/mock_sendmail"}
TMPDIR=${TMPDIR:-/tmp}
WORKDIR=$(mktemp -d "$TMPDIR/rmail-test.XXXXXX")
STDOUT_FILE="$WORKDIR/stdout"
STDERR_FILE="$WORKDIR/stderr"
SENDMAIL_LOG="$WORKDIR/sendmail-argv"
SENDMAIL_INPUT="$WORKDIR/sendmail-input"
LAST_STATUS=0
LAST_STDOUT=
LAST_STDERR=

trap 'rm -rf "$WORKDIR"' EXIT INT TERM

export LC_ALL=C

EX_USAGE=64
EX_DATAERR=65
EX_UNAVAILABLE=69

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
	rm -f "$STDOUT_FILE" "$STDERR_FILE"
	if "$@" >"$STDOUT_FILE" 2>"$STDERR_FILE"; then
		LAST_STATUS=0
	else
		LAST_STATUS=$?
	fi
	LAST_STDOUT=$(cat "$STDOUT_FILE")
	LAST_STDERR=$(cat "$STDERR_FILE")
}

run_with_input() {
	input=$1
	shift
	rm -f "$STDOUT_FILE" "$STDERR_FILE"
	if printf '%b' "$input" | "$@" >"$STDOUT_FILE" 2>"$STDERR_FILE"; then
		LAST_STATUS=0
	else
		LAST_STATUS=$?
	fi
	LAST_STDOUT=$(cat "$STDOUT_FILE")
	LAST_STDERR=$(cat "$STDERR_FILE")
}

run_with_regular_file() {
	input_file=$1
	shift
	rm -f "$STDOUT_FILE" "$STDERR_FILE"
	if "$@" <"$input_file" >"$STDOUT_FILE" 2>"$STDERR_FILE"; then
		LAST_STATUS=0
	else
		LAST_STATUS=$?
	fi
	LAST_STDOUT=$(cat "$STDOUT_FILE")
	LAST_STDERR=$(cat "$STDERR_FILE")
}

read_file() {
	cat "$1"
}

[ -x "$RMAIL_BIN" ] || fail "missing binary: $RMAIL_BIN"
[ -x "$MOCK_SENDMAIL_BIN" ] || fail "missing mock sendmail: $MOCK_SENDMAIL_BIN"

export RMAIL_SENDMAIL="$MOCK_SENDMAIL_BIN"
export RMAIL_SENDMAIL_LOG="$SENDMAIL_LOG"
export RMAIL_SENDMAIL_INPUT="$SENDMAIL_INPUT"
unset RMAIL_SENDMAIL_EXIT

run_capture "$RMAIL_BIN"
assert_status "usage status" "$EX_USAGE" "$LAST_STATUS"
assert_empty "usage stdout" "$LAST_STDOUT"
assert_eq "usage stderr" "usage: rmail [-D domain] [-T] user ..." "$LAST_STDERR"

run_capture "$RMAIL_BIN" -D ''
assert_status "empty domain status" "$EX_USAGE" "$LAST_STATUS"
assert_empty "empty domain stdout" "$LAST_STDOUT"
assert_eq "empty domain stderr" "rmail: -D requires a non-empty domain" "$LAST_STDERR"

run_capture "$RMAIL_BIN" -- -bad@example.test
assert_status "dash recipient status" "$EX_USAGE" "$LAST_STATUS"
assert_empty "dash recipient stdout" "$LAST_STDOUT"
assert_eq "dash recipient stderr" "rmail: dash precedes argument: -bad@example.test" "$LAST_STDERR"

run_with_input '' "$RMAIL_BIN" user@example.test
assert_status "no data status" "$EX_DATAERR" "$LAST_STATUS"
assert_empty "no data stdout" "$LAST_STDOUT"
assert_eq "no data stderr" "rmail: no data" "$LAST_STDERR"

run_with_input 'Subject: hi\n\nbody\n' "$RMAIL_BIN" user@example.test
assert_status "missing from status" "$EX_DATAERR" "$LAST_STATUS"
assert_empty "missing from stdout" "$LAST_STDOUT"
assert_eq "missing from stderr" "rmail: missing or empty From line: Subject: hi" "$LAST_STDERR"

run_with_input 'From !bad Tue Jan  1 00:00:00 2025\n\nbody\n' "$RMAIL_BIN" user@example.test
assert_status "bang starts status" "$EX_DATAERR" "$LAST_STATUS"
assert_empty "bang starts stdout" "$LAST_STDOUT"
assert_eq "bang starts stderr" "rmail: bang starts address: !bad" "$LAST_STDERR"

run_with_input 'From user Tue Jan  1 00:00:00 2025' "$RMAIL_BIN" user@example.test
assert_status "unterminated header status" "$EX_DATAERR" "$LAST_STATUS"
assert_empty "unterminated header stdout" "$LAST_STDOUT"
assert_eq "unterminated header stderr" "rmail: unterminated input line" "$LAST_STDERR"

PIPE_INPUT='From host1!host2!alice Tue Jan  1 00:00:00 2025
>From bob Tue Jan  1 00:01:00 2025 remote from relay.example

hello from pipe
'
run_with_input "$PIPE_INPUT" "$RMAIL_BIN" first@example.test second,third@example.test '<already@wrapped>'
assert_status "pipe happy status" 0 "$LAST_STATUS"
assert_empty "pipe happy stdout" "$LAST_STDOUT"
assert_empty "pipe happy stderr" "$LAST_STDERR"
assert_eq "pipe sendmail argv" "$MOCK_SENDMAIL_BIN
-G
-oee
-odi
-oi
-pUUCP:host1!host2.UUCP
-fhost1!host2!relay.example!bob
first@example.test
<second,third@example.test>
<already@wrapped>" "$(read_file "$SENDMAIL_LOG")"
assert_eq "pipe sendmail input" "
hello from pipe" "$(read_file "$SENDMAIL_INPUT")"

REGULAR_INPUT="$WORKDIR/regular-input"
cat >"$REGULAR_INPUT" <<'EOF'
From charlie Tue Jan  1 00:00:00 2025 remote from mailhub

hello from regular file
EOF
run_with_regular_file "$REGULAR_INPUT" "$RMAIL_BIN" -D LINUX target@example.test
assert_status "regular happy status" 0 "$LAST_STATUS"
assert_empty "regular happy stdout" "$LAST_STDOUT"
assert_empty "regular happy stderr" "$LAST_STDERR"
assert_eq "regular sendmail argv" "$MOCK_SENDMAIL_BIN
-G
-oee
-odi
-oi
-pLINUX:mailhub.LINUX
-fmailhub!charlie
target@example.test" "$(read_file "$SENDMAIL_LOG")"
assert_eq "regular sendmail input" "
hello from regular file" "$(read_file "$SENDMAIL_INPUT")"

DEBUG_INPUT='From user Tue Jan  1 00:00:00 2025 remote from relay

debug body
'
run_with_input "$DEBUG_INPUT" "$RMAIL_BIN" -T user@example.test
assert_status "debug status" 0 "$LAST_STATUS"
assert_empty "debug stdout" "$LAST_STDOUT"
assert_contains "debug stderr remote" "$LAST_STDERR" "remote from: relay"
assert_contains "debug stderr from_sys" "$LAST_STDERR" "from_sys: relay"
assert_contains "debug stderr from_path" "$LAST_STDERR" "from_path: relay!"
assert_contains "debug stderr from_user" "$LAST_STDERR" "from_user: user"
assert_contains "debug stderr args" "$LAST_STDERR" "Sendmail arguments:"

export RMAIL_SENDMAIL_EXIT=23
run_with_input 'From user Tue Jan  1 00:00:00 2025\n\nbody\n' "$RMAIL_BIN" user@example.test
assert_status "sendmail exit status propagation" 23 "$LAST_STATUS"
assert_empty "sendmail exit stdout" "$LAST_STDOUT"
assert_contains "sendmail exit stderr" "$LAST_STDERR" "terminated with 23 (non-zero) status"
unset RMAIL_SENDMAIL_EXIT

export RMAIL_SENDMAIL="$WORKDIR/missing-sendmail"
run_with_input 'From user Tue Jan  1 00:00:00 2025\n\nbody\n' "$RMAIL_BIN" user@example.test
assert_status "missing sendmail status" "$EX_UNAVAILABLE" "$LAST_STATUS"
assert_empty "missing sendmail stdout" "$LAST_STDOUT"
assert_contains "missing sendmail stderr" "$LAST_STDERR" "$WORKDIR/missing-sendmail"

printf '%s\n' "PASS"
