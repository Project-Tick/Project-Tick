# cp

Standalone musl-libc-based Linux port of FreeBSD `cp` for Project Tick BSD/Linux Distribution.

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

- Port strategy is minimum-diff: the original `cp.c` / `utils.c` flow is kept as much as possible.
- `fts(3)` is provided locally in this project because musl does not ship `<fts.h>`.
- FreeBSD-only ACL and file flag preservation paths are disabled on non-FreeBSD builds.
