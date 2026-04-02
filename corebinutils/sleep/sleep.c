/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1988, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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

#include <stdarg.h>
#include <stdbool.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef SIGINFO
#define SLEEP_INFO_SIGNAL SIGUSR1
#else
#define SLEEP_INFO_SIGNAL SIGINFO
#endif

#define NSECS_PER_SEC 1000000000L

static volatile sig_atomic_t report_requested;

static void usage(void) __attribute__((noreturn));
static void die(const char *fmt, ...) __attribute__((noreturn, format(printf, 1, 2)));
static void die_errno(const char *context) __attribute__((noreturn));
static void install_info_handler(void);
static long double seconds_from_timespec(const struct timespec *ts);
static long double maximum_time_t_seconds(void);
static void report_remaining(const struct timespec *remaining, long double original);
static long double scale_interval(long double value, int multiplier, const char *arg);
static long double parse_interval(const char *arg);
static struct timespec seconds_to_timespec(long double seconds);

static void
report_request(int signo)
{
	(void)signo;
	report_requested = 1;
}

static void
usage(void)
{
	fprintf(stderr, "usage: sleep number[unit] [...]\n"
	    "Unit can be 's' (seconds, the default), "
	    "m (minutes), h (hours), or d (days).\n");
	exit(1);
}

static void
die(const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "sleep: ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	exit(1);
}

static void
die_errno(const char *context)
{
	die("%s: %s", context, strerror(errno));
}

static void
install_info_handler(void)
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = report_request;
	sigemptyset(&sa.sa_mask);
	if (sigaction(SLEEP_INFO_SIGNAL, &sa, NULL) != 0)
		die_errno("sigaction");
}

static long double
seconds_from_timespec(const struct timespec *ts)
{
	return ((long double)ts->tv_sec +
	    (long double)ts->tv_nsec / (long double)NSECS_PER_SEC);
}

static long double
maximum_time_t_seconds(void)
{
	int bits;

	bits = (int)(sizeof(time_t) * CHAR_BIT);
	if ((time_t)-1 < (time_t)0)
		return (ldexpl(1.0L, bits - 1) - 1.0L);
	return (ldexpl(1.0L, bits) - 1.0L);
}

static void
report_remaining(const struct timespec *remaining, long double original)
{
	if (printf("about %.9Lf second(s) left out of the original %.9Lf\n",
	    seconds_from_timespec(remaining), original) < 0)
		die_errno("stdout");
	if (fflush(stdout) != 0)
		die_errno("stdout");
}

static long double
scale_interval(long double value, int multiplier, const char *arg)
{
	long double result;

	result = value * (long double)multiplier;
	if (!isfinite(result))
		die("time interval out of range: %s", arg);
	return (result);
}

static long double
parse_interval(const char *arg)
{
	long double value;
	char *end;

	errno = 0;
	value = strtold(arg, &end);
	if (end == arg)
		die("invalid time interval: %s", arg);
	if (errno == ERANGE)
		die("time interval out of range: %s", arg);
	if (!isfinite(value))
		die("non-finite time interval is not supported on Linux: %s", arg);
	if (*end == '\0')
		return (value);
	if (end[1] != '\0')
		die("invalid time interval: %s", arg);

	switch (*end) {
	case 's':
		return (value);
	case 'm':
		return (scale_interval(value, 60, arg));
	case 'h':
		return (scale_interval(value, 60 * 60, arg));
	case 'd':
		return (scale_interval(value, 24 * 60 * 60, arg));
	default:
		die("unsupported time unit in interval '%s': '%c' (supported: s, m, h, d)",
		    arg, *end);
	}
}

static struct timespec
seconds_to_timespec(long double seconds)
{
	struct timespec ts;
	long double whole;
	long double fractional;
	long double nsec;
	long double max_seconds;

	if (seconds <= 0.0L) {
		ts.tv_sec = 0;
		ts.tv_nsec = 0;
		return (ts);
	}

	max_seconds = maximum_time_t_seconds();
	if (seconds > max_seconds + 1.0L)
		die("requested interval is too large for Linux sleep APIs");

	fractional = modfl(seconds, &whole);
	if (whole > max_seconds)
		die("requested interval is too large for Linux sleep APIs");

	ts.tv_sec = (time_t)whole;
	nsec = ceill(fractional * (long double)NSECS_PER_SEC);
	if (nsec >= (long double)NSECS_PER_SEC) {
		if ((long double)ts.tv_sec >= max_seconds)
			die("requested interval is too large for Linux sleep APIs");
		ts.tv_sec += 1;
		ts.tv_nsec = 0;
		return (ts);
	}
	if (nsec <= 0.0L)
		nsec = 1.0L;
	ts.tv_nsec = (long)nsec;
	return (ts);
}

int
main(int argc, char *argv[])
{
	struct timespec time_to_sleep;
	long double original;
	long double seconds;
	int i;

	if (argc > 1 && strcmp(argv[1], "--") == 0) {
		argv++;
		argc--;
	}
	if (argc < 2)
		usage();

	seconds = 0.0L;
	for (i = 1; i < argc; i++) {
		long double interval;
		long double total;

		interval = parse_interval(argv[i]);
		total = seconds + interval;
		if (!isfinite(total))
			die("time interval out of range after adding: %s", argv[i]);
		seconds = total;
	}
	if (seconds <= 0.0L)
		exit(0);

	time_to_sleep = seconds_to_timespec(seconds);
	original = seconds_from_timespec(&time_to_sleep);
	install_info_handler();

	while (nanosleep(&time_to_sleep, &time_to_sleep) != 0) {
		if (errno != EINTR)
			die_errno("nanosleep");
		if (report_requested) {
			report_remaining(&time_to_sleep, original);
			report_requested = 0;
		}
	}

	exit(0);
}
