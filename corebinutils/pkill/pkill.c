/*	$NetBSD: pkill.c,v 1.16 2005/10/10 22:13:20 kleink Exp $	*/

/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * Copyright (c) 2005 Pawel Jakub Dawidek <pjd@FreeBSD.org>
 * Copyright (c) 2026 Project Tick. All rights reserved.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Linux-native port: replaces kvm(3) process enumeration with /proc
 * filesystem parsing.  FreeBSD jail (-j), login class (-c), and core file
 * analysis (-M/-N) options are not available on Linux and produce explicit
 * errors.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <locale.h>
#include <pwd.h>
#include <regex.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* ------------------------------------------------------------------ */
/*  Exit status codes (match manpage)                                  */
/* ------------------------------------------------------------------ */
#define STATUS_MATCH	0
#define STATUS_NOMATCH	1
#define STATUS_BADUSAGE	2
#define STATUS_ERROR	3

/* ------------------------------------------------------------------ */
/*  Signal name table                                                  */
/* ------------------------------------------------------------------ */
struct signal_entry {
	const char *name;
	int         number;
};

#define SIG_ENTRY(n) { #n, SIG##n }

static const struct signal_entry signals[] = {
#ifdef SIGHUP
	SIG_ENTRY(HUP),
#endif
#ifdef SIGINT
	SIG_ENTRY(INT),
#endif
#ifdef SIGQUIT
	SIG_ENTRY(QUIT),
#endif
#ifdef SIGILL
	SIG_ENTRY(ILL),
#endif
#ifdef SIGTRAP
	SIG_ENTRY(TRAP),
#endif
#ifdef SIGABRT
	SIG_ENTRY(ABRT),
#endif
#ifdef SIGBUS
	SIG_ENTRY(BUS),
#endif
#ifdef SIGFPE
	SIG_ENTRY(FPE),
#endif
#ifdef SIGKILL
	SIG_ENTRY(KILL),
#endif
#ifdef SIGUSR1
	SIG_ENTRY(USR1),
#endif
#ifdef SIGSEGV
	SIG_ENTRY(SEGV),
#endif
#ifdef SIGUSR2
	SIG_ENTRY(USR2),
#endif
#ifdef SIGPIPE
	SIG_ENTRY(PIPE),
#endif
#ifdef SIGALRM
	SIG_ENTRY(ALRM),
#endif
#ifdef SIGTERM
	SIG_ENTRY(TERM),
#endif
#ifdef SIGSTKFLT
	SIG_ENTRY(STKFLT),
#endif
#ifdef SIGCHLD
	SIG_ENTRY(CHLD),
#endif
#ifdef SIGCONT
	SIG_ENTRY(CONT),
#endif
#ifdef SIGSTOP
	SIG_ENTRY(STOP),
#endif
#ifdef SIGTSTP
	SIG_ENTRY(TSTP),
#endif
#ifdef SIGTTIN
	SIG_ENTRY(TTIN),
#endif
#ifdef SIGTTOU
	SIG_ENTRY(TTOU),
#endif
#ifdef SIGURG
	SIG_ENTRY(URG),
#endif
#ifdef SIGXCPU
	SIG_ENTRY(XCPU),
#endif
#ifdef SIGXFSZ
	SIG_ENTRY(XFSZ),
#endif
#ifdef SIGVTALRM
	SIG_ENTRY(VTALRM),
#endif
#ifdef SIGPROF
	SIG_ENTRY(PROF),
#endif
#ifdef SIGWINCH
	SIG_ENTRY(WINCH),
#endif
#ifdef SIGIO
	SIG_ENTRY(IO),
#endif
#ifdef SIGPWR
	SIG_ENTRY(PWR),
#endif
#ifdef SIGSYS
	SIG_ENTRY(SYS),
#endif
};
#define NSIGNALS (sizeof(signals) / sizeof(signals[0]))

/* ------------------------------------------------------------------ */
/*  Process information (read from /proc)                              */
/* ------------------------------------------------------------------ */
struct procinfo {
	pid_t		pid;
	pid_t		ppid;
	pid_t		pgrp;
	pid_t		sid;
	uid_t		ruid;
	uid_t		euid;
	gid_t		rgid;
	gid_t		egid;
	dev_t		tdev;		/* controlling tty device number */
	unsigned long long starttime;	/* jiffies since boot */
	char		state;		/* R, S, D, Z, T … */
	char		comm[256];	/* short command name */
	char		*cmdline;	/* full command line (heap) */
	int		kthread;	/* 1 = kernel thread */
};

