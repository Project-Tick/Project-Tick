#!/bin/sh

set -eu
export LC_ALL=C
export TZ=UTC

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
PAX_BIN=${PAX_BIN:-"$ROOT/out/pax"}
TMPDIR=${TMPDIR:-/tmp}
WORKDIR=$(mktemp -d "$TMPDIR/pax-test.XXXXXX")
trap 'rm -rf "$WORKDIR"' EXIT INT TERM

fail() {
	printf '%s\n' "FAIL: $1" >&2
	exit 1
}

run_notty() {
	if command -v setsid >/dev/null 2>&1; then
		setsid "$@"
	else
		"$@"
	fi
}

can_run_script_pty() {
	command -v script >/dev/null 2>&1 || return 1
	printf 'ok\n' | script -qec 'cat >/dev/null' /dev/null >/dev/null 2>&1
}

can_run_userns_root() {
	command -v unshare >/dev/null 2>&1 || return 1
	unshare --user --map-root-user -- sh -c 'test "$(id -u)" -eq 0' >/dev/null 2>&1
}

assert_contains() {
	needle=$1
	haystack=$2
	case $haystack in
		*"$needle"*) ;;
		*) fail "missing '$needle'" ;;
	esac
}

assert_eq() {
	expected=$1
	actual=$2
	[ "$expected" = "$actual" ] || fail "expected '$expected' got '$actual'"
}

assert_ne() {
	left=$1
	right=$2
	[ "$left" != "$right" ] || fail "unexpected equality '$left'"
}

assert_status() {
	expected=$1
	actual=$2
	[ "$expected" -eq "$actual" ] || fail "expected status $expected got $actual"
}

assert_file_eq() {
	left=$1
	right=$2
	cmp "$left" "$right" >/dev/null 2>&1 || fail "files differ: $left $right"
}

run_capture() {
	status_file=$1
	output_file=$2
	shift 2
	if "$@" >"$output_file" 2>&1; then
		status=0
	else
		status=$?
	fi
	printf '%s\n' "$status" > "$status_file"
}

repeat_char() {
	char=$1
	count=$2
	i=0
	out=
	while [ "$i" -lt "$count" ]; do
		out=${out}${char}
		i=$((i + 1))
	done
	printf '%s' "$out"
}

[ -x "$PAX_BIN" ] || fail "missing binary: $PAX_BIN"

TAR_BIN="$WORKDIR/tar"
CPIO_BIN="$WORKDIR/cpio"
ln -s "$PAX_BIN" "$TAR_BIN"
ln -s "$PAX_BIN" "$CPIO_BIN"

run_capture "$WORKDIR/usage.status" "$WORKDIR/usage.err" \
	run_notty "$PAX_BIN" --definitely-invalid
usage_output=$(cat "$WORKDIR/usage.err")
usage_status=$(cat "$WORKDIR/usage.status")
assert_status 1 "$usage_status"
assert_contains "usage: pax " "$usage_output"

run_capture "$WORKDIR/missing.status" "$WORKDIR/missing.err" \
	run_notty "$PAX_BIN" -f "$WORKDIR/missing.pax" </dev/null
missing_archive=$(cat "$WORKDIR/missing.err")
missing_status=$(cat "$WORKDIR/missing.status")
assert_status 1 "$missing_status"
assert_contains "Failed open to read" "$missing_archive"
assert_contains "missing.pax" "$missing_archive"

mkdir -p "$WORKDIR/src/dir/subdir"
printf '%s\n' alpha > "$WORKDIR/src/dir/file.txt"
printf '%s\n' beta > "$WORKDIR/src/dir/subdir/nested.txt"
printf '%s\n' meta > "$WORKDIR/src/meta.txt"
chmod 6755 "$WORKDIR/src/meta.txt"
touch -t 202311142213.20 "$WORKDIR/src/meta.txt"
ln "$WORKDIR/src/dir/file.txt" "$WORKDIR/src/dir/file.hard"
ln -s "dir/file.txt" "$WORKDIR/src/link-to-file"
mkfifo "$WORKDIR/src/archive.fifo"

