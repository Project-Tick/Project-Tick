/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2014 Baptiste Daroussin <bapt@FreeBSD.org>
 * Copyright (c) 2014 Vsevolod Stakhov <vsevolod@FreeBSD.org>
 * Copyright (c) 2025 Aaron LI <aly@aaronly.me>
 * Copyright (c) 2026 Project Tick
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define EXIT_TIMEOUT 124
#define EXIT_INVALID 125
#define EXIT_CMD_ERROR 126
#define EXIT_CMD_NOENT 127

#ifndef NSIG
#define NSIG 128
#endif

struct signal_entry {
	const char *name;
	int number;
};

#define SIGNAL_ENTRY(name) { #name, SIG##name }
#define SIGNAL_ALIAS(name, signal) { name, signal }

static const struct signal_entry canonical_signals[] = {
#ifdef SIGHUP
	SIGNAL_ENTRY(HUP),
#endif
#ifdef SIGINT
	SIGNAL_ENTRY(INT),
#endif
#ifdef SIGQUIT
	SIGNAL_ENTRY(QUIT),
#endif
#ifdef SIGILL
	SIGNAL_ENTRY(ILL),
#endif
#ifdef SIGTRAP
	SIGNAL_ENTRY(TRAP),
#endif
#ifdef SIGABRT
	SIGNAL_ENTRY(ABRT),
#endif
#ifdef SIGBUS
	SIGNAL_ENTRY(BUS),
#endif
#ifdef SIGFPE
	SIGNAL_ENTRY(FPE),
#endif
#ifdef SIGKILL
	SIGNAL_ENTRY(KILL),
#endif
#ifdef SIGUSR1
	SIGNAL_ENTRY(USR1),
#endif
#ifdef SIGSEGV
	SIGNAL_ENTRY(SEGV),
#endif
#ifdef SIGUSR2
	SIGNAL_ENTRY(USR2),
#endif
#ifdef SIGPIPE
	SIGNAL_ENTRY(PIPE),
#endif
#ifdef SIGALRM
	SIGNAL_ENTRY(ALRM),
#endif
#ifdef SIGTERM
	SIGNAL_ENTRY(TERM),
#endif
#ifdef SIGSTKFLT
	SIGNAL_ENTRY(STKFLT),
#endif
#ifdef SIGCHLD
	SIGNAL_ENTRY(CHLD),
#endif
#ifdef SIGCONT
	SIGNAL_ENTRY(CONT),
#endif
#ifdef SIGSTOP
	SIGNAL_ENTRY(STOP),
#endif
#ifdef SIGTSTP
	SIGNAL_ENTRY(TSTP),
#endif
#ifdef SIGTTIN
	SIGNAL_ENTRY(TTIN),
#endif
#ifdef SIGTTOU
	SIGNAL_ENTRY(TTOU),
#endif
#ifdef SIGURG
	SIGNAL_ENTRY(URG),
#endif
#ifdef SIGXCPU
	SIGNAL_ENTRY(XCPU),
#endif
#ifdef SIGXFSZ
	SIGNAL_ENTRY(XFSZ),
#endif
#ifdef SIGVTALRM
	SIGNAL_ENTRY(VTALRM),
#endif
#ifdef SIGPROF
	SIGNAL_ENTRY(PROF),
#endif
#ifdef SIGWINCH
	SIGNAL_ENTRY(WINCH),
#endif
#ifdef SIGIO
	SIGNAL_ENTRY(IO),
#endif
#ifdef SIGPWR
	SIGNAL_ENTRY(PWR),
#endif
#ifdef SIGSYS
	SIGNAL_ENTRY(SYS),
#endif
};

static const struct signal_entry signal_aliases[] = {
#ifdef SIGIOT
	SIGNAL_ALIAS("IOT", SIGIOT),
#endif
#ifdef SIGCLD
	SIGNAL_ALIAS("CLD", SIGCLD),
#endif
#ifdef SIGPOLL
	SIGNAL_ALIAS("POLL", SIGPOLL),
#endif
#ifdef SIGUNUSED
	SIGNAL_ALIAS("UNUSED", SIGUNUSED),
#endif
};