struct proclist {
	struct procinfo	*procs;
	size_t		count;
	size_t		capacity;
};

/* ------------------------------------------------------------------ */
/*  Dynamic filter list                                                */
/* ------------------------------------------------------------------ */
struct filterlist {
	long	*values;
	size_t	count;
	size_t	capacity;
};

/* ------------------------------------------------------------------ */
/*  Globals (reduced from BSD original)                                */
/* ------------------------------------------------------------------ */
static const char *progname;		/* argv[0] basename */
static int	pgrep_mode;		/* 1 if invoked as pgrep */
static int	signum = SIGTERM;
static int	newest;
static int	oldest;
static int	interactive;
static int	inverse;
static int	longfmt;
static int	matchargs;
static int	fullmatch;
static int	kthreads;
static int	cflags = REG_EXTENDED;
static int	quiet;
static int	ancestors;
static int	debug_opt;
static const char *delim = "\n";
static pid_t	mypid;

static struct filterlist euidlist;
static struct filterlist ruidlist;
static struct filterlist rgidlist;
static struct filterlist pgrplist;
static struct filterlist ppidlist;
static struct filterlist tdevlist;
static struct filterlist sidlist;

/* ------------------------------------------------------------------ */
/*  Forward declarations                                               */
/* ------------------------------------------------------------------ */
static void	usage(void) __attribute__((__noreturn__));
static void	addfilter(struct filterlist *, long);
static bool	infilter(const struct filterlist *, long);

static bool	parse_signal_name(const char *, int *);
static int	read_proc_stat(pid_t, struct procinfo *);
static int	read_proc_status(pid_t, struct procinfo *);
static char	*read_proc_cmdline(pid_t);
static int	scan_processes(struct proclist *);
static void	free_proclist(struct proclist *);
static int	takepid(const char *, int);

static void	parse_uid_filter(struct filterlist *, char *);
static void	parse_gid_filter(struct filterlist *, char *);
static void	parse_generic_filter(struct filterlist *, char *);
static void	parse_pgrp_filter(struct filterlist *, char *);
static void	parse_sid_filter(struct filterlist *, char *);
static void	parse_tty_filter(struct filterlist *, char *);

/* ------------------------------------------------------------------ */
/*  usage                                                              */
/* ------------------------------------------------------------------ */
static void
usage(void)
{
	const char *ustr;

	if (pgrep_mode)
		ustr = "[-LSafilnoqvx] [-d delim]";
	else
		ustr = "[-signal] [-ILafilnovx]";

	fprintf(stderr,
	    "usage: %s %s [-F pidfile] [-G gid]\n"
	    "             [-P ppid] [-U uid] [-g pgrp]\n"
	    "             [-s sid] [-t tty] [-u euid] pattern ...\n",
	    progname, ustr);

	exit(STATUS_BADUSAGE);
}

/* ------------------------------------------------------------------ */
/*  Filter list helpers                                                */
/* ------------------------------------------------------------------ */
static void
addfilter(struct filterlist *fl, long value)
{
	if (fl->count >= fl->capacity) {
		size_t newcap = fl->capacity ? fl->capacity * 2 : 8;
		long *nv = realloc(fl->values, newcap * sizeof(long));

		if (nv == NULL)
			err(STATUS_ERROR, "realloc");
		fl->values = nv;
		fl->capacity = newcap;
	}
	fl->values[fl->count++] = value;
}

static bool
infilter(const struct filterlist *fl, long value)
{
	for (size_t i = 0; i < fl->count; i++) {
		if (fl->values[i] == value)
			return true;
	}
	return false;
}

