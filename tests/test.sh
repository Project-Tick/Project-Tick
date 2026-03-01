#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
NPROC_BIN=${NPROC_BIN:-"$ROOT/out/nproc"}
CC=${CC:-cc}
TMPDIR=${TMPDIR:-/tmp}
WORKDIR=$(mktemp -d "$TMPDIR/nproc-test.XXXXXX")
HELPER_C="$WORKDIR/affinity-helper.c"
HELPER_BIN="$WORKDIR/affinity-helper"
STDOUT_FILE="$WORKDIR/stdout"
STDERR_FILE="$WORKDIR/stderr"
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

assert_match() {
	name=$1
	text=$2
	pattern=$3
	printf '%s\n' "$text" | grep -Eq "$pattern" || fail "$name"
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
#define _GNU_SOURCE 1

#include <errno.h>
#include <limits.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static cpu_set_t *
read_affinity(size_t *setsize, int *cpu_count)
{
	cpu_set_t *mask;
	long configured;

	configured = sysconf(_SC_NPROCESSORS_CONF);
	if (configured < 1 || configured > INT_MAX / 2)
		*cpu_count = 128;
	else
		*cpu_count = (int)configured;

	if (*cpu_count < 1)
		*cpu_count = 1;

	for (;;) {
		int saved_errno;

		mask = CPU_ALLOC((size_t)*cpu_count);
		if (mask == NULL) {
			perror("CPU_ALLOC");
			exit(1);
		}
		*setsize = CPU_ALLOC_SIZE((size_t)*cpu_count);
		CPU_ZERO_S(*setsize, mask);

		if (sched_getaffinity(0, *setsize, mask) == 0)
			return (mask);

		saved_errno = errno;
		CPU_FREE(mask);
		if (saved_errno != EINVAL) {
			errno = saved_errno;
			perror("sched_getaffinity");
			exit(1);
		}
		if (*cpu_count > INT_MAX / 2) {
			fprintf(stderr, "affinity mask too large\n");
			exit(1);
		}
		*cpu_count *= 2;
	}
}

static int
online_processors(void)
{
	long value;

	value = sysconf(_SC_NPROCESSORS_ONLN);
	if (value < 1) {
		fprintf(stderr, "invalid online cpu count\n");
		exit(1);
	}
	return ((int)value);
}

static int
subset_size_for_mask(const cpu_set_t *mask, size_t setsize)
{
	int available;

	available = CPU_COUNT_S(setsize, mask);
	if (available <= 1)
		return (0);
	return (available - 1);
}

static void
fill_subset(cpu_set_t *subset, size_t setsize, const cpu_set_t *mask, int subset_size,
    int cpu_count)
{
	int cpu;
	int chosen;

	CPU_ZERO_S(setsize, subset);
	chosen = 0;
	for (cpu = 0; cpu < cpu_count && chosen < subset_size; cpu++) {
		if (!CPU_ISSET_S(cpu, setsize, mask))
			continue;
		CPU_SET_S(cpu, setsize, subset);
		chosen++;
	}
	if (chosen != subset_size) {
		fprintf(stderr, "failed to build subset mask\n");
		exit(1);
	}
}

int
main(int argc, char **argv)
{
	cpu_set_t *mask;
	cpu_set_t *subset;
	size_t setsize;
	int cpu_count;
	int subset_size;

	mask = read_affinity(&setsize, &cpu_count);
	subset_size = subset_size_for_mask(mask, setsize);

	if (argc == 2 && strcmp(argv[1], "counts") == 0) {
		printf("available=%d\n", CPU_COUNT_S(setsize, mask));
		printf("online=%d\n", online_processors());
		printf("subset=%d\n", subset_size);
		CPU_FREE(mask);
		return (0);
	}

	if (argc >= 2 && strcmp(argv[1], "run-subset") == 0) {
		if (argc < 3) {
			fprintf(stderr, "run-subset requires a command\n");
			CPU_FREE(mask);
			return (1);
		}
		if (subset_size == 0) {
			CPU_FREE(mask);
			return (2);
		}

		subset = CPU_ALLOC((size_t)cpu_count);
		if (subset == NULL) {
			perror("CPU_ALLOC");
			CPU_FREE(mask);
			return (1);
		}
		fill_subset(subset, setsize, mask, subset_size, cpu_count);
		if (sched_setaffinity(0, setsize, subset) != 0) {
			perror("sched_setaffinity");
			CPU_FREE(subset);
			CPU_FREE(mask);
			return (1);
		}
		CPU_FREE(subset);
		CPU_FREE(mask);
		execvp(argv[2], &argv[2]);
		perror("execvp");
		return (1);
	}

	fprintf(stderr, "usage: affinity-helper counts | run-subset command [args ...]\n");
	CPU_FREE(mask);
	return (1);
}
EOF

	"$CC" -D_GNU_SOURCE=1 -O2 -std=c17 -Wall -Wextra -Werror \
		"$HELPER_C" -o "$HELPER_BIN" || fail "failed to build helper with $CC"
}

extract_value() {
	name=$1
	text=$2
	value=$(printf '%s\n' "$text" | sed -n "s/^$name=//p")
	[ -n "$value" ] || fail "missing helper value: $name"
	printf '%s' "$value"
}

[ -x "$NPROC_BIN" ] || fail "missing binary: $NPROC_BIN"
build_helper

counts_output=$("$HELPER_BIN" counts)
available_count=$(extract_value available "$counts_output")
online_count=$(extract_value online "$counts_output")
subset_count=$(extract_value subset "$counts_output")