(
	cd "$WORKDIR/src"
	run_notty "$PAX_BIN" -w -f "$WORKDIR/archive.pax" . </dev/null
)

list_output=$(run_notty "$PAX_BIN" -f "$WORKDIR/archive.pax" </dev/null)
assert_contains "dir/file.txt" "$list_output"
assert_contains "dir/file.hard" "$list_output"
assert_contains "link-to-file" "$list_output"

mkdir "$WORKDIR/extract"
(
	cd "$WORKDIR/extract"
	run_notty "$PAX_BIN" -r -f "$WORKDIR/archive.pax" </dev/null
)

assert_file_eq "$WORKDIR/src/dir/file.txt" "$WORKDIR/extract/dir/file.txt"
assert_file_eq "$WORKDIR/src/dir/subdir/nested.txt" "$WORKDIR/extract/dir/subdir/nested.txt"
[ -L "$WORKDIR/extract/link-to-file" ] || fail "symlink not restored"
assert_eq "dir/file.txt" "$(readlink "$WORKDIR/extract/link-to-file")"
[ -p "$WORKDIR/extract/archive.fifo" ] || fail "fifo not restored"

src_inode=$(stat -c '%d:%i' "$WORKDIR/extract/dir/file.txt")
hard_inode=$(stat -c '%d:%i' "$WORKDIR/extract/dir/file.hard")
assert_eq "$src_inode" "$hard_inode"

mkdir "$WORKDIR/extract-meta"
(
	cd "$WORKDIR/extract-meta"
	run_notty "$PAX_BIN" -r -pe -f "$WORKDIR/archive.pax" </dev/null
)
assert_eq "1700000000" "$(stat -c %Y "$WORKDIR/extract-meta/meta.txt")"
assert_eq "6755" "$(stat -c %a "$WORKDIR/extract-meta/meta.txt")"

mkdir "$WORKDIR/extract-pea"
(
	cd "$WORKDIR/extract-pea"
	run_notty "$PAX_BIN" -r -pea -f "$WORKDIR/archive.pax" </dev/null
)
assert_eq "1700000000" "$(stat -c %Y "$WORKDIR/extract-pea/meta.txt")"

mkdir "$WORKDIR/extract-pem"
(
	cd "$WORKDIR/extract-pem"
	run_notty "$PAX_BIN" -r -pem -f "$WORKDIR/archive.pax" </dev/null
)
assert_ne "1700000000" "$(stat -c %Y "$WORKDIR/extract-pem/meta.txt")"

printf '%s\n' changed > "$WORKDIR/extract/dir/file.txt"
(
	cd "$WORKDIR/extract"
	run_notty "$PAX_BIN" -r -k -f "$WORKDIR/archive.pax" </dev/null >/dev/null
)
assert_eq "changed" "$(cat "$WORKDIR/extract/dir/file.txt")"

mkdir "$WORKDIR/copied"
(
	cd "$WORKDIR/src"
	run_notty "$PAX_BIN" -rw -l . "$WORKDIR/copied" </dev/null
)

assert_file_eq "$WORKDIR/src/dir/file.txt" "$WORKDIR/copied/dir/file.txt"
copy_inode_src=$(stat -c '%d:%i' "$WORKDIR/src/dir/file.txt")
copy_inode_dst=$(stat -c '%d:%i' "$WORKDIR/copied/dir/file.txt")
assert_eq "$copy_inode_src" "$copy_inode_dst"

mkdir "$WORKDIR/sparse-src"
printf 'A' > "$WORKDIR/sparse-src/sparse.bin"
dd if=/dev/zero of="$WORKDIR/sparse-src/sparse.bin" bs=1 count=1 seek=1048575 conv=notrunc >/dev/null 2>&1
mkdir "$WORKDIR/sparse-dst"
(
	cd "$WORKDIR/sparse-src"
	run_notty "$PAX_BIN" -rw . "$WORKDIR/sparse-dst" </dev/null
)
assert_file_eq "$WORKDIR/sparse-src/sparse.bin" "$WORKDIR/sparse-dst/sparse.bin"
src_blocks=$(stat -c %b "$WORKDIR/sparse-src/sparse.bin")
dst_blocks=$(stat -c %b "$WORKDIR/sparse-dst/sparse.bin")
[ "$dst_blocks" -le $((src_blocks + 16)) ] || fail "sparse copy expanded unexpectedly"

