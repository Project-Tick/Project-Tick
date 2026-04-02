/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1989, 1993
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

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int
write_all(const char *data, size_t len)
{
	while (len > 0) {
		ssize_t written;

		written = write(STDOUT_FILENO, data, len);
		if (written < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		data += (size_t)written;
		len -= (size_t)written;
	}

	return 0;
}

static void
warn_errno(const char *context)
{
	fprintf(stderr, "echo: %s: %s\n", context, strerror(errno));
}

static bool
trim_trailing_backslash_c(const char *arg, size_t *len)
{
	if (*len < 2)
		return false;
	if (arg[*len - 2] != '\\' || arg[*len - 1] != 'c')
		return false;

	*len -= 2;
	return true;
}

int
main(int argc, char *argv[])
{
	bool suppress_newline;
	int first_arg;

	suppress_newline = false;
	first_arg = 1;

	/*
	 * FreeBSD echo intentionally does not use getopt(3); only a leading
	 * literal "-n" is treated as an option.
	 */
	if (argc > 1 && strcmp(argv[1], "-n") == 0) {
		suppress_newline = true;
		first_arg = 2;
	}

	for (int i = first_arg; i < argc; i++) {
		size_t len;

		len = strlen(argv[i]);
		if (i == argc - 1 && trim_trailing_backslash_c(argv[i], &len))
			suppress_newline = true;

		if (write_all(argv[i], len) < 0) {
			warn_errno("write");
			return 1;
		}

		if (i + 1 < argc && write_all(" ", 1) < 0) {
			warn_errno("write");
			return 1;
		}
	}

	if (!suppress_newline && write_all("\n", 1) < 0) {
		warn_errno("write");
		return 1;
	}

	return 0;
}
