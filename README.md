# sh

Standalone Linux-native port of FreeBSD `sh` for Project Tick BSD/Linux Distribution.

## Build

```sh
gmake -f GNUmakefile
gmake -f GNUmakefile CC=musl-gcc
```

Binary output:

```sh
out/sh
```

## Test

```sh
gmake -f GNUmakefile test
gmake -f GNUmakefile test CC=musl-gcc
```

## Port Strategy

- The FreeBSD `bsd.prog.mk` build is replaced with a standalone `GNUmakefile` that regenerates `builtins.c`, `builtins.h`, `nodes.c`, `nodes.h`, `syntax.c`, `syntax.h`, and `token.h`.
- No shared BSD compatibility shim is introduced. FreeBSD-only assumptions are either rewritten directly for Linux or rejected with explicit runtime errors.
- `wait3(2)` is replaced with Linux `wait4(-1, ...)` for job accounting and child collection.
- `CLOCK_UPTIME`-based `read -t` timing is mapped to `CLOCK_MONOTONIC`, preserving monotonic timeout behavior on Linux.
- BSD `sys_signame` / `sys_nsig` usage is replaced with a local Linux signal table that covers standard signals plus `SIGRTMIN`/`SIGRTMAX`.
- Symbolic `umask` parsing uses the local mode compiler carried from the sibling `bin/chmod` port rather than BSD `setmode(3)` / `getmode(3)`.
- ENOEXEC fallback uses the running shell image from `/proc/self/exe` when available so Linux tests exercise the ported shell rather than the host `/bin/sh`.
- Long-directory `pwd` recovery uses logical `PWD` tracking for `cd -L` and a `/proc/self/cwd` fallback for physical lookups when `getcwd(3)` cannot report the path on Linux.
- The test harness resets inherited `SIGPIPE` disposition before spawning the shell so trap and pipeline tests stay deterministic under CI, PTYs, and container runtimes.

## Supported / Unsupported Linux Semantics

- Supported: POSIX shell scripts, pipelines, expansions, traps, job control, `wait`, `kill %job`, `command -p`, symbolic `umask`, and interactive prompting.
- Supported with Linux-native mapping: `read -t` uses monotonic time, job waiting uses `wait4(2)`, and shell self-reexec falls back through `/proc/self/exe`.
- Unsupported: `set -o verify` / `set +o verify` enabling is rejected with an explicit error because Linux has no `O_VERIFY` equivalent.
- Unsupported in this standalone build: line editing and history support are disabled (`NO_HISTORY`). `fc` and `bind` remain present but fail with explicit runtime errors instead of pretending to work.
