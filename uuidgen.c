/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2002 Marcel Moolenaar
 * Copyright (c) 2022 Tobias C. Berner
 * Copyright (c) 2026 Project Tick
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#define UUID_COUNT_MAX 2048U
#define UUID_GREGORIAN_OFFSET 0x01B21DD213814000ULL
#define UUID_TICKS_PER_SECOND 10000000ULL
#define UUID_NANOSECONDS_PER_TICK 100ULL

struct uuid {
	uint8_t bytes[16];
};

struct uuid_v1_state {
	uint8_t node[6];
	uint16_t clock_seq;
	uint64_t last_timestamp;
	bool initialized;
};

struct options {
	bool iterate;
	bool compact;
	bool random_v4;
	size_t count;
	const char *output_path;
};

static const char *progname = "uuidgen";

extern long	syscall(long number, ...);

static void	die_errno(const char *what) __attribute__((noreturn));
static void	diex(const char *fmt, ...) __attribute__((format(printf, 1, 2),
		    noreturn));
static void	fill_random_bytes(void *buf, size_t len);
static void	format_uuid(const struct uuid *id, bool compact,
		    char out[static 37]);
static ssize_t	linux_getrandom(void *buf, size_t len, unsigned int flags);
static uint64_t	now_uuid_timestamp(void);
static void	pack_uuid_v1(struct uuid *id, uint64_t timestamp,
		    uint16_t clock_seq, const uint8_t node[static 6]);
static void	parse_options(int argc, char *argv[], struct options *opts);
static size_t	parse_count(const char *arg);
static void	set_progname(const char *argv0);
static void	usage(void) __attribute__((noreturn));
static void	uuid_v1_init(struct uuid_v1_state *state);
static void	uuid_v1_generate(struct uuid_v1_state *state,
		    struct uuid *ids, size_t count, bool iterate);
static void	uuid_v4_generate(struct uuid *ids, size_t count);
static void	write_random_from_urandom(void *buf, size_t len);

int
main(int argc, char *argv[])
{
	FILE *out;
	struct options opts;
	struct uuid_v1_state state;
	struct uuid *ids;
	size_t i;

	set_progname(argv[0]);
	parse_options(argc, argv, &opts);

	out = stdout;
	if (opts.output_path != NULL) {
		out = fopen(opts.output_path, "w");
		if (out == NULL)
			diex("cannot open '%s': %s", opts.output_path,
			    strerror(errno));
	}

	ids = calloc(opts.count, sizeof(*ids));
	if (ids == NULL)
		die_errno("calloc");

	memset(&state, 0, sizeof(state));

	if (opts.random_v4)
		uuid_v4_generate(ids, opts.count);
	else
		uuid_v1_generate(&state, ids, opts.count, opts.iterate);

	for (i = 0; i < opts.count; i++) {
		char line[37];

		format_uuid(&ids[i], opts.compact, line);
		if (fputs(line, out) == EOF || fputc('\n', out) == EOF)
			die_errno("write");
	}

	free(ids);

	if (out != stdout && fclose(out) != 0)
		die_errno("fclose");

	return (0);
}

static void
set_progname(const char *argv0)
{
	const char *slash;

	if (argv0 == NULL || argv0[0] == '\0')
		return;

	slash = strrchr(argv0, '/');
	if (slash != NULL && slash[1] != '\0')
		progname = slash + 1;
	else
		progname = argv0;
}

static void
parse_options(int argc, char *argv[], struct options *opts)
{
	bool count_set, output_set;
	int ch;

	memset(opts, 0, sizeof(*opts));
	opts->count = 1;

	count_set = false;
	output_set = false;

	opterr = 0;
	while ((ch = getopt(argc, argv, ":1crn:o:")) != -1) {
		switch (ch) {
		case '1':
			opts->iterate = true;
			break;
		case 'c':
			opts->compact = true;
			break;
		case 'r':
			opts->random_v4 = true;
			break;
		case 'n':
			if (count_set)
				diex("option -n specified more than once");
			opts->count = parse_count(optarg);
			count_set = true;
			break;
		case 'o':
			if (output_set)
				diex("multiple output files not allowed");
			opts->output_path = optarg;
			output_set = true;
			break;
		case ':':
		case '?':
		default:
			usage();
		}
	}

	argc -= optind;
	if (argc != 0)
		usage();
}

