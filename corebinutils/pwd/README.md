# pwd

Standalone Linux-native port of FreeBSD `pwd(1)` for Project Tick BSD/Linux Distribution.

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

`pwd` has no BSD-specific kernel calls; it is purely POSIX.  The only
platform-specific concern was the static `MAXPATHLEN` buffer used by
`getcwd_physical()`.

| BSD mechanism | Linux replacement |
|---|---|
| `<sys/param.h>` / `MAXPATHLEN` | Removed; `getcwd(NULL, 0)` allocates dynamically |
| Static `char pwd[MAXPATHLEN]` buffer | `char *` returned by `getcwd(3)`, heap-allocated |

### API Mapping

| BSD call / constant | Linux / POSIX equivalent | Notes |
|---|---|---|
| `MAXPATHLEN` from `<sys/param.h>` | `getcwd(NULL, 0)` | POSIX.1-2008 §2.2.3; both glibc and musl support passing `NULL,0` |
| `getcwd(buf, sizeof(buf))` | `getcwd(NULL, 0)` | Dynamically allocates; caller must `free()` |
| `getenv("PWD")` + `stat(2)` | unchanged | Fully POSIX; no BSD dependency |

### Logical (-L) path validation

The logical path from `$PWD` is accepted only when all of the following hold:

1. `$PWD` is set and starts with `/`.
2. No path component is `.` or `..` (POSIX 4.13 requirement).
3. `stat("$PWD")` and `stat(".")` agree on device and inode numbers.

If any check fails, `pwd -L` silently falls back to the physical path
(same behaviour as the original FreeBSD implementation and as required
by POSIX).

## Supported Semantics

- `-L` — logical working directory (default); falls back to physical on
  `$PWD` validation failure.
- `-P` — physical working directory (all symlinks resolved via `getcwd(3)`).
- Last flag wins when both `-L` and `-P` are given.
- Trailing newline always written.
- `fflush(stdout)` failure detected and reported as a non-zero exit.
- Dynamic buffer: no static `PATH_MAX` limit; works with paths longer than
  4096 bytes (filesystem-permitting).

## Unsupported / Linux Differences

No BSD semantics from `pwd.1` are missing.  All options (`-L`, `-P`) behave
identically to the FreeBSD binary.

One minor difference: the FreeBSD implementation uses a fixed 4096-byte
(`MAXPATHLEN`) stack buffer for `-P`.  This port uses `getcwd(NULL, 0)` which
dynamically allocates the exact required size, so it handles paths longer than
`PATH_MAX` on kernels and filesystems that support them.

The `$PWD` environment variable must be exported by the shell for `-L` to work
(same as on FreeBSD).
