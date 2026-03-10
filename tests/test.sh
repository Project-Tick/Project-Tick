#!/bin/sh
set -eu

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
UUIDGEN_BIN=${UUIDGEN_BIN:-"$ROOT/out/uuidgen"}
CC=${CC:-cc}
TMPDIR=${TMPDIR:-/tmp}
WORKDIR=$(mktemp -d "$TMPDIR/uuidgen-test.XXXXXX")
PROBE_C="$WORKDIR/probe.c"
PROBE_BIN="$WORKDIR/probe"
STDOUT_FILE="$WORKDIR/stdout"
STDERR_FILE="$WORKDIR/stderr"
LAST_STATUS=0
LAST_STDOUT=
LAST_STDERR=
USAGE_TEXT='usage: uuidgen [-1] [-r] [-c] [-n count] [-o filename]'

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

assert_line_count() {
	name=$1
	expected=$2
	actual=$(wc -l <"$STDOUT_FILE" | tr -d ' ')
	if [ "$expected" -ne "$actual" ]; then
		printf '%s\n' "FAIL: $name" >&2
		printf '%s\n' "expected lines: $expected" >&2
		printf '%s\n' "actual lines: $actual" >&2
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

build_probe() {
	cat >"$PROBE_C" <<'PROBE_EOF'
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int
hex_value(char c)
{
	if (c >= '0' && c <= '9')
		return (c - '0');
	if (c >= 'a' && c <= 'f')
		return (10 + (c - 'a'));
	if (c >= 'A' && c <= 'F')
		return (10 + (c - 'A'));
	return (-1);
}

static int
parse_uuid(const char *s, uint8_t out[16])
{
	char compact[32];
	size_t i;
	size_t len;
	size_t pos;

	len = strlen(s);
	if (len == 36) {
		if (s[8] != '-' || s[13] != '-' || s[18] != '-' || s[23] != '-')
			return (-1);
		pos = 0;
		for (i = 0; i < len; i++) {
			if (s[i] == '-')
				continue;
			if (pos >= sizeof(compact))
				return (-1);
			compact[pos++] = s[i];
		}
		if (pos != sizeof(compact))
			return (-1);
	} else if (len == 32) {
		memcpy(compact, s, sizeof(compact));
	} else {
		return (-1);
	}

	for (i = 0; i < 16; i++) {
		int hi, lo;

		hi = hex_value(compact[i * 2]);
		lo = hex_value(compact[i * 2 + 1]);
		if (hi < 0 || lo < 0)
			return (-1);
		out[i] = (uint8_t)((hi << 4) | lo);
	}
	return (0);
}

int
main(int argc, char *argv[])
{
	uint8_t uuid[16];
	uint64_t timestamp;
	unsigned version;
	unsigned variant_ok;

	if (argc != 2)
		return (2);
	if (parse_uuid(argv[1], uuid) != 0)
		return (1);

	version = (unsigned)((uuid[6] >> 4) & 0x0f);
	variant_ok = (((uuid[8] >> 6) & 0x03) == 0x02) ? 1U : 0U;
	timestamp = ((uint64_t)(uuid[6] & 0x0f) << 56) |
	    ((uint64_t)uuid[7] << 48) |
	    ((uint64_t)uuid[4] << 40) |
	    ((uint64_t)uuid[5] << 32) |
	    ((uint64_t)uuid[0] << 24) |
	    ((uint64_t)uuid[1] << 16) |
	    ((uint64_t)uuid[2] << 8) |
	    (uint64_t)uuid[3];

	printf("%u %u %llu\n", version, variant_ok,
	    (unsigned long long)timestamp);
	return (0);
}
PROBE_EOF

	"$CC" -O2 -std=c17 -Wall -Wextra -Werror "$PROBE_C" -o "$PROBE_BIN" \
		|| fail "failed to build probe with $CC"
}

validate_output() {
	expected_version=$1
	expected_lines=$2
	expected_format=$3
	timestamp_mode=$4

	count=0
	prev_ts=
	while IFS= read -r line; do
		count=$((count + 1))

		case $expected_format in
		hyphen)
			[ ${#line} -eq 36 ] || fail "line length for hyphen UUID"
			case $line in
				*-*) ;;
				*) fail "hyphen UUID missing separators" ;;
			esac
			;;
		compact)
			[ ${#line} -eq 32 ] || fail "line length for compact UUID"
			case $line in
				*-*) fail "compact UUID contains hyphen" ;;
			esac
			;;
		*)
			fail "invalid test format mode"
			;;
		esac

		probe_output=$("$PROBE_BIN" "$line") ||
			fail "probe rejected UUID line: $line"
		set -- $probe_output
		version=$1
		variant_ok=$2
		timestamp=$3

		[ "$version" -eq "$expected_version" ] ||
			fail "unexpected UUID version: got $version expected $expected_version"
		[ "$variant_ok" -eq 1 ] || fail "UUID variant is not RFC4122"

		case $timestamp_mode in
		none)
			;;
		dense)
			if [ -n "$prev_ts" ]; then
				expected_next=$((prev_ts + 1))
				[ "$timestamp" -eq "$expected_next" ] ||
					fail "batch UUID timestamps are not dense"
			fi
			prev_ts=$timestamp
			;;
		increasing)
			if [ -n "$prev_ts" ]; then
				[ "$timestamp" -gt "$prev_ts" ] ||
					fail "iterative UUID timestamps are not strictly increasing"
			fi
			prev_ts=$timestamp
			;;
		*)
			fail "invalid test timestamp mode"
			;;
		esac
	done <"$STDOUT_FILE"

	if [ "$count" -ne "$expected_lines" ]; then
		printf '%s\n' "FAIL: output line count mismatch" >&2
		printf '%s\n' "expected: $expected_lines" >&2
		printf '%s\n' "actual: $count" >&2
		exit 1
	fi
}

