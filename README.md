# chflags

Standalone musl-libc-based Linux port of FreeBSD `chflags` for Project Tick BSD/Linux Distribution.

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

## Notes

- Port strategy is minimum-diff: the original traversal and CLI flow stay in `chflags.c`.
- `fts(3)` is provided locally in this project because musl does not ship `<fts.h>`.
- Linux flag handling is implemented with `FS_IOC_GETFLAGS` and `FS_IOC_SETFLAGS`.
- Only the Linux-mappable subset of FreeBSD flag names is supported in this phase.
