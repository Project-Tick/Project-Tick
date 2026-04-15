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
 * ------+---------+---------+-------- + --------+---------+---------+---------*
 * Copyright (c) 2004  - Garance Alistair Drosehn <gad@FreeBSD.org>.
 * All rights reserved.
 *
 * Significant modifications made to bring `ps' options somewhat closer
 * to the standard for `ps' as described in SingleUnixSpec-v3.
 * ------+---------+---------+-------- + --------+---------+---------+---------*
 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/sysmacros.h>

#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <locale.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "ps.h"

/* Globals */
int	 cflag;			/* -c */
int	 eval;			/* Exit value */
time_t	 now;			/* Current time(3) value */
int	 rawcpu;		/* -C */
int	 sumrusage;		/* -S */
int	 termwidth;		/* Width of the screen (0 == infinity). */
int	 showthreads;		/* will threads be shown? */
int	 hlines;		/* repeat headers every N lines */

struct velisthead varlist = STAILQ_HEAD_INITIALIZER(varlist);
struct velisthead Ovarlist = STAILQ_HEAD_INITIALIZER(Ovarlist);

static int needcomm, needenv, needuser, optfatal;
static enum sort { DEFAULT, SORTMEM, SORTCPU } sortby = DEFAULT;

static long clk_tck;
static double system_uptime;

struct listinfo {
	int		 count;
	int		 maxcount;
	int		 elemsize;
	int        (*addelem)(struct listinfo *, const char *);
	const char	*lname;
	void		*l;
};

/* Forward declarations */
static int addelem_gid(struct listinfo *, const char *);
static int addelem_pid(struct listinfo *, const char *);
static int addelem_tty(struct listinfo *, const char *);
static int addelem_uid(struct listinfo *, const char *);
static void add_list(struct listinfo *, const char *);
static int pscomp(const void *, const void *);
static void scan_vars(void);
static void usage(void) __attribute__((__noreturn__));
static int scan_processes(KINFO **kinfop, int *nentries);
static double get_system_uptime(void);
static char *kludge_oldps_options(const char *, char *, const char *);

static const char dfmt[] = "pid,tt,state,time,command";
static const char jfmt[] = "user,pid,ppid,pgid,sid,jobc,state,tt,time,command";
static const char lfmt[] = "uid,pid,ppid,cpu,pri,nice,vsz,rss,mwchan,state,tt,time,command";
static const char ufmt[] = "user,pid,%cpu,%mem,vsz,rss,tt,state,start,time,command";
static const char vfmt[] = "pid,state,time,sl,vsz,rss,lim,tsiz,%cpu,%mem,command";

#define	PS_ARGS	"AaCcD:defG:gHhjJ:LlM:mN:O:o:p:rSTt:U:uvwXxZ"

