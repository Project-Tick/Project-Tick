/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Copyright (c) 2026
 *  Project Tick. All rights reserved.
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

#define _GNU_SOURCE 1

#include <sys/utsname.h>

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *progname = "domainname";

static void	die_errno(const char *what) __attribute__((noreturn));
static void	diex(const char *fmt, ...) __attribute__((format(printf, 1, 2),
		    noreturn));
static size_t	linux_domainname_max(void);
static void	parse_args(int argc, char *argv[], const char **domainname);
static void	print_domainname(void);
static void	set_domainname(const char *domainname);
static void	usage(void) __attribute__((noreturn));

int
main(int argc, char *argv[])
{
	const char *domainname;
	const char *slash;

	if (argv[0] != NULL && argv[0][0] != '\0') {
		slash = strrchr(argv[0], '/');
		progname = (slash != NULL && slash[1] != '\0') ? slash + 1 : argv[0];
	}

	parse_args(argc, argv, &domainname);
	if (domainname != NULL)
		set_domainname(domainname);
	else
		print_domainname();

	return (0);
}

static void
parse_args(int argc, char *argv[], const char **domainname)
{
	int ch;

	opterr = 0;
	while ((ch = getopt(argc, argv, "")) != -1) {
		switch (ch) {
		case '?':
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc > 1)
		usage();

	*domainname = (argc == 1) ? argv[0] : NULL;
}

static void
print_domainname(void)
{
	struct utsname uts;

	if (uname(&uts) != 0)
		die_errno("uname");

	if (fputs(uts.domainname, stdout) == EOF || fputc('\n', stdout) == EOF)
		die_errno("stdout");
}

static void
set_domainname(const char *domainname)
{
	size_t len;
	size_t max_len;

	len = strlen(domainname);
	max_len = linux_domainname_max();
	if (len > max_len) {
		diex("domain name too long for Linux UTS namespace: %zu bytes given, "
		    "limit is %zu", len, max_len);
	}

	if (setdomainname(domainname, len) != 0)
		die_errno("setdomainname");
}

static size_t
linux_domainname_max(void)
{
	struct utsname uts;

	return (sizeof(uts.domainname) - 1);
}

static void
die_errno(const char *what)
{
	fprintf(stderr, "%s: %s: %s\n", progname, what, strerror(errno));
	exit(1);
}

static void
diex(const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "%s: ", progname);

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	fputc('\n', stderr);
	exit(1);
}

static void
usage(void)
{
	fprintf(stderr, "usage: %s [ypdomain]\n", progname);
	exit(1);
}