(
	cd "$WORKDIR/src"
	run_notty "$TAR_BIN" -cf "$WORKDIR/archive.tar" dir/file.txt dir/subdir/nested.txt </dev/null
)
tar_list=$(run_notty "$TAR_BIN" -tf "$WORKDIR/archive.tar" </dev/null)
assert_contains "dir/file.txt" "$tar_list"
assert_contains "dir/subdir/nested.txt" "$tar_list"

mkdir "$WORKDIR/tar-extract"
(
	cd "$WORKDIR/tar-extract"
	run_notty "$TAR_BIN" -xf "$WORKDIR/archive.tar" </dev/null
)
assert_file_eq "$WORKDIR/src/dir/file.txt" "$WORKDIR/tar-extract/dir/file.txt"
assert_file_eq "$WORKDIR/src/dir/subdir/nested.txt" "$WORKDIR/tar-extract/dir/subdir/nested.txt"

(
	cd "$WORKDIR/src"
	run_notty "$TAR_BIN" -zcf "$WORKDIR/archive.tar.gz" dir/file.txt dir/subdir/nested.txt </dev/null
)
gzip_tar_list=$(run_notty "$TAR_BIN" -ztf "$WORKDIR/archive.tar.gz" </dev/null)
assert_contains "dir/file.txt" "$gzip_tar_list"
mkdir "$WORKDIR/tar-gzip-extract"
(
	cd "$WORKDIR/tar-gzip-extract"
	run_notty "$TAR_BIN" -zxf "$WORKDIR/archive.tar.gz" </dev/null
)
assert_file_eq "$WORKDIR/src/dir/file.txt" "$WORKDIR/tar-gzip-extract/dir/file.txt"

(
	cd "$WORKDIR/src"
	run_notty "$TAR_BIN" -jcf "$WORKDIR/archive.tar.bz2" dir/file.txt dir/subdir/nested.txt </dev/null
)
bzip_tar_list=$(run_notty "$TAR_BIN" -jtf "$WORKDIR/archive.tar.bz2" </dev/null)
assert_contains "dir/subdir/nested.txt" "$bzip_tar_list"
mkdir "$WORKDIR/tar-bzip-extract"
(
	cd "$WORKDIR/tar-bzip-extract"
	run_notty "$TAR_BIN" -jxf "$WORKDIR/archive.tar.bz2" </dev/null
)
assert_file_eq "$WORKDIR/src/dir/subdir/nested.txt" "$WORKDIR/tar-bzip-extract/dir/subdir/nested.txt"

if ! command -v compress >/dev/null 2>&1; then
	(
		cd "$WORKDIR/src"
		run_capture "$WORKDIR/compress.status" "$WORKDIR/compress.err" \
			run_notty "$TAR_BIN" -Zcf "$WORKDIR/archive.tar.Z" dir/file.txt </dev/null
	)
	compress_err=$(cat "$WORKDIR/compress.err")
	assert_status 1 "$(cat "$WORKDIR/compress.status")"
	assert_contains "could not exec" "$compress_err"
fi

(
	cd "$WORKDIR/src"
	run_capture "$WORKDIR/legacy-tar.status" "$WORKDIR/legacy-tar.err" \
		run_notty "$TAR_BIN" -0cf "$WORKDIR/legacy.tar" dir/file.txt </dev/null
)
legacy_tar_err=$(cat "$WORKDIR/legacy-tar.err")
assert_status 1 "$(cat "$WORKDIR/legacy-tar.status")"
assert_contains "legacy BSD tape device" "$legacy_tar_err"

