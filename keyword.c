/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2025 The FreeBSD Foundation
 * Copyright (c) 2026 Project Tick. All rights reserved.
 *
 * Portions of this software were developed by Olivier Certner
 * <olce@FreeBSD.org> at Kumacom SARL under sponsorship from the FreeBSD
 * Foundation.
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

#include <sys/param.h>
#include <sys/resource.h>
#include <sys/time.h>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>

#include "ps.h"

static int vcmp(const void *, const void *);

#define KOFF(x) offsetof(struct kinfo_proc, x)
#define ROFF(x) offsetof(struct rusage, x)

#define UIDFMT "u"
#define PIDFMT "d"

/* Sorted alphabetically by name */
static VAR keywords[] = {
	{"%cpu", {NULL}, "%CPU", NULL, 0, pcpu, 0, UNSPEC, NULL},
	{"%mem", {NULL}, "%MEM", NULL, 0, pmem, 0, UNSPEC, NULL},
	{"args", {NULL}, "COMMAND", NULL, COMM|LJUST|USER, arguments, 0, UNSPEC, NULL},
	{"comm", {NULL}, "COMMAND", NULL, LJUST, ucomm, 0, UNSPEC, NULL},
	{"command", {NULL}, "COMMAND", NULL, COMM|LJUST|USER, command, 0, UNSPEC, NULL},
	{"cpu", {NULL}, "C", NULL, 0, cpunum, 0, UNSPEC, NULL},
	{"cputime", {"time"}, NULL, NULL, 0, NULL, 0, UNSPEC, NULL},
	{"dsiz", {NULL}, "DSIZ", NULL, 0, kvar, KOFF(ki_dsize), PGTOK, "ld"},
	{"egid", {"gid"}, NULL, NULL, 0, NULL, 0, UNSPEC, NULL},
	{"egroup", {"group"}, NULL, NULL, 0, NULL, 0, UNSPEC, NULL},
	{"emul", {NULL}, "EMUL", NULL, LJUST, emulname, 0, UNSPEC, NULL},
	{"etime", {NULL}, "ELAPSED", NULL, USER, elapsed, 0, UNSPEC, NULL},
	{"etimes", {NULL}, "ELAPSED", NULL, USER, elapseds, 0, UNSPEC, NULL},
	{"euid", {"uid"}, NULL, NULL, 0, NULL, 0, UNSPEC, NULL},
	{"f", {NULL}, "F", NULL, 0, kvar, KOFF(ki_flag), LONG, "lx"},
	{"flags", {"f"}, NULL, NULL, 0, NULL, 0, UNSPEC, NULL},
	{"gid", {NULL}, "GID", NULL, 0, kvar, KOFF(ki_groups), UINT, UIDFMT},
	{"group", {NULL}, "GROUP", NULL, LJUST, egroupname, 0, UNSPEC, NULL},
	{"jail", {NULL}, "JAIL", NULL, LJUST, jailname, 0, UNSPEC, NULL},
	{"jid", {NULL}, "JID", NULL, 0, kvar, KOFF(ki_jid), INT, "d"},
	{"jobc", {NULL}, "JOBC", NULL, 0, kvar, KOFF(ki_sid), INT, "d"}, /* session as jobc proxy */
	{"label", {NULL}, "LABEL", NULL, LJUST, label, 0, UNSPEC, NULL},
	{"lim", {NULL}, "LIM", NULL, 0, maxrss, 0, UNSPEC, NULL},
	{"login", {NULL}, "LOGIN", NULL, LJUST, logname, 0, UNSPEC, NULL},
	{"logname", {"login"}, NULL, NULL, 0, NULL, 0, UNSPEC, NULL},
	{"lstart", {NULL}, "STARTED", NULL, LJUST|USER, lstarted, 0, UNSPEC, NULL},
	{"lwp", {NULL}, "LWP", NULL, 0, kvar, KOFF(ki_pid), UINT, PIDFMT},
	{"mwchan", {NULL}, "MWCHAN", NULL, LJUST, mwchan, 0, UNSPEC, NULL},
	{"ni", {"nice"}, NULL, NULL, 0, NULL, 0, UNSPEC, NULL},
	{"nice", {NULL}, "NI", NULL, 0, kvar, KOFF(ki_nice), CHAR, "d"},
	{"nlwp", {NULL}, "NLWP", NULL, 0, kvar, KOFF(ki_numthreads), UINT, "d"},
	{"nwchan", {NULL}, "NWCHAN", NULL, LJUST, nwchan, 0, UNSPEC, NULL},
	{"pagein", {NULL}, "PAGEIN", NULL, USER, pagein, 0, UNSPEC, NULL},
	{"pcpu", {"%cpu"}, NULL, NULL, 0, NULL, 0, UNSPEC, NULL},
	{"pgid", {NULL}, "PGID", NULL, 0, kvar, KOFF(ki_pgid), UINT, PIDFMT},
	{"pid", {NULL}, "PID", NULL, 0, kvar, KOFF(ki_pid), UINT, PIDFMT},
	{"pmem", {"%mem"}, NULL, NULL, 0, NULL, 0, UNSPEC, NULL},
	{"ppid", {NULL}, "PPID", NULL, 0, kvar, KOFF(ki_ppid), UINT, PIDFMT},
	{"pri", {NULL}, "PRI", NULL, 0, pri, 0, UNSPEC, NULL},
	{"rgid", {NULL}, "RGID", NULL, 0, kvar, KOFF(ki_rgid), UINT, UIDFMT},
	{"rgroup", {NULL}, "RGROUP", NULL, LJUST, rgroupname, 0, UNSPEC, NULL},
	{"rss", {NULL}, "RSS", NULL, 0, kvar, KOFF(ki_rssize), PGTOK, "ld"},
	{"ruid", {NULL}, "RUID", NULL, 0, kvar, KOFF(ki_ruid), UINT, UIDFMT},
	{"ruser", {NULL}, "RUSER", NULL, LJUST, runame, 0, UNSPEC, NULL},
	{"sid", {NULL}, "SID", NULL, 0, kvar, KOFF(ki_sid), UINT, PIDFMT},
	{"sl", {NULL}, "SL", NULL, INF127, kvar, KOFF(ki_slptime), UINT, "d"},
	{"ssiz", {NULL}, "SSIZ", NULL, 0, kvar, KOFF(ki_ssize), PGTOK, "ld"},
	{"start", {NULL}, "STARTED", NULL, LJUST|USER, started, 0, UNSPEC, NULL},
	{"stat", {"state"}, NULL, NULL, 0, NULL, 0, UNSPEC, NULL},
	{"state", {NULL}, "STAT", NULL, LJUST, state, 0, UNSPEC, NULL},
	{"svgid", {NULL}, "SVGID", NULL, 0, kvar, KOFF(ki_svgid), UINT, UIDFMT},
	{"svuid", {NULL}, "SVUID", NULL, 0, kvar, KOFF(ki_svuid), UINT, UIDFMT},
	{"systime", {NULL}, "SYSTIME", NULL, USER, systime, 0, UNSPEC, NULL},
	{"tdev", {NULL}, "TDEV", NULL, 0, tdev, 0, UNSPEC, NULL},
	{"tid", {"lwp"}, NULL, NULL, 0, NULL, 0, UNSPEC, NULL},
	{"time", {NULL}, "TIME", NULL, USER, cputime, 0, UNSPEC, NULL},
	{"tpgid", {NULL}, "TPGID", NULL, 0, kvar, KOFF(ki_tpgid), UINT, PIDFMT},
	{"tsiz", {NULL}, "TSIZ", NULL, 0, kvar, KOFF(ki_tsize), PGTOK, "ld"},
	{"tt", {NULL}, "TT ", NULL, 0, tname, 0, UNSPEC, NULL},
	{"tty", {NULL}, "TTY", NULL, LJUST, longtname, 0, UNSPEC, NULL},
	{"ucomm", {NULL}, "UCOMM", NULL, LJUST, ucomm, 0, UNSPEC, NULL},
	{"uid", {NULL}, "UID", NULL, 0, kvar, KOFF(ki_uid), UINT, UIDFMT},
	{"upr", {NULL}, "UPR", NULL, 0, upr, 0, UNSPEC, NULL},
	{"user", {NULL}, "USER", NULL, LJUST, username, 0, UNSPEC, NULL},
	{"usertime", {NULL}, "USERTIME", NULL, USER, usertime, 0, UNSPEC, NULL},
	{"vsize", {"vsz"}, NULL, NULL, 0, NULL, 0, UNSPEC, NULL},
	{"vsz", {NULL}, "VSZ", NULL, 0, vsize, 0, UNSPEC, NULL},
	{"wchan", {NULL}, "WCHAN", NULL, LJUST, wchan, 0, UNSPEC, NULL},
};

