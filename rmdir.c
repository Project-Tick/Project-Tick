#define _POSIX_C_SOURCE 200809L

/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
	const char *progname;
	bool remove_parents;
	bool verbose;
	int exit_status;
} options_t;

static void error_errno(options_t *options, const char *fmt, ...);
static int print_removed(options_t *options, const char *path);
static const char *program_basename(const char *path);
static size_t parent_path_length(const char *path);
static int remove_directory(options_t *options, const char *path,
    const char *display_path);
static int remove_operand(options_t *options, const char *path);
static int remove_parent_directories(options_t *options, const char *path);
static void trim_trailing_slashes(char *path);
static void usage(void) __attribute__((noreturn));
static void *xmalloc(size_t size);

static const char *
program_basename(const char *path)
{
	const char *slash;

	if (path == NULL || path[0] == '\0') {
		return "rmdir";
	}

	slash = strrchr(path, '/');
	return slash == NULL ? path : slash + 1;
}

static void
verror_message(options_t *options, bool with_errno, const char *fmt, va_list ap)
{
	int saved_errno;

	saved_errno = errno;
	(void)fprintf(stderr, "%s: ", options->progname);
	(void)vfprintf(stderr, fmt, ap);
	if (with_errno) {
		(void)fprintf(stderr, ": %s", strerror(saved_errno));
	}
	(void)fputc('\n', stderr);
}

static void
error_errno(options_t *options, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	verror_message(options, true, fmt, ap);
	va_end(ap);
	options->exit_status = 1;
}

static void *
xmalloc(size_t size)
{
	void *ptr;

	ptr = malloc(size);
	if (ptr == NULL) {
		(void)fprintf(stderr, "rmdir: out of memory\n");
		exit(1);
	}
	return ptr;
}

static int
print_removed(options_t *options, const char *path)
{
	if (!options->verbose) {
		return 0;
	}
	if (printf("%s\n", path) < 0) {
		error_errno(options, "stdout");
		return 1;
	}
	return 0;
}

static int
remove_directory(options_t *options, const char *path, const char *display_path)
{
	if (unlinkat(AT_FDCWD, path, AT_REMOVEDIR) == -1) {
		error_errno(options, "%s", display_path);
		return 1;
	}
	return print_removed(options, display_path);
}

static void
trim_trailing_slashes(char *path)
{
	size_t len;

	len = strlen(path);
	while (len > 1 && path[len - 1] == '/') {
		path[len - 1] = '\0';
		--len;
	}
}

static size_t
parent_path_length(const char *path)
{
	size_t len;

	len = strlen(path);
	while (len > 0 && path[len - 1] == '/') {
		--len;
	}
	while (len > 0 && path[len - 1] != '/') {
		--len;
	}
	while (len > 0 && path[len - 1] == '/') {
		--len;
	}

	if (len == 0 && path[0] == '/') {
		return 1;
	}
	return len;
}

static int
remove_parent_directories(options_t *options, const char *path)
{
	char *copy;
	size_t parent_len;

	copy = xmalloc(strlen(path) + 1);
	memcpy(copy, path, strlen(path) + 1);
	trim_trailing_slashes(copy);

	for (;;) {
		parent_len = parent_path_length(copy);
		if (parent_len == 0 || (parent_len == 1 && copy[0] == '/')) {
			break;
		}

		copy[parent_len] = '\0';
		if (remove_directory(options, copy, copy) != 0) {
			free(copy);
			return 1;
		}
	}

	free(copy);
	return 0;
}

static int
remove_operand(options_t *options, const char *path)
{
	int status;

	status = remove_directory(options, path, path);
	if (status != 0 || !options->remove_parents) {
		return status;
	}
	return remove_parent_directories(options, path);
}

static void
usage(void)
{
	(void)fprintf(stderr, "%s\n", "usage: rmdir [-pv] directory ...");
	exit(2);
}

int
main(int argc, char *argv[])
{
	options_t options;
	int ch;
	int i;

	options.progname = program_basename(argv[0]);
	options.remove_parents = false;
	options.verbose = false;
	options.exit_status = 0;

	opterr = 0;
	while ((ch = getopt(argc, argv, "pv")) != -1) {
		switch (ch) {
		case 'p':
			options.remove_parents = true;
			break;
		case 'v':
			options.verbose = true;
			break;
		case '?':
		default:
			usage();
		}
	}

	if (optind >= argc) {
		usage();
	}

	for (i = optind; i < argc; ++i) {
		(void)remove_operand(&options, argv[i]);
	}

	if (fflush(stdout) == EOF) {
		error_errno(&options, "stdout");
	}

	return options.exit_status;
}
