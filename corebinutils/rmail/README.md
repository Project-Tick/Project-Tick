# rmail

Standalone Linux-native port of FreeBSD `rmail` for Project Tick BSD/Linux Distribution.

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

- The FreeBSD/sendmail-lib implementation was rewritten as a standalone C17 utility. BSD build glue, `sm_io`, `sm_snprintf`, `_PATH_SENDMAIL`, and fixed-size header buffers were removed.
- Input parsing remains manpage-compatible: `rmail` consumes leading `From` / `>From` UUCP envelope lines, compresses relay hops into a single return path, and forwards the remaining message body to a sendmail-compatible MTA.
- Header parsing uses `getline(3)` instead of static 2048-byte buffers, so long UUCP `From` lines are handled without `PATH_MAX`-style or fixed-buffer limits.
- Recipient validation stays strict: operands that begin with `-` are rejected instead of being forwarded to sendmail as flags.

## BSD to Linux / POSIX Mapping

| Original mechanism | Linux / POSIX replacement |
|---|---|
| `sm_io_fgets`, `sm_io_fprintf`, `sm_io_open`, `sm_io_close` | `getline(3)`, `fprintf(3)`, `fdopen(3)`, `fclose(3)` |
| `_PATH_SENDMAIL` from sendmail headers | Local default path `/usr/sbin/sendmail` with test override via `RMAIL_SENDMAIL` |
| `sm_snprintf` / sendmail alloc helpers | Local dynamic allocation helpers with `snprintf(3)` sizing |
| Fixed 2048-byte parse buffers | Dynamic `getline(3)` + heap strings |
| `lseek(2)` after `sm_io_tell` on regular stdin | `ftello(3)` to track body start, then `lseek(2)` on `STDIN_FILENO` before `execv(2)` |
| `fork(2)` + `pipe(2)` + `execv(2)` | Preserved directly on Linux |

## Supported / Unsupported Semantics

- Supported: `-D domain`, `-T`, strict UUCP `From` / `>From` parsing, `remote from` precedence, bang-path compression, recipient wrapping for comma-containing addresses, and sendmail exit-status propagation.
- Supported: regular-file stdin optimization. If stdin is a seekable regular file, `rmail` seeks to the start of the message body and `exec`s sendmail directly.
- Unsupported by design: GNU long options and GNU option permutation. Parsing is strict FreeBSD/POSIX style.
- Unsupported on Linux: there is no bundled sendmail ABI shim. `rmail` requires an external sendmail-compatible binary at `/usr/sbin/sendmail` or, for test harnesses, `RMAIL_SENDMAIL`. Missing or non-executable mailers fail explicitly.
- Unsupported semantics are not stubbed. If the sendmail-compatible backend exits non-zero or cannot be executed, `rmail` returns an explicit error instead of pretending delivery succeeded.
