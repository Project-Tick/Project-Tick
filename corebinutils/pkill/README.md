# pkill / pgrep

Standalone Linux-native port of FreeBSD `pkill`/`pgrep` for Project Tick BSD/Linux Distribution.

Both `pkill` and `pgrep` are built from the same binary; `pgrep` is installed as a symlink.

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
| `struct kinfo_proc` | `/proc/[pid]/stat` + `/proc/[pid]/status` + `/proc/[pid]/cmdline` |
| `sys_signame[]` / `NSIG` | Static signal table (same as `bin/kill` port) |
| `SLIST_*` (BSD queue macros) | Dynamic arrays (`realloc`-grown) |
| `getprogname(3)` | `argv[0]` basename via `strrchr` |
| `err(3)` / `warn(3)` | Kept: `err.h` is POSIX-compatible on Linux |
| `flock(2)` for `-L` | `flock(2)` is also available on Linux |

### /proc field mapping

- `pid`, `ppid`, `pgrp`, `sid`, `tdev`: parsed from `/proc/[pid]/stat` fields
- `ruid`, `euid`, `rgid`, `egid`: parsed from `/proc/[pid]/status` (`Uid:`/`Gid:` lines)
- Process start time (`starttime`): field 22 of `/proc/[pid]/stat` (for `-n`/`-o`)
- Command name (`comm`): the `(comm)` field of `/proc/[pid]/stat`; kernel truncates to 15 characters
- Full command line: `/proc/[pid]/cmdline` (NUL-separated, joined with spaces for `-f`)
- Kernel thread detection: empty `/proc/[pid]/cmdline` in a non-zombie process

## Supported Semantics

- `pgrep`/`pkill` with pattern (ERE), `-f`, `-i`, `-x`, `-v`
- `-l` long output, `-d delim` delimiter (pgrep only), `-q` quiet (pgrep only)
- `-n` newest, `-o` oldest
- `-P ppid`, `-u euid`, `-U ruid`, `-G rgid`, `-g pgrp`, `-s sid`
- `-t tty`: resolves `/dev/ttyXX`, `/dev/pts/N`; `-` matches no-tty processes
- `-F pidfile`: read target PID from file
- `-L`: verify pidfile is locked (daemon is running)
- `-a` include ancestors, `-I` interactive confirmation (pkill only)
- `-S` include kernel threads (pgrep only)
- Signal specification: `-SIGTERM`, `-TERM`, `-15`, `-s`, `-USR1`, `RTMIN`, `RTMIN+N`, `RTMAX-N`

## Unsupported / Not Available on Linux

| Option | Reason |
|---|---|
| `-j jail` | FreeBSD jail IDs have no Linux equivalent; exits with explicit error |
| `-c class` | FreeBSD login class (`ki_loginclass`) has no Linux equivalent; exits with explicit error |
| `-M core` | Requires `kvm(3)` for core file analysis; exits with explicit error |
| `-N system` | Requires `kvm(3)` for name list extraction; exits with explicit error |
| `ki_comm` > 15 chars | Linux kernel truncates `comm` to 15 chars; use `-f` for full cmdline matching |

## Known Limits

- `comm` matching is limited to the **first 15 characters** of the executable name (Linux kernel invariant). Use `-f` to match against the full argument list.
- Kernel threads are excluded by default (no cmdline). Use `-S` with `pgrep` to include them.
- TTY matching uses device numbers from `/proc/[pid]/stat` field `tty_nr`. On some container environments this field may be 0 for all processes; `-t` will produce no matches in that case.
