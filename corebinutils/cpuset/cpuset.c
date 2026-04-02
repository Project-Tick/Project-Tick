/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2007, 2008 	Jeffrey Roberson <jeff@freebsd.org>
 * All rights reserved.
 *
 * Copyright (c) 2008 Nokia Corporation
 * All rights reserved.
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

#include <sys/types.h>
#include <sys/syscall.h>

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <sched.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct options {
	bool get_only;
	bool have_cpu_list;
	bool target_is_pid;
	bool target_is_tid;
	const char *cpu_list;
	pid_t target_id;
};

static const char *progname = "cpuset";

static void usage(void) __attribute__((__noreturn__));
static void die(const char *fmt, ...) __attribute__((__noreturn__, format(printf, 1, 2)));
static void die_errno(const char *fmt, ...) __attribute__((__noreturn__, format(printf, 1, 2)));

static void parse_args(int argc, char **argv, struct options *options, int *command_index);
static void validate_options(const struct options *options, bool has_command);
static long parse_long_strict(const char *text, const char *what);
static long current_tid(void);
static int current_affinity_count(void);
static cpu_set_t *alloc_cpuset(size_t *setsize, int *cpu_count);
static void parse_cpu_list(const char *text, cpu_set_t *mask, size_t setsize, int cpu_count);
static void print_mask(const cpu_set_t *mask, size_t setsize, int cpu_count);
static int sched_target(const struct options *options);
static void print_affinity(const struct options *options);
static void set_affinity(const struct options *options);

int
main(int argc, char **argv)
{
	struct options options;
	int command_index;
	bool has_command;

	if (argv[0] != NULL && argv[0][0] != '\0') {
		const char *base;

		base = strrchr(argv[0], '/');
		progname = base != NULL ? base + 1 : argv[0];
	}

	parse_args(argc, argv, &options, &command_index);
	has_command = command_index < argc;
	validate_options(&options, has_command);

	if (options.get_only) {
		print_affinity(&options);
		return (EXIT_SUCCESS);
	}

	if (options.have_cpu_list)
		set_affinity(&options);

	if (has_command) {
		errno = 0;
		execvp(argv[command_index], &argv[command_index]);
		if (errno == ENOENT)
			exit(127);
		die_errno("%s", argv[command_index]);
	}

	return (EXIT_SUCCESS);
}

static void
parse_args(int argc, char **argv, struct options *options, int *command_index)
{
	int ch;

	memset(options, 0, sizeof(*options));
	options->target_id = -1;

	optind = 1;
	while ((ch = getopt(argc, argv, "+Ccd:gij:l:n:p:rs:t:x:")) != -1) {
		switch (ch) {
		case 'g':
			options->get_only = true;
			break;
		case 'l':
			options->have_cpu_list = true;
			options->cpu_list = optarg;
			break;
		case 'p':
			options->target_is_pid = true;
			options->target_id = (pid_t)parse_long_strict(optarg, "pid");
			break;
		case 't':
			options->target_is_tid = true;
			options->target_id = (pid_t)parse_long_strict(optarg, "tid");
			break;
		case 'C':
		case 'c':
		case 'd':
		case 'i':
		case 'j':
		case 'n':
		case 'r':
		case 's':
		case 'x':
			die("option -%c is not supported on Linux", ch);
		default:
			usage();
		}
	}

	*command_index = optind;
}

static void
validate_options(const struct options *options, bool has_command)
{
	if (options->target_is_pid && options->target_is_tid)
		die("choose only one target: -p or -t");
	if (options->get_only && options->have_cpu_list)
		usage();
	if (options->get_only && has_command)
		usage();
	if (!options->get_only && !options->have_cpu_list && !has_command)
		usage();
	if (has_command && (options->target_is_pid || options->target_is_tid))
		usage();
	if (!options->get_only && !options->have_cpu_list)
		usage();
}

static long
parse_long_strict(const char *text, const char *what)
{
	char *end;
	long value;

	errno = 0;
	value = strtol(text, &end, 10);
	if (errno != 0 || text[0] == '\0' || *end != '\0')
		die("invalid %s: %s", what, text);
	if (value < 0 || value > INT_MAX)
		die("%s out of range: %s", what, text);
	return (value);
}

static long
current_tid(void)
{
	return ((long)syscall(SYS_gettid));
}

static int
current_affinity_count(void)
{
	long configured;

	configured = sysconf(_SC_NPROCESSORS_CONF);
	if (configured > 0 && configured <= INT_MAX)
		return ((int)configured);
	return (CPU_SETSIZE);
}

