/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Copyright (c) 2026
 *	Project Tick. All rights reserved.
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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum hostname_mode {
	HOSTNAME_FULL,
	HOSTNAME_SHORT,
	HOSTNAME_DOMAIN
};

struct options {
	enum hostname_mode mode;
	const char *set_name;
};

static const char *progname = "hostname";

static void	die_errno(const char *what) __attribute__((noreturn));
static void	diex(const char *fmt, ...) __attribute__((format(printf, 1, 2),
		    noreturn));
static char	*dup_hostname(void);
static size_t	linux_hostname_max(void);
static void	parse_args(int argc, char *argv[], struct options *options);
static void	print_hostname(enum hostname_mode mode);
static void	set_hostname(const char *hostname);
static void	usage(void) __attribute__((noreturn));

int
main(int argc, char *argv[])
{
	struct options options;
	const char *slash;

	if (argv[0] != NULL && argv[0][0] != '\0') {
		slash = strrchr(argv[0], '/');
		progname = (slash != NULL && slash[1] != '\0') ? slash + 1 : argv[0];
	}

	parse_args(argc, argv, &options);
	if (options.set_name != NULL)
		set_hostname(options.set_name);
	else
		print_hostname(options.mode);

	return (0);
}

static void
parse_args(int argc, char *argv[], struct options *options)
{
	bool mode_selected;
	int ch;

	options->mode = HOSTNAME_FULL;
	options->set_name = NULL;
	mode_selected = false;

	opterr = 0;
	while ((ch = getopt(argc, argv, "fsd")) != -1) {
		switch (ch) {
		case 'f':
			diex("option -f is not supported on Linux: FQDN resolution "
			    "depends on NSS/DNS, not the kernel hostname");
		case 's':
			if (mode_selected)
				usage();
			options->mode = HOSTNAME_SHORT;
			mode_selected = true;
			break;
		case 'd':
			if (mode_selected)
				usage();
			options->mode = HOSTNAME_DOMAIN;
			mode_selected = true;
			break;
		case '?':
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc > 1)
		usage();

	if (argc == 1) {
		if (options->mode != HOSTNAME_FULL)
			usage();
		options->set_name = argv[0];
	}
}

static char *
dup_hostname(void)
{
	struct utsname uts;
	size_t len;
	char *name;

	if (uname(&uts) != 0)
		die_errno("uname");

	len = strnlen(uts.nodename, sizeof(uts.nodename));
	if (len >= sizeof(uts.nodename))
		diex("uname returned a non-terminated nodename");

	name = malloc(len + 1);
	if (name == NULL)
		die_errno("malloc");

	memcpy(name, uts.nodename, len + 1);
	return (name);
}

static void
print_hostname(enum hostname_mode mode)
{
	char *hostname;
	char *dot;
	const char *out;

	hostname = dup_hostname();
	out = hostname;
	dot = strchr(hostname, '.');

	switch (mode) {
	case HOSTNAME_FULL:
		break;
	case HOSTNAME_SHORT:
		if (dot != NULL)
			*dot = '\0';
		break;
	case HOSTNAME_DOMAIN:
		out = (dot != NULL) ? dot + 1 : "";
		break;
	}

	if (fputs(out, stdout) == EOF || fputc('\n', stdout) == EOF)
		die_errno("stdout");

	free(hostname);
}

static void
set_hostname(const char *hostname)
{
	size_t len;
	size_t max_len;

	len = strlen(hostname);
	max_len = linux_hostname_max();
	if (len > max_len) {
		diex("host name too long for Linux UTS namespace: %zu bytes given, "
		    "limit is %zu", len, max_len);
	}

	if (sethostname(hostname, len) != 0)
		die_errno("sethostname");
}

static size_t
linux_hostname_max(void)
{
	struct utsname uts;

	return (sizeof(uts.nodename) - 1);
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
	fprintf(stderr, "usage: %s [-s | -d] [name-of-host]\n", progname);
	exit(1);
}
