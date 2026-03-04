# stty

Linux-native `stty` port of the FreeBSD userland tool, built for Linux + musl without a BSD shim layer.

## Build

```sh
make -C bin/stty
make -C bin/stty clean all CC=musl-gcc
```

## Test

```sh
make -C bin/stty test
make -C bin/stty clean test CC=musl-gcc
```

The test suite creates its own pseudoterminal, verifies termios and winsize state through independent helpers, and exercises negative paths as well as round-trip `-g` restore behavior.

## Port strategy

- Replaced the FreeBSD multi-file implementation with a single Linux-native termios/winsize implementation.
- Kept `stty.1` as the semantic contract and mapped documented behavior directly onto Linux APIs:
  - `tcgetattr(3)` / `tcsetattr(3)` for termios state
  - `cfsetispeed(3)` / `cfsetospeed(3)` for baud rates
  - `TIOCGWINSZ` / `TIOCSWINSZ` for `size`, `rows`, and `columns`
  - `TIOCGETD` / `TIOCSETD` with `N_TTY` for `tty`
  - `EXTPROC` local flag for `extproc`
- Avoided GNU-specific behavior; the build only enables `_DEFAULT_SOURCE` and `_XOPEN_SOURCE=700`.

## Linux-supported and unsupported semantics

Supported:

- POSIX and BSD output modes: default, `-a`, `-e`, `-g`
- Linux termios flags and compatibility aliases documented in `stty.1`
- `speed`, `ispeed`, `ospeed`, bare numeric baud-rate arguments, `raw`, `-raw`, `sane`, `cbreak`, `dec`, `tty`, `rows`, `columns`, `size`

Explicitly unsupported with hard errors:

- `altwerase`, `mdmbuf`, `rtsdtr`
- `kerninfo` / `nokerninfo`
- control characters `status`, `dsusp`, `erase2`
- `ek`, because Linux has no `VERASE2` and partial emulation would silently change semantics
- arbitrary non-table baud rates such as `12345`; this port intentionally stays on the stable Linux termios API instead of silently inventing `termios2` behavior
