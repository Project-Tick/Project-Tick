#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TEST_ROOT="$ROOT/tests"
SH_BIN=${SH_BIN:-"$ROOT/out/sh"}
SH_RUNNER=${SH_RUNNER:-}
TMPDIR=${TMPDIR:-/tmp}
WORKDIR=$(mktemp -d "$TMPDIR/sh-test.XXXXXX")
trap 'rm -rf "$WORKDIR"' EXIT INT TERM

SKIP_CASES='
parameters/mail1.0
parameters/mail2.0
builtins/fc1.0
builtins/fc2.0
builtins/fc3.0
'

fail() {
	printf '%s\n' "FAIL: $1" >&2
	exit 1
}

should_skip() {
	case "
$SKIP_CASES
" in
	*"
$1
"*) return 0 ;;
	esac
	return 1
}

assert_eq() {
	name=$1
	expected_file=$2
	actual_file=$3
	if ! cmp -s "$expected_file" "$actual_file"; then
		printf '%s\n' "FAIL: $name" >&2
		printf '%s\n' "--- expected ---" >&2
		cat "$expected_file" >&2
		printf '\n%s\n' "--- actual ---" >&2
		cat "$actual_file" >&2
		printf '\n' >&2
		exit 1
	fi
}

[ -x "$SH_BIN" ] || fail "missing binary: $SH_BIN"

export SH="$SH_BIN"
export LC_ALL=C
export PATH=/bin:/usr/bin

cases_run=0
cases_skipped=0
case_list="$WORKDIR/cases.list"

find "$TEST_ROOT" -type f \
	! -name '*.stdout' \
	! -name '*.stderr' \
	! -name 'test.sh' \
	! -name 'functional_test.sh' \
	| sed "s#^$TEST_ROOT/##" \
	| sort >"$case_list"

while IFS= read -r rel; do
		case_name=${rel##*/}
		case $case_name in
			*.[0-9]*)
				:
				;;
			*)
				continue
				;;
		esac

		if should_skip "$rel"; then
			cases_skipped=$((cases_skipped + 1))
			printf '%s\n' "SKIP $rel"
			continue
		fi

		status_expected=${case_name##*.}
		stdout_expected="$TEST_ROOT/$rel.stdout"
		stderr_expected="$TEST_ROOT/$rel.stderr"
		safe_name=$(printf '%s' "$rel" | tr '/\n' '__')
		stdout_actual="$WORKDIR/$safe_name.stdout"
		stderr_actual="$WORKDIR/$safe_name.stderr"
		case_workdir="$WORKDIR/$safe_name.wd"

		mkdir -p "$case_workdir"
			(
				cd "$case_workdir"
				set +e
				if [ -n "$SH_RUNNER" ]; then
					"$SH_RUNNER" "$SH_BIN" "$TEST_ROOT/$rel" >"$stdout_actual" 2>"$stderr_actual"
				else
					"$SH_BIN" "$TEST_ROOT/$rel" >"$stdout_actual" 2>"$stderr_actual"
				fi
				rc=$?
				set -e
			[ "$rc" -eq "$status_expected" ] || {
				printf '%s\n' "FAIL: $rel exit $rc expected $status_expected" >&2
				exit 1
			}
		) || exit 1

		if [ -f "$stdout_expected" ]; then
			assert_eq "$rel stdout" "$stdout_expected" "$stdout_actual"
		else
			[ ! -s "$stdout_actual" ] || fail "$rel unexpected stdout"
		fi

		if [ -f "$stderr_expected" ]; then
			assert_eq "$rel stderr" "$stderr_expected" "$stderr_actual"
		else
			[ ! -s "$stderr_actual" ] || fail "$rel unexpected stderr"
		fi

		cases_run=$((cases_run + 1))
		printf '%s\n' "PASS $rel"
done <"$case_list"

printf '%s\n' "PASS: ran $cases_run cases, skipped $cases_skipped"
