/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2004-2009, Jilles Tjoelker
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with
 * or without modification, are permitted provided that the
 * following conditions are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the
 *    following disclaimer.
 * 2. Redistributions in binary form must reproduce the
 *    above copyright notice, this list of conditions and
 *    the following disclaimer in the documentation and/or
 *    other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/timerfd.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <limits.h>

static int
x_pidfd_open(pid_t pid)
{
#if defined(__NR_pidfd_open)
	return syscall(__NR_pidfd_open, pid, 0);
#elif defined(SYS_pidfd_open)
	return syscall(SYS_pidfd_open, pid, 0);
#else
	errno = ENOSYS;
	return -1;
#endif
}

// We need to keep track of pids, whether they are done, and their open pidfd
struct target_pid {
	long pid;
	int fd;
	bool done;
};

static int
pidcmp(const void *a, const void *b)
{
	const struct target_pid *pa = a;
	const struct target_pid *pb = b;
	return (pa->pid > pb->pid ? 1 : pa->pid < pb->pid ? -1 : 0);
}

static void
usage(void)
{
	fprintf(stderr, "usage: pwait [-t timeout] [-opv] pid ...\n");
	exit(EX_USAGE);
}

/*
 * pwait - wait for processes to terminate
 */
int
main(int argc, char *argv[])
{
	struct target_pid *targets = NULL;
	char *end, *s;
	double timeout = 0.0;
	long pid;
	pid_t mypid;
	int ntargets = 0, nallocated = 0;
	int ndone = 0, nleft = 0, opt, ret;
	bool oflag = false, pflag = false, tflag = false, verbose = false;

	while ((opt = getopt(argc, argv, "opt:v")) != -1) {
		switch (opt) {
		case 'o':
			oflag = true;
			break;
		case 'p':
			pflag = true;
			break;
		case 't':
			tflag = true;
			errno = 0;
			timeout = strtod(optarg, &end);
			if (end == optarg || errno == ERANGE || timeout < 0) {
				errx(EX_DATAERR, "timeout value");
			}
			switch (*end) {
			case '\0':
				break;
			case 's':
				end++;
				break;
			case 'h':
				timeout *= 60;
				/* FALLTHROUGH */
			case 'm':
				timeout *= 60;
				end++;
				break;
			default:
				errx(EX_DATAERR, "timeout unit");
			}
			if (*end != '\0') {
				errx(EX_DATAERR, "timeout unit");
			}
			if (timeout > 100000000L) {
				errx(EX_DATAERR, "timeout value");
			}
			break;
		case 'v':
			verbose = true;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0) {
		usage();
	}

	mypid = getpid();
	long pid_max = 4194304;
	FILE *f = fopen("/proc/sys/kernel/pid_max", "r");
	if (f != NULL) {
		if (fscanf(f, "%ld", &pid_max) != 1) {
			pid_max = 4194304;
		}
		fclose(f);
	}

	for (int n = 0; n < argc; n++) {
		s = argv[n];
		/* Undocumented Solaris compat */
		if (strncmp(s, "/proc/", 6) == 0) {
			s += 6;
		}
		errno = 0;
		pid = strtol(s, &end, 10);
		if (pid < 0 || pid > pid_max || *end != '\0' || errno != 0) {
			warnx("%s: bad process id", s);
			continue;
		}
		if (pid == mypid) {
			warnx("%s: skipping my own pid", s);
			continue;
		}

		/* Check for duplicates */
		bool dup = false;
		for (int i = 0; i < ntargets; i++) {
			if (targets[i].pid == pid) {
				dup = true;
				break;
			}
		}
		if (dup) {
			continue;
		}

		if (ntargets >= nallocated) {
			nallocated = nallocated == 0 ? 16 : nallocated * 2;
			targets = realloc(targets, nallocated * sizeof(struct target_pid));
			if (targets == NULL) {
				err(EX_OSERR, "realloc");
			}
		}

		targets[ntargets].pid = pid;
		targets[ntargets].done = false;
		int fd = x_pidfd_open((pid_t)pid);
		if (fd == -1) {
			/* Not found or invalid */
			warn("%ld", pid);
			targets[ntargets].fd = -1;
			targets[ntargets].done = true;
			ndone++;
		} else {
			targets[ntargets].fd = fd;
			nleft++;
		}
		ntargets++;
	}

	/* Sort targets ascending by PID to match BSD behavior */
	qsort(targets, ntargets, sizeof(struct target_pid), pidcmp);

	int timer_fd = -1;
	if ((ndone == 0 || !oflag) && nleft > 0 && tflag) {
		timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
		if (timer_fd == -1) {
			err(EX_OSERR, "timerfd_create");
		}
		struct itimerspec its = {};
		its.it_value.tv_sec = (time_t)timeout;
		its.it_value.tv_nsec = (long)((timeout - its.it_value.tv_sec) * 1000000000UL);
		if (its.it_value.tv_sec == 0 && its.it_value.tv_nsec == 0) {
			/* timerfd needs non-zero value to arm */
			its.it_value.tv_nsec = 1; 
		}
		if (timerfd_settime(timer_fd, 0, &its, NULL) == -1) {
			err(EX_OSERR, "timerfd_settime");
		}
	}

	struct pollfd *pfds = calloc(ntargets + 1, sizeof(struct pollfd));
	if (pfds == NULL) {
		err(EX_OSERR, "calloc");
	}

	ret = EX_OK;
	while ((ndone == 0 || !oflag) && ret == EX_OK && nleft > 0) {
		int npoll = 0;
		if (timer_fd != -1) {
			pfds[npoll].fd = timer_fd;
			pfds[npoll].events = POLLIN;
			pfds[npoll].revents = 0;
			npoll++;
		}

		for (int i = 0; i < ntargets; i++) {
			if (!targets[i].done) {
				pfds[npoll].fd = targets[i].fd;
				pfds[npoll].events = POLLIN | POLLHUP | POLLERR;
				pfds[npoll].revents = 0;
				npoll++;
			}
		}

		int n = poll(pfds, npoll, -1);
		if (n == -1) {
			if (errno == EINTR) {
				continue;
			}
			err(EX_OSERR, "poll");
		}

		if (timer_fd != -1 && (pfds[0].revents & POLLIN)) {
			uint64_t expirations;
			(void)read(timer_fd, &expirations, sizeof(expirations));
			if (verbose) {
				printf("timeout\n");
			}
			ret = 124;
		}

		int pfds_idx = timer_fd != -1 ? 1 : 0;
		for (int i = 0; i < ntargets; i++) {
			if (targets[i].done) {
				continue;
			}
			if (pfds[pfds_idx].revents & (POLLIN | POLLHUP | POLLERR)) {
				pid = targets[i].pid;
				if (verbose) {
					siginfo_t info = {};
					int wret = waitid(P_PIDFD, targets[i].fd, &info, WEXITED | WNOHANG);
					if (wret == 0 && info.si_pid != 0) {
						if (info.si_code == CLD_EXITED) {
							printf("%ld: exited with status %d.\n",
							    pid, info.si_status);
						} else if (info.si_code == CLD_KILLED || info.si_code == CLD_DUMPED) {
							printf("%ld: killed by signal %d.\n",
							    pid, info.si_status);
						} else {
							printf("%ld: terminated.\n", pid);
						}
					} else {
						printf("%ld: terminated.\n", pid);
					}
				}
				close(targets[i].fd);
				targets[i].fd = -1;
				targets[i].done = true;
				ndone++;
				nleft--;
			}
			pfds_idx++;
		}
	}

	if (timer_fd != -1) {
		close(timer_fd);
	}
	free(pfds);

	if (pflag) {
		for (int i = 0; i < ntargets; i++) {
			if (!targets[i].done) {
				printf("%ld\n", targets[i].pid);
			}
		}
	}

	free(targets);
	exit(ret);
}
