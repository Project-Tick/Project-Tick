/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2026 Project Tick. All rights reserved.
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
 *
 * Linux-native port: replaces BSD kinfo_proc with a Linux-compatible equivalent
 * that can be filled from /proc.
 */

#ifndef _PS_H_
#define _PS_H_

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stdbool.h>

/* Minimal STAILQ for musl-gcc compatibility */
#define STAILQ_HEAD(name, type) \
struct name { struct type *stqh_first; struct type **stqh_last; }
#define STAILQ_HEAD_INITIALIZER(head) { NULL, &(head).stqh_first }
#define STAILQ_ENTRY(type) \
struct { struct type *stqe_next; }
#define STAILQ_FIRST(head) ((head)->stqh_first)
#define STAILQ_NEXT(elm, field) ((elm)->field.stqe_next)
#define STAILQ_EMPTY(head) ((head)->stqh_first == NULL)
#define STAILQ_INIT(head) do { (head)->stqh_first = NULL; (head)->stqh_last = &(head)->stqh_first; } while (0)
#define STAILQ_INSERT_TAIL(head, elm, field) do { \
	(elm)->field.stqe_next = NULL; \
	*(head)->stqh_last = (elm); \
	(head)->stqh_last = &(elm)->field.stqe_next; \
} while (0)
#define STAILQ_FOREACH(var, head, field) \
	for ((var) = STAILQ_FIRST(head); (var); (var) = STAILQ_NEXT(var, field))
#define STAILQ_CONCAT(head1, head2) do { \
	if (!STAILQ_EMPTY(head2)) { \
		*(head1)->stqh_last = (head2)->stqh_first; \
		(head1)->stqh_last = (head2)->stqh_last; \
		STAILQ_INIT(head2); \
	} \
} while (0)
#define STAILQ_REMOVE_HEAD(head, field) do { \
	if (((head)->stqh_first = (head)->stqh_first->field.stqe_next) == NULL) \
		(head)->stqh_last = &(head)->stqh_first; \
} while (0)

#define UNLIMITED 0

#ifndef __unused
#define __unused __attribute__((__unused__))
#endif

#define COMMLEN 256
#define WMESGLEN 64
#define KI_NGROUPS 16

/* Linux-port version of kinfo_proc to store data from /proc */
struct kinfo_proc {
	pid_t    ki_pid;
	pid_t    ki_ppid;
	pid_t    ki_pgid;
	pid_t    ki_sid;
	dev_t    ki_tdev;
	pid_t    ki_tpgid;
	uid_t    ki_uid;
	uid_t    ki_ruid;
	uid_t    ki_svuid;
	gid_t    ki_groups[KI_NGROUPS];
	gid_t    ki_rgid;
	gid_t    ki_svgid;
	char     ki_comm[COMMLEN];
	char     ki_tdname[64];
	char     ki_moretdname[64];
	struct timeval ki_start;
	uint64_t ki_runtime; // in microseconds
	uint64_t ki_size;    // VSZ in bytes
	uint64_t ki_rssize;  // RSS in pages
	uint64_t ki_tsize;   // Text size in pages (hard to get precise on Linux)
	uint64_t ki_dsize;   // Data size in pages
	uint64_t ki_ssize;   // Stack size in pages
	int      ki_nice;
	int      ki_pri_level;
	int      ki_pri_class;
	char     ki_stat;    // BSD-like state character (S, R, T, Z, D)
	long     ki_flag;    // Linux task flags
	char     ki_wmesg[WMESGLEN];
	int      ki_numthreads;
	long     ki_slptime; // time since last running (approximate)
	int      ki_oncpu;
	int      ki_lastcpu;
	int      ki_jid;     // Always 0 on Linux
	char     ki_login[64]; // placeholder for logname
	struct rusage ki_rusage; // approximate from /proc/pid/stat
	struct timeval ki_childtime; // placeholder for sumrusage
	struct timeval ki_childstime; 
	struct timeval ki_childutime;
};

/* Compatibility aliases for FreeBSD KVM-based fields used in print.c */
#define ki_pri ki_pri_level

typedef long fixpt_t;
typedef uint64_t segsz_t;

enum type { UNSPEC, CHAR, UCHAR, SHORT, USHORT, INT, UINT, LONG, ULONG, KPTR, PGTOK };

typedef struct kinfo_str {
	STAILQ_ENTRY(kinfo_str) ks_next;
	char *ks_str;	/* formatted string */
} KINFO_STR;

typedef struct kinfo {
	struct kinfo_proc *ki_p;	/* kinfo_proc structure */
	char *ki_args;		/* exec args (heap) */
	char *ki_env;		/* environment (heap) */
	int ki_valid;		/* 1 => data valid */
	double ki_pcpu;		/* calculated in main() */
	segsz_t ki_memsize;	/* calculated in main() */
	union {
		int level;	/* used in descendant_sort() */
		char *prefix;	/* prefix string for tree-view */
	} ki_d;
	STAILQ_HEAD(, kinfo_str) ki_ks;
} KINFO;

struct var;
typedef struct var VAR;

typedef struct varent {
	STAILQ_ENTRY(varent) next_ve;
	const char *header;
	const struct var *var;
	u_int width;
#define VE_KEEP (1 << 0)
	uint16_t flags;
} VARENT;

STAILQ_HEAD(velisthead, varent);

struct var {
	const char *name;
	union {
		const char *aliased;
		const VAR *final_kw;
	};
	const char *header;
	const char *field;
#define COMM 0x01
#define LJUST 0x02
#define USER 0x04
#define INF127 0x10
#define NOINHERIT 0x1000
#define RESOLVING_ALIAS 0x10000
#define RESOLVED_ALIAS 0x20000
	u_int flag;
	char *(*oproc)(struct kinfo *, struct varent *);
	size_t off;
	enum type type;
	const char *fmt;
};

#define KERN_PROC_PROC      0
#define KERN_PROC_THREAD    1
#define KERN_PROC_INC_THREAD 2
#define KERN_PROC_ALL       3

/* Don't define these if they conflict with system headers */
#ifndef NZERO
#define NZERO 20
#endif

/* Linux PRI mapping */
#define PUSER 0

#include "extern.h"

#endif /* _PS_H_ */
