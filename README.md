# mv

Standalone Linux-native port of FreeBSD `mv` for Project Tick BSD/Linux Distribution.

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

- Port structure follows the standalone sibling ports such as `bin/ln`: local `GNUmakefile`, local README, and self-contained shell tests.
- The FreeBSD `mv.c` implementation was rewritten into a Linux-native utility instead of carrying BSD libc helpers such as `err(3)`, `strmode(3)`, `statfs(2)`, `PATH_MAX`-bounded path assembly, or `cp`/`rm` subprocess fallback.
- Fast-path moves map directly to Linux `rename(2)`.
- Cross-filesystem moves map to native recursive copy/remove logic:
  - regular files: `open(2)` + `read(2)` + `write(2)`
  - directories: `mkdir(2)` + `opendir(3)`/`readdir(3)` recursion
  - symlinks: `readlink(2)` + `symlink(2)`
  - FIFOs: `mkfifo(2)`
  - device nodes: `mknod(2)`
  - metadata: `fchown(2)`/`fchmod(2)`/`utimensat(2)`/`futimens(2)`
  - Linux xattrs and ACL xattrs: `listxattr(2)`/`getxattr(2)`/`setxattr(2)` and their `l*`/`f*` variants
- Target path construction is dynamic; the port does not depend on `PATH_MAX`, `realpath(3)` canonicalization, or BSD-only libc APIs.

## Supported / Unsupported Semantics

- Supported: `mv [-f | -i | -n] [-hv] source target`
- Supported: `mv [-f | -i | -n] [-v] source ... directory`
- Supported: same-filesystem rename, cross-filesystem moves for regular files, directories, symlinks, FIFOs, and device nodes.
- Supported: `-h` semantics from `mv.1` for replacing a symbolic link to a directory in the two-operand form.
- Supported: Linux xattr-preserving cross-filesystem moves when both source and destination filesystems support the relevant xattrs.
- Supported and tested: partial regular-file copy failures clean up the destination file instead of leaving a truncated target behind.
- Unsupported by design: GNU long options and GNU option permutation.
- Unsupported on Linux and rejected explicitly during cross-filesystem fallback: socket files.
- Unsupported as a compatibility shim: FreeBSD file flags / `chflags(2)` semantics. Linux has no direct equivalent here.
- Unsupported and rejected explicitly during cross-filesystem fallback: moving a mount point by copy/remove. The port refuses that case instead of partially copying mounted content and failing later.
- Cross-filesystem fallback does not preserve hardlink graphs; linked inputs become distinct copies at the destination. That matches the current recursive copy strategy and is covered by tests.
- Not atomic across filesystems: the implementation follows `mv.1`'s documented copy-then-remove semantics, so a failed cross-filesystem move can leave the destination partially created or the source still present.