assert_unique_lines() {
	if [ -n "$(sort "$STDOUT_FILE" | uniq -d)" ]; then
		fail "duplicate UUID lines detected"
	fi
}

[ -x "$UUIDGEN_BIN" ] || fail "missing binary: $UUIDGEN_BIN"
build_probe

run_capture "$UUIDGEN_BIN" -x
assert_status "invalid flag status" 1 "$LAST_STATUS"
assert_empty "invalid flag stdout" "$LAST_STDOUT"
assert_eq "invalid flag usage" "$USAGE_TEXT" "$LAST_STDERR"

run_capture "$UUIDGEN_BIN" extra-arg
assert_status "extra arg status" 1 "$LAST_STATUS"
assert_empty "extra arg stdout" "$LAST_STDOUT"
assert_eq "extra arg usage" "$USAGE_TEXT" "$LAST_STDERR"

run_capture "$UUIDGEN_BIN" -n 0
assert_status "count zero status" 1 "$LAST_STATUS"
assert_empty "count zero stdout" "$LAST_STDOUT"
assert_eq "count zero error" "uuidgen: invalid count '0' (expected 1..2048)" \
	"$LAST_STDERR"

run_capture "$UUIDGEN_BIN" -n foo
assert_status "count non-numeric status" 1 "$LAST_STATUS"
assert_empty "count non-numeric stdout" "$LAST_STDOUT"
assert_eq "count non-numeric error" \
	"uuidgen: invalid count 'foo' (expected 1..2048)" "$LAST_STDERR"

run_capture "$UUIDGEN_BIN" -n 2049
assert_status "count too-large status" 1 "$LAST_STATUS"
assert_empty "count too-large stdout" "$LAST_STDOUT"
assert_eq "count too-large error" \
	"uuidgen: invalid count '2049' (expected 1..2048)" "$LAST_STDERR"

run_capture "$UUIDGEN_BIN" -n 1 -n 2
assert_status "duplicate -n status" 1 "$LAST_STATUS"
assert_empty "duplicate -n stdout" "$LAST_STDOUT"
assert_eq "duplicate -n error" "uuidgen: option -n specified more than once" \
	"$LAST_STDERR"

run_capture "$UUIDGEN_BIN" -o "$WORKDIR/a" -o "$WORKDIR/b"
assert_status "duplicate -o status" 1 "$LAST_STATUS"
assert_empty "duplicate -o stdout" "$LAST_STDOUT"
assert_eq "duplicate -o error" "uuidgen: multiple output files not allowed" \
	"$LAST_STDERR"

run_capture "$UUIDGEN_BIN"
assert_status "default status" 0 "$LAST_STATUS"
assert_empty "default stderr" "$LAST_STDERR"
assert_line_count "default line count" 1
validate_output 1 1 hyphen increasing

run_capture "$UUIDGEN_BIN" -r
assert_status "-r status" 0 "$LAST_STATUS"
assert_empty "-r stderr" "$LAST_STDERR"
assert_line_count "-r line count" 1
validate_output 4 1 hyphen none

run_capture "$UUIDGEN_BIN" -c
assert_status "-c status" 0 "$LAST_STATUS"
assert_empty "-c stderr" "$LAST_STDERR"
assert_line_count "-c line count" 1
validate_output 1 1 compact increasing

run_capture "$UUIDGEN_BIN" -r -c -n 8
assert_status "-r -c -n status" 0 "$LAST_STATUS"
assert_empty "-r -c -n stderr" "$LAST_STDERR"
assert_line_count "-r -c -n line count" 8
validate_output 4 8 compact none
assert_unique_lines

run_capture "$UUIDGEN_BIN" -n 6
assert_status "batch v1 status" 0 "$LAST_STATUS"
assert_empty "batch v1 stderr" "$LAST_STDERR"
assert_line_count "batch v1 line count" 6
validate_output 1 6 hyphen dense
assert_unique_lines

run_capture "$UUIDGEN_BIN" -1 -n 6
assert_status "iterate v1 status" 0 "$LAST_STATUS"
assert_empty "iterate v1 stderr" "$LAST_STDERR"
assert_line_count "iterate v1 line count" 6
validate_output 1 6 hyphen increasing
assert_unique_lines

OUT_FILE="$WORKDIR/out.txt"
run_capture "$UUIDGEN_BIN" -r -n 3 -o "$OUT_FILE"
assert_status "-o status" 0 "$LAST_STATUS"
assert_empty "-o stdout" "$LAST_STDOUT"
assert_empty "-o stderr" "$LAST_STDERR"
cp "$OUT_FILE" "$STDOUT_FILE"
validate_output 4 3 hyphen none
assert_unique_lines

MISSING_DIR="$WORKDIR/missing"
run_capture "$UUIDGEN_BIN" -o "$MISSING_DIR/out.txt"
assert_status "unopenable output status" 1 "$LAST_STATUS"
assert_empty "unopenable output stdout" "$LAST_STDOUT"
assert_contains "unopenable output prefix" "$LAST_STDERR" \
	"uuidgen: cannot open '$MISSING_DIR/out.txt':"
assert_contains "unopenable output errno" "$LAST_STDERR" \
	"No such file or directory"

run_capture "$UUIDGEN_BIN" -n 2048
assert_status "max-count status" 0 "$LAST_STATUS"
assert_empty "max-count stderr" "$LAST_STDERR"
assert_line_count "max-count line count" 2048
validate_output 1 2048 hyphen dense
assert_unique_lines

printf '%s\n' "PASS"