struct options {
	bool foreground;
	bool preserve;
	bool verbose;
	bool kill_after_set;
	int timeout_signal;
	long double duration;
	long double kill_after;
	const char *command_name;
	char **command_argv;
};

struct child_state {
	bool reaped;
	int status;
};

enum deadline_kind {
	DEADLINE_NONE = 0,
	DEADLINE_FIRST,
	DEADLINE_SECOND,
};

struct runtime_state {
	bool timed_out;
	bool first_timer_active;
	bool second_timer_active;
	bool second_pending;
	long double first_deadline;
	long double second_deadline;
	int active_signal;
};

static const char *program_name = "timeout";

static void usage(void) __attribute__((noreturn));
static void die(int status, const char *fmt, ...)
    __attribute__((noreturn, format(printf, 2, 3)));
static void die_errno(int status, const char *context) __attribute__((noreturn));
static void vlog(const struct options *opt, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
static const char *basename_const(const char *path);
static bool parse_nonnegative_int(const char *text, int *value);
static char *normalize_signal_name(const char *token);
static bool signal_name_for_number(int signum, char *buffer, size_t buffer_size);
static bool parse_signal_name(const char *token, int *signum);
static bool parse_signal_token(const char *token, int *signum);
static long double parse_duration_or_die(const char *text);
static long double monotonic_seconds(void);
static long double max_time_t_seconds(void);
static struct timespec seconds_to_timeout(long double seconds);
static enum deadline_kind next_deadline(const struct runtime_state *state,
    struct timespec *timeout);
static void enable_subreaper_or_die(void);
static bool is_terminating_signal(int signo);
static void send_signal_to_command(pid_t pid, int signo, const struct options *opt);
static void arm_second_timer(struct runtime_state *state, const struct options *opt);
static void reap_children(pid_t command_pid, struct child_state *child,
    bool *have_children, const struct options *opt);
static void child_exec(const struct options *opt, const sigset_t *oldmask)
    __attribute__((noreturn));
static void kill_self(int signo) __attribute__((noreturn));
static bool process_group_exists(pid_t pgid);

static void
usage(void)
{
	fprintf(stderr,
	    "Usage: %s [-f | --foreground] [-k time | --kill-after time]"
	    " [-p | --preserve-status] [-s signal | --signal signal] "
	    "[-v | --verbose] <duration> <command> [arg ...]\n",
	    program_name);
	exit(EXIT_INVALID);
}

static void
die(int status, const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "%s: ", program_name);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	exit(status);
}

static void
die_errno(int status, const char *context)
{
	die(status, "%s: %s", context, strerror(errno));
}

static void
vlog(const struct options *opt, const char *fmt, ...)
{
	va_list ap;

	if (!opt->verbose)
		return;

	fprintf(stderr, "%s: ", program_name);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
}

static const char *
basename_const(const char *path)
{
	const char *base;

	base = strrchr(path, '/');
	if (base == NULL || base[1] == '\0')
		return (path);
	return (base + 1);
}

static bool
parse_nonnegative_int(const char *text, int *value)
{
	intmax_t parsed;
	char *end;

	if (text[0] == '\0')
		return (false);

	errno = 0;
	parsed = strtoimax(text, &end, 10);
	if (errno == ERANGE || *end != '\0' || parsed < 0 || parsed > INT_MAX)
		return (false);
	*value = (int)parsed;
	return (true);
}

static char *
normalize_signal_name(const char *token)
{
	size_t i, length, offset;
	char *name;

	length = strlen(token);
	name = malloc(length + 1);
	if (name == NULL)
		die(EXIT_INVALID, "out of memory");

	for (i = 0; i < length; i++)
		name[i] = (char)toupper((unsigned char)token[i]);
	name[length] = '\0';

	offset = 0;
	if (length > 3 && name[0] == 'S' && name[1] == 'I' && name[2] == 'G')
		offset = 3;
	if (offset != 0)
		memmove(name, name + offset, length - offset + 1);
	return (name);
}

