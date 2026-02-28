#!/bin/sh
set -eu

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
KILL_BIN=${KILL_BIN:-"$ROOT/out/kill"}
CC=${CC:-cc}
TMPDIR=${TMPDIR:-/tmp}
WORKDIR=$(mktemp -d "$TMPDIR/kill-test.XXXXXX")
HELPER_C="$WORKDIR/signal-helper.c"
HELPER_BIN="$WORKDIR/signal-helper"
STDOUT_FILE="$WORKDIR/stdout"
STDERR_FILE="$WORKDIR/stderr"
ACTIVE_PID=
ACTIVE_LOG=
ACTIVE_READY=
trap '
	if [ -n "${ACTIVE_PID}" ]; then
		kill -KILL "$ACTIVE_PID" 2>/dev/null || true
		wait "$ACTIVE_PID" 2>/dev/null || true
	fi
	rm -rf "$WORKDIR"
' EXIT INT TERM

export LC_ALL=C

USAGE_OUTPUT=$(cat <<'EOF'
usage: kill [-s signal_name] pid ...
       kill -l [exit_status]
       kill -signal_name pid ...
       kill -signal_number pid ...
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

run_capture() {
	if "$@" >"$STDOUT_FILE" 2>"$STDERR_FILE"; then
		LAST_STATUS=0
	else
		LAST_STATUS=$?
	fi

	LAST_STDOUT=$(cat "$STDOUT_FILE")
	LAST_STDERR=$(cat "$STDERR_FILE")
}

build_helper() {
cat >"$HELPER_C" <<'EOF'
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t received_signal;

static void
handle_signal(int signo)
{
	received_signal = signo;
}

static const char *
signal_name(int signo)
{
	switch (signo) {
	case SIGTERM:
		return "TERM";
	case SIGUSR1:
		return "USR1";
	default:
		return "UNKNOWN";
	}
}

int
main(int argc, char *argv[])
{
	struct sigaction sa;
	FILE *fp;

	if (argc != 2)
		return (2);

	if (setpgid(0, 0) != 0)
		return (3);

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = handle_signal;
	if (sigemptyset(&sa.sa_mask) != 0)
		return (4);
	if (sigaction(SIGTERM, &sa, NULL) != 0)
		return (5);
	if (sigaction(SIGUSR1, &sa, NULL) != 0)
		return (6);

	if (printf("%ld %ld\n", (long)getpid(), (long)getpgrp()) < 0)
		return (7);
	if (fflush(stdout) != 0)
		return (8);

	while (received_signal == 0)
		pause();

	fp = fopen(argv[1], "w");
	if (fp == NULL)
		return (9);
	if (fprintf(fp, "%s\n", signal_name(received_signal)) < 0) {
		fclose(fp);
		return (10);
	}
	if (fclose(fp) != 0)
		return (11);

	return (0);
}
EOF

	"$CC" -D_POSIX_C_SOURCE=200809L -O2 -std=c17 -Wall -Wextra -Werror \
		"$HELPER_C" -o "$HELPER_BIN" \
		|| fail "failed to build helper with $CC"
}

start_helper() {
	name=$1
	ACTIVE_LOG="$WORKDIR/$name.log"
	ACTIVE_READY="$WORKDIR/$name.ready"
	rm -f "$ACTIVE_LOG" "$ACTIVE_READY"
	mkfifo "$ACTIVE_READY"

	"$HELPER_BIN" "$ACTIVE_LOG" >"$ACTIVE_READY" &
	ACTIVE_PID=$!

	if ! IFS=' ' read -r HELPER_PID HELPER_PGID <"$ACTIVE_READY"; then
		fail "failed to read helper readiness"
	fi
	rm -f "$ACTIVE_READY"
	ACTIVE_READY=

	[ -n "$HELPER_PID" ] || fail "helper pid missing"
	[ -n "$HELPER_PGID" ] || fail "helper pgid missing"
}

wait_helper() {
	wait "$ACTIVE_PID" || fail "helper exited with failure"
	ACTIVE_PID=
}

wait_helper_signaled() {
	if wait "$ACTIVE_PID"; then
		fail "helper was expected to terminate by signal"
	fi
	ACTIVE_PID=
}

[ -x "$KILL_BIN" ] || fail "missing binary: $KILL_BIN"
build_helper

run_capture "$KILL_BIN"
assert_status "usage without args status" 2 "$LAST_STATUS"
assert_empty "usage without args stdout" "$LAST_STDOUT"
assert_eq "usage without args stderr" "$USAGE_OUTPUT" "$LAST_STDERR"

run_capture "$KILL_BIN" -l
assert_status "list signals status" 0 "$LAST_STATUS"
assert_empty "list signals stderr" "$LAST_STDERR"
assert_contains "list signals contains HUP" "$LAST_STDOUT" "HUP"
assert_contains "list signals contains KILL" "$LAST_STDOUT" "KILL"
assert_contains "list signals contains TERM" "$LAST_STDOUT" "TERM"

run_capture "$KILL_BIN" -l 15
assert_status "list signal 15 status" 0 "$LAST_STATUS"
assert_eq "list signal 15 stdout" "TERM" "$LAST_STDOUT"
assert_empty "list signal 15 stderr" "$LAST_STDERR"

run_capture "$KILL_BIN" -l 143
assert_status "list signal 143 status" 0 "$LAST_STATUS"
assert_eq "list signal 143 stdout" "TERM" "$LAST_STDOUT"
assert_empty "list signal 143 stderr" "$LAST_STDERR"

run_capture "$KILL_BIN" -l 0
assert_status "list signal 0 status" 0 "$LAST_STATUS"
assert_eq "list signal 0 stdout" "0" "$LAST_STDOUT"
assert_empty "list signal 0 stderr" "$LAST_STDERR"

run_capture "$KILL_BIN" -l TERM
assert_status "list TERM status" 0 "$LAST_STATUS"
assert_eq "list TERM stdout" "15" "$LAST_STDOUT"
assert_empty "list TERM stderr" "$LAST_STDERR"

run_capture "$KILL_BIN" -s
assert_status "missing -s argument status" 2 "$LAST_STATUS"
assert_empty "missing -s argument stdout" "$LAST_STDOUT"
assert_contains "missing -s argument stderr" "$LAST_STDERR" \
	"kill: option requires an argument -- s"

run_capture "$KILL_BIN" -NOPE 1
assert_status "unknown option signal status" 2 "$LAST_STATUS"
assert_empty "unknown option signal stdout" "$LAST_STDOUT"
assert_contains "unknown option signal stderr" "$LAST_STDERR" \
	"kill: unknown signal NOPE"

run_capture "$KILL_BIN" abc
assert_status "invalid pid status" 2 "$LAST_STATUS"
assert_empty "invalid pid stdout" "$LAST_STDOUT"
assert_eq "invalid pid stderr" "kill: illegal process id: abc" "$LAST_STDERR"

run_capture "$KILL_BIN" %1
assert_status "job spec status" 2 "$LAST_STATUS"
assert_empty "job spec stdout" "$LAST_STDOUT"
assert_eq "job spec stderr" \
	"kill: job control process specifications require a shell builtin: %1" \
	"$LAST_STDERR"

start_helper probe

run_capture "$KILL_BIN" -0 "$HELPER_PID"
assert_status "signal zero short form status" 0 "$LAST_STATUS"
assert_empty "signal zero short form stdout" "$LAST_STDOUT"
assert_empty "signal zero short form stderr" "$LAST_STDERR"

run_capture "$KILL_BIN" -s 0 "$HELPER_PID"
assert_status "signal zero -s status" 0 "$LAST_STATUS"
assert_empty "signal zero -s stdout" "$LAST_STDOUT"
assert_empty "signal zero -s stderr" "$LAST_STDERR"

run_capture "$KILL_BIN" -9 "$HELPER_PID"
assert_status "signal 9 short form status" 0 "$LAST_STATUS"
assert_empty "signal 9 short form stdout" "$LAST_STDOUT"
assert_empty "signal 9 short form stderr" "$LAST_STDERR"
wait_helper_signaled
[ ! -e "$ACTIVE_LOG" ] || fail "signal 9 helper log should not exist"

start_helper numeric_s
run_capture "$KILL_BIN" -s 9 "$HELPER_PID"
assert_status "signal 9 with -s status" 0 "$LAST_STATUS"
assert_empty "signal 9 with -s stdout" "$LAST_STDOUT"
assert_empty "signal 9 with -s stderr" "$LAST_STDERR"
wait_helper_signaled
[ ! -e "$ACTIVE_LOG" ] || fail "signal 9 with -s helper log should not exist"

start_helper probe
run_capture "$KILL_BIN" -s SIGUSR1 "$HELPER_PID"
assert_status "send SIGUSR1 with -s status" 0 "$LAST_STATUS"
assert_empty "send SIGUSR1 with -s stdout" "$LAST_STDOUT"
assert_empty "send SIGUSR1 with -s stderr" "$LAST_STDERR"
wait_helper
assert_eq "SIGUSR1 helper log" "USR1" "$(cat "$ACTIVE_LOG")"

start_helper default_term
run_capture "$KILL_BIN" "$HELPER_PID"
assert_status "default TERM status" 0 "$LAST_STATUS"
assert_empty "default TERM stdout" "$LAST_STDOUT"
assert_empty "default TERM stderr" "$LAST_STDERR"
wait_helper
assert_eq "default TERM helper log" "TERM" "$(cat "$ACTIVE_LOG")"

start_helper group_term
run_capture "$KILL_BIN" -TERM -- "-$HELPER_PGID"
assert_status "group TERM status" 0 "$LAST_STATUS"
assert_empty "group TERM stdout" "$LAST_STDOUT"
assert_empty "group TERM stderr" "$LAST_STDERR"
wait_helper
assert_eq "group TERM helper log" "TERM" "$(cat "$ACTIVE_LOG")"

printf '%s\n' "PASS"
