#!/bin/sh
set -eu

: "${CPUSET_BIN:?CPUSET_BIN is required}"

tmpdir=$(mktemp -d)
child_pid=""
trap 'if [ -n "$child_pid" ]; then kill "$child_pid" 2>/dev/null || true; wait "$child_pid" 2>/dev/null || true; fi; rm -rf "$tmpdir"' EXIT INT TERM

fail() {
	printf '%s\n' "FAIL: $1" >&2
	exit 1
}

assert_match() {
	pattern=$1
	text=$2
	message=$3
	printf '%s\n' "$text" | grep -Eq "$pattern" || fail "$message"
}

[ -x "$CPUSET_BIN" ] || fail "missing binary: $CPUSET_BIN"

usage_output=$("$CPUSET_BIN" 2>&1 || true)
assert_match '^usage: cpuset ' "$usage_output" "usage output missing"

initial_output=$("$CPUSET_BIN" -g)
assert_match '^tid [0-9]+ mask: ' "$initial_output" "current affinity output missing"

first_cpu=$(printf '%s\n' "$initial_output" | sed -n 's/^tid [0-9][0-9]* mask: \([0-9][0-9]*\).*/\1/p' | head -n1)
[ -n "$first_cpu" ] || fail "could not determine an allowed cpu"

"$CPUSET_BIN" -l "$first_cpu" -t $$
self_output=$("$CPUSET_BIN" -g -t $$)
assert_match "^tid $$ mask: $first_cpu$" "$self_output" "self affinity update missing"

"$CPUSET_BIN" -l "$first_cpu" sh -c '"$1" -g' sh "$CPUSET_BIN" >"$tmpdir/cmd.out"
cmd_output=$(cat "$tmpdir/cmd.out")
assert_match "^tid [0-9]+ mask: $first_cpu$" "$cmd_output" "command affinity output missing"

sleep 30 &
child_pid=$!
"$CPUSET_BIN" -l "$first_cpu" -p "$child_pid"
pid_output=$("$CPUSET_BIN" -g -p "$child_pid")
assert_match "^pid $child_pid mask: $first_cpu$" "$pid_output" "pid affinity update missing"
kill "$child_pid" 2>/dev/null || true
wait "$child_pid" 2>/dev/null || true
child_pid=""

invalid_cpu=$("$CPUSET_BIN" -l bogus 2>&1 || true)
assert_match '^cpuset: invalid cpu list: bogus$' "$invalid_cpu" "invalid cpu list not rejected"

bad_pid=$("$CPUSET_BIN" -p nope -l "$first_cpu" 2>&1 || true)
assert_match '^cpuset: invalid pid: nope$' "$bad_pid" "invalid pid not rejected"

bad_mix=$("$CPUSET_BIN" -p 1 -t 2 -l "$first_cpu" 2>&1 || true)
assert_match '^cpuset: choose only one target: -p or -t$' "$bad_mix" "conflicting target check missing"

unsupported=$("$CPUSET_BIN" -n local 2>&1 || true)
assert_match '^cpuset: option -n is not supported on Linux$' "$unsupported" "unsupported option check missing"

printf '%s\n' "PASS"