/* ------------------------------------------------------------------ */
/*  Signal helpers                                                     */
/* ------------------------------------------------------------------ */
static bool
parse_signal_name(const char *token, int *out)
{
	const char *p = token;
	char *end;
	long val;

	/* Try numeric first. */
	errno = 0;
	val = strtol(token, &end, 10);
	if (*end == '\0' && errno == 0 && val >= 0 && val < 256) {
		*out = (int)val;
		return true;
	}

	/* Strip optional "SIG" prefix, case-insensitive. */
	if (strncasecmp(p, "SIG", 3) == 0)
		p += 3;

	for (size_t i = 0; i < NSIGNALS; i++) {
		if (strcasecmp(p, signals[i].name) == 0) {
			*out = signals[i].number;
			return true;
		}
	}

#ifdef SIGRTMIN
#ifdef SIGRTMAX
	if (strcasecmp(p, "RTMIN") == 0) {
		*out = SIGRTMIN;
		return true;
	}
	if (strcasecmp(p, "RTMAX") == 0) {
		*out = SIGRTMAX;
		return true;
	}
	if (strncasecmp(p, "RTMIN+", 6) == 0) {
		errno = 0;
		val = strtol(p + 6, &end, 10);
		if (*end == '\0' && errno == 0 && val >= 0 &&
		    SIGRTMIN + (int)val <= SIGRTMAX) {
			*out = SIGRTMIN + (int)val;
			return true;
		}
	}
	if (strncasecmp(p, "RTMAX-", 6) == 0) {
		errno = 0;
		val = strtol(p + 6, &end, 10);
		if (*end == '\0' && errno == 0 && val >= 0 &&
		    SIGRTMAX - (int)val >= SIGRTMIN) {
			*out = SIGRTMAX - (int)val;
			return true;
		}
	}
#endif
#endif

	return false;
}

/* ------------------------------------------------------------------ */
/*  /proc reading                                                      */
/* ------------------------------------------------------------------ */

/*
 * Parse /proc/<pid>/stat.
 * Format: pid (comm) state ppid pgrp session tty_nr ... starttime ...
 * The comm field is parenthesised and may contain spaces/parens,
 * so we locate the *last* ')' to delimit it reliably.
 */
static int
read_proc_stat(pid_t pid, struct procinfo *pi)
{
	char path[64], buf[4096];
	int fd;
	ssize_t n;
	char *comm_start, *comm_end, *p;

	snprintf(path, sizeof(path), "/proc/%d/stat", (int)pid);
	fd = open(path, O_RDONLY);
	if (fd < 0)
		return -1;
	n = read(fd, buf, sizeof(buf) - 1);
	close(fd);
	if (n <= 0)
		return -1;
	buf[n] = '\0';

	comm_start = strchr(buf, '(');
	if (comm_start == NULL)
		return -1;
	comm_start++;

	comm_end = strrchr(buf, ')');
	if (comm_end == NULL || comm_end <= comm_start)
		return -1;

	/* Copy comm. */
	{
		size_t clen = (size_t)(comm_end - comm_start);

		if (clen >= sizeof(pi->comm))
			clen = sizeof(pi->comm) - 1;
		memcpy(pi->comm, comm_start, clen);
		pi->comm[clen] = '\0';
	}

	/* Tokenise fields after ") " */
	p = comm_end + 2;

	/*
	 * We need tokens 0..19 (field 3..22 in stat), where:
	 *   0  = state     3  = session  4  = tty_nr
	 *   1  = ppid      6  = flags   19  = starttime
	 */
	char *tokens[20];
	int ntok = 0;

	while (ntok < 20) {
		while (*p == ' ')
			p++;
		if (*p == '\0')
			break;
		tokens[ntok++] = p;
		while (*p != ' ' && *p != '\0')
			p++;
		if (*p == ' ')
			*p++ = '\0';
	}
	if (ntok < 20)
		return -1;

	pi->state = tokens[0][0];
	pi->ppid = (pid_t)atoi(tokens[1]);
	pi->pgrp = (pid_t)atoi(tokens[2]);
	pi->sid = (pid_t)atoi(tokens[3]);
	pi->tdev = (dev_t)strtoul(tokens[4], NULL, 10);
	pi->starttime = strtoull(tokens[19], NULL, 10);

	return 0;
}

/*
 * Parse /proc/<pid>/status for Uid: and Gid: lines.
 */
