/*-
 * SPDX-License-Identifier: BSD-3-Clause
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

#include "flags.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef FS_IOC_GETFLAGS
#define FS_IOC_GETFLAGS _IOR('f', 1, long)
#endif

#ifndef FS_IOC_SETFLAGS
#define FS_IOC_SETFLAGS _IOW('f', 2, long)
#endif

#ifndef FS_SECRM_FL
#define FS_SECRM_FL 0x00000001
#endif

#ifndef FS_UNRM_FL
#define FS_UNRM_FL 0x00000002
#endif

#ifndef FS_COMPR_FL
#define FS_COMPR_FL 0x00000004
#endif

#ifndef FS_SYNC_FL
#define FS_SYNC_FL 0x00000008
#endif

#ifndef FS_IMMUTABLE_FL
#define FS_IMMUTABLE_FL 0x00000010
#endif

#ifndef FS_APPEND_FL
#define FS_APPEND_FL 0x00000020
#endif

#ifndef FS_NODUMP_FL
#define FS_NODUMP_FL 0x00000040
#endif

#ifndef FS_NOATIME_FL
#define FS_NOATIME_FL 0x00000080
#endif

#ifndef FS_DIRSYNC_FL
#define FS_DIRSYNC_FL 0x00010000
#endif

#ifndef FS_TOPDIR_FL
#define FS_TOPDIR_FL 0x00020000
#endif

struct flag_name {
	const char *name;
	unsigned long set;
	unsigned long clear;
};

static const struct flag_name flag_names[] = {
	{ "nodump", FS_NODUMP_FL, 0 },
	{ "dump", 0, FS_NODUMP_FL },
	{ "uappnd", FS_APPEND_FL, 0 },
	{ "uappend", FS_APPEND_FL, 0 },
	{ "appnd", FS_APPEND_FL, 0 },
	{ "append", FS_APPEND_FL, 0 },
	{ "sappnd", FS_APPEND_FL, 0 },
	{ "sappend", FS_APPEND_FL, 0 },
	{ "nouappnd", 0, FS_APPEND_FL },
	{ "nouappend", 0, FS_APPEND_FL },
	{ "noappnd", 0, FS_APPEND_FL },
	{ "noappend", 0, FS_APPEND_FL },
	{ "nosappnd", 0, FS_APPEND_FL },
	{ "nosappend", 0, FS_APPEND_FL },
	{ "uchg", FS_IMMUTABLE_FL, 0 },
	{ "uimmutable", FS_IMMUTABLE_FL, 0 },
	{ "chg", FS_IMMUTABLE_FL, 0 },
	{ "immutable", FS_IMMUTABLE_FL, 0 },
	{ "schg", FS_IMMUTABLE_FL, 0 },
	{ "simmutable", FS_IMMUTABLE_FL, 0 },
	{ "nouchg", 0, FS_IMMUTABLE_FL },
	{ "nouimmutable", 0, FS_IMMUTABLE_FL },
	{ "nochg", 0, FS_IMMUTABLE_FL },
	{ "noimmutable", 0, FS_IMMUTABLE_FL },
	{ "noschg", 0, FS_IMMUTABLE_FL },
	{ "nosimmutable", 0, FS_IMMUTABLE_FL },
	{ "noatime", FS_NOATIME_FL, 0 },
	{ "atime", 0, FS_NOATIME_FL },
	{ "sync", FS_SYNC_FL, 0 },
	{ "nosync", 0, FS_SYNC_FL },
	{ "dirsync", FS_DIRSYNC_FL, 0 },
	{ "nodirsync", 0, FS_DIRSYNC_FL },
	{ "topdir", FS_TOPDIR_FL, 0 },
	{ "notopdir", 0, FS_TOPDIR_FL },
	{ "compr", FS_COMPR_FL, 0 },
	{ "compress", FS_COMPR_FL, 0 },
	{ "nocompr", 0, FS_COMPR_FL },
	{ "nocompress", 0, FS_COMPR_FL },
};

static int
open_flag_target(int dirfd, const char *path, int atflags)
{
	struct stat st;
	int fd;
	int open_flags;

	if ((atflags & AT_SYMLINK_NOFOLLOW) != 0) {
		if (fstatat(dirfd, path, &st, AT_SYMLINK_NOFOLLOW) != 0)
			return (-1);
		if (S_ISLNK(st.st_mode)) {
			errno = EOPNOTSUPP;
			return (-1);
		}
	}

	open_flags = O_RDONLY | O_NONBLOCK | O_CLOEXEC;
	if ((atflags & AT_SYMLINK_NOFOLLOW) != 0)
		open_flags |= O_NOFOLLOW;
	fd = openat(dirfd, path, open_flags);
	return (fd);
}

int
getflagsat(int dirfd, const char *path, unsigned long *flagsp, int atflags)
{
	int fd;
	int iflags;

	fd = open_flag_target(dirfd, path, atflags);
	if (fd < 0)
		return (-1);
	if (ioctl(fd, FS_IOC_GETFLAGS, &iflags) != 0) {
		int saved_errno;

		saved_errno = errno;
		close(fd);
		errno = saved_errno;
		return (-1);
	}
	close(fd);
	*flagsp = (unsigned long)(unsigned int)iflags;
	return (0);
}

int
chflagsat(int dirfd, const char *path, unsigned long flags, int atflags)
{
	int fd;
	int iflags;

	fd = open_flag_target(dirfd, path, atflags);
	if (fd < 0)
		return (-1);
	iflags = (int)flags;
	if (ioctl(fd, FS_IOC_SETFLAGS, &iflags) != 0) {
		int saved_errno;

		saved_errno = errno;
		close(fd);
		errno = saved_errno;
		return (-1);
	}
	close(fd);
	return (0);
}

int
strtofflags(char **stringp, unsigned long *setp, unsigned long *clrp)
{
	char *flags;
	char *copy;
	char *saveptr;
	char *token;
	size_t i;

	*setp = 0;
	*clrp = 0;
	flags = *stringp;
	copy = strdup(flags);
	if (copy == NULL)
		return (-1);

	for (token = strtok_r(copy, ",", &saveptr);
	    token != NULL;
	    token = strtok_r(NULL, ",", &saveptr)) {
		bool found;

		while (isspace((unsigned char)*token))
			token++;
		if (*token == '\0') {
			free(copy);
			*stringp = flags;
			return (-1);
		}

		found = false;
		for (i = 0; i < sizeof(flag_names) / sizeof(flag_names[0]); i++) {
			if (strcmp(token, flag_names[i].name) == 0) {
				*setp |= flag_names[i].set;
				*clrp |= flag_names[i].clear;
				found = true;
				break;
			}
		}
		if (!found) {
			free(copy);
			*stringp = flags;
			return (-1);
		}
	}

	free(copy);
	return (0);
}
