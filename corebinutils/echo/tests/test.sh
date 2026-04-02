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

LC_ALL=C
LANG=C
export LC_ALL LANG

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ECHO_BIN=${ECHO_BIN:-"$ROOT/out/echo"}
TMPDIR=${TMPDIR:-/tmp}
WORKDIR=$(mktemp -d "$TMPDIR/echo-test.XXXXXX")
trap 'rm -rf "$WORKDIR"' EXIT INT TERM

fail() {
	printf '%s\n' "FAIL: $1" >&2
	exit 1
}

assert_status() {
	name=$1
	expected=$2
	actual=$3

	[ "$expected" -eq "$actual" ] || fail "$name: expected exit $expected got $actual"
}

assert_file_eq() {
	name=$1
	expected_text=$2
	actual_file=$3
	expected_file=$WORKDIR/expected.$$

	printf '%s' "$expected_text" > "$expected_file"
	if ! cmp -s "$expected_file" "$actual_file"; then
		printf '%s\n' "FAIL: $name" >&2
		printf '%s\n' "--- expected ---" >&2
		od -An -tx1 -v "$expected_file" >&2
		printf '%s\n' "--- actual ---" >&2
		od -An -tx1 -v "$actual_file" >&2
		exit 1
	fi
}

run_case() {
	name=$1
	expected_status=$2
	expected_stdout=$3
	expected_stderr=$4
	shift 4

	stdout_file=$WORKDIR/stdout
	stderr_file=$WORKDIR/stderr

	if "$ECHO_BIN" "$@" >"$stdout_file" 2>"$stderr_file"; then
		status=0
	else
		status=$?
	fi

	assert_status "$name status" "$expected_status" "$status"
	assert_file_eq "$name stdout" "$expected_stdout" "$stdout_file"
	assert_file_eq "$name stderr" "$expected_stderr" "$stderr_file"
}

[ -x "$ECHO_BIN" ] || fail "missing binary: $ECHO_BIN"

run_case "no arguments" 0 "
" ""
run_case "basic output" 0 "hello world
" "" "hello" "world"
run_case "bare -n suppresses newline" 0 "" "" "-n"
run_case "single leading -n" 0 "hello world" "" "-n" "hello" "world"
run_case "second -n is literal" 0 "-n literal" "" "-n" "-n" "literal"
run_case "double hyphen stays literal" 0 "--
" "" "--"
run_case "gnu-style -e stays literal" 0 "-e \t
" "" "-e" "\\t"
run_case "non-final backslash-c stays literal" 0 "left\\c right
" "" "left\\c" "right"
run_case "final backslash-c suppresses newline" 0 "hello" "" "hello\\c"
run_case "empty final backslash-c suppresses newline" 0 "" "" "\\c"

stdout_file=$WORKDIR/write-fail.stdout
stderr_file=$WORKDIR/write-fail.stderr
if (
	exec 1>&-
	"$ECHO_BIN" "write-test"
) >"$stdout_file" 2>"$stderr_file"; then
	status=0
else
	status=$?
fi

assert_status "write failure status" 1 "$status"
assert_file_eq "write failure stdout" "" "$stdout_file"
assert_file_eq "write failure stderr" "echo: write: Bad file descriptor
" "$stderr_file"

printf '%s\n' "PASS"