static int
read_proc_status(pid_t pid, struct procinfo *pi)
{
	char path[64], line[512];
	FILE *fp;
	int got = 0;
	unsigned int r, e;

	snprintf(path, sizeof(path), "/proc/%d/status", (int)pid);
	fp = fopen(path, "r");
	if (fp == NULL)
		return -1;

	while (fgets(line, (int)sizeof(line), fp) != NULL && got < 2) {
		if (strncmp(line, "Uid:", 4) == 0) {
			if (sscanf(line + 4, " %u %u", &r, &e) == 2) {
				pi->ruid = (uid_t)r;
				pi->euid = (uid_t)e;
				got++;
			}
		} else if (strncmp(line, "Gid:", 4) == 0) {
			if (sscanf(line + 4, " %u %u", &r, &e) == 2) {
				pi->rgid = (gid_t)r;
				pi->egid = (gid_t)e;
				got++;
			}
		}
	}
	fclose(fp);
	return (got == 2) ? 0 : -1;
}

/*
 * Read /proc/<pid>/cmdline.  NUL-separated arguments are joined with
 * spaces.  Returns heap-allocated string, or NULL for kernel threads
 * and zombies.
 */
static char *
read_proc_cmdline(pid_t pid)
{
	char path[64];
	int fd;
	ssize_t n;
	size_t total = 0, bufsz = 4096;
	char *buf;

	snprintf(path, sizeof(path), "/proc/%d/cmdline", (int)pid);
	fd = open(path, O_RDONLY);
	if (fd < 0)
		return NULL;

	buf = malloc(bufsz + 1);
	if (buf == NULL) {
		close(fd);
		return NULL;
	}

	while ((n = read(fd, buf + total, bufsz - total)) > 0) {
		total += (size_t)n;
		if (total >= bufsz) {
			bufsz *= 2;
			char *nb = realloc(buf, bufsz + 1);

			if (nb == NULL) {
				free(buf);
				close(fd);
				return NULL;
			}
			buf = nb;
		}
	}
	close(fd);

	if (total == 0) {
		free(buf);
		return NULL;
	}

	/* Strip trailing NULs, then convert internal NULs to spaces. */
	while (total > 0 && buf[total - 1] == '\0')
		total--;

	for (size_t i = 0; i < total; i++) {
		if (buf[i] == '\0')
			buf[i] = ' ';
	}
	buf[total] = '\0';
	return buf;
}

/*
 * Enumerate all processes from /proc.
 */
static int
scan_processes(struct proclist *pl)
{
	DIR *d;
	struct dirent *ent;
	char *endp;
	pid_t pid;

	d = opendir("/proc");
	if (d == NULL)
		err(STATUS_ERROR, "Cannot open /proc");

	while ((ent = readdir(d)) != NULL) {
		pid = (pid_t)strtol(ent->d_name, &endp, 10);
		if (*endp != '\0' || pid <= 0)
			continue;

		if (pl->count >= pl->capacity) {
			size_t nc = pl->capacity ? pl->capacity * 2 : 256;
			struct procinfo *np;

			np = realloc(pl->procs, nc * sizeof(*np));
			if (np == NULL)
				err(STATUS_ERROR, "realloc");
			pl->procs = np;
			pl->capacity = nc;
		}

		struct procinfo *pi = &pl->procs[pl->count];

		memset(pi, 0, sizeof(*pi));
		pi->pid = pid;

		if (read_proc_stat(pid, pi) < 0)
			continue;   /* process vanished */
		if (read_proc_status(pid, pi) < 0)
			continue;

		pi->cmdline = read_proc_cmdline(pid);
		pi->kthread = (pi->cmdline == NULL && pi->state != 'Z');

		pl->count++;
	}
	closedir(d);
	return 0;
}

static void
free_proclist(struct proclist *pl)
{
	for (size_t i = 0; i < pl->count; i++)
		free(pl->procs[i].cmdline);
	free(pl->procs);
	pl->procs = NULL;
	pl->count = pl->capacity = 0;
}

/* ------------------------------------------------------------------ */
/*  Filter parsers                                                     */
/* ------------------------------------------------------------------ */
static void
parse_uid_filter(struct filterlist *fl, char *arg)
{
	char *sp;

	while ((sp = strsep(&arg, ",")) != NULL) {
		if (*sp == '\0')
			errx(STATUS_BADUSAGE, "empty value in user list");
		char *ep;
		long val = strtol(sp, &ep, 10);

		if (*ep == '\0') {
			addfilter(fl, val);
		} else {
			struct passwd *pw = getpwnam(sp);

			if (pw == NULL)
				errx(STATUS_BADUSAGE,
				    "Unknown user '%s'", sp);
			addfilter(fl, (long)pw->pw_uid);
		}
	}
}