mkdir -p "$WORKDIR/cpio-src"
printf '%s\n' one > "$WORKDIR/cpio-src/one.txt"
ln -s "one.txt" "$WORKDIR/cpio-src/link.txt"
(
	cd "$WORKDIR/cpio-src"
	printf '%s\n' one.txt link.txt | run_notty "$CPIO_BIN" -o > "$WORKDIR/archive.cpio"
)
mkdir "$WORKDIR/cpio-dst"
cpio_stderr=$(
	cd "$WORKDIR/cpio-dst"
	run_notty "$CPIO_BIN" -id < "$WORKDIR/archive.cpio" 2>&1
)
cpio_list=$(
	run_notty "$CPIO_BIN" -it < "$WORKDIR/archive.cpio" 2>/dev/null
)
assert_file_eq "$WORKDIR/cpio-src/one.txt" "$WORKDIR/cpio-dst/one.txt"
[ -L "$WORKDIR/cpio-dst/link.txt" ] || fail "cpio symlink not restored"
assert_eq "one.txt" "$(readlink "$WORKDIR/cpio-dst/link.txt")"
assert_contains "Linux does not support symbolic-link mode changes" "$cpio_stderr"
assert_contains "one.txt" "$cpio_list"
assert_contains "link.txt" "$cpio_list"

(
	cd "$WORKDIR/cpio-src"
	printf '%s\n' one.txt | run_capture "$WORKDIR/cpio-bad.status" \
		"$WORKDIR/cpio-bad.err" run_notty "$CPIO_BIN" -o -C nope >/dev/null
)
cpio_bad_block=$(cat "$WORKDIR/cpio-bad.err")
assert_status 1 "$(cat "$WORKDIR/cpio-bad.status")"
assert_contains "Invalid cpio block size" "$cpio_bad_block"

mkdir "$WORKDIR/ustar"
x60=$(repeat_char x 60)
x92=$(repeat_char y 92)
x93=$(repeat_char q 93)
x99=$(repeat_char z 99)
x100=$(repeat_char m 100)
x101=$(repeat_char n 101)
good_path="$WORKDIR/ustar/$x60/$x92/$x99"
bad_len_path="$WORKDIR/ustar/$x60/$x93/$x99"
bad_component_path="$WORKDIR/ustar/$x60/$x101"
mkdir -p "$(dirname "$good_path")" "$(dirname "$bad_len_path")" "$(dirname "$bad_component_path")"
printf '%s\n' good > "$good_path"
printf '%s\n' bad > "$bad_len_path"
printf '%s\n' bad > "$bad_component_path"

(
	cd "$WORKDIR/ustar"
	run_notty "$PAX_BIN" -w -x ustar -f "$WORKDIR/ustar-good.tar" "./$x60/$x92/$x99" </dev/null
)
mkdir "$WORKDIR/ustar-extract"
(
	cd "$WORKDIR/ustar-extract"
	run_notty "$PAX_BIN" -r -f "$WORKDIR/ustar-good.tar" </dev/null
)
assert_file_eq "$good_path" "$WORKDIR/ustar-extract/$x60/$x92/$x99"

(
	cd "$WORKDIR/ustar"
	run_capture "$WORKDIR/ustar-len.status" "$WORKDIR/ustar-len.err" \
		run_notty "$PAX_BIN" -w -x ustar -f "$WORKDIR/ustar-bad-len.tar" "./$x60/$x93/$x99" </dev/null
)
ustar_len_err=$(cat "$WORKDIR/ustar-len.err")
assert_status 1 "$(cat "$WORKDIR/ustar-len.status")"
assert_contains "too long for ustar" "$ustar_len_err"

(
	cd "$WORKDIR/ustar"
	run_capture "$WORKDIR/ustar-component.status" "$WORKDIR/ustar-component.err" \
		run_notty "$PAX_BIN" -w -x ustar -f "$WORKDIR/ustar-bad-component.tar" "./$x60/$x101" </dev/null
)
ustar_component_err=$(cat "$WORKDIR/ustar-component.err")
assert_status 1 "$(cat "$WORKDIR/ustar-component.status")"
assert_contains "too long for ustar" "$ustar_component_err"