int
main(int argc, char *argv[])
{
	struct listinfo gidlist, pidlist, ruidlist, sesslist, ttylist, uidlist, pgrplist;
	KINFO *kinfo = NULL;
	struct varent *vent;
	struct winsize ws;
	char *cols;
	int all, ch, i, nentries, nkept, nselectors, wflag, xkeep, xkeep_implied;

	setlocale(LC_ALL, "");
	time(&now);

	clk_tck = sysconf(_SC_CLK_TCK);
	system_uptime = get_system_uptime();

	if ((cols = getenv("COLUMNS")) != NULL && *cols != '\0')
		termwidth = atoi(cols);
	else if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
		termwidth = ws.ws_col - 1;
		if (ws.ws_row > 0) hlines = ws.ws_row - 1;
	} else {
		termwidth = 79;
		hlines = 22;
	}
	/* -h default is active only if -h is passed, so we use hlines as the value if hflag set */
	int hflag = 0;

	if (argc > 1)
		argv[1] = kludge_oldps_options(PS_ARGS, argv[1], argv[2]);

	all = nselectors = optfatal = wflag = xkeep_implied = 0;
	xkeep = -1;
	
	memset(&gidlist, 0, sizeof(gidlist)); gidlist.addelem = addelem_gid; gidlist.elemsize = sizeof(gid_t);
	memset(&pidlist, 0, sizeof(pidlist)); pidlist.addelem = addelem_pid; pidlist.elemsize = sizeof(pid_t);
	memset(&ruidlist, 0, sizeof(ruidlist)); ruidlist.addelem = addelem_uid; ruidlist.elemsize = sizeof(uid_t);
	memset(&sesslist, 0, sizeof(sesslist)); sesslist.addelem = addelem_pid; sesslist.elemsize = sizeof(pid_t);
	memset(&ttylist, 0, sizeof(ttylist)); ttylist.addelem = addelem_tty; ttylist.elemsize = sizeof(dev_t);
	memset(&uidlist, 0, sizeof(uidlist)); uidlist.addelem = addelem_uid; uidlist.elemsize = sizeof(uid_t);
	memset(&pgrplist, 0, sizeof(pgrplist)); pgrplist.addelem = addelem_pid; pgrplist.elemsize = sizeof(pid_t);

	int _fmt = 0;
	while ((ch = getopt(argc, argv, PS_ARGS)) != -1)
		switch (ch) {
		case 'A': all = xkeep = 1; break;
		case 'a': all = 1; break;
		case 'C': rawcpu = 1; break;
		case 'c': cflag = 1; break;
		case 'e': needenv = 1; break;
		case 'f': /* default in FreeBSD */ break;
		case 'G': add_list(&gidlist, optarg); xkeep_implied = 1; nselectors++; break;
		case 'g': /* BSD compat: leaders (no-op on Linux for now) */ break;
		case 'H': showthreads = KERN_PROC_INC_THREAD; break;
		case 'h': hflag = 1; break;
		case 'j': parsefmt(jfmt, &varlist, 0); _fmt = 1; break;
		case 'L': showkey(); exit(0);
		case 'l': parsefmt(lfmt, &varlist, 0); _fmt = 1; break;
		case 'M': case 'N': warnx("-%c not supported on Linux", ch); break;
		case 'm': sortby = SORTMEM; break;
		case 'O': parsefmt(optarg, &Ovarlist, 1); break;
		case 'o': parsefmt(optarg, &varlist, 1); _fmt = 1; break;
		case 'p': add_list(&pidlist, optarg); nselectors++; break;
		case 'r': sortby = SORTCPU; break;
		case 'S': sumrusage = 1; break;
		case 'T': add_list(&ttylist, ttyname(STDIN_FILENO)); xkeep_implied = 1; nselectors++; break;
		case 't': add_list(&ttylist, optarg); xkeep_implied = 1; nselectors++; break;
		case 'U': add_list(&ruidlist, optarg); xkeep_implied = 1; nselectors++; break;
		case 'u': parsefmt(ufmt, &varlist, 0); sortby = SORTCPU; _fmt = 1; break;
		case 'v': parsefmt(vfmt, &varlist, 0); sortby = SORTMEM; _fmt = 1; break;
		case 'w': if (wflag) termwidth = 0; else if (termwidth < 131) termwidth = 131; wflag++; break;
		case 'X': xkeep = 0; break;
		case 'x': xkeep = 1; break;
		case 'Z': parsefmt("label", &varlist, 0); break;
		default: usage();
		}

	argc -= optind; argv += optind;
	while (*argv && isdigit(**argv)) { add_list(&pidlist, *argv); nselectors++; argv++; }
	if (*argv) errx(1, "illegal argument: %s", *argv);
	if (optfatal) exit(1);
	if (xkeep < 0) xkeep = xkeep_implied;
	if (!hflag) hlines = 0;

	if (!_fmt) parsefmt(dfmt, &varlist, 0);
	if (!STAILQ_EMPTY(&Ovarlist)) {
		/* Simple join for now */
		STAILQ_CONCAT(&varlist, &Ovarlist);
	}

	if (STAILQ_EMPTY(&varlist)) {
		warnx("no keywords specified");
		showkey();
		exit(1);
	}

	scan_vars();

	if (all) nselectors = 0;
	else if (nselectors == 0) {
		uid_t me = geteuid();
		uidlist.l = realloc(uidlist.l, sizeof(uid_t));
		((uid_t*)uidlist.l)[0] = me;
		uidlist.count = 1;
		nselectors = 1;
	}

	if (scan_processes(&kinfo, &nentries) < 0) err(1, "scan_processes");

	for (i = 0; i < nentries; i++)
		kinfo[i].ki_pcpu = getpcpu(&kinfo[i]);

	nkept = 0;
	KINFO *kept = malloc(nentries * sizeof(KINFO));
	if (!kept) err(1, "malloc");

	for (i = 0; i < nentries; i++) {
		struct kinfo_proc *kp = kinfo[i].ki_p;
		int match = 0;
		if (pidlist.count > 0) {
			for (int j = 0; j < pidlist.count; j++) {
				// printf("Checking pid %d against %d\n", kp->ki_pid, ((pid_t*)pidlist.l)[j]);
				if (kp->ki_pid == ((pid_t*)pidlist.l)[j]) { match = 1; break; }
			}
		}
		if (!match && xkeep == 0 && (kp->ki_tdev == (dev_t)-1)) continue;
		if (!match && nselectors > 0) {
			if (gidlist.count > 0) {
				for (int j = 0; j < gidlist.count; j++)
					if (kp->ki_groups[0] == ((gid_t*)gidlist.l)[j]) { match = 1; break; }
			}
			if (!match && ruidlist.count > 0) {
				for (int j = 0; j < ruidlist.count; j++)
					if (kp->ki_ruid == ((uid_t*)ruidlist.l)[j]) { match = 1; break; }
			}
			if (!match && uidlist.count > 0) {
				for (int j = 0; j < uidlist.count; j++)
					if (kp->ki_uid == ((uid_t*)uidlist.l)[j]) { match = 1; break; }
			}
			if (!match && ttylist.count > 0) {
				for (int j = 0; j < ttylist.count; j++)
					if (kp->ki_tdev == ((dev_t*)ttylist.l)[j]) { match = 1; break; }
			}
			if (!match) continue;
		} else if (!match && xkeep == 0 && (kp->ki_tdev == (dev_t)-1)) continue;

		kept[nkept++] = kinfo[i];
	}

	if (nkept == 0) {
		printheader();
		exit(nselectors > 0 ? 1 : 0);
	}

	qsort(kept, nkept, sizeof(KINFO), pscomp);

	printheader();
	for (i = 0; i < nkept; i++) {
		if (hlines > 0 && i > 0 && i % hlines == 0) printheader();
		int linelen = 0;
		STAILQ_FOREACH(vent, &varlist, next_ve) {
			char *str = vent->var->oproc(&kept[i], vent);
			if (!str) str = strdup("-");
			int width = vent->width;
			if (STAILQ_NEXT(vent, next_ve) == NULL) {
				if (termwidth > 0 && linelen + (int)strlen(str) > termwidth) {
					int avail = termwidth - linelen;
					if (avail > 0) str[avail] = '\0';
				}
				printf("%s", str);
			} else {
				if (vent->var->flag & LJUST) printf("%-*s ", width, str);
				else printf("%*s ", width, str);
				linelen += width + 1;
			}
			free(str);
		}
		printf("\n");
	}

	for (i = 0; i < nentries; i++) {
		KINFO_STR *ks;
		while (!STAILQ_EMPTY(&kinfo[i].ki_ks)) {
			ks = STAILQ_FIRST(&kinfo[i].ki_ks);
			STAILQ_REMOVE_HEAD(&kinfo[i].ki_ks, ks_next);
			free(ks->ks_str);
			free(ks);
		}
		free(kinfo[i].ki_p);
		free(kinfo[i].ki_args);
		free(kinfo[i].ki_env);
	}
	free(kinfo);
	free(kept);
	free_devnames();

	return 0;
}

