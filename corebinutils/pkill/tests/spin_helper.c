/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Klara, Inc.
 * Copyright (c) 2026 Project Tick. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Linux-native test helper for pkill/pgrep tests.
 * Replaces BSD __DECONST and <sys/cdefs.h>.
 *
 * Usage:
 *   spin_helper --spin flagfile sentinel
 *     Creates flagfile, then spins until killed.
 *
 *   spin_helper --short flagfile sentinel
 *     Re-execs with short arguments, then spins.
 *
 *   spin_helper --long flagfile sentinel
 *     Re-execs with maximally long arguments, then spins.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t got_signal;

static void
sighandler(int signo)
{
	got_signal = signo;
}

static void
exec_shortargs(char *argv[])
{
	char *nargv[] = { argv[0], (char *)"--spin", argv[2], argv[3], NULL };
	char *nenvp[] = { NULL };

	execve(argv[0], nargv, nenvp);
	perror("execve");
	_exit(1);
}

static void
exec_largeargs(char *argv[])
{
	size_t bufsz;
	char *s = NULL;
	char *nargv[] = {
		argv[0], (char *)"--spin", argv[2], NULL, argv[3], NULL
	};
	char *nenvp[] = { NULL };

	/*
	 * Compute maximum argument size.  Account for each argument + NUL
	 * terminator, plus an extra NUL.
	 */
	long arg_max = sysconf(_SC_ARG_MAX);

	if (arg_max <= 0)
		arg_max = 131072;

	bufsz = (size_t)arg_max -
	    ((strlen(argv[0]) + 1) + sizeof("--spin") +
	    (strlen(argv[2]) + 1) + (strlen(argv[3]) + 1) + 1);

	/*
	 * Keep trying with smaller sizes until execve stops returning
	 * E2BIG.
	 */
	do {
		char *ns = realloc(s, bufsz + 1);

		if (ns == NULL)
			abort();
		s = ns;
		memset(s, 'x', bufsz);
		s[bufsz] = '\0';
		nargv[3] = s;

		execve(argv[0], nargv, nenvp);
		bufsz--;
	} while (errno == E2BIG && bufsz > 0);

	perror("execve");
	_exit(1);
}

int
main(int argc, char *argv[])
{
	if (argc > 1 && strcmp(argv[1], "--spin") == 0) {
		int fd;
		struct sigaction sa;

		if (argc < 4) {
			fprintf(stderr,
			    "usage: %s --spin flagfile sentinel\n",
			    argv[0]);
			return 1;
		}

		/* Install signal handler for SIGUSR1 and SIGTERM. */
		memset(&sa, 0, sizeof(sa));
		sa.sa_handler = sighandler;
		sigemptyset(&sa.sa_mask);
		sigaction(SIGUSR1, &sa, NULL);
		sigaction(SIGTERM, &sa, NULL);

		/* Create flag file to indicate readiness. */
		fd = open(argv[2], O_RDWR | O_CREAT | O_TRUNC, 0644);
		if (fd < 0) {
			perror(argv[2]);
			return 1;
		}
		/* Write our PID to the flag file. */
		dprintf(fd, "%d\n", (int)getpid());
		close(fd);

		/* Spin until signal received. */
		while (got_signal == 0)
			pause();

		/*
		 * Write received signal name to the sentinel file
		 * so the test can verify which signal was delivered.
		 */
		{
			const char *sname = "UNKNOWN";

			if (got_signal == SIGUSR1)
				sname = "USR1";
			else if (got_signal == SIGTERM)
				sname = "TERM";

			FILE *fp = fopen(argv[argc - 1], "w");

			if (fp != NULL) {
				fprintf(fp, "%s\n", sname);
				fclose(fp);
			}
		}

		return 0;
	}

	if (argc != 4) {
		fprintf(stderr,
		    "usage: %s [--short | --long] flagfile sentinel\n",
		    argv[0]);
		return 1;
	}

	if (strcmp(argv[1], "--short") == 0)
		exec_shortargs(argv);
	else
		exec_largeargs(argv);

	return 1;
}