mkdir "$WORKDIR/large"
printf 'L' > "$WORKDIR/large/huge.bin"
dd if=/dev/zero of="$WORKDIR/large/huge.bin" bs=1 count=1 seek=$((8 * 1024 * 1024 * 1024)) conv=notrunc >/dev/null 2>&1
(
	cd "$WORKDIR/large"
	run_capture "$WORKDIR/large-ustar.status" "$WORKDIR/large-ustar.err" \
		run_notty "$PAX_BIN" -w -x ustar -f "$WORKDIR/large-ustar.tar" huge.bin </dev/null
)
large_ustar_err=$(cat "$WORKDIR/large-ustar.err")
assert_status 1 "$(cat "$WORKDIR/large-ustar.status")"
assert_contains "File is too long for ustar" "$large_ustar_err"

if [ "$(id -u)" -eq 0 ]; then
	root_uid=$(awk -F: '$3 != 0 { print $3; exit }' /etc/passwd)
	root_gid=$(awk -F: '$3 != 0 { print $3; exit }' /etc/group)
	if [ -n "${root_uid:-}" ] && [ -n "${root_gid:-}" ]; then
		printf '%s\n' owner > "$WORKDIR/src/owned.txt"
		chown "$root_uid:$root_gid" "$WORKDIR/src/owned.txt"
		(
			cd "$WORKDIR/src"
			run_notty "$PAX_BIN" -w -f "$WORKDIR/owned.pax" owned.txt </dev/null
		)
		mkdir "$WORKDIR/owned-extract"
		(
			cd "$WORKDIR/owned-extract"
			run_notty "$PAX_BIN" -r -pe -f "$WORKDIR/owned.pax" owned.txt </dev/null
		)
		assert_eq "$root_uid:$root_gid" "$(stat -c '%u:%g' "$WORKDIR/owned-extract/owned.txt")"
	fi
elif can_run_userns_root; then
	run_capture "$WORKDIR/userns.status" "$WORKDIR/userns.err" \
		unshare --user --map-root-user -- sh -eu -c '
			work=$1
			pax_bin=$2
			mkdir -p "$work/userns-src"
			printf "%s\n" owner > "$work/userns-src/owned.txt"
			chown 0:0 "$work/userns-src/owned.txt"
			(
				cd "$work/userns-src"
				"$pax_bin" -w -f "$work/userns.pax" owned.txt </dev/null
			)
			mkdir "$work/userns-dst"
			(
				cd "$work/userns-dst"
				"$pax_bin" -r -pe -f "$work/userns.pax" </dev/null
			)
			test "$(stat -c "%u:%g" "$work/userns-dst/owned.txt")" = "0:0"
		' sh "$WORKDIR" "$PAX_BIN"
	assert_status 0 "$(cat "$WORKDIR/userns.status")"
fi

if command -v setfattr >/dev/null 2>&1; then
	printf '%s\n' xattr > "$WORKDIR/src/xattr.txt"
	if setfattr -n user.pax_test -v present "$WORKDIR/src/xattr.txt" 2>/dev/null; then
		(
			cd "$WORKDIR/src"
			run_notty "$PAX_BIN" -w -x ustar -f "$WORKDIR/xattr.pax" xattr.txt </dev/null
		)
		mkdir "$WORKDIR/xattr-extract"
		(
			cd "$WORKDIR/xattr-extract"
			run_notty "$PAX_BIN" -r -pe -f "$WORKDIR/xattr.pax" </dev/null
		)
		assert_eq "present" "$(getfattr --only-values -n user.pax_test "$WORKDIR/xattr-extract/xattr.txt" 2>/dev/null)"

		mkdir "$WORKDIR/xattr-copy"
		(
			cd "$WORKDIR/src"
			run_notty "$PAX_BIN" -rw -pe xattr.txt "$WORKDIR/xattr-copy" </dev/null
		)
		assert_eq "present" "$(getfattr --only-values -n user.pax_test "$WORKDIR/xattr-copy/xattr.txt" 2>/dev/null)"

		(
			cd "$WORKDIR/src"
			run_capture "$WORKDIR/xattr-oldtar.status" "$WORKDIR/xattr-oldtar.err" \
				run_notty "$PAX_BIN" -w -x tar -f "$WORKDIR/xattr-oldtar.tar" xattr.txt </dev/null
		)
		assert_status 1 "$(cat "$WORKDIR/xattr-oldtar.status")"
		assert_contains "cannot preserve Linux extended attributes" "$(cat "$WORKDIR/xattr-oldtar.err")"
	fi
