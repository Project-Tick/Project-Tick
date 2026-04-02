# cpuset

Standalone musl-libc-based Linux port of FreeBSD `cpuset` for Project Tick BSD/Linux Distribution.

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

- Port strategy is syscall translation, not a wrapper around the FreeBSD userland ABI.
- Linux-native affinity operations are mapped to `sched_getaffinity(2)` and `sched_setaffinity(2)`.
- The Linux build preserves `-g`, `-l`, `-p`, `-t`, and command execution with an affinity mask.
- FreeBSD-specific cpuset IDs, jail/irq targets, and NUMA domain policy flags have no direct Linux syscall equivalent in this port and currently return an explicit error.
