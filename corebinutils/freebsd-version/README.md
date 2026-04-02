# linux-version

Standalone shell-based Linux port of FreeBSD `freebsd-version`, renamed to `linux-version` for this repository.

## Build

```sh
gmake -f GNUmakefile
gmake -f GNUmakefile CC=musl-gcc
```

This port is implemented as a POSIX `sh` script, so the `CC=musl-gcc` build is a reproducibility check rather than a separate compilation path.

## Test

```sh
gmake -f GNUmakefile test
gmake -f GNUmakefile test CC=musl-gcc
```

## Notes

- Port strategy is Linux-native translation, not preservation of FreeBSD loader, `sysctl`, or jail semantics.
- `-r` reads the running kernel release from `/proc/sys/kernel/osrelease`, with `uname -r` only as a fallback when procfs is unavailable.
- `-u` reads the installed userland version from the target root's `os-release` file. The port prefers `VERSION_ID`, then falls back to `BUILD_ID`, `VERSION`, and `PRETTY_NAME`.
- `-k` does not inspect a FreeBSD kernel binary. On Linux it resolves the default booted kernel via `/vmlinuz` or `/boot/vmlinuz` symlinks when available, then falls back to a unique kernel release under `/lib/modules` or `/usr/lib/modules`.
- `ROOT=/path` is supported for offline inspection of another Linux filesystem tree for `-k` and `-u`.
- Unsupported semantics are explicit: `-j` is rejected because Linux containers and namespaces do not provide a jail-equivalent installed-userland query with compatible semantics.
- The port does not guess when several kernel module trees exist without a default boot symlink; it fails with an explicit ambiguity error instead.
