/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 */

#pragma once

#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/stat.h>

#include "fts.h"

#ifndef __unused
#define __unused __attribute__((unused))
#endif

typedef struct {
	int dir;
	char base[PATH_MAX + 1];
	char *end;
	char path[PATH_MAX];
} PATH_T;

extern PATH_T to;
extern bool Nflag, fflag, iflag, lflag, nflag, pflag, sflag, vflag;
extern volatile sig_atomic_t info;

int copy_fifo(struct stat *, bool, bool);
int copy_file(const FTSENT *, bool, bool);
int copy_link(const FTSENT *, bool, bool);
int copy_special(struct stat *, bool, bool);
int setfile(struct stat *, int, bool);
int preserve_dir_acls(const char *, const char *);
int preserve_fd_acls(int, int);
void usage(void) __attribute__((noreturn));

#ifdef ENOTCAPABLE
#define warn(...) \
	warnc(errno == ENOTCAPABLE ? EACCES : errno, __VA_ARGS__)
#define err(rv, ...) \
	errc(rv, errno == ENOTCAPABLE ? EACCES : errno, __VA_ARGS__)
#endif
