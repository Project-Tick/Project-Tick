#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
CAT_BIN=${CAT_BIN:-"$ROOT/out/cat"}
TMPDIR=${TMPDIR:-/tmp}
WORKDIR=$(mktemp -d "$TMPDIR/cat-test.XXXXXX")
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

[ -x "$CAT_BIN" ] || fail "missing binary: $CAT_BIN"

printf 'hello\nworld\n' > "$WORKDIR/basic.txt"
assert_eq "file passthrough" \
	"hello
world" \
	"$("$CAT_BIN" "$WORKDIR/basic.txt")"

assert_eq "stdin passthrough" \
	"stdin-check" \
	"$(printf 'stdin-check\n' | "$CAT_BIN" -)"

printf 'a\n\n\nb\t\177\n' > "$WORKDIR/fixture.txt"

assert_eq "number all lines" \
	"     1	a
     2	
     3	
     4	b	" \
	"$("$CAT_BIN" -n "$WORKDIR/fixture.txt")"

assert_eq "number nonblank lines" \
	"     1	a


     2	b	" \
	"$("$CAT_BIN" -b "$WORKDIR/fixture.txt")"

assert_eq "squeeze blanks" \
	"a

b	" \
	"$("$CAT_BIN" -s "$WORKDIR/fixture.txt")"

assert_eq "show ends" \
	"a$
$
$
b	^?$" \
	"$("$CAT_BIN" -e "$WORKDIR/fixture.txt")"

assert_eq "show tabs" \
	"a


b^I^?" \
	"$("$CAT_BIN" -t "$WORKDIR/fixture.txt")"

assert_eq "show nonprinting" \
	"a


b	^?" \
	"$("$CAT_BIN" -v "$WORKDIR/fixture.txt")"

usage_output=$("$CAT_BIN" -z 2>&1 || true)
case $usage_output in
	*"usage: cat [-belnstuv] [file ...]"*) ;;
	*) fail "usage on bad flag" ;;
esac

printf '%s\n' "PASS"
