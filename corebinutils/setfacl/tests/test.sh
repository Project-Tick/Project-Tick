#!/bin/sh
set -eu

LC_ALL=C
export LC_ALL

: "${SETFACL_BIN:?SETFACL_BIN is required}"

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TMPBASE="$ROOT/.tmp-tests"
mkdir -p "$TMPBASE"
WORKDIR=$(mktemp -d "$TMPBASE/setfacl-test.XXXXXX")
STDOUT_FILE="$WORKDIR/stdout"
STDERR_FILE="$WORKDIR/stderr"
trap 'rm -rf "$WORKDIR"' EXIT INT TERM

fail() {
	printf '%s\n' "FAIL: $1" >&2
	exit 1
}

assert_eq() {
	expected=$1
	actual=$2
	message=$3
	[ "$expected" = "$actual" ] || {
		printf '%s\n' "FAIL: $message" >&2
		printf '%s\n' "--- expected ---" >&2
		printf '%s\n' "$expected" >&2
		printf '%s\n' "--- actual ---" >&2
		printf '%s\n' "$actual" >&2
		exit 1
	}
}

assert_match() {
	pattern=$1
	text=$2
	message=$3
	printf '%s\n' "$text" | grep -Eq "$pattern" || fail "$message"
}

assert_not_match() {
	pattern=$1
	text=$2
	message=$3
	if printf '%s\n' "$text" | grep -Eq "$pattern"; then
		fail "$message"
	fi
}

assert_mode() {
	expected=$1
	path=$2
	actual=$(stat -c '%a' "$path")
	[ "$actual" = "$expected" ] || fail "$path mode expected $expected got $actual"
}

run_cmd() {
	: >"$STDOUT_FILE"
	: >"$STDERR_FILE"
	set +e
	"$@" >"$STDOUT_FILE" 2>"$STDERR_FILE"
	CMD_STATUS=$?
	set -e
	CMD_STDOUT=$(cat "$STDOUT_FILE")
	CMD_STDERR=$(cat "$STDERR_FILE")
}

run_stdin_cmd() {
	input=$1
	shift
	: >"$STDOUT_FILE"
	: >"$STDERR_FILE"
	set +e
	printf '%s' "$input" | "$@" >"$STDOUT_FILE" 2>"$STDERR_FILE"
	CMD_STATUS=$?
	set -e
	CMD_STDOUT=$(cat "$STDOUT_FILE")
	CMD_STDERR=$(cat "$STDERR_FILE")
}

read_acl_body() {
	getfacl_bin=$1
	path=$2
	shift 2
	"$getfacl_bin" -cpE "$@" "$path" \
		| sed '/^#/d;/^$/d' \
		| sed 's/[[:space:]]\+/ /g' \
		| sort
}

[ -x "$SETFACL_BIN" ] || fail "missing binary: $SETFACL_BIN"

run_cmd "$SETFACL_BIN" -z
[ "$CMD_STATUS" -eq 1 ] || fail "invalid option should exit 1"
assert_match '^usage: setfacl ' "$CMD_STDERR" "usage output missing for invalid option"

run_cmd "$SETFACL_BIN" -a 0 "u::rwx" /dev/null
[ "$CMD_STATUS" -eq 1 ] || fail "unsupported -a should exit 1"
assert_eq "setfacl: option -a is not supported on Linux (NFSv4 ACLs have no Linux POSIX ACL equivalent)" "$CMD_STDERR" "unsupported -a message mismatch"

run_cmd "$SETFACL_BIN" -R -h -m "u::rwx,g::r-x,o::r-x" /dev/null
[ "$CMD_STATUS" -eq 1 ] || fail "-R with -h should exit 1"
assert_eq "setfacl: the -R and -h options may not be specified together" "$CMD_STDERR" "recursive symlink mode conflict message mismatch"

touch "$WORKDIR/base"
chmod 600 "$WORKDIR/base"
run_cmd "$SETFACL_BIN" -m "u::rw,g::r,o::---" "$WORKDIR/base"
[ "$CMD_STATUS" -eq 0 ] || fail "base-only ACL modify should succeed"
assert_mode 640 "$WORKDIR/base"

run_stdin_cmd "$(printf '\n%s\n' "$WORKDIR/base")" "$SETFACL_BIN" -m "u::rw,g::r,o::---"
[ "$CMD_STATUS" -eq 1 ] || fail "empty stdin pathname should report failure"
assert_match '^setfacl: stdin: empty pathname$' "$CMD_STDERR" "empty stdin pathname message missing"
assert_mode 640 "$WORKDIR/base"

ln -s "$WORKDIR/base" "$WORKDIR/base-link"
run_cmd "$SETFACL_BIN" -h -m "u::rw,g::r,o::---" "$WORKDIR/base-link"
[ "$CMD_STATUS" -eq 1 ] || fail "symlink -h should fail on Linux"
assert_eq "setfacl: $WORKDIR/base-link: symbolic link ACLs are not supported on Linux" "$CMD_STDERR" "symlink -h message mismatch"