static void
parse_gid_filter(struct filterlist *fl, char *arg)
{
	char *sp;

	while ((sp = strsep(&arg, ",")) != NULL) {
		if (*sp == '\0')
			errx(STATUS_BADUSAGE, "empty value in group list");
		char *ep;
		long val = strtol(sp, &ep, 10);

		if (*ep == '\0') {
			addfilter(fl, val);
		} else {
			struct group *gr = getgrnam(sp);

			if (gr == NULL)
				errx(STATUS_BADUSAGE,
				    "Unknown group '%s'", sp);
			addfilter(fl, (long)gr->gr_gid);
		}
	}
}

static void
parse_generic_filter(struct filterlist *fl, char *arg)
{
	char *sp;

	while ((sp = strsep(&arg, ",")) != NULL) {
		if (*sp == '\0')
			errx(STATUS_BADUSAGE, "empty value in list");
		char *ep;
		long val = strtol(sp, &ep, 10);

		if (*ep != '\0')
			errx(STATUS_BADUSAGE,
			    "Invalid numeric value '%s'", sp);
		addfilter(fl, val);
	}
}

static void
parse_pgrp_filter(struct filterlist *fl, char *arg)
{
	char *sp;

	while ((sp = strsep(&arg, ",")) != NULL) {
		if (*sp == '\0')
			errx(STATUS_BADUSAGE,
			    "empty value in process group list");
		char *ep;
		long val = strtol(sp, &ep, 10);

		if (*ep != '\0')
			errx(STATUS_BADUSAGE,
			    "Invalid process group '%s'", sp);
		if (val == 0)
			val = (long)getpgrp();
		addfilter(fl, val);
	}
}

static void
parse_sid_filter(struct filterlist *fl, char *arg)
{
	char *sp;

	while ((sp = strsep(&arg, ",")) != NULL) {
		if (*sp == '\0')
			errx(STATUS_BADUSAGE,
			    "empty value in session list");
		char *ep;
		long val = strtol(sp, &ep, 10);

		if (*ep != '\0')
			errx(STATUS_BADUSAGE,
			    "Invalid session ID '%s'", sp);
		if (val == 0)
			val = (long)getsid(0);
		addfilter(fl, val);
	}
}

static void
parse_tty_filter(struct filterlist *fl, char *arg)
{
	char *sp;

	while ((sp = strsep(&arg, ",")) != NULL) {
		if (*sp == '\0')
			errx(STATUS_BADUSAGE,
			    "empty value in terminal list");
		if (strcmp(sp, "-") == 0) {
			addfilter(fl, -1); /* no controlling terminal */
			continue;
		}

		struct stat st;
		char path[PATH_MAX];
		char *ep;
		long num = strtol(sp, &ep, 10);

		/* Numeric → try /dev/pts/<N> */
		if (*ep == '\0' && num >= 0) {
			snprintf(path, sizeof(path), "/dev/pts/%s", sp);
			if (stat(path, &st) == 0 && S_ISCHR(st.st_mode)) {
				addfilter(fl, (long)st.st_rdev);
				continue;
			}
			errx(STATUS_BADUSAGE,
			    "No such tty: '/dev/pts/%s'", sp);
		}

		/* Try /dev/<name> */
		snprintf(path, sizeof(path), "/dev/%s", sp);
		if (stat(path, &st) == 0 && S_ISCHR(st.st_mode)) {
			addfilter(fl, (long)st.st_rdev);
			continue;
		}

		/* Try /dev/tty<name> */
		snprintf(path, sizeof(path), "/dev/tty%s", sp);
		if (stat(path, &st) == 0 && S_ISCHR(st.st_mode)) {
			addfilter(fl, (long)st.st_rdev);
			continue;
		}

		errx(STATUS_BADUSAGE, "No such tty: '%s'", sp);
	}
}