const size_t known_keywords_nb = nitems(keywords);

size_t
aliased_keyword_index(const VAR *const v)
{
	const VAR *const fv = (v->flag & RESOLVED_ALIAS) == 0 ? v : v->final_kw;
	const size_t idx = fv - keywords;
	assert(idx < known_keywords_nb);
	return (idx);
}

void
check_keywords(void)
{
	const VAR *k, *next_k;
	bool order_violated = false;
	for (size_t i = 0; i < known_keywords_nb - 1; ++i) {
		k = &keywords[i];
		next_k = &keywords[i + 1];
		if (strcmp(k->name, next_k->name) >= 0) {
			warnx("keywords bad order: '%s' followed by '%s'", k->name, next_k->name);
			order_violated = true;
		}
	}
	if (order_violated) errx(2, "keywords not in order");
}

static void
merge_alias(VAR *const k, VAR *const tgt)
{
	if ((tgt->flag & RESOLVED_ALIAS) != 0)
		k->final_kw = tgt->final_kw;
	else
		k->final_kw = tgt;

	if (k->header == NULL) k->header = tgt->header;
	if (k->field == NULL) k->field = tgt->field;
	if (k->flag == 0) k->flag = tgt->flag;

	k->oproc = tgt->oproc;
	k->off = tgt->off;
	k->type = tgt->type;
	k->fmt = tgt->fmt;
}

