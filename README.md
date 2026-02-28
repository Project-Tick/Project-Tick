# hostname

Standalone musl-libc-based Linux port of FreeBSD `hostname` for Project Tick BSD/Linux Distribution.

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

## Notes

- Port strategy is Linux-native UTS syscall/API mapping, not a BSD compatibility shim.
- Reading the hostname uses `uname(2)` and the Linux UTS `nodename` field.
- Setting the hostname uses `sethostname(2)`.
- `-s` returns the label before the first `.` in the kernel hostname; `-d` returns the suffix after the first `.` or an empty string if no suffix exists.
- `-f` is intentionally unsupported on Linux in this port. A canonical FQDN requires NSS/DNS resolution policy outside the kernel UTS hostname, so the program exits with an explicit error instead of guessing.
- Linux supports at most 64 bytes for the UTS hostname. Longer inputs are rejected with an explicit error instead of truncation.
- Mutation tests run only inside a private UTS namespace when `unshare(1)` and the required namespace permissions are available; otherwise they are skipped to keep CI/container runs stable.