static void
scan_vars(void)
{
	struct varent *vent;
	STAILQ_FOREACH(vent, &varlist, next_ve) {
		if (vent->var->flag & COMM) needcomm = 1;
		if (vent->var->flag & USER) needuser = 1;
	}
}

static int
addelem_pid(struct listinfo *inf, const char *arg)
{
	long v = strtol(arg, NULL, 10);
	inf->l = realloc(inf->l, (inf->count + 1) * sizeof(pid_t));
	((pid_t*)inf->l)[inf->count++] = (pid_t)v;
	return 0;
}

static int
addelem_uid(struct listinfo *inf, const char *arg)
{
	struct passwd *pw = getpwnam(arg);
	uid_t uid = pw ? pw->pw_uid : (uid_t)atoi(arg);
	inf->l = realloc(inf->l, (inf->count + 1) * sizeof(uid_t));
	((uid_t*)inf->l)[inf->count++] = uid;
	return 0;
}

static int
addelem_gid(struct listinfo *inf, const char *arg)
{
	struct group *gr = getgrnam(arg);
	gid_t gid = gr ? gr->gr_gid : (gid_t)atoi(arg);
	inf->l = realloc(inf->l, (inf->count + 1) * sizeof(gid_t));
	((gid_t*)inf->l)[inf->count++] = gid;
	return 0;
}

