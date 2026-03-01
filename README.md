# pwait

Standalone Linux-native port of FreeBSD `pwait` for Project Tick BSD/Linux Distribution.

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

| BSD mechanism | Linux replacement |
|---|---|
| `EVFILT_PROC` / `NOTE_EXIT` (kqueue) | `pidfd_open(2)` + `poll(2)` with `waitid(2)` |
| `setitimer(2)` + `SIGALRM` | `timerfd_create(2)` + `poll(2)` |
| `RB_TREE` indexing | Dynamic struct array sorted with `qsort(3)` |

### API Mapping

- Instead of BSD's kqueue `EVFILT_PROC` which can notify on arbitrary process exit, we rely on Linux's `pidfd_open()` which creates a file descriptor representing a process. We then use `poll()` on the array of pidfds.
- When `poll()` indicates the pidfd is readable (`POLLIN`), we use `waitid(P_PIDFD, fd, &info, WEXITED | WNOHANG)` to get the exit status, which only works if the target process is a child of `pwait`. For unrelated processes, `waitid()` will fail with `ECHILD` but we still know the process exited because the pidfd signaled `POLLIN`.
- Timeouts in the original implementation used `setitimer` and `SIGALRM`. Here, we use `timerfd` and wait on it along with the `pidfd` array inside the same `poll()` loop.

## Supported Semantics

- Waiting for one or more process IDs to terminate.
- Outputting the remaining running processes on exit with `-p`.
- Exiting on the first target termination with `-o`.
- Printing the exit code or termination signal of the targets with `-v`.
- Timeout mechanism with `-t` supporting `s`, `m`, and `h` units.

## Unsupported / Not Available on Linux

All BSD semantics from `pwait.1` are cleanly supported in this Linux port. No known major incompatibilities, apart from minor differences in internal implementation limits (e.g. `kern.pid_max` fallback uses `INT_MAX`, preventing hard limitations).

- `waitid` will only fetch the exit status of **children** of the `pwait` process on Linux. For unrelated processes, `pwait -v` will print `terminated.` instead of `exited with status X.` because the Linux kernel does not allow reading another user process's exact exit code cleanly without `PTRACE` or it being a child.
