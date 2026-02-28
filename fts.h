/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2026
 *  Project Tick. All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * David Hitz of Auspex Systems Inc.
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

#pragma once

#include <sys/stat.h>

#include <stddef.h>

typedef struct _ftsent FTSENT;
typedef struct _fts FTS;

struct _ftsent {
	FTSENT *fts_parent;
	char *fts_path;
	char *fts_accpath;
	char *fts_name;
	int fts_errno;
	int fts_info;
	int fts_instr;
	int fts_level;
	long fts_number;
	struct stat *fts_statp;

	FTSENT **_children;
	size_t _child_count;
	size_t _child_index;
	int _state;
};

struct _fts {
	FTSENT **_roots;
	size_t _root_count;
	size_t _root_index;
	int fts_options;
	int (*fts_compar)(const FTSENT * const *, const FTSENT * const *);
	FTSENT *_current;
};

#define FTS_COMFOLLOW 0x0001
#define FTS_LOGICAL   0x0002
#define FTS_NOCHDIR   0x0004
#define FTS_PHYSICAL  0x0008
#define FTS_XDEV      0x0010

#define FTS_ROOTLEVEL 0

#define FTS_D    1
#define FTS_DC   2
#define FTS_DNR  3
#define FTS_DP   4
#define FTS_ERR  5
#define FTS_F    6
#define FTS_NS   7
#define FTS_SL   8

#define FTS_SKIP 1

FTS *fts_open(char * const *paths, int options,
    int (*compar)(const FTSENT * const *, const FTSENT * const *));
FTSENT *fts_read(FTS *ftsp);
int fts_set(FTS *ftsp, FTSENT *f, int instr);
int fts_close(FTS *ftsp);
