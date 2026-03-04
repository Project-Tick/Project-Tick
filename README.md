# sync

Standalone musl-libc-friendly Linux port of FreeBSD `sync` for Project Tick BSD/Linux Distribution.

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

- Port strategy is direct Linux-native syscall/API mapping, not preservation of FreeBSD build glue or a BSD compatibility shim.
- FreeBSD `sync(8)` is specified as a no-argument whole-system flush and is mapped directly to Linux `sync(2)`.
- GNU/coreutils-style operands and options such as `sync FILE...`, `-d`, and `-f` are intentionally unsupported. `sync.8` does not define them, and file-scoped flushing would require `fsync(2)` or `syncfs(2)` on explicit file descriptors, which is not equivalent to the documented utility semantics.
- On Linux, as on FreeBSD, `sync(2)` does not provide per-file error reporting to this utility. Successful execution therefore means the process reached and invoked the kernel-wide flush entry point.
