# tickborg

Distributed CI bot for the Project Tick monorepo, adapted from [ofborg](https://github.com/NixOS/ofborg).

## Overview

tickborg is a RabbitMQ-based distributed CI system that:
- Automatically detects changed projects in PRs
- Builds affected sub-projects using their native build systems (CMake, Meson, Autotools, Cargo, Gradle, Make)
- Posts build results as GitHub check runs
- Supports multi-platform builds (Linux, macOS, Windows, FreeBSD)

## Automatic Building

PRs automatically trigger builds for affected projects based on:
- **File path detection**: Changed files are mapped to sub-projects
- **Conventional Commits**: Commit scopes like `feat(meshmc):` trigger builds for the named project

Example commit titles and the builds they will start:

| Message | Automatic Build |
|---|---|
| `feat(meshmc): add chunk loading` | `meshmc` |
| `cmark: fix buffer overflow` | `cmark` |
| `fix(neozip): handle empty archives` | `neozip` |
| `chore(ci): update workflow` | _(CI changes only)_ |

If the title of a PR begins with `WIP:` or contains `[WIP]` anywhere, its
projects are not built automatically.

## Commands

The comment parser is line-based. Commentary can be interwoven with bot
instructions.

1. To trigger the bot, the line _must_ start with `@tickbot` (case insensitive).
2. To use multiple commands, separate them on different lines.

### build

```
@tickbot build meshmc neozip cmark
```

This will build the specified projects using their configured build systems.

### test

```
@tickbot test meshmc
```

This will run the test suite for the specified projects.

### eval

```
@tickbot eval
```

Triggers a full evaluation of the PR — detects changed projects and labels the PR.

## Supported Platforms

| Platform | Runner |
|---|---|
| `x86_64-linux` | `ubuntu-latest` |
| `aarch64-linux` | `ubuntu-24.04-arm` |
| `x86_64-darwin` | `macos-15` |
| `aarch64-darwin` | `macos-15` |
| `x86_64-windows` | `windows-2025` |
| `aarch64-windows` | `windows-2025` |
| `x86_64-freebsd` | `ubuntu-latest` (VM) |

## Sub-Projects

| Project | Build System | Path |
|---|---|---|
| mnv | Autotools | `mnv/` |
| cgit | Make | `cgit/` |
| cmark | CMake | `cmark/` |
| neozip | CMake | `neozip/` |
| genqrcode | CMake | `genqrcode/` |
| json4cpp | CMake | `json4cpp/` |
| tomlplusplus | Meson | `tomlplusplus/` |
| libnbtplusplus | CMake | `libnbtplusplus/` |
| meshmc | CMake | `meshmc/` |
| forgewrapper | Gradle | `forgewrapper/` |
| corebinutils | Make | `corebinutils/` |

## Hacking

```shell
$ git clone https://github.com/project-tick/Project-Tick/
$ cd Project-Tick/ofborg
$ cd tickborg
$ cargo build
$ cargo check
$ cargo test
```

Make sure to format with `cargo fmt` and lint with `cargo clippy`.

## Configuration

See [`example.config.json`](./example.config.json) for a minimal configuration
and [`config.public.json`](./config.public.json) for the production config.