static int
addelem_tty(struct listinfo *inf, const char *arg)
{
	char path[PATH_MAX];
	struct stat st;
	if (arg[0] != '/') snprintf(path, sizeof(path), "/dev/%s", arg);
	else snprintf(path, sizeof(path), "%s", arg);
	if (stat(path, &st) == 0 && S_ISCHR(st.st_mode)) {
		inf->l = realloc(inf->l, (inf->count + 1) * sizeof(dev_t));
		((dev_t*)inf->l)[inf->count++] = st.st_rdev;
		return 0;
	}
	return -1;
}

static void
add_list(struct listinfo *inf, const char *arg)
{
	char *copy = strdup(arg);
	char *p = copy, *token;
	while ((token = strsep(&p, " \t,")) != NULL) {
		if (*token == '\0') continue;
		if (inf->addelem(inf, token) < 0) optfatal = 1;
	}
	free(copy);
}

static int
pscomp(const void *a, const void *b)
{
	const KINFO *ka = a, *kb = b;
	if (sortby == SORTMEM) {
		if (ka->ki_p->ki_rssize < kb->ki_p->ki_rssize) return 1;
		if (ka->ki_p->ki_rssize > kb->ki_p->ki_rssize) return -1;
		return 0;
	}
	if (sortby == SORTCPU) {
		if (ka->ki_pcpu < kb->ki_pcpu) return 1;
		if (ka->ki_pcpu > kb->ki_pcpu) return -1;
		return 0;
	}
	if (ka->ki_p->ki_pid < kb->ki_p->ki_pid) return -1;
	if (ka->ki_p->ki_pid > kb->ki_p->ki_pid) return 1;
	return 0;
}

static double
get_system_uptime(void)
{
	FILE *fp = fopen("/proc/uptime", "r");
	double uptime = 0.0;
	if (fp) {
		if (fscanf(fp, "%lf", &uptime) != 1) uptime = 0.0;
		fclose(fp);
	}
	return uptime;
}

static long long
safe_strtoll(const char *s)
{
	char *end;
	if (s == NULL || *s == '\0') return 0;
	long long val = strtoll(s, &end, 10);
	if (end == s) return 0;
	return val;
}

static unsigned long
safe_strtoul(const char *s)
{
	char *end;
	if (s == NULL || *s == '\0') return 0;
	unsigned long val = strtoul(s, &end, 10);
	if (end == s) return 0;
	return val;
}