mkdir -p "$WORKDIR/tree/sub"
touch "$WORKDIR/tree/sub/file"
chmod 700 "$WORKDIR/tree"
chmod 600 "$WORKDIR/tree/sub/file"
chmod 700 "$WORKDIR/tree/sub"
run_cmd "$SETFACL_BIN" -R -m "u::rwx,g::rx,o::rx" "$WORKDIR/tree"
[ "$CMD_STATUS" -eq 0 ] || fail "recursive base-only ACL modify should succeed"
assert_mode 755 "$WORKDIR/tree"
assert_mode 755 "$WORKDIR/tree/sub"
assert_mode 755 "$WORKDIR/tree/sub/file"

mkdir "$WORKDIR/realdir"
touch "$WORKDIR/realdir/file"
chmod 700 "$WORKDIR/realdir"
chmod 600 "$WORKDIR/realdir/file"
ln -s "$WORKDIR/realdir" "$WORKDIR/realdir-link"
run_cmd "$SETFACL_BIN" -R -H -m "u::rwx,g::rx,o::rx" "$WORKDIR/realdir-link"
[ "$CMD_STATUS" -eq 0 ] || fail "-R -H should follow command-line symlink"
assert_mode 755 "$WORKDIR/realdir"
assert_mode 755 "$WORKDIR/realdir/file"

run_cmd "$SETFACL_BIN" -m "owner@:rwx::allow" "$WORKDIR/base"
[ "$CMD_STATUS" -eq 1 ] || fail "NFSv4 ACL syntax should be rejected"
assert_eq "setfacl: NFSv4 ACL entries are not supported on Linux" "$CMD_STDERR" "NFSv4 rejection message mismatch"

GETFACL_BIN=
if command -v getfacl >/dev/null 2>&1; then
	GETFACL_BIN=$(command -v getfacl)
elif [ -x "$ROOT/../getfacl/out/getfacl" ]; then
	GETFACL_BIN="$ROOT/../getfacl/out/getfacl"
fi

acl_supported=0
if [ -n "$GETFACL_BIN" ] && command -v setfacl >/dev/null 2>&1; then
	touch "$WORKDIR/probe"
	chmod 640 "$WORKDIR/probe"
	if setfacl -m "u:$(id -u):rw-" "$WORKDIR/probe" 2>/dev/null; then
		if "$GETFACL_BIN" -cpE "$WORKDIR/probe" 2>/dev/null |
			grep -Eq "^user:($(id -u)|$(id -un)):rw-$"; then
			acl_supported=1
		fi
	fi
fi

if [ "$acl_supported" -eq 1 ]; then
	uid=$(id -u)

	touch "$WORKDIR/ours-access" "$WORKDIR/sys-access"
	chmod 640 "$WORKDIR/ours-access" "$WORKDIR/sys-access"

	run_cmd "$SETFACL_BIN" -m "u:$uid:rw-" "$WORKDIR/ours-access"
	[ "$CMD_STATUS" -eq 0 ] || fail "extended ACL modify failed"
	setfacl -m "u:$uid:rw-" "$WORKDIR/sys-access"
	assert_eq "$(read_acl_body "$GETFACL_BIN" "$WORKDIR/sys-access")" "$(read_acl_body "$GETFACL_BIN" "$WORKDIR/ours-access")" "extended ACL modify diverges from system setfacl"

	run_cmd "$SETFACL_BIN" -n -m "g::r--" "$WORKDIR/ours-access"
	[ "$CMD_STATUS" -eq 0 ] || fail "no-mask ACL modify failed"
	setfacl -n -m "g::r--" "$WORKDIR/sys-access"
	assert_eq "$(read_acl_body "$GETFACL_BIN" "$WORKDIR/sys-access")" "$(read_acl_body "$GETFACL_BIN" "$WORKDIR/ours-access")" "no-mask modify diverges from system setfacl"

	run_cmd "$SETFACL_BIN" -x "u:$uid" "$WORKDIR/ours-access"
	[ "$CMD_STATUS" -eq 0 ] || fail "ACL entry removal failed"
	setfacl -x "u:$uid" "$WORKDIR/sys-access"
	assert_eq "$(read_acl_body "$GETFACL_BIN" "$WORKDIR/sys-access")" "$(read_acl_body "$GETFACL_BIN" "$WORKDIR/ours-access")" "ACL entry removal diverges from system setfacl"

	run_cmd "$SETFACL_BIN" -m "u:$uid:rw-" "$WORKDIR/ours-access"
	[ "$CMD_STATUS" -eq 0 ] || fail "ACL entry re-add failed for index removal test"
	run_cmd "$SETFACL_BIN" -x 1 "$WORKDIR/ours-access"
	[ "$CMD_STATUS" -eq 0 ] || fail "index-based ACL removal failed"
	assert_not_match "^user:($(id -u)|$(id -un)):rw-$" "$(read_acl_body "$GETFACL_BIN" "$WORKDIR/ours-access")" "index-based removal left named user entry behind"

	run_cmd "$SETFACL_BIN" -x 99 "$WORKDIR/ours-access"
	[ "$CMD_STATUS" -eq 1 ] || fail "out-of-range index removal should fail"
	assert_eq "setfacl: $WORKDIR/ours-access: ACL entry index 99 is out of range" "$CMD_STDERR" "out-of-range index removal message mismatch"

	run_cmd "$SETFACL_BIN" -b "$WORKDIR/ours-access"
	[ "$CMD_STATUS" -eq 0 ] || fail "remove-all should succeed"
	setfacl -b "$WORKDIR/sys-access"
	assert_eq "$(read_acl_body "$GETFACL_BIN" "$WORKDIR/sys-access")" "$(read_acl_body "$GETFACL_BIN" "$WORKDIR/ours-access")" "remove-all diverges from system setfacl"

	spec_file="$WORKDIR/spec.txt"
	cat >"$spec_file" <<EOF