static size_t
parse_count(const char *arg)
{
	char *end;
	uintmax_t value;

	errno = 0;
	value = strtoumax(arg, &end, 10);
	if (arg[0] == '\0' || *end != '\0' || errno == ERANGE || value == 0 ||
	    value > UUID_COUNT_MAX) {
		diex("invalid count '%s' (expected 1..%u)", arg, UUID_COUNT_MAX);
	}

	return ((size_t)value);
}

static void
uuid_v4_generate(struct uuid *ids, size_t count)
{
	size_t i;

	if (count == 0)
		return;

	fill_random_bytes(ids, sizeof(*ids) * count);
	for (i = 0; i < count; i++) {
		ids[i].bytes[6] = (uint8_t)((ids[i].bytes[6] & 0x0fU) | 0x40U);
		ids[i].bytes[8] = (uint8_t)((ids[i].bytes[8] & 0x3fU) | 0x80U);
	}
}

static void
uuid_v1_init(struct uuid_v1_state *state)
{
	uint8_t seed[8];

	fill_random_bytes(seed, sizeof(seed));
	memcpy(state->node, seed, sizeof(state->node));
	state->node[0] |= 0x01U;
	state->clock_seq = (uint16_t)(((uint16_t)seed[6] << 8) | seed[7]);
	state->clock_seq &= 0x3fffU;
	state->last_timestamp = 0;
	state->initialized = true;
}

static void
uuid_v1_generate(struct uuid_v1_state *state, struct uuid *ids, size_t count,
    bool iterate)
{
	size_t i;

	if (count == 0)
		return;
	if (!state->initialized)
		uuid_v1_init(state);

	if (!iterate) {
		uint64_t base;

		base = now_uuid_timestamp();
		if (base < state->last_timestamp)
			state->clock_seq = (uint16_t)((state->clock_seq + 1) &
			    0x3fffU);
		if (base <= state->last_timestamp)
			base = state->last_timestamp + 1;
		if (UINT64_MAX - base < (uint64_t)(count - 1))
			diex("timestamp overflow while generating UUID batch");

		for (i = 0; i < count; i++)
			pack_uuid_v1(&ids[i], base + i, state->clock_seq,
			    state->node);
		state->last_timestamp = base + (count - 1);
		return;
	}

	for (i = 0; i < count; i++) {
		uint64_t now;

		now = now_uuid_timestamp();
		if (now < state->last_timestamp)
			state->clock_seq = (uint16_t)((state->clock_seq + 1) &
			    0x3fffU);
		if (now <= state->last_timestamp)
			now = state->last_timestamp + 1;

		pack_uuid_v1(&ids[i], now, state->clock_seq, state->node);
		state->last_timestamp = now;
	}
}

static uint64_t
now_uuid_timestamp(void)
{
	struct timespec ts;
	uint64_t seconds, ticks;

	if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
		die_errno("clock_gettime");
	if (ts.tv_sec < 0)
		diex("clock_gettime returned a pre-1970 timestamp");

	seconds = (uint64_t)ts.tv_sec;
	ticks = seconds * UUID_TICKS_PER_SECOND;
	ticks += (uint64_t)ts.tv_nsec / UUID_NANOSECONDS_PER_TICK;
	ticks += UUID_GREGORIAN_OFFSET;

	return (ticks);
}