static bool
signal_name_for_number(int signum, char *buffer, size_t buffer_size)
{
	size_t i;

	if (signum == 0) {
		if (buffer != NULL && buffer_size > 0) {
			buffer[0] = '0';
			if (buffer_size > 1)
				buffer[1] = '\0';
		}
		return (true);
	}

	for (i = 0; i < sizeof(canonical_signals) / sizeof(canonical_signals[0]); i++) {
		if (canonical_signals[i].number != signum)
			continue;
		if (buffer != NULL && buffer_size > 0) {
			if (snprintf(buffer, buffer_size, "%s",
			    canonical_signals[i].name) >= (int)buffer_size)
				die(EXIT_INVALID, "internal signal name overflow");
		}
		return (true);
	}

#ifdef SIGRTMIN
#ifdef SIGRTMAX
	if (signum == SIGRTMIN) {
		if (buffer != NULL && buffer_size > 0)
			snprintf(buffer, buffer_size, "RTMIN");
		return (true);
	}
	if (signum == SIGRTMAX) {
		if (buffer != NULL && buffer_size > 0)
			snprintf(buffer, buffer_size, "RTMAX");
		return (true);
	}
	if (signum > SIGRTMIN && signum < SIGRTMAX) {
		if (buffer != NULL && buffer_size > 0) {
			if (snprintf(buffer, buffer_size, "RTMIN+%d",
			    signum - SIGRTMIN) >= (int)buffer_size)
				die(EXIT_INVALID, "internal signal name overflow");
		}
		return (true);
	}
#endif
#endif

	return (false);
}

static bool
parse_signal_name(const char *token, int *signum)
{
	char *name, *end;
	int offset;
	size_t i;

	if (strcmp(token, "0") == 0) {
		*signum = 0;
		return (true);
	}

	name = normalize_signal_name(token);

	for (i = 0; i < sizeof(canonical_signals) / sizeof(canonical_signals[0]); i++) {
		if (strcmp(name, canonical_signals[i].name) == 0) {
			*signum = canonical_signals[i].number;
			free(name);
			return (true);
		}
	}
	for (i = 0; i < sizeof(signal_aliases) / sizeof(signal_aliases[0]); i++) {
		if (strcmp(name, signal_aliases[i].name) == 0) {
			*signum = signal_aliases[i].number;
			free(name);
			return (true);
		}
	}

#ifdef SIGRTMIN
#ifdef SIGRTMAX
	if (strcmp(name, "RTMIN") == 0) {
		*signum = SIGRTMIN;
		free(name);
		return (true);
	}
	if (strncmp(name, "RTMIN+", 6) == 0) {
		errno = 0;
		offset = (int)strtol(name + 6, &end, 10);
		if (errno == 0 && *end == '\0' && offset >= 0 &&
		    SIGRTMIN + offset <= SIGRTMAX) {
			*signum = SIGRTMIN + offset;
			free(name);
			return (true);
		}
	}
	if (strcmp(name, "RTMAX") == 0) {
		*signum = SIGRTMAX;
		free(name);
		return (true);
	}
	if (strncmp(name, "RTMAX-", 6) == 0) {
		errno = 0;
		offset = (int)strtol(name + 6, &end, 10);
		if (errno == 0 && *end == '\0' && offset >= 0 &&
		    SIGRTMAX - offset >= SIGRTMIN) {
			*signum = SIGRTMAX - offset;
			free(name);
			return (true);
		}
	}
#endif
#endif

	free(name);
	return (false);
}

static bool
parse_signal_token(const char *token, int *signum)
{
	if (parse_nonnegative_int(token, signum)) {
		if (*signum >= NSIG)
			return (false);
		return (true);
	}
	return (parse_signal_name(token, signum));
}

