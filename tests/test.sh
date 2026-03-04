#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SLEEP_BIN=${SLEEP_BIN:-"$ROOT/out/sleep"}
CC=${CC:-cc}
TMPDIR=${TMPDIR:-/tmp}
WORKDIR=$(mktemp -d "$TMPDIR/sleep-test.XXXXXX")
HELPER_C="$WORKDIR/run_sleep_case.c"
HELPER_BIN="$WORKDIR/run_sleep_case"
STDOUT_FILE="$WORKDIR/stdout"
STDERR_FILE="$WORKDIR/stderr"
RESULT_FILE="$WORKDIR/result"
trap 'rm -rf "$WORKDIR"' EXIT INT TERM

export LC_ALL=C

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

assert_match() {
	name=$1
	pattern=$2
	text=$3
	printf '%s\n' "$text" | grep -Eq "$pattern" || fail "$name"
}

build_helper() {
	cat >"$HELPER_C" <<'EOF'
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define NSECS_PER_SEC 1000000000LL

static int64_t
ns_since_epoch(const struct timespec *ts)
{
	return ((int64_t)ts->tv_sec * NSECS_PER_SEC + (int64_t)ts->tv_nsec);
}

static void
sleep_ms(int milliseconds)
{
	struct timespec request;

	request.tv_sec = milliseconds / 1000;
	request.tv_nsec = (long)(milliseconds % 1000) * 1000000L;
	while (nanosleep(&request, &request) != 0) {
		if (errno != EINTR) {
			perror("nanosleep");
			exit(2);
		}
	}
}

int
main(int argc, char *argv[])
{
	struct timespec started;
	struct timespec now;
	pid_t child;
	int stdout_fd;
	int stderr_fd;
	int timeout_ms;
	int signal_delay_ms;
	int status;
	int exit_status;
	int64_t elapsed_ns;
	bool signal_sent;
	char *end;

	if (argc < 6) {
		fprintf(stderr, "usage: run_sleep_case timeout_ms signal_delay_ms stdout stderr bin [args ...]\n");
		return (2);
	}

	errno = 0;
	timeout_ms = (int)strtol(argv[1], &end, 10);
	if (errno != 0 || *end != '\0' || timeout_ms <= 0) {
		fprintf(stderr, "invalid timeout: %s\n", argv[1]);
		return (2);
	}

	errno = 0;
	signal_delay_ms = (int)strtol(argv[2], &end, 10);
	if (errno != 0 || *end != '\0' || signal_delay_ms < -1) {
		fprintf(stderr, "invalid signal delay: %s\n", argv[2]);
		return (2);
	}

	stdout_fd = open(argv[3], O_WRONLY | O_CREAT | O_TRUNC, 0600);
	if (stdout_fd < 0) {
		perror("open stdout");
		return (2);
	}

	stderr_fd = open(argv[4], O_WRONLY | O_CREAT | O_TRUNC, 0600);
	if (stderr_fd < 0) {
		perror("open stderr");
		return (2);
	}

	if (clock_gettime(CLOCK_MONOTONIC, &started) != 0) {
		perror("clock_gettime");
		return (2);
	}

	child = fork();
	if (child < 0) {
		perror("fork");
		return (2);
	}
	if (child == 0) {
		if (dup2(stdout_fd, STDOUT_FILENO) < 0 || dup2(stderr_fd, STDERR_FILENO) < 0) {
			perror("dup2");
			_exit(127);
		}
		close(stdout_fd);
		close(stderr_fd);
		execv(argv[5], &argv[5]);
		perror("execv");
		_exit(127);
	}

	close(stdout_fd);
	close(stderr_fd);

	signal_sent = false;
	for (;;) {
		pid_t waited;
		int64_t elapsed_ms;

		waited = waitpid(child, &status, WNOHANG);
		if (waited < 0) {
			perror("waitpid");
			return (2);
		}
		if (waited == child)
			break;

		if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
			perror("clock_gettime");
			return (2);
		}

		elapsed_ms = (ns_since_epoch(&now) - ns_since_epoch(&started)) / 1000000LL;
		if (!signal_sent && signal_delay_ms >= 0 && elapsed_ms >= signal_delay_ms) {
			if (kill(child, SIGUSR1) != 0) {
				perror("kill");
				return (2);
			}
			signal_sent = true;
		}
		if (elapsed_ms >= timeout_ms) {
			if (kill(child, SIGKILL) != 0 && errno != ESRCH) {
				perror("kill timeout");
				return (2);
			}
			if (waitpid(child, &status, 0) < 0) {
				perror("waitpid timeout");
				return (2);
			}
			break;
		}

		sleep_ms(5);
	}

	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
		perror("clock_gettime");
		return (2);
	}
	elapsed_ns = ns_since_epoch(&now) - ns_since_epoch(&started);

	if (WIFEXITED(status))
		exit_status = WEXITSTATUS(status);
	else if (WIFSIGNALED(status))
		exit_status = 128 + WTERMSIG(status);
	else
		exit_status = 255;

	printf("status=%d\n", exit_status);
	printf("elapsed_ns=%lld\n", (long long)elapsed_ns);
	return (0);
}
EOF

	"$CC" -O2 -std=c17 -Wall -Wextra -Werror "$HELPER_C" -o "$HELPER_BIN" \
		|| fail "failed to build helper with $CC"
}

run_case() {
	timeout_ms=$1
	signal_delay_ms=$2
	shift 2

	"$HELPER_BIN" "$timeout_ms" "$signal_delay_ms" "$STDOUT_FILE" "$STDERR_FILE" \
		"$SLEEP_BIN" "$@" >"$RESULT_FILE" \
		|| fail "helper failed for: $*"

	LAST_STDOUT=$(cat "$STDOUT_FILE")
	LAST_STDERR=$(cat "$STDERR_FILE")
	LAST_STATUS=$(sed -n 's/^status=//p' "$RESULT_FILE")
	LAST_ELAPSED_NS=$(sed -n 's/^elapsed_ns=//p' "$RESULT_FILE")
}