/* Linux-specific /proc scanning */

static char*
read_file(const char *path)
{
	int fd = open(path, O_RDONLY);
	if (fd < 0) return NULL;
	char *buf = malloc(8192);
	if (!buf) { close(fd); return NULL; }
	ssize_t n = read(fd, buf, 8191);
	close(fd);
	if (n <= 0) { free(buf); return NULL; }
	buf[n] = '\0';
	return buf;
}

static int
read_proc_stat(pid_t pid, struct kinfo_proc *ki)
{
	char path[64], *buf;
	snprintf(path, sizeof(path), "/proc/%d/stat", pid);
	buf = read_file(path);
	if (!buf) return -1;

	char *p = strrchr(buf, ')');
	if (!p) { free(buf); return -1; }
	
	/* Field 1: pid (already known) */
	/* Field 2: comm (already got p) */
	char *comm_start = strchr(buf, '(');
	if (comm_start) {
		size_t len = p - (comm_start + 1);
		if (len >= sizeof(ki->ki_comm)) len = sizeof(ki->ki_comm) - 1;
		memcpy(ki->ki_comm, comm_start + 1, len);
		ki->ki_comm[len] = '\0';
	}

	p += 2; /* Skip ") " */
	char *tokens[50];
	int ntok = 0;
	char *sp;
	while ((sp = strsep(&p, " ")) != NULL && ntok < 50) {
		tokens[ntok++] = sp;
	}

	if (ntok < 20) { free(buf); return -1; }

	ki->ki_stat = (tokens[0] && tokens[0][0]) ? tokens[0][0] : '?';
	ki->ki_ppid = (int)safe_strtoll(tokens[1]);
	ki->ki_pgid = (int)safe_strtoll(tokens[2]);
	ki->ki_sid = (int)safe_strtoll(tokens[3]);

	unsigned int tty_nr = (unsigned int)safe_strtoul(tokens[4]);
	if (tty_nr == 0) {
		ki->ki_tdev = (dev_t)-1;
	} else {
		unsigned int maj = (tty_nr >> 8) & 0xFFF;
		unsigned int min = (tty_nr & 0xFF) | ((tty_nr >> 12) & 0xFFF00);
		ki->ki_tdev = makedev(maj, min);
	}

	ki->ki_tpgid = tokens[5] ? (int)safe_strtoll(tokens[5]) : -1;
	ki->ki_flag = (long)safe_strtoll(tokens[6]);
	
	long long utime = safe_strtoll(tokens[11]);
	long long stime = safe_strtoll(tokens[12]);
	ki->ki_runtime = (uint64_t)((utime + stime) * 1000000 / clk_tck);

	ki->ki_pri = (int)safe_strtoll(tokens[15]);
	ki->ki_nice = (int)safe_strtoll(tokens[16]);
	ki->ki_numthreads = (ntok > 17) ? (int)safe_strtoll(tokens[17]) : 1;
	if (ki->ki_numthreads <= 0) ki->ki_numthreads = 1;
	
	if (ntok > 19 && tokens[19]) {
		double boot_time = (double)now - system_uptime;
		double start_ticks = (double)safe_strtoll(tokens[19]);
		ki->ki_start.tv_sec = (time_t)(boot_time + (start_ticks / clk_tck));
		ki->ki_start.tv_usec = 0;
	} else {
		ki->ki_start.tv_sec = now;
		ki->ki_start.tv_usec = 0;
	}

	ki->ki_size = (ntok > 20) ? (uint64_t)safe_strtoll(tokens[20]) : 0;
	ki->ki_rssize = (ntok > 21) ? (uint64_t)safe_strtoll(tokens[21]) * (getpagesize() / 1024) : 0;
	
	free(buf);
	return 0;
}