static void
resolve_alias(VAR *const k)
{
	VAR *t, key;
	if ((k->flag & RESOLVED_ALIAS) != 0 || k->aliased == NULL) return;
	if ((k->flag & RESOLVING_ALIAS) != 0) errx(2, "cycle in alias '%s'", k->name);
	k->flag |= RESOLVING_ALIAS;
	key.name = k->aliased;
	t = bsearch(&key, keywords, known_keywords_nb, sizeof(VAR), vcmp);
	if (t == NULL) errx(2, "unknown alias target '%s'", k->aliased);
	resolve_alias(t);
	merge_alias(k, t);
	k->flag &= ~RESOLVING_ALIAS;
	k->flag |= RESOLVED_ALIAS;
}

void
resolve_aliases(void)
{
	for (size_t i = 0; i < known_keywords_nb; ++i)
		resolve_alias(&keywords[i]);
}

void
showkey(void)
{
	const VAR *v;
	int i = 0;
	printf("Keywords:\n");
	for (v = keywords; v < keywords + known_keywords_nb; ++v) {
		printf("%-10s%s", v->name, (++i % 7 == 0) ? "\n" : " ");
	}
	printf("\n");
}

void
parsefmt(const char *p, struct velisthead *const var_list, const int user)
{
	char *copy = strdup(p);
	char *cp = copy;
	char *token;
	VAR *v, key;
	struct varent *vent;

	while ((token = strsep(&cp, " \t,\n")) != NULL) {
		if (*token == '\0') continue;
		char *hdr = strchr(token, '=');
		if (hdr) *hdr++ = '\0';

		key.name = token;
		v = bsearch(&key, keywords, known_keywords_nb, sizeof(VAR), vcmp);
		if (v == NULL) {
			warnx("%s: keyword not found", token);
			eval = 1;
			continue;
		}
		resolve_alias(v);
		vent = malloc(sizeof(struct varent));
		if (!vent) err(1, "malloc");
		vent->header = hdr ? strdup(hdr) : v->header;
		vent->width = strlen(vent->header);
		vent->var = v;
		vent->flags = user ? VE_KEEP : 0;
		STAILQ_INSERT_TAIL(var_list, vent, next_ve);
	}
	free(copy);
}

static int
vcmp(const void *a, const void *b)
{
	return strcmp(((const VAR *)a)->name, ((const VAR *)b)->name);
}
