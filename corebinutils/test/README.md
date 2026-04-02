# test

Standalone Linux-native port of FreeBSD `test(1)` / `[(1)` for Project Tick BSD/Linux Distribution.

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

- The standalone layout follows sibling ports such as `bin/expr` and `bin/pwd`: local `GNUmakefile`, short technical `README.md`, and shell regression tests under `tests/`.
- The FreeBSD shared-source shape (`/bin/test` plus `/bin/sh` builtin hooks) was removed. This port is a standalone utility and keeps the expression parser in one Linux-focused source file.
- The original recursive-descent parser and POSIX ambiguity handling are preserved, but the parser state is moved into an explicit `struct parser` instead of global cursor variables.
- BSD libc-only diagnostics (`err(3)`, `__dead2`, `__printf0like`, `__nonstring`) are replaced with plain `fprintf(3)` / `vfprintf(3)` error paths so the code builds cleanly on glibc and musl.

## Linux API Mapping

| FreeBSD / BSD mechanism | Linux-native replacement | Reasoning |
|---|---|---|
| `eaccess(2)` for `-r`, `-w`, `-x` | `faccessat(AT_FDCWD, ..., AT_EACCESS)` | Linux exposes effective-ID permission checks through `faccessat` without a BSD shim layer |
| `-e` existence check | `stat(2)` / `lstat(2)` result | existence does not need a separate access probe on Linux |
| `err(3)` / `verrx(3)` | local `fprintf(3)` / `vfprintf(3)` diagnostics | removes BSD libc dependency and keeps exact exit status control |
| shared `/bin/sh` builtin glue (`#ifdef SHELL`, `bltin.h`) | removed | target deliverable here is the standalone userland tool, not a shell builtin |
| implicit parser globals (`nargc`, `t_wp`, `parenlevel`) | `struct parser` state object | reduces global state and makes ambiguity handling easier to audit |
| `stat(2)` / `lstat(2)` timestamp compare with BSD struct access | Linux `st_mtim` comparison | POSIX.1-2008 / Linux-compatible nanosecond mtime comparison for `-nt` and `-ot` |

## Supported Semantics

- All primaries documented in [`test.1`](/home/samet/freebsd/bin/test/test.1), including `-L`, `-h`, `-S`, `-O`, `-G`, `-nt`, `-ot`, `-ef`, and the compatibility `==` operator.
- Ambiguous POSIX grammar cases handled the same way as the historical FreeBSD parser, including the non-short-circuit `-a` / `-o` behaviour documented in `BUGS`.
- `[` is built as a symlink to `test`; missing closing `]` is a hard error.
- Numeric parsing is strict: trailing garbage is rejected, and values outside `intmax_t` or file-descriptor `int` range fail explicitly.

## Intentionally Unsupported Semantics

- The shared-source `/bin/sh` builtin integration is not carried into this standalone Linux port. That is a shell-porting concern, not a `test(1)` binary concern.
- If the running Linux libc/kernel combination cannot provide `faccessat(..., AT_EACCESS)`, the port exits with an explicit error instead of silently approximating effective-ID permission checks.
