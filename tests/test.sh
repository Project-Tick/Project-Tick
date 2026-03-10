#!/bin/sh
set -eu

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
TIMEOUT_BIN=${TIMEOUT_BIN:-"$ROOT/out/timeout"}
TMPDIR=${TMPDIR:-/tmp}
WORKDIR=$(mktemp -d "$TMPDIR/timeout-test.XXXXXX")
STDOUT_FILE="$WORKDIR/stdout"
STDERR_FILE="$WORKDIR/stderr"
LAST_STATUS=0
LAST_ELAPSED=0
LAST_STDOUT=
LAST_STDERR=
trap 'rm -rf "$WORKDIR"' EXIT INT TERM

export LC_ALL=C

USAGE_TEXT='Usage: timeout [-f | --foreground] [-k time | --kill-after time] [-p | --preserve-status] [-s signal | --signal signal] [-v | --verbose] <duration> <command> [arg ...]'

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

assert_empty() {
	name=$1
	text=$2
	if [ -n "$text" ]; then
		printf '%s\n' "FAIL: $name" >&2
		printf '%s\n' "--- expected empty ---" >&2
		printf '%s\n' "--- actual ---" >&2
		printf '%s\n' "$text" >&2
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

assert_ge() {
	name=$1
	actual=$2
	minimum=$3
	if [ "$actual" -lt "$minimum" ]; then
		printf '%s\n' "FAIL: $name" >&2
		printf '%s\n' "expected >= $minimum" >&2
		printf '%s\n' "actual: $actual" >&2
		exit 1
	fi
}

assert_le() {
	name=$1
	actual=$2
	maximum=$3
	if [ "$actual" -gt "$maximum" ]; then
		printf '%s\n' "FAIL: $name" >&2
		printf '%s\n' "expected <= $maximum" >&2
		printf '%s\n' "actual: $actual" >&2
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

run_timed_capture() {
	start=$(date +%s)
	if "$@" >"$STDOUT_FILE" 2>"$STDERR_FILE"; then
		LAST_STATUS=0
	else
		LAST_STATUS=$?
	fi
	end=$(date +%s)
	LAST_ELAPSED=$((end - start))
	LAST_STDOUT=$(cat "$STDOUT_FILE")
	LAST_STDERR=$(cat "$STDERR_FILE")
}

[ -x "$TIMEOUT_BIN" ] || fail "missing binary: $TIMEOUT_BIN"

run_capture "$TIMEOUT_BIN"
assert_status "usage status" 125 "$LAST_STATUS"
assert_empty "usage stdout" "$LAST_STDOUT"
assert_eq "usage stderr" "$USAGE_TEXT" "$LAST_STDERR"

run_capture "$TIMEOUT_BIN" -z
assert_status "invalid option status" 125 "$LAST_STATUS"
assert_empty "invalid option stdout" "$LAST_STDOUT"
assert_eq "invalid option stderr" "$USAGE_TEXT" "$LAST_STDERR"

run_capture "$TIMEOUT_BIN" -- 1 sh -c 'exit 0'
assert_status "double-dash separator status" 0 "$LAST_STATUS"
assert_empty "double-dash separator stdout" "$LAST_STDOUT"
assert_empty "double-dash separator stderr" "$LAST_STDERR"

run_capture "$TIMEOUT_BIN" invalid sleep 0
assert_status "invalid duration token status" 125 "$LAST_STATUS"
assert_empty "invalid duration token stdout" "$LAST_STDOUT"
assert_eq "invalid duration token stderr" \
	"timeout: duration is not a number" "$LAST_STDERR"

run_capture "$TIMEOUT_BIN" -k invalid 1 sleep 0
assert_status "invalid kill-after status" 125 "$LAST_STATUS"
assert_empty "invalid kill-after stdout" "$LAST_STDOUT"
assert_eq "invalid kill-after stderr" \
	"timeout: duration is not a number" "$LAST_STDERR"

run_capture "$TIMEOUT_BIN" 42D sleep 0
assert_status "invalid duration unit status" 125 "$LAST_STATUS"
assert_empty "invalid duration unit stdout" "$LAST_STDOUT"
assert_eq "invalid duration unit stderr" \
	"timeout: duration unit suffix invalid" "$LAST_STDERR"

run_capture "$TIMEOUT_BIN" 1ss sleep 0
assert_status "duration suffix too long status" 125 "$LAST_STATUS"
assert_empty "duration suffix too long stdout" "$LAST_STDOUT"
assert_eq "duration suffix too long stderr" \
	"timeout: duration unit suffix too long" "$LAST_STDERR"

run_capture "$TIMEOUT_BIN" -- -1 sleep 0
assert_status "negative duration status" 125 "$LAST_STATUS"
assert_empty "negative duration stdout" "$LAST_STDOUT"
assert_eq "negative duration stderr" \
	"timeout: duration out of range" "$LAST_STDERR"

run_capture "$TIMEOUT_BIN" inf sleep 0
assert_status "non-finite duration status" 125 "$LAST_STATUS"
assert_empty "non-finite duration stdout" "$LAST_STDOUT"
assert_eq "non-finite duration stderr" \
	"timeout: duration out of range" "$LAST_STDERR"

run_capture "$TIMEOUT_BIN" -s not-a-signal 1 sleep 0
assert_status "invalid signal status" 125 "$LAST_STATUS"
assert_empty "invalid signal stdout" "$LAST_STDOUT"
assert_eq "invalid signal stderr" \
	"timeout: invalid signal: not-a-signal" "$LAST_STDERR"

run_capture "$TIMEOUT_BIN" 1 sh -c 'exit 7'
assert_status "normal exit passthrough status" 7 "$LAST_STATUS"
assert_empty "normal exit passthrough stdout" "$LAST_STDOUT"
assert_empty "normal exit passthrough stderr" "$LAST_STDERR"

run_capture "$TIMEOUT_BIN" 0 sh -c 'exit 23'
assert_status "zero duration status passthrough status" 23 "$LAST_STATUS"
assert_empty "zero duration status passthrough stdout" "$LAST_STDOUT"
assert_empty "zero duration status passthrough stderr" "$LAST_STDERR"

run_capture "$TIMEOUT_BIN" 1 sleep 5
assert_status "timeout status" 124 "$LAST_STATUS"
assert_empty "timeout stdout" "$LAST_STDOUT"
assert_empty "timeout stderr" "$LAST_STDERR"

run_capture "$TIMEOUT_BIN" -v 1 sleep 5
assert_status "verbose timeout status" 124 "$LAST_STATUS"
assert_empty "verbose timeout stdout" "$LAST_STDOUT"
assert_contains "verbose timeout stderr marker" "$LAST_STDERR" "time limit reached"

run_capture "$TIMEOUT_BIN" -p 1 sleep 5
assert_status "preserve timeout status" 143 "$LAST_STATUS"
assert_empty "preserve timeout stdout" "$LAST_STDOUT"
assert_empty "preserve timeout stderr" "$LAST_STDERR"

run_capture "$TIMEOUT_BIN" -p -s KILL 1 sleep 5
assert_status "preserve timeout SIGKILL status" 137 "$LAST_STATUS"
assert_empty "preserve timeout SIGKILL stdout" "$LAST_STDOUT"
assert_empty "preserve timeout SIGKILL stderr" "$LAST_STDERR"

run_capture "$TIMEOUT_BIN" -p -k 1 1 sh -c 'trap "" TERM; sleep 10'
assert_status "kill-after escalation status" 137 "$LAST_STATUS"
assert_empty "kill-after escalation stdout" "$LAST_STDOUT"
assert_empty "kill-after escalation stderr" "$LAST_STDERR"

NOEXEC="$WORKDIR/noexec.sh"
printf '%s\n' '#!/bin/sh' 'exit 0' >"$NOEXEC"
chmod 0644 "$NOEXEC"
run_capture "$TIMEOUT_BIN" 1 "$NOEXEC"
assert_status "non-executable command status" 126 "$LAST_STATUS"
assert_empty "non-executable command stdout" "$LAST_STDOUT"
assert_contains "non-executable command stderr path" "$LAST_STDERR" "exec($NOEXEC)"

run_capture "$TIMEOUT_BIN" 1 does-not-exist-timeout-test-cmd
assert_status "missing command status" 127 "$LAST_STATUS"
assert_empty "missing command stdout" "$LAST_STDOUT"
assert_contains "missing command stderr cmd" "$LAST_STDERR" "exec(does-not-exist-timeout-test-cmd)"

"$TIMEOUT_BIN" 30 sleep 10 >"$STDOUT_FILE" 2>"$STDERR_FILE" &
bg_timeout_pid=$!
sleep 1
kill -ALRM "$bg_timeout_pid"
if wait "$bg_timeout_pid"; then
	bg_timeout_status=0
else
	bg_timeout_status=$?
fi
assert_status "SIGALRM external timeout status" 124 "$bg_timeout_status"
assert_empty "SIGALRM external timeout stdout" "$(cat "$STDOUT_FILE")"
assert_empty "SIGALRM external timeout stderr" "$(cat "$STDERR_FILE")"

"$TIMEOUT_BIN" 30 sh -c 'trap "exit 77" USR1; while :; do sleep 1; done' \
	>"$STDOUT_FILE" 2>"$STDERR_FILE" &
prop_pid=$!
sleep 1
kill -USR1 "$prop_pid"
if wait "$prop_pid"; then
	prop_status=0
else
	prop_status=$?
fi
assert_status "signal propagation status" 77 "$prop_status"
assert_empty "signal propagation stdout" "$(cat "$STDOUT_FILE")"
assert_empty "signal propagation stderr" "$(cat "$STDERR_FILE")"

PIDFILE="$WORKDIR/desc.pid"
run_timed_capture \
	"$TIMEOUT_BIN" -s INT 1 \
	sh -c '(trap "" INT HUP; sleep 5) & echo $! >"$1"; sleep 10' \
	sh "$PIDFILE"
assert_status "non-foreground descendant wait status" 124 "$LAST_STATUS"
assert_ge "non-foreground descendant wait elapsed seconds" "$LAST_ELAPSED" 4
assert_empty "non-foreground descendant wait stdout" "$LAST_STDOUT"
assert_empty "non-foreground descendant wait stderr" "$LAST_STDERR"
[ -s "$PIDFILE" ] || fail "missing descendant pid for non-foreground case"
desc_pid=$(cat "$PIDFILE")
if kill -0 "$desc_pid" 2>/dev/null; then
	fail "descendant still alive after non-foreground timeout"
fi

rm -f "$PIDFILE"
run_timed_capture \
	"$TIMEOUT_BIN" -f -s INT 1 \
	sh -c '(trap "" INT HUP; sleep 5) & echo $! >"$1"; sleep 10' \
	sh "$PIDFILE"
assert_status "foreground timeout status" 124 "$LAST_STATUS"
assert_le "foreground timeout elapsed seconds" "$LAST_ELAPSED" 3
assert_empty "foreground timeout stdout" "$LAST_STDOUT"
assert_empty "foreground timeout stderr" "$LAST_STDERR"
[ -s "$PIDFILE" ] || fail "missing descendant pid for foreground case"
desc_pid=$(cat "$PIDFILE")
if ! kill -0 "$desc_pid" 2>/dev/null; then
	fail "descendant not alive after foreground timeout"
fi
kill -KILL "$desc_pid" 2>/dev/null || true

printf '%s\n' "PASS"