run_capture "$NPROC_BIN"
assert_status "default status" 0 "$LAST_STATUS"
assert_eq "default stdout" "$available_count" "$LAST_STDOUT"
assert_empty "default stderr" "$LAST_STDERR"

run_capture "$NPROC_BIN" --all
assert_status "all status" 0 "$LAST_STATUS"
assert_eq "all stdout" "$online_count" "$LAST_STDOUT"
assert_empty "all stderr" "$LAST_STDERR"

run_capture "$NPROC_BIN" --ignore=0
assert_status "ignore zero status" 0 "$LAST_STATUS"
assert_eq "ignore zero stdout" "$available_count" "$LAST_STDOUT"
assert_empty "ignore zero stderr" "$LAST_STDERR"

run_capture "$NPROC_BIN" --ignore 0
assert_status "ignore split status" 0 "$LAST_STATUS"
assert_eq "ignore split stdout" "$available_count" "$LAST_STDOUT"
assert_empty "ignore split stderr" "$LAST_STDERR"

run_capture "$NPROC_BIN" --ignore="$available_count"
assert_status "ignore saturates status" 0 "$LAST_STATUS"
assert_eq "ignore saturates stdout" "1" "$LAST_STDOUT"
assert_empty "ignore saturates stderr" "$LAST_STDERR"

run_capture "$NPROC_BIN" --all --ignore="$online_count"
assert_status "all ignore saturates status" 0 "$LAST_STATUS"
assert_eq "all ignore saturates stdout" "1" "$LAST_STDOUT"
assert_empty "all ignore saturates stderr" "$LAST_STDERR"

run_capture "$NPROC_BIN" --
assert_status "double dash status" 0 "$LAST_STATUS"
assert_eq "double dash stdout" "$available_count" "$LAST_STDOUT"
assert_empty "double dash stderr" "$LAST_STDERR"

if [ "$subset_count" -gt 0 ]; then
	run_capture "$HELPER_BIN" run-subset "$NPROC_BIN"
	assert_status "subset affinity status" 0 "$LAST_STATUS"
	assert_eq "subset affinity stdout" "$subset_count" "$LAST_STDOUT"
	assert_empty "subset affinity stderr" "$LAST_STDERR"
fi

help_text="usage: nproc [--all] [--ignore=count]
       nproc --help
       nproc --version"

run_capture "$NPROC_BIN" --help
assert_status "help status" 0 "$LAST_STATUS"
assert_empty "help stdout" "$LAST_STDOUT"
assert_eq "help stderr" "$help_text" "$LAST_STDERR"

run_capture "$NPROC_BIN" --version
assert_status "version status" 0 "$LAST_STATUS"
assert_match "version stdout" "$LAST_STDOUT" '^nproc \(neither_GNU nor_coreutils\) 8\.32$'
assert_empty "version stderr" "$LAST_STDERR"

run_capture "$NPROC_BIN" --ignore=abc
assert_status "invalid ignore alpha status" 1 "$LAST_STATUS"
assert_empty "invalid ignore alpha stdout" "$LAST_STDOUT"
assert_eq "invalid ignore alpha stderr" \
	"nproc: invalid ignore count: abc" "$LAST_STDERR"

run_capture "$NPROC_BIN" --ignore=-1
assert_status "invalid ignore negative status" 1 "$LAST_STATUS"
assert_empty "invalid ignore negative stdout" "$LAST_STDOUT"
assert_eq "invalid ignore negative stderr" \
	"nproc: invalid ignore count: -1" "$LAST_STDERR"

run_capture "$NPROC_BIN" --ignore=
assert_status "invalid ignore empty status" 1 "$LAST_STATUS"
assert_empty "invalid ignore empty stdout" "$LAST_STDOUT"
assert_eq "invalid ignore empty stderr" \
	"nproc: invalid ignore count: " "$LAST_STDERR"

run_capture "$NPROC_BIN" --ignore
assert_status "missing ignore status" 1 "$LAST_STATUS"
assert_empty "missing ignore stdout" "$LAST_STDOUT"
assert_eq "missing ignore stderr" \
	"nproc: option requires an argument: --ignore
usage: nproc [--all] [--ignore=count]
       nproc --help
       nproc --version" "$LAST_STDERR"

run_capture "$NPROC_BIN" --bogus
assert_status "unknown option status" 1 "$LAST_STATUS"
assert_empty "unknown option stdout" "$LAST_STDOUT"
assert_eq "unknown option stderr" \
	"nproc: unknown option: --bogus
usage: nproc [--all] [--ignore=count]
       nproc --help
       nproc --version" "$LAST_STDERR"

run_capture "$NPROC_BIN" extra
assert_status "unexpected operand status" 1 "$LAST_STATUS"
assert_empty "unexpected operand stdout" "$LAST_STDOUT"
assert_eq "unexpected operand stderr" \
	"nproc: unexpected operand: extra
usage: nproc [--all] [--ignore=count]
       nproc --help
       nproc --version" "$LAST_STDERR"

run_capture "$NPROC_BIN" --help --all
assert_status "help exclusive status" 1 "$LAST_STATUS"
assert_empty "help exclusive stdout" "$LAST_STDOUT"
assert_eq "help exclusive stderr" \
	"nproc: option --help must be used alone
usage: nproc [--all] [--ignore=count]
       nproc --help
       nproc --version" "$LAST_STDERR"

printf '%s\n' "PASS"
