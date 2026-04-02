# ln

Standalone Linux-native port of FreeBSD `ln` for Project Tick BSD/Linux Distribution.

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

- The FreeBSD `ln` control flow was rewritten into a standalone Linux-native utility instead of preserving BSD build glue or `err(3)` helpers.
- Hard-link creation maps directly to Linux `linkat(2)` with `AT_SYMLINK_FOLLOW` for `-L` and flag `0` for `-P`.
- The default hard-link behavior follows the FreeBSD utility, not GNU coreutils: source symlinks are dereferenced by default, equivalent to `-L`.
- Symbolic-link creation maps directly to Linux `symlink(2)`.
- Existing-target replacement uses Linux `unlink(2)` for non-directories and `rmdir(2)` for `-sF` empty-directory replacement.
- Path handling is dynamic; the port does not depend on `PATH_MAX`, `strlcpy(3)`, `getprogname(3)`, or other BSD-only libc interfaces.

## Supported / Unsupported Semantics

- Supported: `ln source [target]`, `ln source ... target_dir`, `link source target`, `-L`, `-P`, `-s`, `-f`, `-F`, `-h`, `-n`, `-i`, `-v`, and `-w`.
- Supported: replacing a symlink that points at a directory with `-snhf` / `-snf`, and replacing an empty directory with `-sF`.
- Unsupported by design: GNU long options and GNU-specific parsing extensions. This port keeps the BSD/POSIX command-line surface strict.
- Unsupported on Linux by kernel/filesystem semantics: hard-linking directories and cross-filesystem hard links. Those fail with the native Linux error instead of being masked.
