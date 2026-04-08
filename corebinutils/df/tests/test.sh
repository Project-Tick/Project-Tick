#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
DF_BIN=${DF_BIN:-"$ROOT/out/df"}
TMPDIR=${TMPDIR:-/tmp}
WORKDIR=$(mktemp -d "$TMPDIR/df-test.XXXXXX")
trap 'rm -rf "$WORKDIR"' EXIT INT TERM

fail() {
	printf '%s\n' "FAIL: $1" >&2
	exit 1
}

assert_match() {
	pattern=$1
	text=$2
	message=$3
	printf '%s\n' "$text" | grep -Eq "$pattern" || fail "$message"
}

assert_eq() {
	expected=$1
	actual=$2
	message=$3
	[ "$expected" = "$actual" ] || fail "$message: expected [$expected] got [$actual]"
}

assert_row_fields() {
	line=$1
	expected_first=$2
	expected_last=$3
	min_fields=$4
	message=$5
	printf '%s\n' "$line" | awk -v first="$expected_first" -v last="$expected_last" -v min_fields="$min_fields" '
		{
			if (NF < min_fields)
				exit 1
			if ($1 != first)
				exit 1
			if ($NF != last)
				exit 1
		}
	' || fail "$message"
}

expected_mount_line() {
	path=$1
	awk -v target="$path" '
	function unescape(value,    out, i, octal) {
		out = ""
		for (i = 1; i <= length(value); i++) {
			if (substr(value, i, 1) == "\\" && i + 3 <= length(value) &&
			    substr(value, i + 1, 3) ~ /^[0-7]{3}$/) {
				octal = substr(value, i + 1, 3)
				out = out sprintf("%c", (substr(octal, 1, 1) + 0) * 64 + (substr(octal, 2, 1) + 0) * 8 + (substr(octal, 3, 1) + 0))
				i += 3
			} else {
				out = out substr(value, i, 1)
			}
		}
		return out
	}
	function within(path, mountpoint,    n) {
		if (mountpoint == "/")
			return substr(path, 1, 1) == "/"
		n = length(mountpoint)
		return substr(path, 1, n) == mountpoint &&
		    (length(path) == n || substr(path, n + 1, 1) == "/")
	}
	{
		split($0, halves, " - ")
		split(halves[1], left, " ")
		split(halves[2], right, " ")
		mountpoint = unescape(left[5])
		if (!within(target, mountpoint))
			next
		if (length(mountpoint) >= best_len) {
			best_len = length(mountpoint)
			best_src = unescape(right[2])
			best_type = right[1]
			best_target = mountpoint
		}
	}
	END {
		if (best_len == 0)
			exit 1
		printf "%s\t%s\t%s\n", best_src, best_type, best_target
	}
	' /proc/self/mountinfo
}

parse_source() {
	printf '%s\n' "$1" | cut -f1
}

parse_type() {
	printf '%s\n' "$1" | cut -f2
}

parse_target() {
	printf '%s\n' "$1" | cut -f3
}

[ -x "$DF_BIN" ] || fail "missing binary: $DF_BIN"

export LC_ALL=C

current_path=$(pwd -P)
mount_info=$(expected_mount_line "$current_path") || fail "could not resolve current mount"
expected_source=$(parse_source "$mount_info")
expected_type=$(parse_type "$mount_info")
expected_target=$(parse_target "$mount_info")

usage_output=$("$DF_BIN" -z 2>&1 || true)
assert_match '^usage: df ' "$usage_output" "usage output missing"

unsupported_output=$("$DF_BIN" -n 2>&1 || true)
assert_eq "df: option -n is not supported on Linux" "$unsupported_output" "unsupported -n check failed"

missing_output=$("$DF_BIN" "$WORKDIR/does-not-exist" 2>&1 || true)
assert_match "^df: $WORKDIR/does-not-exist: " "$missing_output" "missing operand error missing"

invalid_type_output=$("$DF_BIN" -t '' 2>&1 || true)
assert_eq "df: empty filesystem type list: " "$invalid_type_output" "empty -t list check failed"

