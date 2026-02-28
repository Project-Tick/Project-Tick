# cat

Standalone musl-libc-based Linux port of FreeBSD `cat` for Project Tick BSD/Linux Distribution.

## Build

```sh
gmake -f GNUmakefile
```

Binary output:

```sh
out/cat
```

## Test

```sh
gmake -f GNUmakefile test
```

## Notes

- This project is intentionally decoupled from the top-level FreeBSD build.
- Linux builds disable the FreeBSD Capsicum/Casper path.
- No shared BSD compat shim is used; source is adjusted directly for Linux.