static long double
parse_duration_or_die(const char *text)
{
	long double value;
	char *end;

	if (text[0] == '\0' || isspace((unsigned char)text[0]))
		die(EXIT_INVALID, "duration is not a number");

	errno = 0;
	value = strtold(text, &end);
	if (end == text)
		die(EXIT_INVALID, "duration is not a number");
	if (errno == ERANGE || !isfinite(value))
		die(EXIT_INVALID, "duration out of range");
	if (*end != '\0') {
		if (end[1] != '\0')
			die(EXIT_INVALID, "duration unit suffix too long");
		switch (*end) {
		case 's':
			break;
		case 'm':
			value *= 60.0L;
			break;
		case 'h':
			value *= 3600.0L;
			break;
		case 'd':
			value *= 86400.0L;
			break;
		default:
			die(EXIT_INVALID, "duration unit suffix invalid");
		}
	}
	if (!isfinite(value) || value < 0.0L)
		die(EXIT_INVALID, "duration out of range");
	return (value);
}

static long double
monotonic_seconds(void)
{
	struct timespec ts;

	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
		die_errno(EXIT_INVALID, "clock_gettime(CLOCK_MONOTONIC)");

	return ((long double)ts.tv_sec +
	    (long double)ts.tv_nsec / 1000000000.0L);
}

static long double
max_time_t_seconds(void)
{
	int bits;

	bits = (int)(sizeof(time_t) * CHAR_BIT);
	if ((time_t)-1 < (time_t)0)
		return (ldexpl(1.0L, bits - 1) - 1.0L);
	return (ldexpl(1.0L, bits) - 1.0L);
}

static struct timespec
seconds_to_timeout(long double seconds)
{
	struct timespec ts;
	long double whole, fractional, max_seconds;

	if (seconds <= 0.0L) {
		ts.tv_sec = 0;
		ts.tv_nsec = 0;
		return (ts);
	}

	max_seconds = max_time_t_seconds();
	if (seconds > max_seconds)
		seconds = max_seconds;

	fractional = modfl(seconds, &whole);
	ts.tv_sec = (time_t)whole;
	ts.tv_nsec = (long)(fractional * 1000000000.0L);
	if (ts.tv_nsec < 0)
		ts.tv_nsec = 0;
	if (ts.tv_nsec >= 1000000000L)
		ts.tv_nsec = 999999999L;
	return (ts);
}

static enum deadline_kind
next_deadline(const struct runtime_state *state, struct timespec *timeout)
{
	long double now, best, remaining;
	enum deadline_kind kind;

	now = monotonic_seconds();
	best = 0.0L;
	kind = DEADLINE_NONE;

	if (state->first_timer_active) {
		remaining = state->first_deadline - now;
		if (remaining < 0.0L)
			remaining = 0.0L;
		best = remaining;
		kind = DEADLINE_FIRST;
	}
	if (state->second_timer_active) {
		remaining = state->second_deadline - now;
		if (remaining < 0.0L)
			remaining = 0.0L;
		if (kind == DEADLINE_NONE || remaining < best) {
			best = remaining;
			kind = DEADLINE_SECOND;
		}
	}

	if (kind != DEADLINE_NONE)
		*timeout = seconds_to_timeout(best);
	return (kind);
}

static void
enable_subreaper_or_die(void)
{
#ifdef PR_SET_CHILD_SUBREAPER
	if (prctl(PR_SET_CHILD_SUBREAPER, 1L, 0L, 0L, 0L) != 0)
		die_errno(EXIT_INVALID, "prctl(PR_SET_CHILD_SUBREAPER)");
#else
	die(EXIT_INVALID,
	    "Linux child subreaper API is unavailable; use -f/--foreground");
#endif
}