/* ------------------------------------------------------------------ */
/*  PID-file reader                                                    */
/* ------------------------------------------------------------------ */
static int
takepid(const char *pidfile, int pidfilelock)
{
	char line[BUFSIZ], *endp;
	FILE *fh;
	long rval;

	fh = fopen(pidfile, "r");
	if (fh == NULL)
		err(STATUS_ERROR, "Cannot open pidfile '%s'", pidfile);

	if (pidfilelock) {
		if (flock(fileno(fh), LOCK_EX | LOCK_NB) == 0) {
			(void)fclose(fh);
			errx(STATUS_ERROR,
			    "File '%s' can be locked; daemon not running",
			    pidfile);
		} else if (errno != EWOULDBLOCK) {
			(void)fclose(fh);
			err(STATUS_ERROR,
			    "Error while locking file '%s'", pidfile);
		}
	}

	if (fgets(line, (int)sizeof(line), fh) == NULL) {
		if (feof(fh)) {
			(void)fclose(fh);
			errx(STATUS_ERROR,
			    "Pidfile '%s' is empty", pidfile);
		}
		(void)fclose(fh);
		err(STATUS_ERROR,
		    "Cannot read from pidfile '%s'", pidfile);
	}
	(void)fclose(fh);

	errno = 0;
	rval = strtol(line, &endp, 10);
	if ((*endp != '\0' && !isspace((unsigned char)*endp)) || errno != 0)
		errx(STATUS_ERROR,
		    "Invalid pid in file '%s'", pidfile);
	if (rval <= 0 || rval > (long)INT_MAX)
		errx(STATUS_ERROR,
		    "Invalid pid in file '%s'", pidfile);

	return (int)rval;
}

/* ------------------------------------------------------------------ */
/*  Process display                                                    */
/* ------------------------------------------------------------------ */
static void
show_process(const struct procinfo *pi)
{
	if (quiet)
		return;

	if ((longfmt || !pgrep_mode) && matchargs && pi->cmdline != NULL)
		printf("%d %s", (int)pi->pid, pi->cmdline);
	else if (longfmt || !pgrep_mode)
		printf("%d %s", (int)pi->pid, pi->comm);
	else
		printf("%d", (int)pi->pid);
}

/* ------------------------------------------------------------------ */
/*  Actions                                                            */
/* ------------------------------------------------------------------ */
static int
killact(const struct procinfo *pi)
{
	if (interactive) {
		int ch, first;

		printf("kill ");
		show_process(pi);
		printf("? ");
		fflush(stdout);
		first = ch = getchar();
		while (ch != '\n' && ch != EOF)
			ch = getchar();
		if (first != 'y' && first != 'Y')
			return 1;
	}
	if (kill(pi->pid, signum) == -1) {
		if (errno != ESRCH)
			warn("signalling pid %d", (int)pi->pid);
		return 0;
	}
	return 1;
}

static int
grepact(const struct procinfo *pi)
{
	static bool first = true;

	if (!quiet && !first)
		printf("%s", delim);
	show_process(pi);
	first = false;
	return 1;
}

