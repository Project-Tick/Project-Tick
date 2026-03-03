# rmdir

Standalone Linux-native port of FreeBSD `rmdir` for Project Tick BSD/Linux Distribution.

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

- The FreeBSD utility was rewritten into a standalone Linux-native build instead of preserving BSD build glue, `err(3)`, ATF integration, or in-place mutation of operand strings.
- Directory removal maps directly to Linux `unlinkat(2)` with `AT_REMOVEDIR`, using `AT_FDCWD` for path-based operation.
- `-p` is implemented as a lexical parent walk over a duplicated path buffer. Trailing slashes are trimmed dynamically, parent components are derived without `PATH_MAX`, and removal stops at the first parent that is not empty or otherwise cannot be removed.
- Verbose output stays manpage-aligned: the explicit operand is printed after successful removal, and with `-p` each successfully removed parent is printed in removal order.

## Supported / Unsupported Semantics

- Supported: FreeBSD/POSIX option surface `-p` and `-v`, strict option parsing, `--` end-of-options handling, and partial-success behavior across multiple operands.
- Supported: lexical parent pruning for repeated `/` separators and trailing `/` components, without relying on GNU extensions or glibc-specific behavior.
- Unsupported by design: GNU long options and GNU-specific parsing extensions. This port keeps the FreeBSD/POSIX command-line surface strict.
- Linux-native limitation: kernel error text for unsupported removals such as `/`, non-empty directories, or non-directory operands comes from Linux `unlinkat(2)`/VFS semantics, but failures are surfaced explicitly and never silently ignored.
