/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2026 Dag-Erling Smørgrav
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
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Linux-native port of FreeBSD pwd(1).
 *
 * BSD dependency removed:
 *   - <sys/param.h> / MAXPATHLEN: replaced by dynamic getcwd(NULL, 0)
 *     which is POSIX.1-2008 and supported by both glibc and musl.
 *
 * Logical (-L) and physical (-P) semantics are fully POSIX and portable;
 * no BSD-specific kernel calls are involved.
 */

#include <sys/stat.h>

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct options {
	bool logical;
};

static const char *progname = "pwd";

static void	usage(void) __attribute__((noreturn));
static char	*getcwd_logical(void);
static char	*getcwd_physical(void);

/*
 * Attempt to return the logical working directory from $PWD.
 *
 * Returns a pointer to the $PWD string (not heap-allocated; do not free)
 * if $PWD is set, absolute, contains no /./ or /../ components, and
 * stat(2) confirms it refers to the same inode as ".".
 *
 * Returns NULL on any validation failure; callers must fall through to
 * getcwd_physical().
 */
static char *
getcwd_logical(void)
{
	struct stat log_st, phy_st;
	char *pwd, *p, *q;

	/* $PWD must be set and must be an absolute path. */
	if ((pwd = getenv("PWD")) == NULL || *pwd != '/')
		return (NULL);

	/*
	 * $PWD must not contain /./ or /../ components.
	 * Walk each slash-delimited segment and reject "." and "..".
	 */
	for (p = pwd + 1; *p != '\0'; p = q) {
		/* p points to the first character of a path segment. */
		for (q = p; *q != '\0' && *q != '/'; q++)
			/* advance to end of segment */;
		size_t seglen = (size_t)(q - p);
		if (seglen == 1 && p[0] == '.')
			return (NULL);
		if (seglen == 2 && p[0] == '.' && p[1] == '.')
			return (NULL);
		/* Skip the slash separator for next iteration. */
		if (*q == '/')
			q++;
	}

	/* $PWD must refer to the same inode/device as ".". */
	if (stat(pwd, &log_st) != 0 || stat(".", &phy_st) != 0)
		return (NULL);
	if (log_st.st_dev != phy_st.st_dev || log_st.st_ino != phy_st.st_ino)
		return (NULL);

	return (pwd);
}

/*
 * Return the physical working directory via getcwd(3).
 *
 * POSIX.1-2008 §2.2.3 allows getcwd(NULL, 0) which dynamically allocates
 * a buffer of the required size.  Both glibc and musl implement this.
 * The caller is responsible for free()ing the returned string.
 *
 * Returns NULL with errno set on failure.
 */
static char *
getcwd_physical(void)
{
	return (getcwd(NULL, 0));
}

static void
usage(void)
{
	(void)fprintf(stderr, "usage: %s [-L | -P]\n", progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct options opts;
	char *pwd;
	int ch;

	if (argv[0] != NULL && argv[0][0] != '\0') {
		const char *slash = strrchr(argv[0], '/');
		progname = (slash != NULL && slash[1] != '\0') ?
		    slash + 1 : argv[0];
	}

	opts.logical = true;

	while ((ch = getopt(argc, argv, "LP")) != -1) {
		switch (ch) {
		case 'L':
			opts.logical = true;
			break;
		case 'P':
			opts.logical = false;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 0)
		usage();

	/*
	 * If the logical path is requested and $PWD validates successfully,
	 * use it.  Otherwise (or if -P was given), fall through to the
	 * physical getcwd(3).
	 *
	 * getcwd_physical() allocates via getcwd(NULL,0); we own the memory.
	 * getcwd_logical() returns a pointer into environ; do not free it.
	 */
	if (opts.logical && (pwd = getcwd_logical()) != NULL) {
		/* pwd points into environ — no free needed */
		if (printf("%s\n", pwd) < 0)
			goto stdout_err;
	} else {
		char *phypwd = getcwd_physical();
		if (phypwd == NULL) {
			fprintf(stderr, "%s: .: %s\n", progname,
			    strerror(errno));
			exit(1);
		}
		if (printf("%s\n", phypwd) < 0) {
			free(phypwd);
			goto stdout_err;
		}
		free(phypwd);
	}

	if (fflush(stdout) != 0)
		goto stdout_err;

	return (0);

stdout_err:
	fprintf(stderr, "%s: stdout: %s\n", progname, strerror(errno));
	exit(1);
}