static void
pack_uuid_v1(struct uuid *id, uint64_t timestamp, uint16_t clock_seq,
    const uint8_t node[static 6])
{
	uint16_t time_mid, time_hi;
	uint32_t time_low;

	time_low = (uint32_t)(timestamp & 0xffffffffU);
	time_mid = (uint16_t)((timestamp >> 32) & 0xffffU);
	time_hi = (uint16_t)((timestamp >> 48) & 0x0fffU);

	id->bytes[0] = (uint8_t)(time_low >> 24);
	id->bytes[1] = (uint8_t)(time_low >> 16);
	id->bytes[2] = (uint8_t)(time_low >> 8);
	id->bytes[3] = (uint8_t)time_low;
	id->bytes[4] = (uint8_t)(time_mid >> 8);
	id->bytes[5] = (uint8_t)time_mid;
	id->bytes[6] = (uint8_t)(((time_hi >> 8) & 0x0fU) | 0x10U);
	id->bytes[7] = (uint8_t)time_hi;
	id->bytes[8] = (uint8_t)(((clock_seq >> 8) & 0x3fU) | 0x80U);
	id->bytes[9] = (uint8_t)clock_seq;
	memcpy(&id->bytes[10], node, 6);
}

static void
format_uuid(const struct uuid *id, bool compact, char out[static 37])
{
	int n;

	if (compact) {
		n = snprintf(out, 33,
		    "%02x%02x%02x%02x%02x%02x%02x%02x"
		    "%02x%02x%02x%02x%02x%02x%02x%02x",
		    id->bytes[0], id->bytes[1], id->bytes[2], id->bytes[3],
		    id->bytes[4], id->bytes[5], id->bytes[6], id->bytes[7],
		    id->bytes[8], id->bytes[9], id->bytes[10], id->bytes[11],
		    id->bytes[12], id->bytes[13], id->bytes[14], id->bytes[15]);
		if (n != 32)
			diex("internal error formatting compact UUID");
		return;
	}

	n = snprintf(out, 37,
	    "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
	    id->bytes[0], id->bytes[1], id->bytes[2], id->bytes[3],
	    id->bytes[4], id->bytes[5], id->bytes[6], id->bytes[7],
	    id->bytes[8], id->bytes[9], id->bytes[10], id->bytes[11],
	    id->bytes[12], id->bytes[13], id->bytes[14], id->bytes[15]);
	if (n != 36)
		diex("internal error formatting UUID");
}

static void
fill_random_bytes(void *buf, size_t len)
{
	uint8_t *p;

	p = buf;
	while (len > 0) {
		ssize_t n;

		n = linux_getrandom(p, len, 0);
		if (n > 0) {
			p += n;
			len -= (size_t)n;
			continue;
		}
		if (n == 0)
			continue;
		if (errno == EINTR)
			continue;
		if (errno == ENOSYS) {
			write_random_from_urandom(p, len);
			return;
		}
		die_errno("getrandom");
	}
}

static ssize_t
linux_getrandom(void *buf, size_t len, unsigned int flags)
{
#ifdef SYS_getrandom
	return ((ssize_t)syscall(SYS_getrandom, buf, len, flags));
#else
	(void)buf;
	(void)len;
	(void)flags;
	errno = ENOSYS;
	return (-1);
#endif
}

static void
write_random_from_urandom(void *buf, size_t len)
{
	const char *path;
	uint8_t *p;
	int fd;

	path = "/dev/urandom";
	fd = open(path, O_RDONLY);
	if (fd < 0)
		die_errno(path);

	p = buf;
	while (len > 0) {
		ssize_t n;

		n = read(fd, p, len);
		if (n > 0) {
			p += n;
			len -= (size_t)n;
			continue;
		}
		if (n == 0) {
			(void)close(fd);
			diex("%s: unexpected EOF", path);
		}
		if (errno == EINTR)
			continue;
		(void)close(fd);
		die_errno(path);
	}

	if (close(fd) != 0)
		die_errno("close");
}

static void
die_errno(const char *what)
{
	fprintf(stderr, "%s: %s: %s\n", progname, what, strerror(errno));
	exit(1);
}

static void
diex(const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "%s: ", progname);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	exit(1);
}

static void
usage(void)
{
	fprintf(stderr, "usage: %s [-1] [-r] [-c] [-n count] [-o filename]\n",
	    progname);
	exit(1);
}
