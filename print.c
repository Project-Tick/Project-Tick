/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1990, 1993, 1994
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
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sysmacros.h>

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <err.h>
#include <grp.h>
#include <langinfo.h>
#include <locale.h>
#include <math.h>
#include <pwd.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "ps.h"

#define COMMAND_WIDTH	16
#define ARGUMENTS_WIDTH	16

#define SAFE_ASPRINTF(ptr, fmt, ...) do { \
	if (asprintf(ptr, fmt, __VA_ARGS__) == -1) \
		err(1, "asprintf"); \
} while (0)

/* Simple strvis-like implementation */
static char *
simple_strvis(const char *src)
{
	if (!src) return NULL;
	size_t len = strlen(src);
	char *dst = malloc(len * 4 + 1);
	if (!dst) return NULL;
	char *d = dst;
	for (const char *s = src; *s; s++) {
		if (isprint((unsigned char)*s) || *s == ' ') {
			*d++ = *s;
		} else {
			d += sprintf(d, "\\%03o", (unsigned char)*s);
		}
	}
	*d = '\0';
	return dst;
}

void
printheader(void)
{
	struct varent *vent;

	STAILQ_FOREACH(vent, &varlist, next_ve)
		if (*vent->header != '\0')
			break;
	if (!vent)
		return;

	STAILQ_FOREACH(vent, &varlist, next_ve) {
		const VAR *v = vent->var;
		if (v->flag & LJUST) {
			if (STAILQ_NEXT(vent, next_ve) == NULL)	/* last one */
				printf("%s", vent->header);
			else
				printf("%-*s", vent->width, vent->header);
		} else
			printf("%*s", vent->width, vent->header);
		if (STAILQ_NEXT(vent, next_ve) != NULL)
			printf(" ");
	}
	printf("\n");
}

char *
arguments(KINFO *k, VARENT *ve)
{
	char *vis_args = simple_strvis(k->ki_args);
	if (!vis_args) return NULL;

	if (STAILQ_NEXT(ve, next_ve) != NULL && strlen(vis_args) > ARGUMENTS_WIDTH)
		vis_args[ARGUMENTS_WIDTH] = '\0';

	return (vis_args);
}

char *
command(KINFO *k, VARENT *ve)
{
	char *str = NULL;

	if (cflag) {
		if (STAILQ_NEXT(ve, next_ve) == NULL) {
			SAFE_ASPRINTF(&str, "%s%s",
			    k->ki_d.prefix ? k->ki_d.prefix : "",
			    k->ki_p->ki_comm);
		} else {
			str = strdup(k->ki_p->ki_comm);
			if (!str) err(1, "strdup");
		}
		return (str);
	}

	char *vis_args = simple_strvis(k->ki_args);
	if (!vis_args) return strdup("-");

	if (STAILQ_NEXT(ve, next_ve) == NULL) {
		/* last field */
		char *vis_env = k->ki_env ? simple_strvis(k->ki_env) : NULL;
		asprintf(&str, "%s%s%s%s",
		    k->ki_d.prefix ? k->ki_d.prefix : "",
		    vis_env ? vis_env : "",
		    vis_env ? " " : "",
		    vis_args);
		free(vis_env);
		free(vis_args);
	} else {
		str = vis_args;
		if (strlen(str) > COMMAND_WIDTH)
			str[COMMAND_WIDTH] = '\0';
	}
	return (str);
}

char *
ucomm(KINFO *k, VARENT *ve)
{
	char *str;
	if (STAILQ_NEXT(ve, next_ve) == NULL) {
		asprintf(&str, "%s%s",
		    k->ki_d.prefix ? k->ki_d.prefix : "",
		    k->ki_p->ki_comm);
	} else {
		str = strdup(k->ki_p->ki_comm);
	}
	return (str);
}

char *
tdnam(KINFO *k, VARENT *ve __unused)
{
	if (k->ki_p->ki_numthreads > 1)
		return strdup(k->ki_p->ki_tdname);
	return strdup("      ");
}

char *
logname(KINFO *k, VARENT *ve __unused)
{
	if (*k->ki_p->ki_login == '\0')
		return NULL;
	return strdup(k->ki_p->ki_login);
}

char *
state(KINFO *k, VARENT *ve __unused)
{
	char *buf = malloc(16);
	if (!buf) return NULL;
	char *cp = buf;

	*cp++ = k->ki_p->ki_stat;

	if (k->ki_p->ki_nice < 0) *cp++ = '<';
	else if (k->ki_p->ki_nice > 0) *cp++ = 'N';

	/* Approximate flags */
	if (k->ki_p->ki_sid == k->ki_p->ki_pid) *cp++ = 's';
	if (k->ki_p->ki_tdev != (dev_t)-1 && k->ki_p->ki_pgid == k->ki_p->ki_tpgid) *cp++ = '+';

	*cp = '\0';
	return buf;
}

char *
pri(KINFO *k, VARENT *ve __unused)
{
	char *str;
	SAFE_ASPRINTF(&str, "%d", k->ki_p->ki_pri);
	return str;
}