[ -x "$SLEEP_BIN" ] || fail "missing binary: $SLEEP_BIN"
build_helper

usage_text=$(printf '%s\n%s' \
	"usage: sleep number[unit] [...]" \
	"Unit can be 's' (seconds, the default), m (minutes), h (hours), or d (days).")

run_case 500 -1
assert_status "no-arg usage status" 1 "$LAST_STATUS"
assert_empty "no-arg usage stdout" "$LAST_STDOUT"
assert_eq "no-arg usage stderr" "$usage_text" "$LAST_STDERR"

run_case 500 -1 --
assert_status "bare -- usage status" 1 "$LAST_STATUS"
assert_empty "bare -- usage stdout" "$LAST_STDOUT"
assert_eq "bare -- usage stderr" "$usage_text" "$LAST_STDERR"

run_case 500 -1 0.03
assert_status "seconds operand status" 0 "$LAST_STATUS"
assert_empty "seconds operand stdout" "$LAST_STDOUT"
assert_empty "seconds operand stderr" "$LAST_STDERR"
assert_ge "seconds operand elapsed" "$LAST_ELAPSED_NS" 20000000

run_case 500 -1 0.0005m
assert_status "minutes operand status" 0 "$LAST_STATUS"
assert_empty "minutes operand stdout" "$LAST_STDOUT"
assert_empty "minutes operand stderr" "$LAST_STDERR"
assert_ge "minutes operand elapsed" "$LAST_ELAPSED_NS" 20000000

run_case 500 -1 0.00001h
assert_status "hours operand status" 0 "$LAST_STATUS"
assert_empty "hours operand stdout" "$LAST_STDOUT"
assert_empty "hours operand stderr" "$LAST_STDERR"
assert_ge "hours operand elapsed" "$LAST_ELAPSED_NS" 25000000

run_case 500 -1 0.0000005d
assert_status "days operand status" 0 "$LAST_STATUS"
assert_empty "days operand stdout" "$LAST_STDOUT"
assert_empty "days operand stderr" "$LAST_STDERR"
assert_ge "days operand elapsed" "$LAST_ELAPSED_NS" 30000000

run_case 500 -1 -0.02 0.07s
assert_status "negative operand addition status" 0 "$LAST_STATUS"
assert_empty "negative operand addition stdout" "$LAST_STDOUT"
assert_empty "negative operand addition stderr" "$LAST_STDERR"
assert_ge "negative operand addition elapsed" "$LAST_ELAPSED_NS" 30000000

run_case 500 -1 -- -0.01 0.03s
assert_status "double-dash negative status" 0 "$LAST_STATUS"
assert_empty "double-dash negative stdout" "$LAST_STDOUT"
assert_empty "double-dash negative stderr" "$LAST_STDERR"
assert_ge "double-dash negative elapsed" "$LAST_ELAPSED_NS" 15000000

run_case 200 -1 1 -1
assert_status "zero-sum immediate status" 0 "$LAST_STATUS"
assert_empty "zero-sum immediate stdout" "$LAST_STDOUT"
assert_empty "zero-sum immediate stderr" "$LAST_STDERR"

run_case 200 -1 bogus
assert_status "bogus operand status" 1 "$LAST_STATUS"
assert_empty "bogus operand stdout" "$LAST_STDOUT"
assert_eq "bogus operand stderr" "sleep: invalid time interval: bogus" "$LAST_STDERR"

run_case 200 -1 1ss
assert_status "trailing garbage status" 1 "$LAST_STATUS"
assert_empty "trailing garbage stdout" "$LAST_STDOUT"
assert_eq "trailing garbage stderr" "sleep: invalid time interval: 1ss" "$LAST_STDERR"

run_case 200 -1 1w
assert_status "unsupported unit status" 1 "$LAST_STATUS"
assert_empty "unsupported unit stdout" "$LAST_STDOUT"
assert_eq "unsupported unit stderr" \
	"sleep: unsupported time unit in interval '1w': 'w' (supported: s, m, h, d)" \
	"$LAST_STDERR"

run_case 200 -1 inf
assert_status "inf status" 1 "$LAST_STATUS"
assert_empty "inf stdout" "$LAST_STDOUT"
assert_eq "inf stderr" \
	"sleep: non-finite time interval is not supported on Linux: inf" \
	"$LAST_STDERR"

run_case 200 -1 nan
assert_status "nan status" 1 "$LAST_STATUS"
assert_empty "nan stdout" "$LAST_STDOUT"
assert_eq "nan stderr" \
	"sleep: non-finite time interval is not supported on Linux: nan" \
	"$LAST_STDERR"

run_case 200 -1 1e30d
assert_status "too-large status" 1 "$LAST_STATUS"
assert_empty "too-large stdout" "$LAST_STDOUT"
assert_eq "too-large stderr" \
	"sleep: requested interval is too large for Linux sleep APIs" \
	"$LAST_STDERR"

run_case 1500 50 0.4s
assert_status "signal report status" 0 "$LAST_STATUS"
assert_empty "signal report stderr" "$LAST_STDERR"
assert_match "signal report stdout" \
	'^about [0-9]+\.[0-9]{9} second\(s\) left out of the original 0\.400000000$' \
	"$LAST_STDOUT"
assert_ge "signal report elapsed" "$LAST_ELAPSED_NS" 300000000

printf '%s\n' "PASS"
