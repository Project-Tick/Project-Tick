# echo

Standalone musl-libc-based Linux port of FreeBSD `echo` for Project Tick BSD/Linux Distribution.

## Build

```sh
gmake -f GNUmakefile
gmake -f GNUmakefile CC=musl-gcc
```

Binary output:

```sh
out/echo
```

## Test

```sh
gmake -f GNUmakefile test
```

## Port Strategy

- The standalone layout matches sibling Linux ports such as `bin/cat` and `bin/chmod`.
- FreeBSD Capsicum entry points were removed instead of stubbed; this utility only needs stdout and writes directly through `write(2)`.
- The Linux port no longer batches output with `writev(2)`. It streams arguments with a retry-safe `write(2)` loop, which avoids `IOV_MAX` handling and correctly deals with partial writes on Linux and musl.

## Supported Semantics On Linux

- A single leading literal `-n` suppresses the trailing newline.
- A trailing `\c` only has special meaning when it appears at the end of the final operand; it suppresses the trailing newline and is not printed.
- `--` is not treated as an end-of-options marker and is printed literally.

## Intentionally Unsupported Semantics

- GNU `echo` option parsing is not implemented. `-e`, `-E`, `--help`, and `--version` are treated as literal operands, matching FreeBSD `echo` rather than GNU coreutils.
- Backslash escape processing other than the historical final-operand `\c` rule is not supported.
