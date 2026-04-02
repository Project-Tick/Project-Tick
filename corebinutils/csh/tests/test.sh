#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
CSH_BIN=${CSH_BIN:-"$ROOT/out/csh"}
TMPDIR=${TMPDIR:-/tmp}
WORKDIR=$(mktemp -d "$TMPDIR/csh-test.XXXXXX")
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

assert_match() {
	name=$1
	pattern=$2
	text=$3
	printf '%s\n' "$text" | grep -Eq "$pattern" || fail "$name"
}

[ -x "$CSH_BIN" ] || fail "missing binary: $CSH_BIN"

HOME_DIR=$WORKDIR/home
mkdir -p "$HOME_DIR"

basic_output=$(env -i HOME="$HOME_DIR" PATH=/usr/bin:/bin TERM=dumb LC_ALL=C \
	"$CSH_BIN" -f -c 'echo hello')
assert_eq "basic command" "hello" "$basic_output"

set +e
env -i HOME="$HOME_DIR" PATH=/usr/bin:/bin TERM=dumb LC_ALL=C \
	"$CSH_BIN" -f -c 'exit 7'
exit_status=$?
set -e
[ "$exit_status" -eq 7 ] || fail "exit status propagation"

mkdir -p "$WORKDIR/glob"
printf '%s\n' "match" > "$WORKDIR/glob/sample.txt"
glob_output=$(cd "$WORKDIR/glob" && env -i HOME="$HOME_DIR" PATH=/usr/bin:/bin TERM=dumb LC_ALL=C \
	"$CSH_BIN" -f -c 'echo *.txt')
assert_eq "globbing" "sample.txt" "$glob_output"

limit_output=$(env -i HOME="$HOME_DIR" PATH=/usr/bin:/bin TERM=dumb LC_ALL=C \
	"$CSH_BIN" -f -c 'limit cputime 1; limit cputime')
assert_match "limit builtin" '^cputime[[:space:]]+(1 seconds|0:01)$' "$limit_output"

cat > "$WORKDIR/argv.csh" <<'EOF'
echo ${1}:${2}
EOF
argv_output=$(env -i HOME="$HOME_DIR" PATH=/usr/bin:/bin TERM=dumb LC_ALL=C \
	"$CSH_BIN" -f "$WORKDIR/argv.csh" foo bar)
assert_eq "script argv" "foo:bar" "$argv_output"

termcap_output=$(env -i HOME="$HOME_DIR" PATH=/usr/bin:/bin TERM=testcap \
	TERMCAP='testcap|demo:am:cl=\E[H\E[J:' LC_ALL=C \
	"$CSH_BIN" -f -c 'echotc am; echotc cl' | od -An -tx1)
assert_eq "vendored termcap" " 79 65 73 0a 1b 5b 48 1b 5b 4a" "$termcap_output"

set +e
undefined_output=$(env -i HOME="$HOME_DIR" PATH=/usr/bin:/bin TERM=dumb LC_ALL=C \
	"$CSH_BIN" -f -c 'echo $NO_SUCH_VAR' 2>&1)
undefined_status=$?
set -e
[ "$undefined_status" -ne 0 ] || fail "undefined variable exit status"
assert_match "undefined variable stderr" 'NO_SUCH_VAR: Undefined variable\.' "$undefined_output"

set +e
missing_output=$(env -i HOME="$HOME_DIR" PATH=/usr/bin:/bin TERM=dumb LC_ALL=C \
	"$CSH_BIN" -f -c 'command_does_not_exist' 2>&1)
missing_status=$?
set -e
[ "$missing_status" -ne 0 ] || fail "missing command exit status"
assert_match "missing command stderr" 'command_does_not_exist: Command not found\.' "$missing_output"

set +e
usage_output=$(env -i HOME="$HOME_DIR" PATH=/usr/bin:/bin TERM=dumb LC_ALL=C \
	"$CSH_BIN" -Z 2>&1)
usage_status=$?
set -e
[ "$usage_status" -ne 0 ] || fail "invalid option exit status"
assert_match "usage stderr" 'Usage: (csh|tcsh) ' "$usage_output"

printf '%s\n' "PASS"