static bool
is_terminating_signal(int signo)
{
	switch (signo) {
#ifdef SIGHUP
	case SIGHUP:
#endif
#ifdef SIGINT
	case SIGINT:
#endif
#ifdef SIGQUIT
	case SIGQUIT:
#endif
#ifdef SIGILL
	case SIGILL:
#endif
#ifdef SIGTRAP
	case SIGTRAP:
#endif
#ifdef SIGABRT
	case SIGABRT:
#endif
#ifdef SIGBUS
	case SIGBUS:
#endif
#ifdef SIGFPE
	case SIGFPE:
#endif
#ifdef SIGSEGV
	case SIGSEGV:
#endif
#ifdef SIGPIPE
	case SIGPIPE:
#endif
#ifdef SIGTERM
	case SIGTERM:
#endif
#ifdef SIGXCPU
	case SIGXCPU:
#endif
#ifdef SIGXFSZ
	case SIGXFSZ:
#endif
#ifdef SIGVTALRM
	case SIGVTALRM:
#endif
#ifdef SIGPROF
	case SIGPROF:
#endif
#ifdef SIGUSR1
	case SIGUSR1:
#endif
#ifdef SIGUSR2
	case SIGUSR2:
#endif
#ifdef SIGSYS
	case SIGSYS:
#endif
		return (true);
	default:
		return (false);
	}
}

static void
send_signal_to_command(pid_t pid, int signo, const struct options *opt)
{
	char sigbuf[32];
	const char *signame;
	pid_t target;

	if (!signal_name_for_number(signo, sigbuf, sizeof(sigbuf)))
		snprintf(sigbuf, sizeof(sigbuf), "%d", signo);
	signame = sigbuf;

	if (opt->foreground) {
		target = pid;
		vlog(opt, "sending signal %s(%d) to command '%s'",
		    signame, signo, opt->command_name);
	} else {
		target = -pid;
		vlog(opt, "sending signal %s(%d) to command group '%s'",
		    signame, signo, opt->command_name);
	}

	if (kill(target, signo) != 0 && errno != ESRCH) {
		fprintf(stderr, "%s: kill(%ld, %s): %s\n", program_name,
		    (long)target, signame, strerror(errno));
	}

	if (signo == SIGKILL || signo == SIGSTOP || signo == SIGCONT)
		return;

	if (!signal_name_for_number(SIGCONT, sigbuf, sizeof(sigbuf)))
		snprintf(sigbuf, sizeof(sigbuf), "%d", SIGCONT);
	vlog(opt, "sending signal %s(%d) to command '%s'",
	    sigbuf, SIGCONT, opt->command_name);
	(void)kill(target, SIGCONT);
}

static void
arm_second_timer(struct runtime_state *state, const struct options *opt)
{
	long double now;

	if (!state->second_pending)
		return;

	now = monotonic_seconds();
	state->second_deadline = now + opt->kill_after;
	state->second_timer_active = true;
	state->second_pending = false;
	state->active_signal = SIGKILL;
	state->first_timer_active = false;
}

static void
reap_children(pid_t command_pid, struct child_state *child, bool *have_children,
    const struct options *opt)
{
	pid_t waited;
	int status;

	*have_children = false;
	for (;;) {
		waited = waitpid(-1, &status, WNOHANG);
		if (waited > 0) {
			if (waited == command_pid) {
				child->reaped = true;
				child->status = status;
				if (WIFEXITED(status)) {
					vlog(opt, "child terminated: pid=%d exit=%d",
					    (int)waited, WEXITSTATUS(status));
				} else if (WIFSIGNALED(status)) {
					vlog(opt, "child terminated: pid=%d sig=%d",
					    (int)waited, WTERMSIG(status));
				} else {
					vlog(opt, "child changed state: pid=%d status=0x%x",
					    (int)waited, status);
				}
			} else {
				vlog(opt, "collected descendant: pid=%d status=0x%x",
				    (int)waited, status);
			}
			continue;
		}
		if (waited == 0) {
			*have_children = true;
			return;
		}
		if (errno == EINTR)
			continue;
		if (errno == ECHILD)
			return;
		die_errno(EXIT_INVALID, "waitpid");
	}
}