static cpu_set_t *
alloc_cpuset(size_t *setsize, int *cpu_count)
{
	cpu_set_t *mask;

	*cpu_count = current_affinity_count();
	if (*cpu_count < 1)
		*cpu_count = 1;

	mask = CPU_ALLOC((size_t)*cpu_count);
	if (mask == NULL)
		die_errno("CPU_ALLOC");
	*setsize = CPU_ALLOC_SIZE((size_t)*cpu_count);
	CPU_ZERO_S(*setsize, mask);
	return (mask);
}

static void
parse_cpu_list(const char *text, cpu_set_t *mask, size_t setsize, int cpu_count)
{
	const char *p;

	CPU_ZERO_S(setsize, mask);
	p = text;
	while (*p != '\0') {
		long start;
		long end;
		char *next;

		while (isspace((unsigned char)*p))
			p++;
		if (*p == '\0')
			break;
		if (!isdigit((unsigned char)*p))
			die("invalid cpu list: %s", text);

		errno = 0;
		start = strtol(p, &next, 10);
		if (errno != 0 || start < 0 || start >= cpu_count)
			die("invalid cpu list: %s", text);
		end = start;
		p = next;

		if (*p == '-') {
			p++;
			if (!isdigit((unsigned char)*p))
				die("invalid cpu list: %s", text);
			errno = 0;
			end = strtol(p, &next, 10);
			if (errno != 0 || end < start || end >= cpu_count)
				die("invalid cpu list: %s", text);
			p = next;
		}

		for (long cpu = start; cpu <= end; cpu++)
			CPU_SET_S((int)cpu, setsize, mask);

		while (isspace((unsigned char)*p))
			p++;
		if (*p == ',') {
			p++;
			continue;
		}
		if (*p != '\0')
			die("invalid cpu list: %s", text);
	}

	if (CPU_COUNT_S(setsize, mask) == 0)
		die("invalid cpu list: %s", text);
}

static void
print_mask(const cpu_set_t *mask, size_t setsize, int cpu_count)
{
	bool first;

	first = true;
	for (int cpu = 0; cpu < cpu_count; cpu++) {
		if (!CPU_ISSET_S(cpu, setsize, mask))
			continue;
		if (!first)
			fputs(", ", stdout);
		printf("%d", cpu);
		first = false;
	}
	putchar('\n');
}

static int
sched_target(const struct options *options)
{
	if (options->target_is_pid || options->target_is_tid)
		return ((int)options->target_id);
	return (0);
}

static void
print_affinity(const struct options *options)
{
	cpu_set_t *mask;
	size_t setsize;
	int cpu_count;
	int target;

	mask = alloc_cpuset(&setsize, &cpu_count);
	target = sched_target(options);
	if (sched_getaffinity(target, setsize, mask) != 0) {
		CPU_FREE(mask);
		die_errno("sched_getaffinity");
	}

	if (options->target_is_pid)
		printf("pid %jd mask: ", (intmax_t)options->target_id);
	else if (options->target_is_tid)
		printf("tid %jd mask: ", (intmax_t)options->target_id);
	else
		printf("tid %ld mask: ", current_tid());
	print_mask(mask, setsize, cpu_count);
	CPU_FREE(mask);
}

static void
set_affinity(const struct options *options)
{
	cpu_set_t *mask;
	size_t setsize;
	int cpu_count;
	int target;

	mask = alloc_cpuset(&setsize, &cpu_count);
	parse_cpu_list(options->cpu_list, mask, setsize, cpu_count);
	target = sched_target(options);
	if (sched_setaffinity(target, setsize, mask) != 0) {
		CPU_FREE(mask);
		die_errno("sched_setaffinity");
	}
	CPU_FREE(mask);
}

static void
usage(void)
{
	fprintf(stderr, "usage: %s [-l cpu-list] cmd ...\n", progname);
	fprintf(stderr, "       %s [-l cpu-list] [-p pid | -t tid]\n", progname);
	fprintf(stderr, "       %s -g [-p pid | -t tid]\n", progname);
	fprintf(stderr,
	    "note: Linux port supports affinity operations only; FreeBSD cpuset,\n");
	fprintf(stderr,
	    "      jail, irq, cpuset-id, and domain policy options are unavailable.\n");
	exit(1);
}

static void
die(const char *fmt, ...)
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
die_errno(const char *fmt, ...)
{
	va_list ap;
	int saved_errno;

	saved_errno = errno;
	fprintf(stderr, "%s: ", progname);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, ": %s\n", strerror(saved_errno));
	exit(1);
}