default_output=$("$DF_BIN" -P .)
printf '%s\n' "$default_output" > "$WORKDIR/default.out"
header_line=$(sed -n '1p' "$WORKDIR/default.out")
data_line=$(sed -n '2p' "$WORKDIR/default.out")
assert_match '^Filesystem +512-blocks +Used +Avail +Capacity +Mounted on$' "$header_line" "default header mismatch"
assert_row_fields "$data_line" "$expected_source" "$expected_target" 6 "default row mismatch"

type_output=$("$DF_BIN" -PT .)
printf '%s\n' "$type_output" > "$WORKDIR/type.out"
assert_match '^Filesystem +Type +512-blocks +Used +Avail +Capacity +Mounted on$' "$(sed -n '1p' "$WORKDIR/type.out")" "type header mismatch"
printf '%s\n' "$(sed -n '2p' "$WORKDIR/type.out")" | awk -v first="$expected_source" -v type="$expected_type" -v last="$expected_target" '
	{
		if (NF < 7 || $1 != first || $2 != type || $NF != last)
			exit 1
	}
' || fail "type row mismatch"

inode_output=$("$DF_BIN" -Pi .)
printf '%s\n' "$inode_output" > "$WORKDIR/inode.out"
assert_match '^Filesystem +512-blocks +Used +Avail +Capacity +iused +ifree +%iused +Mounted on$' "$(sed -n '1p' "$WORKDIR/inode.out")" "inode header mismatch"
printf '%s\n' "$(sed -n '2p' "$WORKDIR/inode.out")" | awk -v first="$expected_source" -v last="$expected_target" '
	{
		if (NF < 9 || $1 != first || $NF != last)
			exit 1
		if ($(NF - 1) != "-" && $(NF - 1) !~ /^[0-9]+%$/)
			exit 1
	}
' || fail "inode row mismatch"

human_output=$("$DF_BIN" -h .)
printf '%s\n' "$human_output" > "$WORKDIR/human.out"
assert_match '^Filesystem +Size +Used +Avail +Capacity +Mounted on$' "$(sed -n '1p' "$WORKDIR/human.out")" "human header mismatch"
assert_row_fields "$(sed -n '2p' "$WORKDIR/human.out")" "$expected_source" "$expected_target" 6 "human row mismatch"

si_output=$("$DF_BIN" --si .)
assert_match '^Filesystem +Size +Used +Avail +Capacity +Mounted on$' "$(printf '%s\n' "$si_output" | sed -n '1p')" "si header mismatch"

filtered_output=$("$DF_BIN" -t definitely-not-a-real-fstype . 2>&1 || true)
assert_eq "df: .: filtered out by current filesystem selection" "$filtered_output" "filtered operand error missing"

type_filter_output=$("$DF_BIN" -t "$expected_type" -P .)
assert_match "^Filesystem +512-blocks +Used +Avail +Capacity +Mounted on$" "$(printf '%s\n' "$type_filter_output" | sed -n '1p')" "type filter header mismatch"
assert_row_fields "$(printf '%s\n' "$type_filter_output" | sed -n '2p')" "$expected_source" "$expected_target" 6 "type filter row missing"

total_output=$("$DF_BIN" -c -P .)
printf '%s\n' "$total_output" > "$WORKDIR/total.out"
assert_match '^Filesystem +512-blocks +Used +Avail +Capacity +Mounted on$' "$(sed -n '1p' "$WORKDIR/total.out")" "total header mismatch"
assert_match "^total +[0-9]+ +[0-9]+ +[0-9]+ +[0-9]+%$" "$(sed -n '3p' "$WORKDIR/total.out")" "total row missing"

default_again=$("$DF_BIN" -a -P .)
default_cols=$(printf '%s\n' "$default_output" | awk '{print $1, $NF}')
again_cols=$(printf '%s\n' "$default_again" | awk '{print $1, $NF}')
assert_eq "$default_cols" "$again_cols" "-a should be a no-op on Linux"

printf '%s\n' "PASS"
