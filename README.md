# ps

Standalone Linux-native port of FreeBSD `ps` for Project Tick BSD/Linux Distribution.

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

| BSD mechanism | Linux replacement |
|---|---|
| `kvm_openfiles(3)` / `kvm_getprocs(3)` | `/proc` directory enumeration |
| `struct kinfo_proc` | `/proc/[pid]/stat` + `/proc/[pid]/status` + `/proc/[pid]/cmdline` + `/proc/[pid]/wchan` |
| `sysctl(3)` / `nlist(3)` | `/proc/meminfo` (for physical memory stats) |
| `libxo` | Standard `printf` output (libxo not available on target Linux-musl) |
| `vis(3)` / `strvis(3)` | Simplified visual encoding for non-printable characters |
| `SLIST_*` / `STAILQ_*` (BSD queue macros) | Kept or replaced with standard C logic |

### /proc field mapping

- `pid`, `ppid`, `pgrp`, `sid`, `tdev`: parsed from `/proc/[pid]/stat` fields
- `ruid`, `euid`, `rgid`, `egid`: parsed from `/proc/[pid]/status` (`Uid:`/`Gid:` lines)
- `cpu-time`: `utime` + `stime` from `/proc/[pid]/stat` (fields 14 and 15)
- `percent-cpu`: calculated from `cpu-time` and process uptime (from jiffies)
- `virtual-size`: field 23 of `/proc/[pid]/stat` (in bytes)
- `rss`: field 24 of `/proc/[pid]/stat` (in pages) * pagesize
- `state`: field 3 of `/proc/[pid]/stat` (mapped from Linux to BSD-like characters)
- `wchan`: read from `/proc/[pid]/wchan`

## Supported Semantics (subset)

- Default output: `pid,tt,state,time,command`
- Common formats: `-j`, `-l`, `-u`, `-v` (BSD-style formats)
- Selection: `-A` (all), `-a` (all with tty), `-p pid`, `-t tty`, `-U uid`, `-G gid`
- Sorting: `-m` (by memory), `-r` (by CPU)
- Output control: `-O`, `-o`, `-w` (width control), `-h` (repeat headers)

## Unsupported / Not Available on Linux

| Option | Reason |
|---|---|
| `-j jail` | FreeBSD jail IDs have no Linux equivalent |
| `-class class` | FreeBSD login class has no Linux equivalent |
| `-M core` | Requires `kvm(3)` for core file analysis |
| `-N system` | Requires `kvm(3)` for name list extraction |
| `-Z` | FreeBSD MAC labels don't map to a portable Linux interface |

## Known Differences

- `state` flags: Mapping is approximate (e.g., BSD `P_CONTROLT` has no exact flag in Linux `/proc/[pid]/stat`).
- Memory stats: `tsiz`, `dsiz`, `ssiz` are hard to extract precisely on Linux via `/proc` compared to `struct kinfo_proc`. `vsz` and `rss` are accurate.
- CPU % calculation: BSD-style decaying average (`ki_pctcpu`) is hard to replicate exactly; a life-time average is used instead.

## Test Categories

- CLI parsing and usage
- Process selection: `-p`, `-A`, `-t`
- Format selection: `-j`, `-l`, `-u`, `-o`
- Sorting: `-m`, `-r`
- Negative tests: invalid flags, non-existent pids.
