# domainname

Standalone musl-libc-based Linux port of FreeBSD `domainname` for Project Tick BSD/Linux Distribution.

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

- Port strategy is Linux-native syscall/API translation rather than preserving FreeBSD libc assumptions.
- Reading the NIS/YP domain uses `uname(2)` and the Linux UTS namespace `domainname` field.
- Setting the NIS/YP domain uses `setdomainname(2)`.
- Linux supports at most 64 bytes for the UTS domain name. Longer FreeBSD inputs are rejected with an explicit error instead of truncation.
- Mutation tests run only inside a private UTS namespace when `unshare(1)` and the required namespace permissions are available; otherwise they are skipped to keep CI/container runs stable.
