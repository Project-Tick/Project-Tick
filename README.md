# kill

Standalone Linux-native port of FreeBSD `kill` for Project Tick BSD/Linux Distribution.

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

- Process signaling is mapped directly to Linux `kill(2)`.
- Signal name parsing and `kill -l` output are implemented locally from Linux `<signal.h>` constants instead of FreeBSD `str2sig(3)`, `sig2str(3)`, `sys_signame`, or `sys_nsig`.
- Real-time signals are exposed through Linux `SIGRTMIN`/`SIGRTMAX` as `RTMIN`, `RTMIN+N`, and `RTMAX-N`.
- Negative process-group targets are supported through the native Linux `kill(2)` PID semantics; callers must use `--` before a negative PID to avoid ambiguity with signal options.

## Supported / Unsupported Semantics

- Supported: `kill pid ...`, `kill -s signal_name pid ...`, `kill -signal_name pid ...`, `kill -signal_number pid ...`, `kill -l`, `kill -l exit_status`, signal names with optional `SIG` prefix, and signal `0`.
- Unsupported by design: shell job-control operands such as `%1`. Those require a shell builtin with job table state, so this standalone binary exits with a clear error instead of guessing.
- Unsupported by design: GNU reverse lookup forms such as `kill -l TERM` and numeric `kill -s 9`. This port keeps the BSD/POSIX interface strict rather than adding GNU-only parsing.
- Unsupported on Linux: BSD-only signals that do not exist in Linux `<signal.h>` such as `INFO`. They fail explicitly as unknown signals.
