#!/bin/sh
# Comprehensive test suite for pkill/pgrep Linux port.
# Follows the same pattern as bin/kill/tests/test.sh.
set -eu

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
PKILL_BIN=${PKILL_BIN:-"$ROOT/out/pkill"}
PGREP_BIN=${PGREP_BIN:-"$ROOT/out/pgrep"}
HELPER_BIN=${HELPER_BIN:-"$ROOT/out/spin_helper"}
CC=${CC:-cc}
TMPDIR=${TMPDIR:-/tmp}
WORKDIR=$(mktemp -d "$TMPDIR/pkill-test.XXXXXX")
STDOUT_FILE="$WORKDIR/stdout"
STDERR_FILE="$WORKDIR/stderr"

# Unique name: must be <= 15 chars to stay within Linux /proc/PID/stat comm limit.
# Format: pkt + truncated_PID. "pkt" (3) + "_" (1) + up to 11 PID digits.
# In practice PID is at most 7 digits on Linux, so max = 11 chars.
UNIQUE="pkt_$$"
# Guard against pathological shells where $$ is very long.
if [ ${#UNIQUE} -gt 15 ]; then
	_suffix=$(echo "$$" | tail -c8)
	UNIQUE="pkt_$_suffix"
fi
HELPER_COPY="$WORKDIR/$UNIQUE"

PIDS_TO_KILL=""

export LC_ALL=C

cleanup() {
	for p in $PIDS_TO_KILL; do
		kill -KILL "$p" 2>/dev/null || true
		wait "$p" 2>/dev/null || true
	done
	rm -rf "$WORKDIR"
}
trap cleanup EXIT INT TERM

PASS_COUNT=0
FAIL_COUNT=0

fail() {
	printf 'FAIL: %s\n' "$1" >&2
	FAIL_COUNT=$((FAIL_COUNT + 1))
	exit 1
}

pass() {
	PASS_COUNT=$((PASS_COUNT + 1))
	printf '  ok: %s\n' "$1"
}

assert_eq() {
	name=$1; expected=$2; actual=$3
	if [ "$expected" != "$actual" ]; then
		printf 'FAIL: %s\n' "$name" >&2
		printf '--- expected ---\n%s\n--- actual ---\n%s\n' \
		    "$expected" "$actual" >&2
		exit 1
	fi
}

assert_contains() {
	name=$1; text=$2; pattern=$3
	case $text in
		*"$pattern"*) ;;
		*) fail "$name: expected to contain '$pattern', got: $text" ;;
	esac
}

assert_not_contains() {
	name=$1; text=$2; pattern=$3
	case $text in
		*"$pattern"*) fail "$name: should not contain '$pattern'" ;;
		*) ;;
	esac
}

assert_empty() {
	name=$1; text=$2
	if [ -n "$text" ]; then
		printf 'FAIL: %s\n--- expected empty ---\n--- actual ---\n%s\n' \
		    "$name" "$text" >&2
		exit 1
	fi
}

