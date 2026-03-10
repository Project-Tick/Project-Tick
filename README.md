# uuidgen

Standalone Linux-native port of FreeBSD `uuidgen` for Project Tick BSD/Linux Distribution.

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

- Port structure follows sibling standalone ports (`bin/hostname`, `bin/domainname`, `bin/timeout`): local `GNUmakefile`, technical `README.md`, shell tests under `tests/`.
- FreeBSD-only layers (`capsicum`, `err(3)`, `uuidgen(2)`, `uuid(3)` string helpers) were removed.
- Linux-native RFC 4122 generation is implemented directly in-tree, with strict argument parsing and explicit diagnostics.
- No GNU feature macros are enabled; the code is written for Linux + musl without glibc-specific wrappers.

## Linux API Mapping

- FreeBSD `uuidgen(2)` / `uuidgen(3)` generation maps to an in-process RFC 4122 implementation:
  - Version 1 UUIDs: `clock_gettime(CLOCK_REALTIME)` timestamp source + in-process clock-sequence logic + random multicast node identifier.
  - Version 4 UUIDs (`-r`): direct Linux `SYS_getrandom` syscall entropy with `/dev/urandom` fallback when unavailable (`ENOSYS`).
- FreeBSD `uuid_to_string(3)` and compact formatting helper map to direct lowercase textual formatting in this utility.
- FreeBSD Capsicum confinement is intentionally not emulated on Linux.

## Supported Linux Semantics

- `uuidgen [-1] [-r] [-c] [-n count] [-o filename]` from `uuidgen.1`.
- Default output is one RFC 4122 version 1 UUID.
- `-n count` supports strict range `1..2048` (matching `uuidgen.1` hard limit).
- Batched version 1 generation (`-n` without `-1`) produces dense consecutive timestamp ticks.
- `-1` generation mode emits UUIDs one-at-a-time (timestamp still strictly monotonic).
- `-c` outputs compact UUIDs without hyphens.

## Intentionally Unsupported / Different

- Cross-process global ordering identical to FreeBSD kernel `uuidgen(2)` is not provided; ordering guarantees are process-local.
- Version 1 node identity is randomized per process (multicast bit set) instead of exposing hardware MAC-derived identifiers.
