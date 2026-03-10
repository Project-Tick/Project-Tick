# bin

Top-level monolithic build entrypoint for Project Tick BSD/Linux `bin` ports.

This directory provides a `./configure && make` workflow that orchestrates all
ported subcommands while keeping each subdirectory's own `GNUmakefile` as the
build source of truth.

## Build

```sh
./configure
make -f GNUmakefile -j"$(nproc)" all
```

Default behavior is musl-first toolchain selection (`musl-gcc`/musl-capable
clang candidates), with generated top-level overrides for common variables
(`CC`, `AR`, `RANLIB`, `CPPFLAGS`, `CFLAGS`, `LDFLAGS`, etc.).

## Test

```sh
make -f GNUmakefile test
```

This runs each subdirectory test target through the top-level orchestrator.
Environment-limited tests may print `SKIP` and continue.

## Outputs

All generated artifacts are centralized under:

```text
build/   # object and generated intermediate files
out/     # final binaries and test helper outputs
```

Subdirectories are prepared to use these shared roots during top-level builds.

## Maintenance Targets

```sh
make -f GNUmakefile clean
make -f GNUmakefile distclean
make -f GNUmakefile reconfigure
```

- `clean`: removes shared `build/` and `out/`
- `distclean`: also removes generated top-level `GNUmakefile` and `config.mk`
- `reconfigure`: regenerates top-level build files

## Notes

- Subdirectory `GNUmakefile` files are not replaced; the top-level file only
  drives and overrides invocation context.
- `GNUmakefile` and `config.mk` are generated artifacts from `./configure`.
- For command-specific behavior, limitations, and port notes, see each
  subdirectory `README.md`.
