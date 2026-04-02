#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SYNC_BIN=${SYNC_BIN:-"$ROOT/out/sync"}
CC=${CC:-cc}
TMPDIR=${TMPDIR:-/tmp}
WORKDIR=$(mktemp -d "$TMPDIR/sync-test.XXXXXX")
HOOK_C="$WORKDIR/sync-hook.c"
HOOK_SO="$WORKDIR/sync-hook.so"
STDOUT_FILE="$WORKDIR/stdout"
STDERR_FILE="$WORKDIR/stderr"
SYNC_MARKER="$WORKDIR/sync-marker"
trap 'rm -rf "$WORKDIR"' EXIT INT TERM

export LC_ALL=C

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

assert_empty() {
	name=$1
	text=$2
	if [ -n "$text" ]; then
		printf '%s\n' "FAIL: $name" >&2
		printf '%s\n' "--- expected empty ---" >&2
		printf '%s\n' "--- actual ---" >&2
		printf '%s' "$text" >&2
		printf '\n' >&2
		exit 1
	fi
}

assert_status() {
	name=$1
	expected=$2
	actual=$3
	if [ "$expected" -ne "$actual" ]; then
		printf '%s\n' "FAIL: $name" >&2
		printf '%s\n' "expected status: $expected" >&2
		printf '%s\n' "actual status: $actual" >&2
		exit 1
	fi
}

run_capture() {
	if "$@" >"$STDOUT_FILE" 2>"$STDERR_FILE"; then
		LAST_STATUS=0
	else
		LAST_STATUS=$?
	fi

	LAST_STDOUT=$(cat "$STDOUT_FILE")
	LAST_STDERR=$(cat "$STDERR_FILE")
}

build_sync_hook() {
	cat >"$HOOK_C" <<'EOF'
#define _GNU_SOURCE 1

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

typedef void (*sync_fn_t)(void);

static void
write_marker(const char *path)
{
	static const char marker[] = "sync-called\n";
	size_t offset;
	int fd;

	if (path == NULL || path[0] == '\0')
		return;

	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
	if (fd < 0)
		return;

	offset = 0;
	while (offset < sizeof(marker) - 1) {
		ssize_t written;

		written = write(fd, marker + offset, sizeof(marker) - 1 - offset);
		if (written < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		offset += (size_t)written;
	}

	(void)close(fd);
}

void
sync(void)
{
	static sync_fn_t real_sync;
	static int resolved;

	if (!resolved) {
		real_sync = (sync_fn_t)dlsym(RTLD_NEXT, "sync");
		resolved = 1;
	}

	write_marker(getenv("SYNC_HOOK_OUTPUT"));
	if (real_sync != NULL)
		real_sync();
}
EOF

	"$CC" -shared -fPIC -O2 -std=c17 -Wall -Wextra -Werror \
		"$HOOK_C" -o "$HOOK_SO" -ldl || fail "failed to build sync hook with $CC"
}

[ -x "$SYNC_BIN" ] || fail "missing binary: $SYNC_BIN"
build_sync_hook

run_capture "$SYNC_BIN"
assert_status "sync status" 0 "$LAST_STATUS"
assert_empty "sync stdout" "$LAST_STDOUT"
assert_empty "sync stderr" "$LAST_STDERR"

run_capture "$SYNC_BIN" unexpected
assert_status "operand status" 1 "$LAST_STATUS"
assert_empty "operand stdout" "$LAST_STDOUT"
assert_eq "operand stderr" \
	"sync: unexpected argument: unexpected
usage: sync" \
	"$LAST_STDERR"

run_capture "$SYNC_BIN" --
assert_status "double dash status" 1 "$LAST_STATUS"
assert_empty "double dash stdout" "$LAST_STDOUT"
assert_eq "double dash stderr" \
	"sync: unexpected argument: --
usage: sync" \
	"$LAST_STDERR"

run_capture "$SYNC_BIN" first second
assert_status "too many args status" 1 "$LAST_STATUS"
assert_empty "too many args stdout" "$LAST_STDOUT"
assert_eq "too many args stderr" \
	"sync: unexpected argument: first
usage: sync" \
	"$LAST_STDERR"

rm -f "$SYNC_MARKER"
run_capture env LD_PRELOAD="$HOOK_SO" SYNC_HOOK_OUTPUT="$SYNC_MARKER" "$SYNC_BIN"
assert_status "hooked sync status" 0 "$LAST_STATUS"
assert_empty "hooked sync stdout" "$LAST_STDOUT"
assert_empty "hooked sync stderr" "$LAST_STDERR"
[ -f "$SYNC_MARKER" ] || fail "sync() was not invoked"
assert_eq "hook marker" "sync-called" "$(cat "$SYNC_MARKER")"

rm -f "$SYNC_MARKER"
run_capture env LD_PRELOAD="$HOOK_SO" SYNC_HOOK_OUTPUT="$SYNC_MARKER" \
	"$SYNC_BIN" unexpected
assert_status "hooked invalid status" 1 "$LAST_STATUS"
assert_empty "hooked invalid stdout" "$LAST_STDOUT"
assert_eq "hooked invalid stderr" \
	"sync: unexpected argument: unexpected
usage: sync" \
	"$LAST_STDERR"
[ ! -e "$SYNC_MARKER" ] || fail "sync() ran on invalid arguments"

printf '%s\n' "PASS"
