# dd

Standalone musl-libc-based Linux port of FreeBSD `dd` for Project Tick BSD/Linux Distribution.

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

- Port structure follows the existing standalone sibling ports: local `GNUmakefile`, short technical `README.md`, and shell tests under `tests/`.
- Port strategy is Linux-native API mapping, not a BSD compatibility shim. The mature FreeBSD copy/conversion core is kept, but FreeBSD-only process isolation and device typing paths are removed or replaced.
- Input/output typing uses Linux `fstat(2)`, `lseek(2)`, and `MTIOCGET`/`MTIOCTOP` from `sys/mtio.h`. `files=` remains available only for real tape devices; on non-tape inputs it fails explicitly.
- FreeBSD `SIGINFO` status requests map to Linux `SIGUSR1`. Periodic `status=progress` updates continue to use `setitimer(2)`/`SIGALRM`.
- `iflag=direct` and `oflag=direct` use Linux `O_DIRECT`; buffers are allocated with page-aligned `posix_memalign(3)`. Linux still enforces filesystem/device-specific alignment for block size and offsets, so invalid direct-I/O combinations fail with the kernel error instead of being silently emulated.
- `oflag=sync`/`oflag=fsync` use Linux `O_SYNC` for named outputs and an explicit `fsync(2)` after each completed write so inherited descriptors such as stdout are not silently downgraded.
- `conv=fdatasync` and `conv=fsync` map to `fdatasync(2)` and `fsync(2)` at close, matching Linux-native durability APIs.
- Unsupported semantics are explicit rather than stubbed: FreeBSD Capsicum/Casper integration is removed, non-tape `files=` is rejected, and `seek=` on a non-seekable non-tape output returns a clear error instead of taking a fake compatibility path.
