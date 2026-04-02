/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Copyright (c) 2026
 *	Project Tick. All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Keith Muller of the University of California, San Diego and Lance
 * Visser of Convex Computer Corporation.
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

#include <sys/types.h>

#include <err.h>
#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "dd.h"
#include "extern.h"

static void format_scaled(char *, size_t, double, unsigned int,
    const char *const *);

static void
format_scaled(char *buf, size_t bufsz, double value, unsigned int base,
    const char *const *units)
{
	size_t unit;
	double scaled;

	scaled = value;
	unit = 0;
	while (scaled >= base && units[unit + 1] != NULL) {
		scaled /= base;
		unit++;
	}
	if (unit == 0)
		(void)snprintf(buf, bufsz, "%.0f %s", scaled, units[unit]);
	else if (scaled >= 10.0)
		(void)snprintf(buf, bufsz, "%.0f %s", scaled, units[unit]);
	else
		(void)snprintf(buf, bufsz, "%.1f %s", scaled, units[unit]);
}

double
secs_elapsed(void)
{
	struct timespec end, ts_res;
	double secs, res;

	if (clock_gettime(CLOCK_MONOTONIC, &end))
		err(1, "clock_gettime");
	if (clock_getres(CLOCK_MONOTONIC, &ts_res))
		err(1, "clock_getres");
	secs = (end.tv_sec - st.start.tv_sec) + \
	       (end.tv_nsec - st.start.tv_nsec) * 1e-9;
	res = ts_res.tv_sec + ts_res.tv_nsec * 1e-9;
	if (secs < res)
		secs = res;

	return (secs);
}

int
summary_signal_number(void)
{
#ifdef SIGINFO
	return (SIGINFO);
#else
	return (SIGUSR1);
#endif
}

const char *
summary_signal_name(void)
{
#ifdef SIGINFO
	return ("SIGINFO");
#else
	return ("SIGUSR1");
#endif
}

void
summary(void)
{
	double secs;

	if (ddflags & C_NOINFO)
		return;

	if (ddflags & C_PROGRESS)
		fprintf(stderr, "\n");

	secs = secs_elapsed();

	(void)fprintf(stderr,
	    "%ju+%ju records in\n%ju+%ju records out\n",
	    st.in_full, st.in_part, st.out_full, st.out_part);
	if (st.swab)
		(void)fprintf(stderr, "%ju odd length swab %s\n",
		     st.swab, (st.swab == 1) ? "block" : "blocks");
	if (st.trunc)
		(void)fprintf(stderr, "%ju truncated %s\n",
		     st.trunc, (st.trunc == 1) ? "block" : "blocks");
	if (!(ddflags & C_NOXFER)) {
		(void)fprintf(stderr,
		    "%ju bytes transferred in %.6f secs (%.0f bytes/sec)\n",
		    st.bytes, secs, st.bytes / secs);
	}
	need_summary = 0;
}

void
progress(void)
{
	static int outlen;
	static const char *const si_units[] = {
		"B", "kB", "MB", "GB", "TB", "PB", "EB", NULL
	};
	static const char *const iec_units[] = {
		"B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB", NULL
	};
	char buf[160];
	char iec[16];
	double secs;
	char persec[16];
	char si[16];

	secs = secs_elapsed();
	format_scaled(si, sizeof(si), (double)st.bytes, 1000, si_units);
	format_scaled(iec, sizeof(iec), (double)st.bytes, 1024, iec_units);
	format_scaled(persec, sizeof(persec), st.bytes / secs, 1000, si_units);
	(void)snprintf(buf, sizeof(buf),
	    "  %ju bytes (%s, %s) transferred %.3fs, %s/s",
	    (uintmax_t)st.bytes, si, iec, secs, persec);
	outlen = fprintf(stderr, "%-*s\r", outlen, buf) - 1;
	fflush(stderr);
	need_progress = 0;
}

/* ARGSUSED */
void
siginfo_handler(int signo __unused)
{

	need_summary = 1;
}

/* ARGSUSED */
void
sigalarm_handler(int signo __unused)
{

	need_progress = 1;
}

void
service_pending_signals(void)
{
	if (need_summary)
		summary();
	if (need_progress)
		progress();
}

static void terminate(int signo) __dead2;
static void
terminate(int signo)
{
	kill_signal = signo;
	summary();
	(void)fflush(stderr);
	raise(kill_signal);
	/* NOT REACHED */
	_exit(1);
}

static sig_atomic_t in_io = 0;
static sig_atomic_t sigint_seen = 0;

static void
sigint_handler(int signo __unused)
{
	atomic_signal_fence(memory_order_acquire);
	if (in_io)
		terminate(SIGINT);
	sigint_seen = 1;
}

void
prepare_io(void)
{
	struct sigaction sa;
	int error;

	memset(&sa, 0, sizeof(sa));
	sa.sa_flags = SA_NODEFER | SA_RESETHAND;
	sa.sa_handler = sigint_handler;
	error = sigaction(SIGINT, &sa, 0);
	if (error != 0)
		err(1, "sigaction");
}

void
before_io(void)
{
	in_io = 1;
	atomic_signal_fence(memory_order_seq_cst);
	if (sigint_seen)
		terminate(SIGINT);
}

void
after_io(void)
{
	in_io = 0;
	atomic_signal_fence(memory_order_seq_cst);
	if (sigint_seen)
		terminate(SIGINT);
}
