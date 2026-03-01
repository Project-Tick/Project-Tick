# nproc

Standalone musl-libc-friendly Linux port of FreeBSD `nproc` for Project Tick BSD/Linux Distribution.

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

- Port strategy is Linux-native API translation, not preservation of FreeBSD `cpuset(2)` ABI.
- Default output is mapped to `sched_getaffinity(2)` for the calling thread and counts the CPUs in the returned affinity mask.
- `--all` is mapped to `sysconf(_SC_NPROCESSORS_ONLN)` because `nproc.1` defines it as the number of processors currently online.
- `--ignore=count` is parsed strictly as an unsigned decimal integer and the result is clamped to a minimum of `1`, matching the documented semantics.
- The affinity mask buffer grows dynamically until `sched_getaffinity(2)` accepts it, so the port does not silently truncate systems with more CPUs than a fixed `cpu_set_t` can describe.
- Intentionally unsupported semantic: Linux CPU quota throttling such as cgroup `cpu.max` is not converted into fractional processor counts. This port reports schedulable CPUs from affinity and the online CPU count because that is the kernel API described by the manpage.
