/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1988, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2026
 *	Project Tick. All rights reserved.
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

#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <unistd.h>

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

static void usage(void);
static void die(int status, const char *fmt, ...);
static char *normalize_signal_name(const char *token);
static bool parse_pid_argument(const char *text, pid_t *pid);
static bool parse_nonnegative_number(const char *text, int *value);
static bool parse_signal_name(const char *token, int *signum);
static bool parse_signal_option_token(const char *token, int *signum);
static bool parse_signal_for_dash_s(const char *token, int *signum);
static bool signal_name_for_number(int signum, char *buffer, size_t buffer_size);
static void printsignals(FILE *fp);
static void unknown_signal(const char *token);
static int max_signal_number(void);

static void
usage(void)
{
	fprintf(stderr,
	    "usage: kill [-s signal_name] pid ...\n"
	    "       kill -l [exit_status]\n"
	    "       kill -signal_name pid ...\n"
	    "       kill -signal_number pid ...\n");
	exit(2);
}

static void
die(int status, const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "kill: ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	exit(status);
}

static char *
normalize_signal_name(const char *token)
{
	size_t i, length, offset;
	char *name;

	length = strlen(token);
	name = malloc(length + 1);
	if (name == NULL)
		die(2, "out of memory");

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
parse_pid_argument(const char *text, pid_t *pid)
{
	intmax_t value;
	pid_t converted;
	char *end;

	if (text[0] == '\0')
		return (false);

	errno = 0;
	value = strtoimax(text, &end, 10);
	if (errno == ERANGE || *end != '\0')
		return (false);

	converted = (pid_t)value;
	if ((intmax_t)converted != value)
		return (false);

	*pid = converted;
	return (true);
}

static bool
parse_nonnegative_number(const char *text, int *value)
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

static bool
parse_signal_name(const char *token, int *signum)
{
	char *name;
	char *end;
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
parse_signal_option_token(const char *token, int *signum)
{
	if (parse_nonnegative_number(token, signum))
		return (true);
	return (parse_signal_name(token, signum));
}

static bool
parse_signal_for_dash_s(const char *token, int *signum)
{
	if (parse_nonnegative_number(token, signum))
		return (true);
	return (parse_signal_name(token, signum));
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
				die(2, "internal signal name buffer overflow");
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
				die(2, "internal signal name buffer overflow");
		}
		return (true);
	}
#endif
#endif

	return (false);
}

static void
printsignals(FILE *fp)
{
	char signame[32];
	int line_length, maxsig, signum;

	line_length = 0;
	maxsig = max_signal_number();
	for (signum = 1; signum <= maxsig; signum++) {
		size_t name_length;

		if (!signal_name_for_number(signum, signame, sizeof(signame)))
			continue;

		name_length = strlen(signame);
		if (line_length != 0) {
			if (line_length + 1 + (int)name_length > 72) {
				fputc('\n', fp);
				line_length = 0;
			} else {
				fputc(' ', fp);
				line_length++;
			}
		}
		fputs(signame, fp);
		line_length += (int)name_length;
	}
	fputc('\n', fp);
}

static void
unknown_signal(const char *token)
{
	fprintf(stderr, "kill: unknown signal %s; valid signals:\n", token);
	printsignals(stderr);
	exit(2);
}

static int
max_signal_number(void)
{
	size_t i;
	int maxsig;

	maxsig = 0;
	for (i = 0; i < sizeof(canonical_signals) / sizeof(canonical_signals[0]); i++) {
		if (canonical_signals[i].number > maxsig)
			maxsig = canonical_signals[i].number;
	}
#ifdef SIGRTMAX
	if (SIGRTMAX > maxsig)
		maxsig = SIGRTMAX;
#endif
	return (maxsig);
}

int
main(int argc, char *argv[])
{
	char signame[32];
	pid_t pid;
	int errors, signum, status_value;

	if (argc < 2)
		usage();

	signum = SIGTERM;
	argc--;
	argv++;

	if (strcmp(argv[0], "-l") == 0) {
		argc--;
		argv++;
		if (argc > 1)
			usage();
		if (argc == 0) {
			printsignals(stdout);
			return (0);
		}

		if (parse_nonnegative_number(argv[0], &status_value)) {
			if (status_value >= 128)
				status_value -= 128;
			if (status_value == 0) {
				puts("0");
				return (0);
			}
			if (!signal_name_for_number(status_value, signame, sizeof(signame)))
				unknown_signal(argv[0]);
			puts(signame);
			return (0);
		}
		if (!parse_signal_name(argv[0], &status_value))
			unknown_signal(argv[0]);
		printf("%d\n", status_value);
		return (0);
	}

	if (strcmp(argv[0], "-s") == 0) {
		argc--;
		argv++;
		if (argc == 0) {
			fprintf(stderr, "kill: option requires an argument -- s\n");
			usage();
		}
		if (!parse_signal_for_dash_s(argv[0], &signum))
			die(2, "option -s requires a signal name, number, or 0: %s",
			    argv[0]);
		argc--;
		argv++;
	} else if (argv[0][0] == '-' && argv[0][1] != '\0' && argv[0][1] != '-') {
		if (!parse_signal_option_token(argv[0] + 1, &signum))
			unknown_signal(argv[0] + 1);
		argc--;
		argv++;
	}

	if (argc > 0 && strcmp(argv[0], "--") == 0) {
		argc--;
		argv++;
	}

	if (argc == 0)
		usage();

	errors = 0;
	for (; argc > 0; argc--, argv++) {
		if (argv[0][0] == '%')
			die(2, "job control process specifications require a shell builtin: %s",
			    argv[0]);
		if (!parse_pid_argument(argv[0], &pid))
			die(2, "illegal process id: %s", argv[0]);
		if (kill(pid, signum) != 0) {
			fprintf(stderr, "kill: %s: %s\n", argv[0], strerror(errno));
			errors = 1;
		}
	}

	return (errors);
}