static void
child_exec(const struct options *opt, const sigset_t *oldmask)
{
	struct sigaction sa;
	int saved_errno;

	if (!opt->foreground) {
		if (setpgid(0, 0) != 0) {
			dprintf(STDERR_FILENO, "%s: setpgid: %s\n",
			    program_name, strerror(errno));
			_exit(EXIT_INVALID);
		}
	}

	if (opt->timeout_signal != SIGKILL && opt->timeout_signal != SIGSTOP) {
		memset(&sa, 0, sizeof(sa));
		sa.sa_handler = SIG_DFL;
		sigemptyset(&sa.sa_mask);
		if (sigaction(opt->timeout_signal, &sa, NULL) != 0) {
			dprintf(STDERR_FILENO, "%s: sigaction(%d): %s\n",
			    program_name, opt->timeout_signal, strerror(errno));
			_exit(EXIT_INVALID);
		}
	}

	if (sigprocmask(SIG_SETMASK, oldmask, NULL) != 0) {
		dprintf(STDERR_FILENO, "%s: sigprocmask: %s\n",
		    program_name, strerror(errno));
		_exit(EXIT_INVALID);
	}

	execvp(opt->command_argv[0], opt->command_argv);
	saved_errno = errno;
	dprintf(STDERR_FILENO, "%s: exec(%s): %s\n", program_name,
	    opt->command_argv[0], strerror(saved_errno));
	_exit(saved_errno == ENOENT ? EXIT_CMD_NOENT : EXIT_CMD_ERROR);
}

static void
kill_self(int signo)
{
	struct sigaction sa;
	struct rlimit rl;
	sigset_t mask;

	memset(&rl, 0, sizeof(rl));
	(void)setrlimit(RLIMIT_CORE, &rl);

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_DFL;
	sigemptyset(&sa.sa_mask);
	(void)sigaction(signo, &sa, NULL);

	sigfillset(&mask);
	sigdelset(&mask, signo);
	(void)sigprocmask(SIG_SETMASK, &mask, NULL);

	raise(signo);
	_exit(128 + signo);
}

static bool
process_group_exists(pid_t pgid)
{
	if (pgid <= 0)
		return (false);
	if (kill(-pgid, 0) == 0)
		return (true);
	if (errno == EPERM)
		return (true);
	if (errno == ESRCH)
		return (false);
	return (true);
}