char *
upr(KINFO *k, VARENT *ve __unused)
{
	char *str;
	SAFE_ASPRINTF(&str, "%d", k->ki_p->ki_pri);
	return str;
}

char *
username(KINFO *k, VARENT *ve __unused)
{
	return strdup(user_from_uid(k->ki_p->ki_uid, 0));
}

char *
egroupname(KINFO *k, VARENT *ve __unused)
{
	return strdup(group_from_gid(k->ki_p->ki_groups[0], 0));
}

char *
rgroupname(KINFO *k, VARENT *ve __unused)
{
	return strdup(group_from_gid(k->ki_p->ki_rgid, 0));
}

char *
runame(KINFO *k, VARENT *ve __unused)
{
	return strdup(user_from_uid(k->ki_p->ki_ruid, 0));
}

char *
tdev(KINFO *k, VARENT *ve __unused)
{
	char *str;
	if (k->ki_p->ki_tdev == (dev_t)-1)
		str = strdup("-");
	else
		asprintf(&str, "%#jx", (uintmax_t)k->ki_p->ki_tdev);
	return str;
}

char *
tname(KINFO *k, VARENT *ve __unused)
{
	char *name = devname(k->ki_p->ki_tdev, S_IFCHR);
	if (!name) return strdup("- ");
	
	char *str;
	if (strncmp(name, "tty", 3) == 0) name += 3;
	if (strncmp(name, "pts/", 4) == 0) name += 4;
	
	asprintf(&str, "%s ", name);
	return str;
}

char *
longtname(KINFO *k, VARENT *ve __unused)
{
	char *name = devname(k->ki_p->ki_tdev, S_IFCHR);
	return strdup(name ? name : "-");
}

char *
started(KINFO *k, VARENT *ve __unused)
{
	if (!k->ki_valid) return NULL;
	char *buf = malloc(100);
	if (!buf) return NULL;
	time_t then = k->ki_p->ki_start.tv_sec;
	struct tm *tp = localtime(&then);
	if (now - then < 24 * 3600)
		strftime(buf, 100, "%H:%M  ", tp);
	else if (now - then < 7 * 86400)
		strftime(buf, 100, "%a%H  ", tp);
	else
		strftime(buf, 100, "%e%b%y", tp);
	return buf;
}

char *
lstarted(KINFO *k, VARENT *ve __unused)
{
	if (!k->ki_valid) return NULL;
	char *buf = malloc(100);
	if (!buf) return NULL;
	time_t then = k->ki_p->ki_start.tv_sec;
	strftime(buf, 100, "%c", localtime(&then));
	return buf;
}

char *
lockname(KINFO *k, VARENT *ve __unused)
{
	return NULL;
}

char *
wchan(KINFO *k, VARENT *ve __unused)
{
	if (k->ki_p->ki_wmesg[0])
		return strdup(k->ki_p->ki_wmesg);
	return NULL;
}

char *
nwchan(KINFO *k, VARENT *ve __unused)
{
	return NULL;
}

char *
mwchan(KINFO *k, VARENT *ve __unused)
{
	if (k->ki_p->ki_wmesg[0])
		return strdup(k->ki_p->ki_wmesg);
	return NULL;
}

char *
vsize(KINFO *k, VARENT *ve __unused)
{
	char *str;
	asprintf(&str, "%lu", (unsigned long)(k->ki_p->ki_size / 1024));
	return str;
}

static char *
printtime(KINFO *k, VARENT *ve __unused, long secs, long psecs)
{
	char *str;
	if (!k->ki_valid) { secs = 0; psecs = 0; }
	asprintf(&str, "%ld:%02ld.%02ld", secs / 60, secs % 60, psecs / 10000);
	return str;
}

char *
cputime(KINFO *k, VARENT *ve)
{
	long secs = k->ki_p->ki_runtime / 1000000;
	long psecs = k->ki_p->ki_runtime % 1000000;
	return printtime(k, ve, secs, psecs);
}

char *
cpunum(KINFO *k, VARENT *ve __unused)
{
	char *str;
	asprintf(&str, "%d", k->ki_p->ki_lastcpu);
	return str;
}

char *
systime(KINFO *k, VARENT *ve)
{
	return strdup("0:00.00");
}

char *
usertime(KINFO *k, VARENT *ve)
{
	return strdup("0:00.00");
}

char *
elapsed(KINFO *k, VARENT *ve __unused)
{
	if (!k->ki_valid) return NULL;
	time_t val = now - k->ki_p->ki_start.tv_sec;
	int days = val / 86400; val %= 86400;
	int hours = val / 3600; val %= 3600;
	int mins = val / 60;
	int secs = val % 60;
	char *str;
	if (days != 0) asprintf(&str, "%3d-%02d:%02d:%02d", days, hours, mins, secs);
	else if (hours != 0) asprintf(&str, "%02d:%02d:%02d", hours, mins, secs);
	else asprintf(&str, "%02d:%02d", mins, secs);
	return str;
}

char *
elapseds(KINFO *k, VARENT *ve __unused)
{
	if (!k->ki_valid) return NULL;
	char *str;
	asprintf(&str, "%jd", (intmax_t)(now - k->ki_p->ki_start.tv_sec));
	return str;
}

