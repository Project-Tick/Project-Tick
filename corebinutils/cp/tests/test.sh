#!/bin/sh
#
# Copyright (c) 2026
#  Project Tick. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
CP_BIN=${CP_BIN:-"$ROOT/out/cp"}
TMPDIR=${TMPDIR:-/tmp}
WORKDIR=$(mktemp -d "$TMPDIR/cp-test.XXXXXX")
trap 'rm -rf "$WORKDIR"' EXIT INT TERM

fail() {
	printf '%s\n' "FAIL: $1" >&2
	exit 1
}

[ -x "$CP_BIN" ] || fail "missing binary: $CP_BIN"

printf '%s\n' "hello" > "$WORKDIR/src.txt"
"$CP_BIN" "$WORKDIR/src.txt" "$WORKDIR/dst.txt"
cmp "$WORKDIR/src.txt" "$WORKDIR/dst.txt" >/dev/null 2>&1 || fail "file copy mismatch"

mkdir -p "$WORKDIR/outdir"
"$CP_BIN" "$WORKDIR/src.txt" "$WORKDIR/outdir"
cmp "$WORKDIR/src.txt" "$WORKDIR/outdir/src.txt" >/dev/null 2>&1 || fail "file to dir mismatch"

mkdir -p "$WORKDIR/tree/sub"
printf '%s\n' "nested" > "$WORKDIR/tree/sub/file.txt"
"$CP_BIN" -R "$WORKDIR/tree" "$WORKDIR/tree-copy"
cmp "$WORKDIR/tree/sub/file.txt" "$WORKDIR/tree-copy/sub/file.txt" >/dev/null 2>&1 || fail "recursive copy mismatch"

ln -s "$WORKDIR/src.txt" "$WORKDIR/src-link"
"$CP_BIN" -P "$WORKDIR/src-link" "$WORKDIR/dst-link"
[ -L "$WORKDIR/dst-link" ] || fail "symlink not preserved under -P"

printf '%s\n' "old" > "$WORKDIR/existing.txt"
printf '%s\n' "new" > "$WORKDIR/new.txt"
"$CP_BIN" -n "$WORKDIR/new.txt" "$WORKDIR/existing.txt" || true
[ "$(cat "$WORKDIR/existing.txt")" = "old" ] || fail "no-clobber failed"

usage_output=$("$CP_BIN" 2>&1 || true)
case $usage_output in
	*"usage: cp "*) ;;
	*) fail "usage output missing" ;;
esac

printf '%s\n' "PASS"