# comment
 user:$uid:rw-
 group::r--
EOF
	run_cmd "$SETFACL_BIN" -M "$spec_file" "$WORKDIR/ours-access"
	[ "$CMD_STATUS" -eq 0 ] || fail "ACL modify from file failed"
	setfacl -M "$spec_file" "$WORKDIR/sys-access"
	assert_eq "$(read_acl_body "$GETFACL_BIN" "$WORKDIR/sys-access")" "$(read_acl_body "$GETFACL_BIN" "$WORKDIR/ours-access")" "ACL modify-file diverges from system setfacl"

	remove_file="$WORKDIR/remove.txt"
	cat >"$remove_file" <<EOF
user:$uid
EOF
	run_cmd "$SETFACL_BIN" -X "$remove_file" "$WORKDIR/ours-access"
	[ "$CMD_STATUS" -eq 0 ] || fail "ACL remove-file failed"
	setfacl -X "$remove_file" "$WORKDIR/sys-access"
	assert_eq "$(read_acl_body "$GETFACL_BIN" "$WORKDIR/sys-access")" "$(read_acl_body "$GETFACL_BIN" "$WORKDIR/ours-access")" "ACL remove-file diverges from system setfacl"

	run_cmd "$SETFACL_BIN" -n -m "u:$uid:rw-" "$WORKDIR/base"
	[ "$CMD_STATUS" -eq 1 ] || fail "named entry without mask under -n should fail"
	assert_eq "setfacl: $WORKDIR/base: ACL requires a mask entry when -n is used" "$CMD_STDERR" "missing-mask error mismatch"

	run_cmd "$SETFACL_BIN" -x "u:0" "$WORKDIR/base"
	[ "$CMD_STATUS" -eq 1 ] || fail "removing missing ACL entry should fail"
	assert_eq "setfacl: $WORKDIR/base: cannot remove non-existent ACL entry" "$CMD_STDERR" "missing removal entry message mismatch"

	mkdir "$WORKDIR/ours-dir" "$WORKDIR/sys-dir"
	run_cmd "$SETFACL_BIN" -d -m "u::rwx,g::r-x,o::---,u:$uid:rwx" "$WORKDIR/ours-dir"
	[ "$CMD_STATUS" -eq 0 ] || fail "default ACL modify failed"
	setfacl -d -m "u::rwx,g::r-x,o::---,u:$uid:rwx" "$WORKDIR/sys-dir"
	assert_eq "$(read_acl_body "$GETFACL_BIN" "$WORKDIR/sys-dir")" "$(read_acl_body "$GETFACL_BIN" "$WORKDIR/ours-dir")" "default ACL modify diverges from system setfacl"

	run_cmd "$SETFACL_BIN" -k "$WORKDIR/ours-dir"
	[ "$CMD_STATUS" -eq 0 ] || fail "default ACL removal failed"
	setfacl -k "$WORKDIR/sys-dir"
	assert_eq "$(read_acl_body "$GETFACL_BIN" "$WORKDIR/sys-dir")" "$(read_acl_body "$GETFACL_BIN" "$WORKDIR/ours-dir")" "default ACL removal diverges from system setfacl"

	default_remove_file="$WORKDIR/default-remove.txt"
	cat >"$default_remove_file" <<EOF
user:$uid
EOF
	run_cmd "$SETFACL_BIN" -d -X "$default_remove_file" "$WORKDIR/ours-dir"
	[ "$CMD_STATUS" -eq 1 ] || fail "removing a missing entry from an empty default ACL should fail"
	assert_eq "setfacl: $WORKDIR/ours-dir: cannot remove non-existent ACL entry" "$CMD_STDERR" "empty default ACL follow-up error mismatch"

	run_cmd "$SETFACL_BIN" -d -m "u:$uid:rwx" "$WORKDIR/ours-dir"
	[ "$CMD_STATUS" -eq 1 ] || fail "incomplete default ACL should fail"
	assert_eq "setfacl: $WORKDIR/ours-dir: ACL is missing required user:: entry" "$CMD_STDERR" "incomplete default ACL message mismatch"
else
	printf '%s\n' "SKIP: extended ACL checks (system setfacl/getfacl unavailable or filesystem lacks POSIX ACL support)" >&2
fi

printf '%s\n' "PASS"
