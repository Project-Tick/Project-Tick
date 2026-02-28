# csh

Standalone musl-libc-based Linux port of FreeBSD `csh` for Project Tick BSD/Linux Distribution.

## Build

```sh
gmake -f GNUmakefile
gmake -f GNUmakefile CC=musl-gcc
```

Binary output:

```sh
out/csh
```

## Test

```sh
gmake -f GNUmakefile test
gmake -f GNUmakefile test CC=musl-gcc
```

## Port Strategy

- The source base stays close to FreeBSD's `tcsh`, but the Linux build is standalone and generates its own `tc.defs.c`, `sh.err.h`, `ed.defns.h`, and `tc.const.h`.
- The required upstream `tcsh` sources now live under `bin/csh/tcsh`, and the Linux terminfo/termcap stack is built from vendored ncurses sources under `bin/csh/ncurses`.
- No shared BSD compatibility shim is introduced. FreeBSD-specific assumptions are replaced with Linux-native interfaces in the affected source files.
- Child process accounting on Linux uses `wait4(2)` instead of BSD `wait3(2)`, which keeps job-control resource accounting working on musl.
- Login/watch state reads Linux utmp/utmpx data from `_PATH_UTMP` when available, otherwise `/run/utmp`, rather than FreeBSD's `utx.active`.
- Terminal capability lookup is satisfied by the vendored `libtinfow` build, so Linux and musl builds do not depend on host `ncursesw`/`tinfo`.
- NLS catalogs are disabled in the standalone port at source level; the build does not install or load translated message catalogs.

## Supported / Unsupported Linux Semantics

- Supported: non-interactive `-c` execution, shell scripts, globbing, history-less `-f` operation, vendored termcap/terminfo lookups, job control/resource accounting via `wait4(2)`, `setpriority(2)`, `getrlimit(2)`, and `setrlimit(2)`.
- Supported with explicit runtime dependency: login watch uses the host's utmp/utmpx database; if the database is absent, tcsh reports the missing file instead of silently pretending the feature works.
- Unsupported in this standalone port: NLS catalog loading is disabled, so translated message catalogs are not installed or loaded.
