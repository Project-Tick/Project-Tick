# sleep

Standalone musl-libc-based Linux port of FreeBSD `sleep` for Project Tick BSD/Linux Distribution.

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

- Port structure follows the standalone sibling ports such as `bin/hostname` and `bin/date`: local `GNUmakefile`, short technical `README.md`, and shell tests under `tests/`.
- The FreeBSD source was converted directly to a Linux-native implementation instead of preserving BSD build glue, Capsicum entry, or BSD libc diagnostics.
- Interval parsing is strict and manpage-driven: operands are parsed with `strtold(3)`, unit handling is explicit, invalid trailing data is rejected, and the final summed interval is rounded up to the next nanosecond so Linux does not undersleep the requested minimum duration.

## Linux API Mapping

- FreeBSD `nanosleep(2)` usage remains `nanosleep(2)` on Linux; interrupted sleeps are resumed with the kernel-provided remaining interval.
- FreeBSD `SIGINFO` progress reporting maps to Linux `SIGUSR1`, because Linux does not provide `SIGINFO`. The report still prints the estimated remaining time for the current sleep request.
- FreeBSD `<err.h>` diagnostics are replaced with local `fprintf(3)`-based error handling so the port builds cleanly on musl without BSD libc helpers.
- FreeBSD Capsicum setup (`caph_limit_stdio()` / `caph_enter()`) is removed. Linux has no equivalent process-sandbox semantic that belongs in `sleep(1)`, and adding a fake compatibility path would be incorrect.

## Supported Semantics On Linux

- `sleep number[unit] [...]` with `s`, `m`, `h`, or `d` suffixes, fractional operands, and multiple operands added together
- Negative operands when the final sum remains positive, matching the manpage statement that zero or negative final sums exit immediately
- `--` as an operand separator for callers that want explicit disambiguation before negative numeric operands
- Progress reporting on Linux via `SIGUSR1`, written to stdout as the manpage requires for the `SIGINFO` path

## Intentionally Unsupported Semantics

- Non-finite `strtold(3)` forms such as `inf`, `infinity`, and `nan` are rejected with an explicit error. Linux has no finite `nanosleep(2)` representation for those values, and the port does not guess at an emulation.
- Requests that exceed Linux `time_t` / `struct timespec` range fail with an explicit error instead of truncating or wrapping.
- No GNU `sleep` extensions such as `--help` or `--version` are implemented; `sleep.1` defines no such interface.