double
getpcpu(const KINFO *k)
{
	if (!k->ki_valid) return 0.0;
	time_t uptime = now - k->ki_p->ki_start.tv_sec;
	if (uptime <= 0) uptime = 1;
	return (100.0 * (k->ki_p->ki_runtime / 1000000.0)) / uptime;
}

char *
pcpu(KINFO *k, VARENT *ve __unused)
{
	char *str;
	SAFE_ASPRINTF(&str, "%.1f", k->ki_pcpu);
	return str;
}

char *
pmem(KINFO *k, VARENT *ve __unused)
{
	if (mem_total_kb == 0) donlist();
	char *str;
	double pct = (100.0 * k->ki_p->ki_rssize) / mem_total_kb;
	asprintf(&str, "%.1f", pct);
	return str;
}

char *
pagein(KINFO *k, VARENT *ve __unused)
{
	return strdup("0");
}

char *
maxrss(KINFO *k, VARENT *ve __unused)
{
	return NULL;
}

char *
priorityr(KINFO *k, VARENT *ve __unused)
{
	char *str;
	SAFE_ASPRINTF(&str, "%d", k->ki_p->ki_pri);
	return str;
}

char *
kvar(KINFO *k, VARENT *ve)
{
	const VAR *v = ve->var;
	char *bp = (char *)k->ki_p + v->off;
	char *str;
	char fmt[32];
	snprintf(fmt, sizeof(fmt), "%%%s", v->fmt ? v->fmt : "s");
	switch (v->type) {
	case INT: asprintf(&str, fmt, *(int *)bp); break;
	case UINT: asprintf(&str, fmt, *(unsigned int *)bp); break;
	case LONG: asprintf(&str, fmt, *(long *)bp); break;
	case ULONG: asprintf(&str, fmt, *(unsigned long *)bp); break;
	case SHORT: asprintf(&str, fmt, *(short *)bp); break;
	case USHORT: asprintf(&str, fmt, *(unsigned short *)bp); break;
	case CHAR: asprintf(&str, fmt, *(char *)bp); break;
	case UCHAR: asprintf(&str, fmt, *(unsigned char *)bp); break;
	case PGTOK:
		{
			unsigned long val = *(unsigned long *)bp;
			asprintf(&str, fmt, (unsigned long)(val * getpagesize() / 1024));
			break;
		}
	default: asprintf(&str, "?"); break;
	}
	return str;
}

char *
rvar(KINFO *k, VARENT *ve)
{
	return strdup("-");
}

char *
emulname(KINFO *k, VARENT *ve)
{
	return strdup("Linux");
}

char *
jailname(KINFO *k, VARENT *ve)
{
	return strdup("-");
}

char *
label(KINFO *k, VARENT *ve)
{
	return strdup("-");
}

char *
loginclass(KINFO *k, VARENT *ve)
{
	return strdup("-");
}

/* Linux-specific helpers */

char *
user_from_uid(uid_t uid, int nouser)
{
	static char buf[32];
	struct passwd *pw = getpwuid(uid);
	if (!pw || nouser) {
		snprintf(buf, sizeof(buf), "%u", uid);
		return buf;
	}
	return pw->pw_name;
}

char *
group_from_gid(gid_t gid, int nogroup)
{
	static char buf[32];
	struct group *gr = getgrgid(gid);
	if (!gr || nogroup) {
		snprintf(buf, sizeof(buf), "%u", gid);
		return buf;
	}
	return gr->gr_name;
}

struct dev_cache {
	dev_t dev;
	char name[PATH_MAX];
};

static struct dev_cache *dcache = NULL;
static int dcache_size = 0;

static void
cache_dir(const char *path, const char *prefix)
{
	DIR *dir = opendir(path);
	struct dirent *ent;
	struct stat st;
	char buf[PATH_MAX];

	if (!dir) return;
	while ((ent = readdir(dir))) {
		if (ent->d_name[0] == '.') continue;
		snprintf(buf, sizeof(buf), "%s/%s", path, ent->d_name);
		if (stat(buf, &st) == 0 && S_ISCHR(st.st_mode)) {
			struct dev_cache *tmp;
			tmp = realloc(dcache, (dcache_size + 1) * sizeof(struct dev_cache));
			if (!tmp) err(1, "realloc");
			dcache = tmp;
			dcache[dcache_size].dev = st.st_rdev;
			snprintf(dcache[dcache_size].name, PATH_MAX, "%s%s", prefix, ent->d_name);
			dcache_size++;
		}
	}
	closedir(dir);
}

char *
devname(dev_t dev, mode_t type)
{
	if (dev == (dev_t)-1) return NULL;
	if (dcache == NULL) {
		cache_dir("/dev/pts", "pts/");
		cache_dir("/dev", "");
	}
	for (int i = 0; i < dcache_size; i++) {
		if (dcache[i].dev == dev)
			return dcache[i].name;
	}
	return NULL;
}

void
free_devnames(void)
{
	free(dcache);
	dcache = NULL;
	dcache_size = 0;
}
