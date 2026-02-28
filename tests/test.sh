#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
HOSTNAME_BIN=${HOSTNAME_BIN:-"$ROOT/out/hostname"}
CC=${CC:-cc}
TMPDIR=${TMPDIR:-/tmp}
WORKDIR=$(mktemp -d "$TMPDIR/hostname-test.XXXXXX")
HELPER_C="$WORKDIR/get-hostname.c"
HELPER_BIN="$WORKDIR/get-hostname"
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
#define _GNU_SOURCE 1
#include <sys/utsname.h>

#include <stdio.h>

int
main(void)
{
	struct utsname uts;

	if (uname(&uts) != 0)
		return (1);
	if (fputs(uts.nodename, stdout) == EOF || fputc('\n', stdout) == EOF)
		return (1);
	return (0);
}
EOF

	"$CC" -O2 -std=c17 -Wall -Wextra -Werror "$HELPER_C" -o "$HELPER_BIN" \
		|| fail "failed to build helper with $CC"
}

repeat_a() {
	count=$1
	result=
	while [ "$count" -gt 0 ]; do
		result="${result}a"
		count=$((count - 1))
	done
	printf '%s' "$result"
}

run_namespace_mutation_test() {
	max_name=$1
	unshare_script='
		bin=$1
		helper=$2
		max_name=$3

		"$bin" freebsdport
		[ "$("$helper")" = "freebsdport" ]
		[ "$("$bin")" = "freebsdport" ]
		[ "$("$bin" -s)" = "freebsdport" ]
		[ "$("$bin" -d)" = "" ]

		"$bin" "$max_name"
		[ "$("$helper")" = "$max_name" ]
		[ "$("$bin")" = "$max_name" ]
		[ "$("$bin" -s)" = "$max_name" ]
		[ "$("$bin" -d)" = "" ]

		"$bin" freebsd-linux-port.example.test
		[ "$("$helper")" = "freebsd-linux-port.example.test" ]
		[ "$("$bin")" = "freebsd-linux-port.example.test" ]
		[ "$("$bin" -s)" = "freebsd-linux-port" ]
		[ "$("$bin" -d)" = "example.test" ]

		"$bin" -- -leading-dash
		[ "$("$helper")" = "-leading-dash" ]
		[ "$("$bin" -s)" = "-leading-dash" ]
		[ "$("$bin" -d)" = "" ]
	'

	if ! command -v unshare >/dev/null 2>&1; then
		printf '%s\n' "SKIP: mutation test (missing unshare)" >&2
		return 0
	fi

	set +e
	output=$(unshare -Ur -u sh -eu -c "$unshare_script" sh "$HOSTNAME_BIN" \
		"$HELPER_BIN" "$max_name" 2>&1)
	status=$?
	set -e

	if [ "$status" -eq 0 ]; then
		return 0
	fi

	case $output in
		*"cannot open /proc/self/uid_map: Permission denied"*|\
		*"unshare: invalid option -- 'r'"*|\
		*"unshare: unrecognized option '--map-root-user'"*)
			set +e
			output=$(unshare -u sh -eu -c "$unshare_script" sh "$HOSTNAME_BIN" \
				"$HELPER_BIN" "$max_name" 2>&1)
			status=$?
			set -e
			if [ "$status" -eq 0 ]; then
				return 0
			fi
			;;
	esac

	case $output in
		*"unshare failed: Operation not permitted"*|\
		*"unshare failed: Invalid argument"*|\
		*"unshare failed: No space left on device"*|\
		*"unshare: cannot open /proc/self/ns/uts: Permission denied"*|\
		*"unshare: invalid option"*|\
		*"unshare: unshare failed: Operation not permitted"*|\
		*"unshare: unshare failed: Invalid argument"*|\
		*"unshare: unshare failed: No space left on device"*|\
		*"sethostname: Operation not permitted"*)
			printf '%s\n' "SKIP: mutation test ($output)" >&2
			return 0
			;;
	esac

	fail "namespace mutation test failed: $output"
}

[ -x "$HOSTNAME_BIN" ] || fail "missing binary: $HOSTNAME_BIN"
build_helper

expected_hostname=$("$HELPER_BIN")
case $expected_hostname in
*.*)
	expected_short=${expected_hostname%%.*}
	expected_domain=${expected_hostname#*.}
	;;
*)
	expected_short=$expected_hostname
	expected_domain=
	;;
esac

run_capture "$HOSTNAME_BIN"
assert_status "current hostname status" 0 "$LAST_STATUS"
assert_eq "current hostname stdout" "$expected_hostname" "$LAST_STDOUT"
assert_empty "current hostname stderr" "$LAST_STDERR"

run_capture "$HOSTNAME_BIN" -s
assert_status "short hostname status" 0 "$LAST_STATUS"
assert_eq "short hostname stdout" "$expected_short" "$LAST_STDOUT"
assert_empty "short hostname stderr" "$LAST_STDERR"

run_capture "$HOSTNAME_BIN" -d
assert_status "domain hostname status" 0 "$LAST_STATUS"
assert_eq "domain hostname stdout" "$expected_domain" "$LAST_STDOUT"
assert_empty "domain hostname stderr" "$LAST_STDERR"

run_capture "$HOSTNAME_BIN" -x
assert_status "bad flag status" 1 "$LAST_STATUS"
assert_empty "bad flag stdout" "$LAST_STDOUT"
assert_eq "bad flag stderr" "usage: hostname [-s | -d] [name-of-host]" \
	"$LAST_STDERR"

run_capture "$HOSTNAME_BIN" first second
assert_status "too many args status" 1 "$LAST_STATUS"
assert_empty "too many args stdout" "$LAST_STDOUT"
assert_eq "too many args stderr" "usage: hostname [-s | -d] [name-of-host]" \
	"$LAST_STDERR"

run_capture "$HOSTNAME_BIN" -s -d
assert_status "conflicting flags status" 1 "$LAST_STATUS"
assert_empty "conflicting flags stdout" "$LAST_STDOUT"
assert_eq "conflicting flags stderr" "usage: hostname [-s | -d] [name-of-host]" \
	"$LAST_STDERR"

run_capture "$HOSTNAME_BIN" -s newname
assert_status "set with mode status" 1 "$LAST_STATUS"
assert_empty "set with mode stdout" "$LAST_STDOUT"
assert_eq "set with mode stderr" "usage: hostname [-s | -d] [name-of-host]" \
	"$LAST_STDERR"

run_capture "$HOSTNAME_BIN" -f
assert_status "unsupported -f status" 1 "$LAST_STATUS"
assert_empty "unsupported -f stdout" "$LAST_STDOUT"
assert_eq "unsupported -f stderr" \
	"hostname: option -f is not supported on Linux: FQDN resolution depends on NSS/DNS, not the kernel hostname" \
	"$LAST_STDERR"

long_name=$(repeat_a 65)
max_name=$(repeat_a 64)

run_capture "$HOSTNAME_BIN" "$long_name"
assert_status "too-long hostname status" 1 "$LAST_STATUS"
assert_empty "too-long hostname stdout" "$LAST_STDOUT"
assert_eq "too-long hostname stderr" \
	"hostname: host name too long for Linux UTS namespace: 65 bytes given, limit is 64" \
	"$LAST_STDERR"

run_namespace_mutation_test "$max_name"

printf '%s\n' "PASS"