fi

if command -v setfacl >/dev/null 2>&1; then
	acl_user=$(awk -F: -v self="$(id -un)" '$1 != self { print $1; exit }' /etc/passwd)
	if [ -n "${acl_user:-}" ]; then
		printf '%s\n' acl > "$WORKDIR/src/acl.txt"
		if setfacl -m "u:$acl_user:r--" "$WORKDIR/src/acl.txt" 2>/dev/null; then
			(
				cd "$WORKDIR/src"
				run_notty "$PAX_BIN" -w -x ustar -f "$WORKDIR/acl.pax" acl.txt </dev/null
			)
			mkdir "$WORKDIR/acl-extract"
			(
				cd "$WORKDIR/acl-extract"
				run_notty "$PAX_BIN" -r -pe -f "$WORKDIR/acl.pax" </dev/null
			)
			assert_contains "user:$acl_user:r--" "$(getfacl -cp "$WORKDIR/acl-extract/acl.txt" 2>/dev/null)"
		fi
	fi
fi

if command -v selinuxenabled >/dev/null 2>&1 && command -v getfattr >/dev/null 2>&1 \
    && command -v chcon >/dev/null 2>&1 && selinuxenabled; then
	printf '%s\n' selinux > "$WORKDIR/src/selinux.txt"
	if chcon -t user_tmp_t "$WORKDIR/src/selinux.txt" 2>/dev/null || \
	    chcon -t tmp_t "$WORKDIR/src/selinux.txt" 2>/dev/null; then
		if getfattr --only-values -n security.selinux "$WORKDIR/src/selinux.txt" >"$WORKDIR/src-selinux.label" 2>/dev/null \
		    && [ -s "$WORKDIR/src-selinux.label" ]; then
			(
				cd "$WORKDIR/src"
				run_notty "$PAX_BIN" -w -x ustar -f "$WORKDIR/selinux.pax" selinux.txt </dev/null
			)
			mkdir "$WORKDIR/selinux-extract"
			(
				cd "$WORKDIR/selinux-extract"
				run_notty "$PAX_BIN" -r -pe -f "$WORKDIR/selinux.pax" </dev/null
			)
			getfattr --only-values -n security.selinux "$WORKDIR/selinux-extract/selinux.txt" >"$WORKDIR/dst-selinux.label" 2>/dev/null
			assert_file_eq "$WORKDIR/src-selinux.label" "$WORKDIR/dst-selinux.label"
		fi
	fi
fi

if can_run_script_pty; then
	python3 - <<'PY' "$WORKDIR/multi-src.bin"
import sys
with open(sys.argv[1], 'wb') as f:
    f.write((b'abcdef0123456789' * 1024))