static int
read_proc_status(pid_t pid, struct kinfo_proc *ki)
{
	char path[64];
	snprintf(path, sizeof(path), "/proc/%d/status", pid);
	FILE *fp = fopen(path, "r");
	if (!fp) return -1;
	char line[256];
	while (fgets(line, sizeof(line), fp)) {
		if (strncmp(line, "Uid:", 4) == 0) {
			sscanf(line + 4, "%u %u %u", &ki->ki_ruid, &ki->ki_uid, &ki->ki_svuid);
		} else if (strncmp(line, "Gid:", 4) == 0) {
			sscanf(line + 4, "%u %u %u", &ki->ki_rgid, &ki->ki_groups[0], &ki->ki_svgid);
		}
	}
	fclose(fp);
	return 0;
}

static char*
read_proc_cmdline(pid_t pid)
{
	char path[64];
	snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
	int fd = open(path, O_RDONLY);
	if (fd < 0) return NULL;
	char *buf = malloc(4096);
	if (!buf) { close(fd); return NULL; }
	ssize_t n = read(fd, buf, 4095);
	close(fd);
	if (n <= 0) { free(buf); return NULL; }
	for (int i = 0; i < n - 1; i++) if (buf[i] == '\0') buf[i] = ' ';
	buf[n] = '\0';
	return buf;
}

static char*
read_proc_environ(pid_t pid)
{
	char path[64];
	snprintf(path, sizeof(path), "/proc/%d/environ", pid);
	int fd = open(path, O_RDONLY);
	if (fd < 0) return NULL;
	char *buf = malloc(4096);
	if (!buf) { close(fd); return NULL; }
	ssize_t n = read(fd, buf, 4095);
	close(fd);
	if (n <= 0) { free(buf); return NULL; }
	for (int i = 0; i < n - 1; i++) if (buf[i] == '\0') buf[i] = ' ';
	buf[n] = '\0';
	return buf;
}

static int
scan_processes(KINFO **kinfop, int *nentries)
{
	DIR *dir = opendir("/proc");
	if (!dir) return -1;
	struct dirent *ent;
	int count = 0, cap = 128;
	KINFO *k = malloc(cap * sizeof(KINFO));

	while ((ent = readdir(dir))) {
		if (!isdigit(ent->d_name[0])) continue;
		pid_t pid = atoi(ent->d_name);
		struct kinfo_proc *kp = calloc(1, sizeof(struct kinfo_proc));
		kp->ki_pid = pid;
		if (read_proc_stat(pid, kp) < 0 || read_proc_status(pid, kp) < 0) {
			free(kp);
			continue;
		}
		// printf("Scanned pid %d: comm=%s ppid=%d\n", kp->ki_pid, kp->ki_comm, kp->ki_ppid);
		
		if (count >= cap) {
			cap *= 2;
			k = realloc(k, cap * sizeof(KINFO));
		}
		k[count].ki_p = kp;
		k[count].ki_valid = 1;
		k[count].ki_args = needcomm ? read_proc_cmdline(pid) : NULL;
		if (!k[count].ki_args) k[count].ki_args = strdup(kp->ki_comm);
		k[count].ki_env = (needenv) ? read_proc_environ(pid) : NULL;
		k[count].ki_pcpu = 0; /* filled later if needed */
		k[count].ki_memsize = kp->ki_rssize * getpagesize();
		k[count].ki_d.prefix = NULL;
		STAILQ_INIT(&k[count].ki_ks);
		count++;
	}
	closedir(dir);
	*kinfop = k;
	*nentries = count;
	return 0;
}

static void
usage(void)
{
	fprintf(stderr, "usage: ps [%s]\n", PS_ARGS);
	exit(1);
}

static char *
kludge_oldps_options(const char *optstring, char *arg, const char *nextarg)
{
	/* Simple version of BSD kludge: if first arg doesn't start with '-', prepend one */
	if (arg && arg[0] != '-') {
		char *newarg = malloc(strlen(arg) + 2);
		newarg[0] = '-';
		strcpy(newarg + 1, arg);
		return newarg;
	}
	return arg;
}
