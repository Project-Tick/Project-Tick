# pax

Standalone musl-libc-based Linux port of FreeBSD `pax` for Project Tick BSD/Linux Distribution.

## Build

```sh
gmake -f GNUmakefile
gmake -f GNUmakefile CC=musl-gcc
```

## Test

```sh
gmake -f GNUmakefile test
gmake -f GNUmakefile test CC=musl-gcc
```

## Port Strategy

- Port structure follows the existing standalone sibling ports: local `GNUmakefile`, short technical `README.md`, and shell tests under `tests/`.
- The FreeBSD archive core is kept, but Linux-only API mapping is done directly in the code instead of adding a generic BSD compatibility layer.
- Traversal uses the same in-tree `fts(3)` implementation already used by sibling Linux ports because musl does not provide `<fts.h>`.
- FreeBSD libc-only pieces were removed or replaced locally: `fgetln(3)` was rewritten to `getline(3)`, `lutimes(2)` to `utimensat(2)` with `AT_SYMLINK_NOFOLLOW`, and `strmode(3)` / `strlcpy(3)` are provided in-tree for musl-clean builds.
- `pax.1` remains the contract for supported CLI semantics. Where Linux cannot represent the original behavior exactly, the port emits an explicit warning instead of silently pretending success.

## Linux Semantics

Supported mappings:

- Archive and copy traversal use POSIX/Linux file APIs and the in-tree `fts(3)` implementation.
- File timestamp restoration uses `utimensat(2)` with `AT_SYMLINK_NOFOLLOW`.
- Ownership restoration uses `lchown(2)`.
- Device nodes and FIFOs use Linux `mknod(2)` / `mkfifo(3)` when permitted by the caller and kernel.
- Numeric size options now use strict overflow-checked parsing for `pax -b`, `tar -b`, and `cpio -C`.
- Linux extended attributes, POSIX ACLs, and `security.selinux` labels are captured with `llistxattr(2)` / `lgetxattr(2)`, serialized into POSIX pax extended headers on `ustar` archives, and restored with `lsetxattr(2)` when extraction/copy runs with mode-preservation semantics (`-p p` or `-p e`).
- `gzip` and `bzip2` helper-based archive flows are supported and their helper exit status is now propagated back to `pax`.
- Linux character-device archives, including tape nodes opened via `-f`, are handled with the generic sequential-device path instead of BSD tape ioctl assumptions. Multi-volume archive continuation to a new path is supported for both regular files and character devices.

Intentionally unsupported or reduced:

- Linux does not support symbolic-link mode changes. Permission restoration on symlinks is skipped with an explicit warning.
- `tar` legacy device shorthands `-0`, `-1`, `-4`, `-5`, `-7`, and `-8` are FreeBSD tape shortcuts and fail explicitly on Linux; use `-f`.
- Legacy `tar` format and the `cpio` family do not have a compatible metadata carrier for Linux xattrs/ACLs/SELinux labels in this port. Files with non-SELinux xattrs are rejected explicitly in those formats instead of being archived with silent metadata loss.
- Ambient `security.selinux` labels on unlabeled formats are warned about once and dropped so ordinary `tar`/`cpio` usage on labeled systems remains usable.
- `compress`/`.Z` mode depends on an external `compress` helper. If the helper is missing or fails, the port now exits non-zero instead of silently succeeding.

Test scope:

- The shell suite fixes `LC_ALL=C` and `TZ=UTC`, runs without a controlling TTY when possible, and asserts exit status as well as output for usage failures, missing archives, strict parse failures, Linux-specific unsupported tar device shortcuts, metadata-format rejection on old tar/cpio, and ustar boundary failures.
- Archive/extract coverage includes round-trips for regular files, hard links, symlinks, FIFOs, tar/cpio personalities, gzip and bzip2 helper flows, `-k` no-clobber behavior, `-rw -l` hard-link copy, sparse-file `-rw` copy, regular-file mode restoration including setuid/setgid bits, selective timestamp restoration with `-pea` / `-pem`, `user.*` xattr round-trips, POSIX ACL round-trips, and SELinux label round-trips on hosts that expose them.
- Ownership restoration is exercised either directly as uid `0` or inside a root-mapped user namespace when the host permits it. Large-file coverage uses a sparse file beyond the 8 GiB ustar size boundary to assert the Linux port fails cleanly before writing an invalid archive.
- When a pseudo-terminal is available, the suite drives both regular-file and character-device multi-volume archive continuation, plus a real multi-volume write/read round-trip. It also asserts that missing `compress` helpers return non-zero.
