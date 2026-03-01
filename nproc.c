/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Mateusz Guzik
 * Copyright (c) 2026 Project Tick
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

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <sched.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum command_mode {
	MODE_RUN,
	MODE_HELP,
	MODE_VERSION
};

struct options {
	enum command_mode mode;
	bool all;
	uintmax_t ignore;
};

static const char *progname = "nproc";

static void die(const char *fmt, ...) __attribute__((__noreturn__, format(printf, 1, 2)));
static void die_errno(const char *what) __attribute__((__noreturn__));
static void help(FILE *stream);
static size_t available_processors(void);
static size_t count_set_bits(const unsigned char *mask, size_t mask_size);
static size_t initial_mask_size(void);
static size_t online_processors(void);
static void parse_args(int argc, char **argv, struct options *options);
static uintmax_t parse_ignore_count(const char *text);
static size_t subtract_ignore(size_t count, uintmax_t ignore);
static void usage_error(const char *fmt, ...) __attribute__((__noreturn__, format(printf, 1, 2)));
static void version(void);

int
main(int argc, char **argv)
{
	struct options options;
	size_t count;

	if (argv[0] != NULL && argv[0][0] != '\0') {
		const char *slash;

		slash = strrchr(argv[0], '/');
		progname = (slash != NULL && slash[1] != '\0') ? slash + 1 : argv[0];
	}

	parse_args(argc, argv, &options);

	switch (options.mode) {
	case MODE_HELP:
		help(stderr);
		return (EXIT_SUCCESS);
	case MODE_VERSION:
		version();
		return (EXIT_SUCCESS);
	case MODE_RUN:
		break;
	}

	count = options.all ? online_processors() : available_processors();
	count = subtract_ignore(count, options.ignore);

	if (printf("%zu\n", count) < 0)
		die_errno("stdout");

	return (EXIT_SUCCESS);
}

static void
parse_args(int argc, char **argv, struct options *options)
{
	int i;

	memset(options, 0, sizeof(*options));
	options->mode = MODE_RUN;

	for (i = 1; i < argc; i++) {
		const char *arg;

		arg = argv[i];
		if (strcmp(arg, "--") == 0) {
			i++;
			break;
		}
		if (strcmp(arg, "--all") == 0) {
			options->all = true;
			continue;
		}
		if (strcmp(arg, "--help") == 0) {
			if (argc != 2)
				usage_error("option --help must be used alone");
			options->mode = MODE_HELP;
			return;
		}
		if (strcmp(arg, "--version") == 0) {
			if (argc != 2)
				usage_error("option --version must be used alone");
			options->mode = MODE_VERSION;
			return;
		}
		if (strcmp(arg, "--ignore") == 0) {
			if (i + 1 >= argc)
				usage_error("option requires an argument: --ignore");
			i++;
			options->ignore = parse_ignore_count(argv[i]);
			continue;
		}
		if (strncmp(arg, "--ignore=", 9) == 0) {
			options->ignore = parse_ignore_count(arg + 9);
			continue;
		}
		if (arg[0] == '-') {
			usage_error("unknown option: %s", arg);
		}
		break;
	}

	if (i < argc)
		usage_error("unexpected operand: %s", argv[i]);
}

static uintmax_t
parse_ignore_count(const char *text)
{
	char *end;
	uintmax_t value;
	const unsigned char *p;

	if (text[0] == '\0')
		die("invalid ignore count: %s", text);

	for (p = (const unsigned char *)text; *p != '\0'; p++) {
		if (!isdigit(*p))
			die("invalid ignore count: %s", text);
	}

	errno = 0;
	value = strtoumax(text, &end, 10);
	if (errno == ERANGE || *end != '\0')
		die("invalid ignore count: %s", text);
	return (value);
}

static size_t
online_processors(void)
{
	long value;

	errno = 0;
	value = sysconf(_SC_NPROCESSORS_ONLN);
	if (value < 1) {
		if (errno != 0)
			die_errno("sysconf(_SC_NPROCESSORS_ONLN)");
		die("sysconf(_SC_NPROCESSORS_ONLN) returned %ld", value);
	}

	return ((size_t)value);
}

static size_t
initial_mask_size(void)
{
	long configured;
	size_t bits;
	size_t bytes;

	errno = 0;
	configured = sysconf(_SC_NPROCESSORS_CONF);
	if (configured < 1 || configured > LONG_MAX / 2) {
		if (errno != 0)
			return (128);
		return (128);
	}

	bits = (size_t)configured;
	if (bits < CHAR_BIT)
		bits = CHAR_BIT;

	bytes = bits / CHAR_BIT;
	if ((bits % CHAR_BIT) != 0)
		bytes++;
	if (bytes < sizeof(unsigned long))
		bytes = sizeof(unsigned long);
	return (bytes);
}

static size_t
available_processors(void)
{
	unsigned char *mask;
	size_t count;
	size_t mask_size;

	mask_size = initial_mask_size();

	for (;;) {
		int saved_errno;

		mask = calloc(1, mask_size);
		if (mask == NULL)
			die_errno("calloc");

		if (sched_getaffinity(0, mask_size, (cpu_set_t *)(void *)mask) == 0) {
			count = count_set_bits(mask, mask_size);
			free(mask);
			if (count == 0)
				die("sched_getaffinity returned an empty CPU mask");
			return (count);
		}

		saved_errno = errno;
		free(mask);
		if (saved_errno != EINVAL) {
			errno = saved_errno;
			die_errno("sched_getaffinity");
		}
		if (mask_size > SIZE_MAX / 2)
			die("sched_getaffinity CPU mask is too large");
		mask_size *= 2;
	}
}

static size_t
count_set_bits(const unsigned char *mask, size_t mask_size)
{
	size_t count;
	size_t i;

	count = 0;
	for (i = 0; i < mask_size; i++) {
		unsigned int bits;

		bits = mask[i];
		while (bits != 0) {
			count++;
			bits &= bits - 1;
		}
	}

	return (count);
}

static size_t
subtract_ignore(size_t count, uintmax_t ignore)
{
	if (ignore >= count)
		return (1);
	return (count - (size_t)ignore);
}

static void
help(FILE *stream)
{
	if (fprintf(stream,
	    "usage: %s [--all] [--ignore=count]\n"
	    "       %s --help\n"
	    "       %s --version\n",
	    progname, progname, progname) < 0) {
		if (stream == stdout)
			die_errno("stdout");
		die_errno("stderr");
	}
}

static void
version(void)
{
	if (printf("%s (neither_GNU nor_coreutils) 8.32\n", progname) < 0)
		die_errno("stdout");
}

static void
usage_error(const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "%s: ", progname);

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	fputc('\n', stderr);
	help(stderr);
	exit(EXIT_FAILURE);
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
	exit(EXIT_FAILURE);
}

static void
die_errno(const char *what)
{
	fprintf(stderr, "%s: %s: %s\n", progname, what, strerror(errno));
	exit(EXIT_FAILURE);
}
