/*

SPDX-License-Identifier: BSD-3-Clause

Copyright (c) 2026
 Project Tick. All rights reserved.

This code is derived from software contributed to Berkeley by
the Institute of Electrical and Electronics Engineers, Inc.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. Neither the name of the University nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE.

*/

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void
die_errno(const char *what)
{
	fprintf(stderr, "fd_helper: %s: %s\n", what, strerror(errno));
	exit(126);
}

static int
parse_fd(const char *text)
{
	char *end;
	long value;

	errno = 0;
	value = strtol(text, &end, 10);
	if (end == text || *end != '\0' || errno == ERANGE ||
	    value < 0 || value > INT_MAX) {
		fprintf(stderr, "fd_helper: invalid file descriptor: %s\n", text);
		exit(126);
	}
	return (int)value;
}

int
main(int argc, char **argv)
{
	int fd;
	int master_fd;
	int slave_fd;
	char *slave_name;

	if (argc < 4) {
		fprintf(stderr, "usage: fd_helper fd program arg ...\n");
		return 126;
	}

	fd = parse_fd(argv[1]);

	master_fd = posix_openpt(O_RDWR | O_NOCTTY);
	if (master_fd < 0)
		die_errno("posix_openpt");
	if (grantpt(master_fd) != 0)
		die_errno("grantpt");
	if (unlockpt(master_fd) != 0)
		die_errno("unlockpt");

	slave_name = ptsname(master_fd);
	if (slave_name == NULL)
		die_errno("ptsname");

	slave_fd = open(slave_name, O_RDWR | O_NOCTTY);
	if (slave_fd < 0)
		die_errno("open slave pty");

	if (dup2(slave_fd, fd) < 0)
		die_errno("dup2");

	if (!isatty(fd))
		die_errno("isatty");

	if (slave_fd != fd)
		close(slave_fd);
	close(master_fd);

	execv(argv[2], &argv[2]);
	die_errno("execv");
}