/* ------------------------------------------------------------------ */
/*  main                                                               */
/* ------------------------------------------------------------------ */
int
main(int argc, char **argv)
{
	char *pidfile = NULL;
	int ch, criteria, pidfilelock, pidfromfile;
	int (*action)(const struct procinfo *);
	struct proclist pl;
	char *selected;
	char *p;

	setlocale(LC_ALL, "");

	/* Determine program name and mode from argv[0]. */
	progname = strrchr(argv[0], '/');
	progname = progname ? progname + 1 : argv[0];
	if (strcmp(progname, "pgrep") == 0) {
		action = grepact;
		pgrep_mode = 1;
	} else {
		action = killact;

		/* pkill: try to parse leading signal argument. */
		if (argc > 1 && argv[1][0] == '-') {
			p = argv[1] + 1;
			if (*p != '\0' && *p != '-') {
				int sig;

				if (parse_signal_name(p, &sig)) {
					signum = sig;
					argv++;
					argc--;
				}
			}
		}
	}

	criteria = 0;
	pidfilelock = 0;
	memset(&pl, 0, sizeof(pl));

	while ((ch = getopt(argc, argv,
	    "DF:G:ILM:N:P:SU:ac:d:fg:ij:lnoqs:t:u:vx")) != -1)
		switch (ch) {
		case 'D':
			debug_opt++;
			break;
		case 'F':
			pidfile = optarg;
			criteria = 1;
			break;
		case 'G':
			parse_gid_filter(&rgidlist, optarg);
			criteria = 1;
			break;
		case 'I':
			if (pgrep_mode)
				usage();
			interactive = 1;
			break;
		case 'L':
			pidfilelock = 1;
			break;
		case 'M':
			errx(STATUS_BADUSAGE,
			    "Core file analysis (-M) requires kvm(3) "
			    "and is not supported on Linux");
			break;
		case 'N':
			errx(STATUS_BADUSAGE,
			    "Kernel name list (-N) requires kvm(3) "
			    "and is not supported on Linux");
			break;
		case 'P':
			parse_generic_filter(&ppidlist, optarg);
			criteria = 1;
			break;
		case 'S':
			if (!pgrep_mode)
				usage();
			kthreads = 1;
			break;
		case 'U':
			parse_uid_filter(&ruidlist, optarg);
			criteria = 1;
			break;
		case 'a':
			ancestors++;
			break;
		case 'c':
			errx(STATUS_BADUSAGE,
			    "FreeBSD login class filtering (-c) "
			    "is not supported on Linux");
			break;
		case 'd':
			if (!pgrep_mode)
				usage();
			delim = optarg;
			break;
		case 'f':
			matchargs = 1;
			break;
		case 'g':
			parse_pgrp_filter(&pgrplist, optarg);
			criteria = 1;
			break;
		case 'i':
			cflags |= REG_ICASE;
			break;
		case 'j':
			errx(STATUS_BADUSAGE,
			    "FreeBSD jail filtering (-j) "
			    "is not supported on Linux");
			break;
		case 'l':
			longfmt = 1;
			break;
		case 'n':
			newest = 1;
			criteria = 1;
			break;
		case 'o':
			oldest = 1;
			criteria = 1;
			break;
		case 'q':
			if (!pgrep_mode)
				usage();
			quiet = 1;
			break;
		case 's':
			parse_sid_filter(&sidlist, optarg);
			criteria = 1;
			break;
		case 't':
			parse_tty_filter(&tdevlist, optarg);
			criteria = 1;
			break;
		case 'u':
			parse_uid_filter(&euidlist, optarg);
			criteria = 1;
			break;
		case 'v':
			inverse = 1;
			break;
		case 'x':
			fullmatch = 1;
			break;
		default:
			usage();
			/* NOTREACHED */
		}

	argc -= optind;
	argv += optind;
	if (argc != 0)
		criteria = 1;
	if (!criteria)
		usage();
	if (newest && oldest)
		errx(STATUS_ERROR,
		    "Options -n and -o are mutually exclusive");

	if (pidfile != NULL)
		pidfromfile = takepid(pidfile, pidfilelock);
	else {
		if (pidfilelock)
			errx(STATUS_ERROR,
			    "Option -L doesn't make sense without -F");
		pidfromfile = -1;
	}

	mypid = getpid();

	/* ---- Read the process table from /proc. ---- */
	scan_processes(&pl);

	/* Allocate selection bitmap. */
	selected = calloc(pl.count, 1);
	if (selected == NULL)
		err(STATUS_ERROR, "calloc");

	/* ---- Apply regex patterns. ---- */
	{
		char errbuf[256];
		regex_t reg;
		regmatch_t rm;

		for (int a = 0; a < argc; a++) {
			int rv = regcomp(&reg, argv[a], cflags);

			if (rv != 0) {
				regerror(rv, &reg, errbuf, sizeof(errbuf));
				errx(STATUS_BADUSAGE,
				    "Cannot compile regular expression "
				    "'%s' (%s)", argv[a], errbuf);
			}

			for (size_t i = 0; i < pl.count; i++) {
				struct procinfo *pi = &pl.procs[i];

				/* Always skip self. */
				if (pi->pid == mypid)
					continue;
				/* Skip kernel threads unless -S. */
				if (!kthreads && pi->kthread)
					continue;

				const char *mstr;

				if (matchargs && pi->cmdline != NULL)
					mstr = pi->cmdline;
				else
					mstr = pi->comm;

				rv = regexec(&reg, mstr, 1, &rm, 0);
				if (rv == 0) {
					if (fullmatch) {
						if (rm.rm_so == 0 &&
						    rm.rm_eo ==
						    (regoff_t)strlen(mstr))
							selected[i] = 1;
					} else {
						selected[i] = 1;
					}
				} else if (rv != REG_NOMATCH) {
					regerror(rv, &reg, errbuf,
					    sizeof(errbuf));
					errx(STATUS_ERROR,
					    "Regular expression evaluation "
					    "error (%s)", errbuf);
				}

				if (debug_opt > 1) {
					fprintf(stderr, "* %s %5d %3u %s\n",
					    selected[i] ? "Matched"
					    : "NoMatch",
					    (int)pi->pid,
					    (unsigned)pi->euid, mstr);
				}
			}
			regfree(&reg);
		}
	}

	/* ---- Apply attribute filters. ---- */
	for (size_t i = 0; i < pl.count; i++) {
		struct procinfo *pi = &pl.procs[i];

		if (pi->pid == mypid)
			continue;
		if (!kthreads && pi->kthread)
			continue;

		if (pidfromfile >= 0 && pi->pid != pidfromfile) {
			selected[i] = 0;
			continue;
		}

		if (ruidlist.count > 0 &&
		    !infilter(&ruidlist, (long)pi->ruid)) {
			selected[i] = 0;
			continue;
		}
		if (rgidlist.count > 0 &&
		    !infilter(&rgidlist, (long)pi->rgid)) {
			selected[i] = 0;
			continue;
		}
		if (euidlist.count > 0 &&
		    !infilter(&euidlist, (long)pi->euid)) {
			selected[i] = 0;
			continue;
		}
		if (ppidlist.count > 0 &&
		    !infilter(&ppidlist, (long)pi->ppid)) {
			selected[i] = 0;
			continue;
		}
		if (pgrplist.count > 0 &&
		    !infilter(&pgrplist, (long)pi->pgrp)) {
			selected[i] = 0;
			continue;
		}
		if (tdevlist.count > 0) {
			bool match = false;

			for (size_t j = 0; j < tdevlist.count; j++) {
				if (tdevlist.values[j] == -1 &&
				    pi->tdev == 0) {
					match = true;
					break;
				}
				if ((long)pi->tdev == tdevlist.values[j]) {
					match = true;
					break;
				}
			}
			if (!match) {
				selected[i] = 0;
				continue;
			}
		}
		if (sidlist.count > 0 &&
		    !infilter(&sidlist, (long)pi->sid)) {
			selected[i] = 0;
			continue;
		}

		/* If no regex patterns given, select by filter only. */
		if (argc == 0)
			selected[i] = 1;
	}

	/* ---- Exclude ancestors (unless -a). ---- */
	if (!ancestors) {
		pid_t pid = mypid;

		while (pid > 1) {
			for (size_t i = 0; i < pl.count; i++) {
				if (pl.procs[i].pid == pid) {
					selected[i] = 0;
					pid = pl.procs[i].ppid;
					goto next_ancestor;
				}
			}
			/* Process not found in list. */
			if (pid == mypid)
				pid = getppid();
			else
				break;
next_ancestor:;
		}
	}

	/* ---- Handle -n (newest) / -o (oldest). ---- */
	if (newest || oldest) {
		unsigned long long best = 0;
		int bestidx = -1;

		for (size_t i = 0; i < pl.count; i++) {
			if (!selected[i])
				continue;
			if (bestidx == -1) {
				/* first match */
			} else if (pl.procs[i].starttime > best) {
				if (oldest)
					continue;
			} else {
				if (newest)
					continue;
			}
			best = pl.procs[i].starttime;
			bestidx = (int)i;
		}

		memset(selected, 0, pl.count);
		if (bestidx != -1)
			selected[bestidx] = 1;
	}

	/* ---- Execute action. ---- */
	int did_action = 0, rv = 0;

	for (size_t i = 0; i < pl.count; i++) {
		struct procinfo *pi = &pl.procs[i];

		if (pi->pid == mypid)
			continue;
		if (!kthreads && pi->kthread)
			continue;

		if (selected[i]) {
			if (longfmt && !pgrep_mode) {
				did_action = 1;
				printf("kill -%d %d\n", signum,
				    (int)pi->pid);
			}
			if (inverse)
				continue;
		} else if (!inverse) {
			continue;
		}
		rv |= (*action)(pi);
	}
	if (rv && pgrep_mode && !quiet)
		putchar('\n');
	if (!did_action && !pgrep_mode && longfmt)
		fprintf(stderr,
		    "No matching processes belonging to you were found\n");

	free(selected);
	free_proclist(&pl);

	return rv ? STATUS_MATCH : STATUS_NOMATCH;
}
