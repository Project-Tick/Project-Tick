# timeout

Standalone Linux-native port of FreeBSD `timeout` for Project Tick BSD/Linux Distribution.

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

- Port structure follows sibling standalone ports (`bin/sleep`, `bin/rmdir`, `bin/pwait`): local `GNUmakefile`, short technical `README.md`, shell tests under `tests/`.
- The FreeBSD source was converted to Linux-native process control instead of carrying BSD-only reaper APIs (`procctl(PROC_REAP_*)`, `str2sig`, `err(3)` helpers).
- Signal handling was redesigned around blocked-signal waiting (`sigwaitinfo(2)`/`sigtimedwait(2)`) to avoid asynchronous global-flag races and keep strict, deterministic state transitions.
- Duration and signal parsing are strict and explicit: malformed tokens fail with clear diagnostics and status `125` instead of silent coercion.

## Linux API Mapping

- FreeBSD process reaper control (`procctl(PROC_REAP_ACQUIRE/KILL/STATUS/RELEASE)`) maps to:
  - `prctl(PR_SET_CHILD_SUBREAPER)` for descendant reparent/wait behavior.
  - Process-group signaling (`kill(-pgid, sig)`) for default non-foreground propagation.
  - `waitpid(-1, ..., WNOHANG)` loops to reap both command and adopted descendants.
- FreeBSD timer path (`setitimer(ITIMER_REAL)` + `SIGALRM`) maps to monotonic deadlines driven by `clock_gettime(CLOCK_MONOTONIC)` and `sigtimedwait(2)` timeout waits.
- FreeBSD `str2sig(3)` maps to an in-tree, libc-independent signal parser (numbers, `SIG`-prefixed names, aliases, and realtime forms).
- FreeBSD `err(3)`/`warn(3)` maps to direct `fprintf(3)`/`dprintf(2)` diagnostics, preserving musl portability.

## Supported Semantics On Linux

- `timeout [-f] [-k time] [-p] [-s signal] [-v] duration command [arg ...]` as documented by `timeout.1`.
- `duration`/`-k time` with `s`, `m`, `h`, `d` suffixes and strict decimal parsing.
- Exit statuses `124`, `125`, `126`, `127`, plus command status passthrough and self-termination with the command's signal status when required.
- External `SIGALRM` handling as "time limit reached" behavior, matching `timeout.1`.

## Intentionally Unsupported / Limited Semantics

- Non-finite durations (`inf`, `infinity`, `nan`) are rejected with explicit `125` errors.
- Descendants that intentionally escape the command process group (e.g. explicit `setsid(2)`/`setpgid(2)` re-grouping) are treated as unsupported during timeout enforcement: if timed-out descendants remain but the original command process group no longer exists, the utility fails explicitly with status `125` instead of silently waiting indefinitely.
- If Linux subreaper support is unavailable (`prctl(PR_SET_CHILD_SUBREAPER)` missing/failing), non-foreground mode fails explicitly with an error instead of silently downgrading behavior.