PY
	: > "$WORKDIR/multi-volumes.in"
	n=2
	while [ "$n" -le 10 ]; do
		printf '%s\n' "$WORKDIR/vol$n.tar" >> "$WORKDIR/multi-volumes.in"
		n=$((n + 1))
	done
	printf '.\n' >> "$WORKDIR/multi-volumes.in"
	multi_write_cmd="cd '$WORKDIR' && '$PAX_BIN' -w -x tar -b 1024 -B 1024 -f '$WORKDIR/vol1.tar' multi-src.bin"
	script -qec "$multi_write_cmd" /dev/null < "$WORKDIR/multi-volumes.in" >"$WORKDIR/multi-write.log" 2>&1
	assert_contains "archive volume change required" "$(cat "$WORKDIR/multi-write.log")"
	[ -s "$WORKDIR/vol2.tar" ] || fail "second multi-volume archive was not created"
	mkdir "$WORKDIR/multi-extract"
	multi_read_cmd="cd '$WORKDIR/multi-extract' && '$PAX_BIN' -r -f '$WORKDIR/vol1.tar'"
	script -qec "$multi_read_cmd" /dev/null < "$WORKDIR/multi-volumes.in" >"$WORKDIR/multi-read.log" 2>&1
	assert_contains "End of archive volume 1 reached" "$(cat "$WORKDIR/multi-read.log")"
	assert_file_eq "$WORKDIR/multi-src.bin" "$WORKDIR/multi-extract/multi-src.bin"
fi

if can_run_script_pty && [ -c /dev/full ]; then
	printf '%s\n' char-volume > "$WORKDIR/char-src.bin"
	printf '%s\n' "$WORKDIR/char-vol2.tar" > "$WORKDIR/char-volumes.in"
	char_write_cmd="cd '$WORKDIR' && '$PAX_BIN' -w -x tar -b 1024 -B 1024 -f /dev/full char-src.bin"
	script -qec "$char_write_cmd" /dev/null < "$WORKDIR/char-volumes.in" >"$WORKDIR/char-write.log" 2>&1
	assert_contains "archive volume change required" "$(cat "$WORKDIR/char-write.log")"
	[ -s "$WORKDIR/char-vol2.tar" ] || fail "char-device next archive volume was not created"
	mkdir "$WORKDIR/char-extract"
	(
		cd "$WORKDIR/char-extract"
		run_notty "$PAX_BIN" -r -f "$WORKDIR/char-vol2.tar" </dev/null
	)
	assert_file_eq "$WORKDIR/char-src.bin" "$WORKDIR/char-extract/char-src.bin"
fi

# ---------------------------------------------------------------------------
# Hardening tests for pax extended-attribute parsing
# ---------------------------------------------------------------------------
if command -v python3 >/dev/null 2>&1; then
	printf 'Running hardening tests...\n'

	# Test 1: invalid base64 value → parser must reject
	python3 -c "
import tarfile, io
buf = io.BytesIO()
with tarfile.open(fileobj=buf, mode='w', format=tarfile.PAX_FORMAT) as t:
    ti = tarfile.TarInfo('dummy.txt'); ti.size=4
    ti.pax_headers = {'LIBARCHIVE.xattr.user.test': 'dmF'}
    t.addfile(ti, io.BytesIO(b'data'))
with open('$WORKDIR/harden_b64.tar', 'wb') as f: f.write(buf.getvalue())
"
	run_capture "$WORKDIR/hb64.status" "$WORKDIR/hb64.err" \
		"$PAX_BIN" -f "$WORKDIR/harden_b64.tar"
	assert_contains "Invalid Linux extended-attribute pax header" \
		"$(cat "$WORKDIR/hb64.err")"

	# Test 2: invalid URL-encoded key name → parser must reject
	python3 -c "
import tarfile, io
buf = io.BytesIO()
with tarfile.open(fileobj=buf, mode='w', format=tarfile.PAX_FORMAT) as t:
    ti = tarfile.TarInfo('dummy.txt'); ti.size=4
    ti.pax_headers = {'LIBARCHIVE.xattr.user.goodname': 'dmFs'}
    t.addfile(ti, io.BytesIO(b'data'))
raw = bytearray(buf.getvalue())
idx = raw.find(b'goodname')
if idx != -1:
    raw[idx:idx+8] = b'%ZZname\x00'
with open('$WORKDIR/harden_url.tar', 'wb') as f: f.write(raw)
"
	run_capture "$WORKDIR/hurl.status" "$WORKDIR/hurl.err" \
		"$PAX_BIN" -f "$WORKDIR/harden_url.tar"
	assert_contains "Invalid Linux extended-attribute pax header" \
		"$(cat "$WORKDIR/hurl.err")"
fi

printf '%s\n' "PASS"
