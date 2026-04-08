#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
DOMAINNAME_BIN=${DOMAINNAME_BIN:-"$ROOT/out/domainname"}
CC=${CC:-cc}
TMPDIR=${TMPDIR:-/tmp}
WORKDIR=$(mktemp -d "$TMPDIR/domainname-test.XXXXXX")
HELPER_C="$WORKDIR/get-domainname.c"
HELPER_BIN="$WORKDIR/get-domainname"
trap 'rm -rf "$WORKDIR"' EXIT INT TERM

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

build_helper() {
	cat >"$HELPER_C" <<'EOF'
#define _GNU_SOURCE 1
#include <sys/utsname.h>

#include <stdio.h>
#include <stdlib.h>

int
main(void)
{
	struct utsname uts;

	if (uname(&uts) != 0)
		return (1);
	if (fputs(uts.domainname, stdout) == EOF || fputc('\n', stdout) == EOF)
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
	unshare_script='
		bin=$1
		helper=$2

		"$bin" ""
		[ "$("$helper")" = "" ]

		"$bin" freebsd-linux-port
		[ "$("$helper")" = "freebsd-linux-port" ]

		"$bin" -- -leading-dash
		[ "$("$helper")" = "-leading-dash" ]
	'

	if ! command -v unshare >/dev/null 2>&1; then
		printf '%s\n' "SKIP: mutation test (missing unshare)" >&2
		return 0
	fi

	set +e
	output=$(unshare -Ur -u sh -eu -c "$unshare_script" sh "$DOMAINNAME_BIN" \
		"$HELPER_BIN" 2>&1)
	status=$?
	set -e

	if [ "$status" -eq 0 ]; then
		return 0
	fi

	case $output in
		*"cannot open /proc/self/uid_map: Permission denied"*|\
		*"write failed /proc/self/uid_map: Operation not permitted"*|\
		*"unshare: invalid option -- 'r'"*|\
		*"unshare: unrecognized option '--map-root-user'"*)
			set +e
			output=$(unshare -u sh -eu -c "$unshare_script" sh "$DOMAINNAME_BIN" \
				"$HELPER_BIN" 2>&1)
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
		*"setdomainname: Operation not permitted"*)
			printf '%s\n' "SKIP: mutation test ($output)" >&2
			return 0
			;;
	esac

	fail "namespace mutation test failed: $output"
}

[ -x "$DOMAINNAME_BIN" ] || fail "missing binary: $DOMAINNAME_BIN"
build_helper

expected_domain=$("$HELPER_BIN")
actual_domain=$("$DOMAINNAME_BIN")
assert_eq "current domainname" "$expected_domain" "$actual_domain"

usage_bad_flag=$("$DOMAINNAME_BIN" -x 2>&1 || true)
assert_contains "bad flag should print usage" "$usage_bad_flag" \
	"usage: domainname [ypdomain]"

usage_too_many=$("$DOMAINNAME_BIN" first second 2>&1 || true)
assert_contains "too many args should print usage" "$usage_too_many" \
	"usage: domainname [ypdomain]"

long_name=$(repeat_a 65)
long_name_output=$("$DOMAINNAME_BIN" "$long_name" 2>&1 || true)
assert_contains "too-long domain should be rejected" "$long_name_output" \
	"domainname: domain name too long for Linux UTS namespace: 65 bytes given, limit is 64"

run_namespace_mutation_test

printf '%s\n' "PASS"