int
main(int argc, char **argv)
{
	const char optstr[] = "+fk:ps:v";
	const struct option longopts[] = {
		{ "foreground", no_argument, NULL, 'f' },
		{ "kill-after", required_argument, NULL, 'k' },
		{ "preserve-status", no_argument, NULL, 'p' },
		{ "signal", required_argument, NULL, 's' },
		{ "verbose", no_argument, NULL, 'v' },
		{ NULL, 0, NULL, 0 },
	};
	struct options opt;
	struct child_state child;
	struct runtime_state state;
	struct timespec timeout;
	sigset_t allmask, oldmask;
	pid_t child_pid;
	int ch;

	if (argv[0] != NULL && argv[0][0] != '\0')
		program_name = basename_const(argv[0]);

	memset(&opt, 0, sizeof(opt));
	opt.timeout_signal = SIGTERM;

	opterr = 0;
	while ((ch = getopt_long(argc, argv, optstr, longopts, NULL)) != -1) {
		switch (ch) {
		case 'f':
			opt.foreground = true;
			break;
		case 'k':
			opt.kill_after = parse_duration_or_die(optarg);
			opt.kill_after_set = true;
			break;
		case 'p':
			opt.preserve = true;
			break;
		case 's':
			if (!parse_signal_token(optarg, &opt.timeout_signal))
				die(EXIT_INVALID, "invalid signal: %s", optarg);
			break;
		case 'v':
			opt.verbose = true;
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;
	if (argc < 2)
		usage();

	opt.duration = parse_duration_or_die(argv[0]);
	opt.command_argv = &argv[1];
	opt.command_name = argv[1];

	if (!opt.foreground)
		enable_subreaper_or_die();

	sigfillset(&allmask);
	sigdelset(&allmask, SIGKILL);
	sigdelset(&allmask, SIGSTOP);
	if (sigprocmask(SIG_SETMASK, &allmask, &oldmask) != 0)
		die_errno(EXIT_INVALID, "sigprocmask");

	child_pid = fork();
	if (child_pid < 0)
		die_errno(EXIT_INVALID, "fork");
	if (child_pid == 0)
		child_exec(&opt, &oldmask);

	if (!opt.foreground) {
		if (setpgid(child_pid, child_pid) != 0 &&
		    errno != EACCES && errno != ESRCH) {
			die_errno(EXIT_INVALID, "setpgid");
		}
	}

	memset(&child, 0, sizeof(child));
	memset(&state, 0, sizeof(state));
	state.active_signal = opt.timeout_signal;
	state.second_pending = opt.kill_after_set;
	if (opt.duration > 0.0L) {
		state.first_timer_active = true;
		state.first_deadline = monotonic_seconds() + opt.duration;
	}

	for (;;) {
		enum deadline_kind deadline_kind;
		bool have_children;
		siginfo_t si;
		int signo;

		reap_children(child_pid, &child, &have_children, &opt);
		if (opt.foreground) {
			if (child.reaped)
				break;
		} else {
			if (state.timed_out && have_children &&
			    !process_group_exists(child_pid)) {
				die(EXIT_INVALID,
				    "descendant processes escaped command process group; "
				    "unsupported on Linux without --foreground");
			}
			if (child.reaped && !have_children)
				break;
		}

		deadline_kind = next_deadline(&state, &timeout);
		if (deadline_kind == DEADLINE_NONE)
			signo = sigwaitinfo(&allmask, &si);
		else
			signo = sigtimedwait(&allmask, &si, &timeout);

		if (signo >= 0) {
			char sigbuf[32];

			if (signo == SIGCHLD)
				continue;

			if (!signal_name_for_number(signo, sigbuf, sizeof(sigbuf)))
				snprintf(sigbuf, sizeof(sigbuf), "%d", signo);

			if (signo == SIGALRM) {
				vlog(&opt, "received SIGALRM, treating as timeout");
				state.timed_out = true;
				state.first_timer_active = false;
				send_signal_to_command(child_pid, state.active_signal, &opt);
				arm_second_timer(&state, &opt);
				continue;
			}

			if (signo == state.active_signal || is_terminating_signal(signo)) {
				vlog(&opt, "received terminating signal %s(%d)",
				    sigbuf, signo);
				send_signal_to_command(child_pid, signo, &opt);
				arm_second_timer(&state, &opt);
			} else {
				vlog(&opt, "received signal %s(%d)", sigbuf, signo);
				send_signal_to_command(child_pid, signo, &opt);
			}
			continue;
		}

		if (errno == EINTR)
			continue;
		if (errno != EAGAIN)
			die_errno(EXIT_INVALID, "sigwait");

		if (deadline_kind == DEADLINE_FIRST) {
			state.timed_out = true;
			state.first_timer_active = false;
			vlog(&opt, "time limit reached");
			send_signal_to_command(child_pid, state.active_signal, &opt);
			arm_second_timer(&state, &opt);
		} else if (deadline_kind == DEADLINE_SECOND) {
			state.second_timer_active = false;
			vlog(&opt, "kill-after limit reached");
			send_signal_to_command(child_pid, SIGKILL, &opt);
		}
	}

	(void)sigprocmask(SIG_SETMASK, &oldmask, NULL);

	if (!child.reaped)
		die(EXIT_INVALID, "failed to retrieve command status");

	if (state.timed_out && !opt.preserve)
		return (EXIT_TIMEOUT);

	if (WIFEXITED(child.status))
		return (WEXITSTATUS(child.status));
	if (WIFSIGNALED(child.status))
		kill_self(WTERMSIG(child.status));

	return (EXIT_INVALID);
}
