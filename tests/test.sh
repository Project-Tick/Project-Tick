#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
CHFLAGS_BIN=${CHFLAGS_BIN:-"$ROOT/out/chflags"}
TMPDIR=${TMPDIR:-/tmp}
WORKDIR=$(mktemp -d "$TMPDIR/chflags-test.XXXXXX")
trap 'rm -rf "$WORKDIR"' EXIT INT TERM

fail() {
	printf '%s\n' "FAIL: $1" >&2
	exit 1
}

expect_usage() {
	output=$("$CHFLAGS_BIN" "$@" 2>&1 || true)
	case $output in
		*"usage: chflags "*) ;;
		*) fail "usage output missing for args: $*" ;;
	esac
}

[ -x "$CHFLAGS_BIN" ] || fail "missing binary: $CHFLAGS_BIN"

expect_usage
expect_usage -f
expect_usage -H
expect_usage -h
expect_usage -L
expect_usage -P
expect_usage -R
expect_usage -v
expect_usage -x

invalid_output=$("$CHFLAGS_BIN" badflag "$WORKDIR/missing" 2>&1 || true)
case $invalid_output in
	*"invalid flag: badflag"*) ;;
	*) fail "invalid flag output missing" ;;
esac

invalid_octal=$("$CHFLAGS_BIN" 88 "$WORKDIR/missing" 2>&1 || true)
case $invalid_octal in
	*"invalid flag: 88"*) ;;
	*) fail "invalid octal output missing" ;;
esac

conflict_output=$("$CHFLAGS_BIN" -R -h nodump "$WORKDIR/missing" 2>&1 || true)
case $conflict_output in
	*"the -R and -h options may not be specified together."*) ;;
	*) fail "missing -R/-h conflict output" ;;
esac

touch "$WORKDIR/file"
if "$CHFLAGS_BIN" -v nodump "$WORKDIR/file" >/tmp/chflags.out.$$ 2>/tmp/chflags.err.$$; then
	verbose_text=$(cat /tmp/chflags.out.$$)
	case $verbose_text in
		*"$WORKDIR/file"*) ;;
		*) rm -f /tmp/chflags.out.$$ /tmp/chflags.err.$$ /tmp/chflags.out2.$$ /tmp/chflags.err2.$$; fail "verbose output missing file path" ;;
	esac
	"$CHFLAGS_BIN" -v dump "$WORKDIR/file" >/tmp/chflags.out2.$$ 2>/tmp/chflags.err2.$$ || true
else
	err_text=$(cat /tmp/chflags.err.$$)
	case $err_text in
		*"Operation not supported"*|*"Inappropriate ioctl for device"*|*"Operation not permitted"*) ;;
		*) rm -f /tmp/chflags.out.$$ /tmp/chflags.err.$$ /tmp/chflags.out2.$$ /tmp/chflags.err2.$$; fail "unexpected flag application failure: $err_text" ;;
	esac
fi

mkdir -p "$WORKDIR/tree/sub"
touch "$WORKDIR/tree/sub/file"
recursive_verbose=$("$CHFLAGS_BIN" -R -vv nodump "$WORKDIR/tree" 2>/tmp/chflags-rec.err.$$ || true)
recursive_err=$(cat /tmp/chflags-rec.err.$$)
if [ -n "$recursive_err" ]; then
	case $recursive_err in
		*"Operation not supported"*|*"Inappropriate ioctl for device"*|*"Operation not permitted"*) ;;
		*) rm -f /tmp/chflags.out.$$ /tmp/chflags.err.$$ /tmp/chflags.out2.$$ /tmp/chflags.err2.$$ /tmp/chflags-rec.err.$$; fail "unexpected recursive failure: $recursive_err" ;;
	esac
else
	case $recursive_verbose in
		*"$WORKDIR/tree/sub/file"*"$WORKDIR/tree/sub"*"$WORKDIR/tree"*) ;;
		*) rm -f /tmp/chflags.out.$$ /tmp/chflags.err.$$ /tmp/chflags.out2.$$ /tmp/chflags.err2.$$ /tmp/chflags-rec.err.$$; fail "recursive verbose output missing expected paths" ;;
	esac

	recursive_noop=$("$CHFLAGS_BIN" -R -vv nodump "$WORKDIR/tree" 2>/tmp/chflags-noop.err.$$ || true)
	[ -z "$recursive_noop" ] || fail "expected recursive no-op to produce no stdout"
	[ ! -s /tmp/chflags-noop.err.$$ ] || fail "expected recursive no-op to produce no stderr"

	"$CHFLAGS_BIN" -R dump "$WORKDIR/tree" >/tmp/chflags-rec-clear.out.$$ 2>/tmp/chflags-rec-clear.err.$$ || true
fi

ln -s "$WORKDIR/file" "$WORKDIR/link"
"$CHFLAGS_BIN" -fh nodump "$WORKDIR/link" >/tmp/chflags-link.out.$$ 2>/tmp/chflags-link.err.$$ || fail "-fh on symlink should not fail"
[ ! -s /tmp/chflags-link.err.$$ ] || fail "-fh on symlink should suppress stderr"

rm -f /tmp/chflags.out.$$ /tmp/chflags.err.$$ /tmp/chflags.out2.$$ /tmp/chflags.err2.$$ /tmp/chflags-rec.err.$$ /tmp/chflags-noop.err.$$ /tmp/chflags-rec-clear.out.$$ /tmp/chflags-rec-clear.err.$$ /tmp/chflags-link.out.$$ /tmp/chflags-link.err.$$
printf '%s\n' "PASS"