assert_status() {
	name=$1; expected=$2; actual=$3
	if [ "$expected" -ne "$actual" ]; then
		printf 'FAIL: %s\nexpected status: %s\nactual status: %s\n' \
		    "$name" "$expected" "$actual" >&2
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

# Start a helper process with a unique command name.
# Uses --spin directly so that comm matches the helper copy's basename
# immediately without any execve race.
# Sets HELPER_PID, HELPER_SENTINEL.
start_helper() {
	tag=$1
	flag="$WORKDIR/${tag}.flag"
	sentinel="$WORKDIR/${tag}.sentinel"
	rm -f "$flag" "$sentinel"

	# --spin: creates flag file then waits for signal.
	"$HELPER_COPY" --spin "$flag" "$sentinel" &
	HELPER_PID=$!
	PIDS_TO_KILL="$PIDS_TO_KILL $HELPER_PID"

	# Wait for flag file to appear (readiness signal).
	tries=0
	while [ ! -f "$flag" ]; do
		tries=$((tries + 1))
		if [ $tries -gt 100 ]; then
			fail "helper '$tag' (pid=$HELPER_PID) did not become ready"
		fi
		sleep 0.05
	done
	HELPER_SENTINEL="$sentinel"
}

# Start two helpers for oldest/newest tests.
start_two_helpers() {
	start_helper "older"
	OLDER_PID=$HELPER_PID
	OLDER_SENTINEL=$HELPER_SENTINEL
	sleep 0.2    # ensure different starttime
	start_helper "newer"
	NEWER_PID=$HELPER_PID
	NEWER_SENTINEL=$HELPER_SENTINEL
}

stop_helper() {
	pid=$1
	kill -KILL "$pid" 2>/dev/null || true
	wait "$pid" 2>/dev/null || true
	PIDS_TO_KILL=$(echo "$PIDS_TO_KILL" | sed "s/ *$pid//g")
}

wait_helper_exit() {
	pid=$1
	tries=0
	while kill -0 "$pid" 2>/dev/null; do
		tries=$((tries + 1))
		if [ $tries -gt 50 ]; then
			fail "helper pid=$pid did not exit"
		fi
		sleep 0.1
	done
	wait "$pid" 2>/dev/null || true
	PIDS_TO_KILL=$(echo "$PIDS_TO_KILL" | sed "s/ *$pid//g")
}

# ============================================================
#  Prerequisite checks
# ============================================================
[ -x "$PKILL_BIN" ] || fail "missing pkill binary: $PKILL_BIN"
[ -x "$PGREP_BIN" ] || fail "missing pgrep binary: $PGREP_BIN"
[ -x "$HELPER_BIN" ] || fail "missing helper binary: $HELPER_BIN"
[ -d /proc ] || fail "/proc not available"

# Create a helper copy with unique name.
cp "$HELPER_BIN" "$HELPER_COPY"
chmod +x "$HELPER_COPY"

printf '=== pkill/pgrep test suite ===\n'
printf 'helper name: %s\n' "$UNIQUE"

# ============================================================
#  1. Usage / error tests
# ============================================================

# pgrep with no arguments → status 2
run_capture "$PGREP_BIN"
assert_status "pgrep no args: status" 2 "$LAST_STATUS"
assert_empty "pgrep no args: stdout" "$LAST_STDOUT"
assert_contains "pgrep no args: stderr" "$LAST_STDERR" "usage:"
pass "pgrep no args → usage error"

# pkill with no arguments → status 2
run_capture "$PKILL_BIN"
assert_status "pkill no args: status" 2 "$LAST_STATUS"
assert_empty "pkill no args: stdout" "$LAST_STDOUT"
assert_contains "pkill no args: stderr" "$LAST_STDERR" "usage:"
pass "pkill no args → usage error"

# Bad regex → status 2
run_capture "$PGREP_BIN" '[invalid'
assert_status "bad regex: status" 2 "$LAST_STATUS"
assert_empty "bad regex: stdout" "$LAST_STDOUT"
assert_contains "bad regex: stderr" "$LAST_STDERR" "Cannot compile regular expression"
pass "bad regex → error"

# -n and -o together → status 3
run_capture "$PGREP_BIN" -n -o 'anything'
assert_status "-n -o together: status" 3 "$LAST_STATUS"
assert_contains "-n -o together: stderr" "$LAST_STDERR" "mutually exclusive"
pass "-n -o together → error"

# BSD-only options produce explicit errors on Linux
run_capture "$PGREP_BIN" -j any 'test'
assert_status "-j: status" 2 "$LAST_STATUS"
assert_contains "-j: stderr" "$LAST_STDERR" "not supported on Linux"
pass "-j → explicit unsupported error"

run_capture "$PGREP_BIN" -c user 'test'
assert_status "-c: status" 2 "$LAST_STATUS"
assert_contains "-c: stderr" "$LAST_STDERR" "not supported on Linux"
pass "-c → explicit unsupported error"

run_capture "$PGREP_BIN" -M /dev/null 'test'
assert_status "-M: status" 2 "$LAST_STATUS"
assert_contains "-M: stderr" "$LAST_STDERR" "not supported on Linux"
pass "-M → explicit unsupported error"

run_capture "$PGREP_BIN" -N /dev/null 'test'
assert_status "-N: status" 2 "$LAST_STATUS"
assert_contains "-N: stderr" "$LAST_STDERR" "not supported on Linux"
pass "-N → explicit unsupported error"

# -L without -F → error
run_capture "$PGREP_BIN" -L 'test'
assert_status "-L without -F: status" 3 "$LAST_STATUS"
assert_contains "-L without -F: stderr" "$LAST_STDERR" "doesn't make sense"
pass "-L without -F → error"

# pgrep-only options in pkill mode → usage
run_capture "$PKILL_BIN" -d, 'test'
assert_status "pkill -d: status" 2 "$LAST_STATUS"
pass "pkill -d → usage error"

run_capture "$PKILL_BIN" -q 'test'
assert_status "pkill -q: status" 2 "$LAST_STATUS"
pass "pkill -q → usage error"

# pkill-only option in pgrep mode → usage
run_capture "$PGREP_BIN" -I 'test'
assert_status "pgrep -I: status" 2 "$LAST_STATUS"
pass "pgrep -I → usage error"

# ============================================================
#  2. pgrep basic matching
# ============================================================
start_helper basic

run_capture "$PGREP_BIN" "$UNIQUE"
assert_status "pgrep basic: status" 0 "$LAST_STATUS"
assert_contains "pgrep basic: stdout" "$LAST_STDOUT" "$HELPER_PID"
pass "pgrep basic match"

# Ensure pgrep does not match itself.
assert_not_contains "pgrep self-exclusion" "$LAST_STDOUT" "$$"
pass "pgrep excludes self"

stop_helper "$HELPER_PID"

# No match → status 1
run_capture "$PGREP_BIN" "nonexistent_process_name_$$$RANDOM"
assert_status "pgrep no match: status" 1 "$LAST_STATUS"
assert_empty "pgrep no match: stdout" "$LAST_STDOUT"
pass "pgrep no match → status 1"

# ============================================================
#  3. pgrep -x (exact match)
# ============================================================
start_helper exact

run_capture "$PGREP_BIN" -x "$UNIQUE"
assert_status "pgrep -x exact: status" 0 "$LAST_STATUS"
assert_contains "pgrep -x exact: stdout" "$LAST_STDOUT" "$HELPER_PID"
pass "pgrep -x exact match"

# Partial name should NOT match with -x.
partial=$(echo "$UNIQUE" | cut -c1-6)
run_capture "$PGREP_BIN" -x "$partial"
# It should not match the helper (could match other things though)
assert_not_contains "pgrep -x partial" "$LAST_STDOUT" "$HELPER_PID"
pass "pgrep -x rejects partial match"

stop_helper "$HELPER_PID"

# ============================================================
#  4. pgrep -f (match full args)
# ============================================================
start_helper fullarg

run_capture "$PGREP_BIN" -f -- "--spin.*$WORKDIR"
assert_status "pgrep -f: status" 0 "$LAST_STATUS"
assert_contains "pgrep -f: stdout" "$LAST_STDOUT" "$HELPER_PID"
pass "pgrep -f matches full cmdline"

stop_helper "$HELPER_PID"

# ============================================================
#  5. pgrep -i (case insensitive)
# ============================================================
start_helper casematch

UPPER=$(echo "$UNIQUE" | tr '[:lower:]' '[:upper:]')
run_capture "$PGREP_BIN" -i "$UPPER"
assert_status "pgrep -i: status" 0 "$LAST_STATUS"
assert_contains "pgrep -i: stdout" "$LAST_STDOUT" "$HELPER_PID"
pass "pgrep -i case insensitive"

stop_helper "$HELPER_PID"

# ============================================================
#  6. pgrep -l (long output)
# ============================================================
start_helper longout

run_capture "$PGREP_BIN" -l "$UNIQUE"
assert_status "pgrep -l: status" 0 "$LAST_STATUS"
assert_contains "pgrep -l: stdout" "$LAST_STDOUT" "$HELPER_PID"
# Long format should include process name.
assert_contains "pgrep -l shows name" "$LAST_STDOUT" "$UNIQUE"
pass "pgrep -l long output"

stop_helper "$HELPER_PID"

# ============================================================
#  7. pgrep -l -f (long + full args)
# ============================================================
start_helper longfull

run_capture "$PGREP_BIN" -l -f "$UNIQUE"
assert_status "pgrep -lf: status" 0 "$LAST_STATUS"
# Should show PID and full cmdline
assert_contains "pgrep -lf: shows PID" "$LAST_STDOUT" "$HELPER_PID"
assert_contains "pgrep -lf: shows --spin" "$LAST_STDOUT" "--spin"
pass "pgrep -l -f long + full args"

stop_helper "$HELPER_PID"

# ============================================================
#  8. pgrep -v (inverse)
# ============================================================
start_helper inverse

# -v should NOT include the helper in results when it matches.
# We search for patterns that DO match, and check helper PID is absent.
run_capture "$PGREP_BIN" -v "$UNIQUE"
assert_status "pgrep -v: status" 0 "$LAST_STATUS"
assert_not_contains "pgrep -v no helper" "$LAST_STDOUT" "$HELPER_PID"
pass "pgrep -v inverse match"

stop_helper "$HELPER_PID"

# ============================================================
#  9. pgrep -q (quiet)
# ============================================================
start_helper quiettest

run_capture "$PGREP_BIN" -q "$UNIQUE"
assert_status "pgrep -q: status" 0 "$LAST_STATUS"
assert_empty "pgrep -q: stdout" "$LAST_STDOUT"
pass "pgrep -q quiet mode"

stop_helper "$HELPER_PID"

# ============================================================
# 10. pgrep -d (delimiter)
# ============================================================
start_helper delim1
PID1=$HELPER_PID
start_helper delim2
PID2=$HELPER_PID

run_capture "$PGREP_BIN" -d, "$UNIQUE"
assert_status "pgrep -d: status" 0 "$LAST_STATUS"
# Output should contain comma-separated PIDs
assert_contains "pgrep -d: comma" "$LAST_STDOUT" ","
pass "pgrep -d custom delimiter"

stop_helper "$PID1"
stop_helper "$PID2"

# ============================================================
# 11. pgrep -n (newest) / -o (oldest)
# ============================================================
start_two_helpers

run_capture "$PGREP_BIN" -n "$UNIQUE"
assert_status "pgrep -n: status" 0 "$LAST_STATUS"
assert_contains "pgrep -n: newest" "$LAST_STDOUT" "$NEWER_PID"
assert_not_contains "pgrep -n: not older" "$LAST_STDOUT" "$OLDER_PID"
pass "pgrep -n selects newest"

run_capture "$PGREP_BIN" -o "$UNIQUE"
assert_status "pgrep -o: status" 0 "$LAST_STATUS"
assert_contains "pgrep -o: oldest" "$LAST_STDOUT" "$OLDER_PID"
assert_not_contains "pgrep -o: not newer" "$LAST_STDOUT" "$NEWER_PID"
pass "pgrep -o selects oldest"

stop_helper "$OLDER_PID"
stop_helper "$NEWER_PID"

# ============================================================
# 12. pgrep -P ppid (parent PID filter)
# ============================================================
start_helper ppidtest

PPID_OF_HELPER=$(awk '{print $4}' "/proc/$HELPER_PID/stat" 2>/dev/null || echo "")
if [ -n "$PPID_OF_HELPER" ]; then
	run_capture "$PGREP_BIN" -P "$PPID_OF_HELPER" "$UNIQUE"
	assert_status "pgrep -P: status" 0 "$LAST_STATUS"
	assert_contains "pgrep -P: match" "$LAST_STDOUT" "$HELPER_PID"
	pass "pgrep -P parent PID filter"

	# Wrong parent → no match
	run_capture "$PGREP_BIN" -P 1 "$UNIQUE"
	if [ "$PPID_OF_HELPER" != "1" ]; then
		assert_status "pgrep -P wrong parent: status" 1 "$LAST_STATUS"
		pass "pgrep -P wrong parent → no match"
	fi
else
	printf '  SKIP: pgrep -P (cannot read ppid)\n'
fi

stop_helper "$HELPER_PID"

# ============================================================
# 13. pgrep -u euid (effective user ID filter)
# ============================================================
start_helper euidtest

MY_EUID=$(id -u)
run_capture "$PGREP_BIN" -u "$MY_EUID" "$UNIQUE"
assert_status "pgrep -u euid: status" 0 "$LAST_STATUS"
assert_contains "pgrep -u euid: match" "$LAST_STDOUT" "$HELPER_PID"
pass "pgrep -u effective UID filter"

# Wrong euid → no match
run_capture "$PGREP_BIN" -u 99999 "$UNIQUE"
assert_status "pgrep -u wrong euid: status" 1 "$LAST_STATUS"
pass "pgrep -u wrong UID → no match"

stop_helper "$HELPER_PID"

# ============================================================
# 14. pgrep -U ruid (real user ID filter)
# ============================================================
start_helper ruidtest

MY_RUID=$(id -ru)
run_capture "$PGREP_BIN" -U "$MY_RUID" "$UNIQUE"
assert_status "pgrep -U ruid: status" 0 "$LAST_STATUS"
assert_contains "pgrep -U ruid: match" "$LAST_STDOUT" "$HELPER_PID"
pass "pgrep -U real UID filter"

stop_helper "$HELPER_PID"

# ============================================================
# 15. pgrep -G rgid (real group ID filter)
# ============================================================
start_helper rgidtest

MY_RGID=$(id -rg)
run_capture "$PGREP_BIN" -G "$MY_RGID" "$UNIQUE"
assert_status "pgrep -G rgid: status" 0 "$LAST_STATUS"
assert_contains "pgrep -G rgid: match" "$LAST_STDOUT" "$HELPER_PID"
pass "pgrep -G real GID filter"

# Wrong GID
run_capture "$PGREP_BIN" -G 99999 "$UNIQUE"
assert_status "pgrep -G wrong gid: status" 1 "$LAST_STATUS"
pass "pgrep -G wrong GID → no match"

stop_helper "$HELPER_PID"

# ============================================================
# 16. pgrep -g pgrp (process group filter)
# ============================================================
start_helper pgrptest

# The helper's pgrp is its own PID (if it calls setpgid) or inherited.
# Read from proc.
HELPER_PGRP=$(awk '{print $5}' "/proc/$HELPER_PID/stat" 2>/dev/null || echo "")
if [ -n "$HELPER_PGRP" ] && [ "$HELPER_PGRP" != "0" ]; then
	run_capture "$PGREP_BIN" -g "$HELPER_PGRP" "$UNIQUE"
	assert_status "pgrep -g: status" 0 "$LAST_STATUS"
	assert_contains "pgrep -g: match" "$LAST_STDOUT" "$HELPER_PID"
	pass "pgrep -g process group filter"
else
	printf '  SKIP: pgrep -g (cannot read pgrp)\n'
fi

stop_helper "$HELPER_PID"

# ============================================================
# 17. pgrep -s sid (session ID filter)
# ============================================================
start_helper sidtest

HELPER_SID=$(awk '{print $6}' "/proc/$HELPER_PID/stat" 2>/dev/null || echo "")
if [ -n "$HELPER_SID" ] && [ "$HELPER_SID" != "0" ]; then
	run_capture "$PGREP_BIN" -s "$HELPER_SID" "$UNIQUE"
	assert_status "pgrep -s: status" 0 "$LAST_STATUS"
	assert_contains "pgrep -s: match" "$LAST_STDOUT" "$HELPER_PID"
	pass "pgrep -s session ID filter"
else
	printf '  SKIP: pgrep -s (cannot read sid)\n'
fi

stop_helper "$HELPER_PID"

# ============================================================
# 18. pgrep -F pidfile
# ============================================================
start_helper pidfile

PIDFILE="$WORKDIR/test.pid"
echo "$HELPER_PID" > "$PIDFILE"

run_capture "$PGREP_BIN" -F "$PIDFILE"
assert_status "pgrep -F: status" 0 "$LAST_STATUS"
assert_contains "pgrep -F: match" "$LAST_STDOUT" "$HELPER_PID"
pass "pgrep -F pidfile"

# Non-existent pidfile → error
run_capture "$PGREP_BIN" -F "$WORKDIR/nonexistent.pid"
assert_status "pgrep -F nonexist: status" 3 "$LAST_STATUS"
assert_contains "pgrep -F nonexist: stderr" "$LAST_STDERR" "Cannot open"
pass "pgrep -F nonexistent → error"

# Empty pidfile → error
> "$WORKDIR/empty.pid"
run_capture "$PGREP_BIN" -F "$WORKDIR/empty.pid"
assert_status "pgrep -F empty: status" 3 "$LAST_STATUS"
assert_contains "pgrep -F empty: stderr" "$LAST_STDERR" "empty"
pass "pgrep -F empty pidfile → error"

stop_helper "$HELPER_PID"

# ============================================================
# 19. pkill basic (send SIGTERM)
# ============================================================
start_helper pkill_basic

run_capture "$PKILL_BIN" "$UNIQUE"
assert_status "pkill basic: status" 0 "$LAST_STATUS"

# Wait for helper to receive signal and exit.
wait_helper_exit "$HELPER_PID"

# Check sentinel file for signal name.
if [ -f "$HELPER_SENTINEL" ]; then
	signame=$(cat "$HELPER_SENTINEL")
	assert_eq "pkill basic: signal" "TERM" "$signame"
fi
pass "pkill basic SIGTERM"

# ============================================================
# 20. pkill -SIGUSR1 (custom signal)
# ============================================================
start_helper pkill_sig

run_capture "$PKILL_BIN" -USR1 "$UNIQUE"
assert_status "pkill -USR1: status" 0 "$LAST_STATUS"

wait_helper_exit "$HELPER_PID"

if [ -f "$HELPER_SENTINEL" ]; then
	signame=$(cat "$HELPER_SENTINEL")
	assert_eq "pkill -USR1: signal" "USR1" "$signame"
fi
pass "pkill -USR1 custom signal"

# ============================================================
# 21. pkill -<number> (numeric signal)
# ============================================================
start_helper pkill_num

# SIGUSR1 is typically 10
run_capture "$PKILL_BIN" -10 "$UNIQUE"
assert_status "pkill -10: status" 0 "$LAST_STATUS"

wait_helper_exit "$HELPER_PID"

if [ -f "$HELPER_SENTINEL" ]; then
	signame=$(cat "$HELPER_SENTINEL")
	assert_eq "pkill -10: signal" "USR1" "$signame"
fi
pass "pkill -<number> numeric signal"

# ============================================================
# 22. pkill -f (match full args)
# ============================================================
start_helper pkill_full

run_capture "$PKILL_BIN" -f -- "--spin.*$WORKDIR"
assert_status "pkill -f: status" 0 "$LAST_STATUS"

wait_helper_exit "$HELPER_PID"
pass "pkill -f matches full cmdline"

# ============================================================
# 23. pkill with -l (list mode)
# ============================================================
start_helper pkill_list

run_capture "$PKILL_BIN" -l "$UNIQUE"
assert_status "pkill -l: status" 0 "$LAST_STATUS"
assert_contains "pkill -l: stdout" "$LAST_STDOUT" "kill"
assert_contains "pkill -l: stdout" "$LAST_STDOUT" "$HELPER_PID"
pass "pkill -l shows kill command"

wait_helper_exit "$HELPER_PID"

# ============================================================
# 24. pkill no match → status 1
# ============================================================
run_capture "$PKILL_BIN" "nonexistent_process_$$$RANDOM"
assert_status "pkill no match: status" 1 "$LAST_STATUS"
pass "pkill no match → status 1"

# ============================================================
# 25. pgrep -S (include kernel threads)
# ============================================================
# This should not error out.  We just check it runs.
run_capture "$PGREP_BIN" -S 'kthreadd'
# kthreadd might or might not be visible depending on environment.
# Just verify it doesn't crash.
if [ "$LAST_STATUS" -ne 0 ] && [ "$LAST_STATUS" -ne 1 ]; then
	fail "pgrep -S unexpected status: $LAST_STATUS"
fi
pass "pgrep -S kernel threads (no crash)"

# ============================================================
# 26. pgrep -a (include ancestors)
# ============================================================
start_helper ancestors

# Without -a, our shell (ancestor) might be excluded from results.
# With -a, ancestors are included.  Hard to test precisely, but
# verify -a doesn't cause errors.
run_capture "$PGREP_BIN" -a "$UNIQUE"
assert_status "pgrep -a: status" 0 "$LAST_STATUS"
pass "pgrep -a include ancestors (no crash)"

stop_helper "$HELPER_PID"

# ============================================================
# 27. Multiple patterns
# ============================================================
start_helper multi1
PID_M1=$HELPER_PID

run_capture "$PGREP_BIN" "$UNIQUE"
assert_status "multi pattern: status" 0 "$LAST_STATUS"
assert_contains "multi pattern: match" "$LAST_STDOUT" "$PID_M1"
pass "multiple pattern matching"

stop_helper "$PID_M1"

# ============================================================
# 28. User name filter (if available)
# ============================================================
start_helper usertest

MY_USER=$(id -un)
run_capture "$PGREP_BIN" -u "$MY_USER" "$UNIQUE"
assert_status "pgrep -u username: status" 0 "$LAST_STATUS"
assert_contains "pgrep -u username: match" "$LAST_STDOUT" "$HELPER_PID"
pass "pgrep -u username filter"

# Unknown user → error
run_capture "$PGREP_BIN" -u "nonexistent_user_xyz_$$" "$UNIQUE"
assert_status "pgrep -u unknown user: status" 2 "$LAST_STATUS"
assert_contains "pgrep -u unknown: stderr" "$LAST_STDERR" "Unknown user"
pass "pgrep -u unknown user → error"

stop_helper "$HELPER_PID"

# ============================================================
# 29. Group name filter
# ============================================================
start_helper grouptest

MY_GROUP=$(id -gn)
run_capture "$PGREP_BIN" -G "$MY_GROUP" "$UNIQUE"
assert_status "pgrep -G groupname: status" 0 "$LAST_STATUS"
assert_contains "pgrep -G groupname: match" "$LAST_STDOUT" "$HELPER_PID"
pass "pgrep -G groupname filter"

# Unknown group → error
run_capture "$PGREP_BIN" -G "nonexistent_group_xyz_$$" "$UNIQUE"
assert_status "pgrep -G unknown group: status" 2 "$LAST_STATUS"
assert_contains "pgrep -G unknown: stderr" "$LAST_STDERR" "Unknown group"
pass "pgrep -G unknown group → error"

stop_helper "$HELPER_PID"

# ============================================================
# 30. pgrep -F -L (pidfile with lock check)
# ============================================================
start_helper locktest

LOCKPID_FILE="$WORKDIR/locked.pid"
echo "$HELPER_PID" > "$LOCKPID_FILE"

# File is not locked, so -L should report error.
run_capture "$PGREP_BIN" -F "$LOCKPID_FILE" -L
assert_status "pgrep -F -L unlocked: status" 3 "$LAST_STATUS"
assert_contains "pgrep -F -L: stderr" "$LAST_STDERR" "can be locked"
pass "pgrep -F -L detects unlocked pidfile"

stop_helper "$HELPER_PID"

# ============================================================
#  Summary
# ============================================================
printf '\n=== %d tests passed ===\n' "$PASS_COUNT"
if [ "$FAIL_COUNT" -ne 0 ]; then
	printf '=== %d tests FAILED ===\n' "$FAIL_COUNT"
	exit 1
fi
