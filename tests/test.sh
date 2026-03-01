#!/bin/sh

# tests/test.sh for pwait Linux port

set -eu

# Fallbacks for paths
CC="${CC:-cc}"
PWAIT_BIN="${PWAIT_BIN:-$(pwd)/out/pwait}"

fail() {
	echo "FAIL: $*" >&2
	exit 1
}

assert_exit_code() {
	local expected=$1
	shift
	local cmd="$@"
	set +e
	eval "$cmd" > /tmp/pwait_test.out 2> /tmp/pwait_test.err
	local st=$?
	set -e
	if [ "$st" != "$expected" ]; then
		echo "Expected exit code $expected, got $st for: $cmd" >&2
		cat /tmp/pwait_test.out >&2
		cat /tmp/pwait_test.err >&2
		fail "exit code assertion failed"
	fi
}

assert_stdout() {
	local expected_str="$1"
	shift
	local cmd="$@"
	set +e
	eval "$cmd" > /tmp/pwait_test.out 2> /tmp/pwait_test.err
	local st=$?
	set -e
	local out=$(cat /tmp/pwait_test.out)
	if ! echo "$out" | grep -qF "$expected_str"; then
		echo "Expected stdout to contain '$expected_str' for: $cmd" >&2
		echo "Got stdout:" >&2
		cat /tmp/pwait_test.out >&2
		fail "stdout assertion failed"
	fi
}

assert_stderr() {
	local expected_str="$1"
	shift
	local cmd="$@"
	set +e
	eval "$cmd" > /tmp/pwait_test.out 2> /tmp/pwait_test.err
	local st=$?
	set -e
	local err=$(cat /tmp/pwait_test.err)
	if ! echo "$err" | grep -qF "$expected_str"; then
		echo "Expected stderr to contain '$expected_str' for: $cmd" >&2
		echo "Got stderr:" >&2
		cat /tmp/pwait_test.err >&2
		fail "stderr assertion failed"
	fi
}

echo "=== Basic tests ==="
sleep 1 &
p1=$!
sleep 3 &
p3=$!

assert_exit_code 0 timeout 10 "$PWAIT_BIN" $p1 $p3

echo "=== Timeout tests ==="
sleep 10 &
p10=$!

assert_exit_code 124 timeout 15 "$PWAIT_BIN" -t 1 $p10
assert_exit_code 0 timeout 15 "$PWAIT_BIN" -t 12 $p10

kill $p10 2>/dev/null || true
wait $p10 2>/dev/null || true

echo "=== OR (-o) tests ==="
sleep 1 &
p1=$!
sleep 10 &
p10=$!
assert_exit_code 0 timeout 4 "$PWAIT_BIN" -o $p1 $p10
# $p10 might still be running
kill $p10 2>/dev/null || true

echo "=== Missing PID tests ==="
# Give an invalid process ID, should err and consider it done
assert_stderr "999999" timeout 2 "$PWAIT_BIN" 999999
assert_exit_code 0 timeout 2 "$PWAIT_BIN" 999999

echo "=== -p (print remaining) tests ==="
sleep 1 &
p1=$!
sleep 10 &
p10=$!
# Wait for 3 sec, p1 finishes, p10 remains
assert_stdout "$p10" timeout 5 "$PWAIT_BIN" -t 3 -p $p1 $p10
kill $p10 2>/dev/null || true

echo "=== -v (verbose exit) tests ==="
sleep 1 &
p1=$!
# It's a child process because we started it from our shell script. However, pwait doesn't own it.
# So pwait will print `terminated.` instead of exact exit code.
assert_stdout "$p1: terminated" timeout 3 "$PWAIT_BIN" -v $p1

echo "=== ENOSYS (Mock failure) test ==="
# We can't easily mock ENOSYS for pidfd_open from shell.
# We'll skip it in automated tests for now unless we LD_PRELOAD.

echo "=== timeout=0 test ==="
sleep 2 &
p2=$!
assert_exit_code 124 timeout 2 "$PWAIT_BIN" -t 0 $p2
kill $p2 2>/dev/null || true

echo "=== Duplicate PID test ==="
sleep 1 &
p1=$!
# wait for duplicate PID, should just wait normally once
assert_exit_code 0 timeout 4 "$PWAIT_BIN" $p1 $p1

echo "=== Killed by signal test ==="
sleep 10 &
p10=$!
# We don't trace child signals for unrelated procs,
# but we can check if pwait returns successfully when the target dies from a signal.
kill -9 $p10 2>/dev/null || true
assert_exit_code 0 timeout 2 "$PWAIT_BIN" $p10

echo "=== Mixed valid/invalid test ==="
sleep 1 &
p1=$!
sleep 10 &
# 999999 invalid
assert_stderr "bad process id" timeout 2 "$PWAIT_BIN" abc xyz || true
assert_stderr "999999" timeout 2 "$PWAIT_BIN" 999999 $p1
# wait should still succeed as soon as valid process exits
assert_exit_code 0 timeout 4 "$PWAIT_BIN" 999999 $p1

echo "ALL TESTS PASSED."
exit 0
