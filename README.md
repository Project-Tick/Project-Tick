# setfacl

Standalone musl-libc-friendly Linux port of FreeBSD `setfacl` for Project Tick BSD/Linux Distribution.

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

## Port strategy

- FreeBSD `sys/acl.h`, `acl_*_np()` helpers, and `fts(3)` traversal were removed.
- Linux access/default ACLs are decoded from and encoded to `system.posix_acl_access` / `system.posix_acl_default` with `getxattr(2)`, `setxattr(2)`, `removexattr(2)`, and their `l*` variants.
- Access ACLs without an xattr start from file mode bits; default ACLs do not get synthesized, matching the FreeBSD manpage expectation that callers create mandatory default entries explicitly.
- Base-only access ACL updates are applied with `chmod(2)` plus access-xattr cleanup so trivial ACL operations still work cleanly on filesystems that have no POSIX ACL xattr.
- Recursive walking is Linux-native `opendir(3)` / `readdir(3)` based and implements `-P`, `-L`, and `-H` without relying on GNU or glibc-only traversal APIs.

## Supported semantics on Linux

- POSIX access ACL modify/remove operations with `-m`, `-M`, `-x`, `-X`, `-b`, `-k`, `-d`, `-n`, `-R`, `-H`, `-L`, `-P`, and `-h`.
- `-M -` / `-X -` ACL files with comments and whitespace ignored per `setfacl(1)`.
- Path operands from standard input when no path operands are given or the sole operand is `-`.
- Explicit mask handling and recalculation that follows the FreeBSD manpage ordering model instead of silently deferring kernel-side failures.

## Unsupported semantics on Linux

- `-a` and NFSv4 ACL entry syntax (`owner@`, `allow`, inheritance flags, and similar) fail with an explicit error because this port targets Linux POSIX ACLs only.
- `-h` on a symbolic link fails with an explicit error because Linux does not support POSIX ACLs on symlink inodes.
- Creating a default ACL from only named entries is rejected; callers must provide the mandatory `user::`, `group::`, and `other::` entries, as documented in `setfacl.1`.

## Notes

- The parser is strict: malformed entries, duplicate ACL members, missing required entries, and impossible `-n` mask states are rejected before any file is modified.
- Verified with `gmake -f GNUmakefile clean test` and `gmake -f GNUmakefile clean test CC=musl-gcc`.
