/*

SPDX-License-Identifier: BSD-3-Clause

Copyright (c) 2026
 Project Tick. All rights reserved.

This code is derived from software contributed to Berkeley by
the Institute of Electrical and Electronics Engineers, Inc.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. Neither the name of the University nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE.

*/

#define _DEFAULT_SOURCE 1
#define _XOPEN_SOURCE 700

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdnoreturn.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#ifndef N_TTY
#define N_TTY 0
#endif

#ifndef CTRL
#define CTRL(x) ((x) & 037)
#endif
#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))
#define OUTPUT_WIDTH 72

#ifndef CINTR
#define CINTR CTRL('c')
#endif
#ifndef CQUIT
#define CQUIT 034
#endif
#ifndef CERASE
#define CERASE 0177
#endif
#ifndef CKILL
#define CKILL CTRL('u')
#endif
#ifndef CEOF
#define CEOF CTRL('d')
#endif
#ifndef CEOL
#define CEOL _POSIX_VDISABLE
#endif
#ifndef CSUSP
#define CSUSP CTRL('z')
#endif
#ifndef CSTART
#define CSTART CTRL('q')
#endif
#ifndef CSTOP
#define CSTOP CTRL('s')
#endif
#ifndef CDISCARD
#define CDISCARD CTRL('o')
#endif
#ifndef CLNEXT
#define CLNEXT CTRL('v')
#endif
#ifndef CREPRINT
#define CREPRINT CTRL('r')
#endif
#ifndef CWERASE
#define CWERASE CTRL('w')
#endif

enum output_format {
	FORMAT_DEFAULT,
	FORMAT_BSD,
	FORMAT_POSIX,
	FORMAT_GFMT,
};

enum termios_field {
	FIELD_IFLAG,
	FIELD_OFLAG,
	FIELD_CFLAG,
	FIELD_LFLAG,
};

struct state {
	int fd;
	bool close_fd;
	const char *label;
	struct termios termios;
	struct termios sane;
	struct winsize winsize;
	bool have_winsize;
	bool termios_dirty;
	bool winsize_dirty;
	int line_discipline;
	bool have_line_discipline;
};

struct speed_map {
	unsigned int baud;
	speed_t symbol;
};

struct flag_action {
	const char *name;
	enum termios_field field;
	tcflag_t set_mask;
	tcflag_t clear_mask;
};

struct print_flag {
	const char *off_name;
	enum termios_field field;
	tcflag_t mask;
};

struct control_char {
	const char *name;
	size_t index;
	cc_t default_value;
	bool numeric;
};

struct named_command {
	const char *name;
	bool allow_off;
	bool needs_argument;
	enum {
		CMD_PRINT_BSD,
		CMD_SANE,
		CMD_CBREAK,
		CMD_COLUMNS,
		CMD_ROWS,
		CMD_SIZE,
		CMD_SPEED,
		CMD_ISPEED,
		CMD_OSPEED,
		CMD_DEC,
		CMD_TTY,
		CMD_NL,
		CMD_RAW,
		CMD_EK
	} id;
};

struct line_builder {
	const char *label;
	size_t column;
};

static noreturn void usage(void);
static noreturn void usage_error(const char *fmt, ...);
static noreturn void die(const char *fmt, ...);
static noreturn void die_errno(const char *fmt, ...);
static noreturn void die_unsupported(const char *name, const char *reason);

static void init_state(struct state *state, const char *path);
static void make_sane_termios(struct termios *sane, const struct termios *current);
static void apply_named_argument(struct state *state, char **argv, int argc, int *index);
static bool maybe_apply_gfmt(struct state *state, const char *argument);
static bool maybe_apply_flag_action(struct state *state, const char *argument);
static bool maybe_apply_command(struct state *state, char **argv, int argc, int *index);
static bool maybe_apply_control_char(struct state *state, char **argv, int argc, int *index);
static bool maybe_apply_speed_token(struct state *state, const char *argument);
static void print_state(const struct state *state, enum output_format format);
static void print_boolean_group(const struct state *state, enum output_format format,
    const char *label, const struct print_flag *flags, size_t count);
static void print_control_chars(const struct state *state, enum output_format format);
static void line_builder_begin(struct line_builder *builder, const char *label);
static void line_builder_put(struct line_builder *builder, const char *token);
static void line_builder_finish(struct line_builder *builder);
static tcflag_t *field_ptr(struct termios *termios, enum termios_field field);
static tcflag_t field_value(const struct termios *termios, enum termios_field field);
static const struct control_char *find_control_char(const char *name);
static const struct named_command *find_command(const char *name);
static const char *unsupported_reason(const char *name);
static void set_rows_or_columns(unsigned short *slot, const char *keyword, const char *value);
static void set_speed(struct state *state, const char *keyword, const char *value,
    bool set_input, bool set_output);
static speed_t lookup_speed_symbol(unsigned int baud, bool *found);
static unsigned int speed_to_baud(speed_t symbol);
static unsigned long long parse_unsigned(const char *text, int base,
    unsigned long long max_value, const char *what);
static bool is_decimal_number(const char *text);
static cc_t parse_control_char_value(const struct control_char *control, const char *value);
static const char *format_control_char(const struct control_char *control, cc_t value,
    char *buffer, size_t buffer_size);
static void apply_sane(struct state *state);
static void apply_cbreak(struct state *state, bool off);
static void apply_dec(struct state *state);
static void apply_nl(struct state *state, bool off);
static void apply_raw(struct state *state, bool off);
static void apply_tty(struct state *state);
static void apply_ek(struct state *state);

static const struct speed_map speed_table[] = {
#ifdef B0
	{ 0U, B0 },
#endif
#ifdef B50
	{ 50U, B50 },
#endif
#ifdef B75
	{ 75U, B75 },
#endif
#ifdef B110
	{ 110U, B110 },
#endif
#ifdef B134
	{ 134U, B134 },
#endif
#ifdef B150
	{ 150U, B150 },
#endif
#ifdef B200
	{ 200U, B200 },
#endif
#ifdef B300
	{ 300U, B300 },
#endif
#ifdef B600
	{ 600U, B600 },
#endif
#ifdef B1200
	{ 1200U, B1200 },
#endif
#ifdef B1800
	{ 1800U, B1800 },
#endif
#ifdef B2400
	{ 2400U, B2400 },
#endif
#ifdef B4800
	{ 4800U, B4800 },
#endif
#ifdef B9600
	{ 9600U, B9600 },
#endif
#ifdef B19200
	{ 19200U, B19200 },
#endif
#ifdef B38400
	{ 38400U, B38400 },
#endif
#ifdef B57600
	{ 57600U, B57600 },
#endif
#ifdef B115200
	{ 115200U, B115200 },
#endif
#ifdef B230400
	{ 230400U, B230400 },
#endif
#ifdef B460800
	{ 460800U, B460800 },
#endif
#ifdef B500000
	{ 500000U, B500000 },
#endif
#ifdef B576000
	{ 576000U, B576000 },
#endif
#ifdef B921600
	{ 921600U, B921600 },
#endif
#ifdef B1000000
	{ 1000000U, B1000000 },
#endif
#ifdef B1152000
	{ 1152000U, B1152000 },
#endif
#ifdef B1500000
	{ 1500000U, B1500000 },
#endif
#ifdef B2000000
	{ 2000000U, B2000000 },
#endif
#ifdef B2500000
	{ 2500000U, B2500000 },
#endif
#ifdef B3000000
	{ 3000000U, B3000000 },
#endif
#ifdef B3500000
	{ 3500000U, B3500000 },
#endif
#ifdef B4000000
	{ 4000000U, B4000000 },
#endif
};

static const struct flag_action flag_actions[] = {
	{ "cs5", FIELD_CFLAG, CS5, CSIZE },
	{ "cs6", FIELD_CFLAG, CS6, CSIZE },
	{ "cs7", FIELD_CFLAG, CS7, CSIZE },
	{ "cs8", FIELD_CFLAG, CS8, CSIZE },
	{ "cstopb", FIELD_CFLAG, CSTOPB, 0 },
	{ "-cstopb", FIELD_CFLAG, 0, CSTOPB },
	{ "cread", FIELD_CFLAG, CREAD, 0 },
	{ "-cread", FIELD_CFLAG, 0, CREAD },
	{ "parenb", FIELD_CFLAG, PARENB, 0 },
	{ "-parenb", FIELD_CFLAG, 0, PARENB },
	{ "parodd", FIELD_CFLAG, PARODD, 0 },
	{ "-parodd", FIELD_CFLAG, 0, PARODD },
	{ "parity", FIELD_CFLAG, PARENB | CS7, PARODD | CSIZE },
	{ "-parity", FIELD_CFLAG, CS8, PARENB | PARODD | CSIZE },
	{ "evenp", FIELD_CFLAG, PARENB | CS7, PARODD | CSIZE },
	{ "-evenp", FIELD_CFLAG, CS8, PARENB | PARODD | CSIZE },
	{ "oddp", FIELD_CFLAG, PARENB | PARODD | CS7, CSIZE },
	{ "-oddp", FIELD_CFLAG, CS8, PARENB | PARODD | CSIZE },
	{ "pass8", FIELD_CFLAG, CS8, PARENB | PARODD | CSIZE },
	{ "hupcl", FIELD_CFLAG, HUPCL, 0 },
	{ "-hupcl", FIELD_CFLAG, 0, HUPCL },
	{ "hup", FIELD_CFLAG, HUPCL, 0 },
	{ "-hup", FIELD_CFLAG, 0, HUPCL },
	{ "clocal", FIELD_CFLAG, CLOCAL, 0 },
	{ "-clocal", FIELD_CFLAG, 0, CLOCAL },
#ifdef CRTSCTS
	{ "crtscts", FIELD_CFLAG, CRTSCTS, 0 },
	{ "-crtscts", FIELD_CFLAG, 0, CRTSCTS },
#endif

	{ "ignbrk", FIELD_IFLAG, IGNBRK, 0 },
	{ "-ignbrk", FIELD_IFLAG, 0, IGNBRK },
	{ "brkint", FIELD_IFLAG, BRKINT, 0 },
	{ "-brkint", FIELD_IFLAG, 0, BRKINT },
	{ "ignpar", FIELD_IFLAG, IGNPAR, 0 },
	{ "-ignpar", FIELD_IFLAG, 0, IGNPAR },
	{ "parmrk", FIELD_IFLAG, PARMRK, 0 },
	{ "-parmrk", FIELD_IFLAG, 0, PARMRK },
	{ "inpck", FIELD_IFLAG, INPCK, 0 },
	{ "-inpck", FIELD_IFLAG, 0, INPCK },
	{ "istrip", FIELD_IFLAG, ISTRIP, 0 },
	{ "-istrip", FIELD_IFLAG, 0, ISTRIP },
	{ "inlcr", FIELD_IFLAG, INLCR, 0 },
	{ "-inlcr", FIELD_IFLAG, 0, INLCR },
	{ "igncr", FIELD_IFLAG, IGNCR, 0 },
	{ "-igncr", FIELD_IFLAG, 0, IGNCR },
	{ "icrnl", FIELD_IFLAG, ICRNL, 0 },
	{ "-icrnl", FIELD_IFLAG, 0, ICRNL },
	{ "ixon", FIELD_IFLAG, IXON, 0 },
	{ "-ixon", FIELD_IFLAG, 0, IXON },
	{ "ixoff", FIELD_IFLAG, IXOFF, 0 },
	{ "-ixoff", FIELD_IFLAG, 0, IXOFF },
	{ "tandem", FIELD_IFLAG, IXOFF, 0 },
	{ "-tandem", FIELD_IFLAG, 0, IXOFF },
	{ "ixany", FIELD_IFLAG, IXANY, 0 },
	{ "-ixany", FIELD_IFLAG, 0, IXANY },
	{ "decctlq", FIELD_IFLAG, 0, IXANY },
	{ "-decctlq", FIELD_IFLAG, IXANY, 0 },
#ifdef IMAXBEL
	{ "imaxbel", FIELD_IFLAG, IMAXBEL, 0 },
	{ "-imaxbel", FIELD_IFLAG, 0, IMAXBEL },
#endif
#ifdef IUTF8
	{ "iutf8", FIELD_IFLAG, IUTF8, 0 },
	{ "-iutf8", FIELD_IFLAG, 0, IUTF8 },
#endif

	{ "echo", FIELD_LFLAG, ECHO, 0 },
	{ "-echo", FIELD_LFLAG, 0, ECHO },
	{ "echoe", FIELD_LFLAG, ECHOE, 0 },
	{ "-echoe", FIELD_LFLAG, 0, ECHOE },
	{ "crterase", FIELD_LFLAG, ECHOE, 0 },
	{ "-crterase", FIELD_LFLAG, 0, ECHOE },
	{ "crtbs", FIELD_LFLAG, ECHOE, 0 },
	{ "-crtbs", FIELD_LFLAG, 0, ECHOE },
	{ "echok", FIELD_LFLAG, ECHOK, 0 },
	{ "-echok", FIELD_LFLAG, 0, ECHOK },
#ifdef ECHOKE
	{ "echoke", FIELD_LFLAG, ECHOKE, 0 },
	{ "-echoke", FIELD_LFLAG, 0, ECHOKE },
	{ "crtkill", FIELD_LFLAG, ECHOKE, 0 },
	{ "-crtkill", FIELD_LFLAG, 0, ECHOKE },
#endif
	{ "iexten", FIELD_LFLAG, IEXTEN, 0 },
	{ "-iexten", FIELD_LFLAG, 0, IEXTEN },
	{ "echonl", FIELD_LFLAG, ECHONL, 0 },
	{ "-echonl", FIELD_LFLAG, 0, ECHONL },
#ifdef ECHOCTL
	{ "echoctl", FIELD_LFLAG, ECHOCTL, 0 },
	{ "-echoctl", FIELD_LFLAG, 0, ECHOCTL },
	{ "ctlecho", FIELD_LFLAG, ECHOCTL, 0 },
	{ "-ctlecho", FIELD_LFLAG, 0, ECHOCTL },
#endif
#ifdef ECHOPRT
	{ "echoprt", FIELD_LFLAG, ECHOPRT, 0 },
	{ "-echoprt", FIELD_LFLAG, 0, ECHOPRT },
	{ "prterase", FIELD_LFLAG, ECHOPRT, 0 },
	{ "-prterase", FIELD_LFLAG, 0, ECHOPRT },
#endif
	{ "isig", FIELD_LFLAG, ISIG, 0 },
	{ "-isig", FIELD_LFLAG, 0, ISIG },
	{ "icanon", FIELD_LFLAG, ICANON, 0 },
	{ "-icanon", FIELD_LFLAG, 0, ICANON },
	{ "noflsh", FIELD_LFLAG, NOFLSH, 0 },
	{ "-noflsh", FIELD_LFLAG, 0, NOFLSH },
	{ "tostop", FIELD_LFLAG, TOSTOP, 0 },
	{ "-tostop", FIELD_LFLAG, 0, TOSTOP },
#ifdef FLUSHO
	{ "flusho", FIELD_LFLAG, FLUSHO, 0 },
	{ "-flusho", FIELD_LFLAG, 0, FLUSHO },
#endif
#ifdef PENDIN
	{ "pendin", FIELD_LFLAG, PENDIN, 0 },
	{ "-pendin", FIELD_LFLAG, 0, PENDIN },
#endif
#ifdef EXTPROC
	{ "extproc", FIELD_LFLAG, EXTPROC, 0 },
	{ "-extproc", FIELD_LFLAG, 0, EXTPROC },
#endif
#if defined(ECHOKE) && defined(ECHOCTL) && defined(ECHOPRT)
	{ "crt", FIELD_LFLAG, ECHOE | ECHOKE | ECHOCTL, ECHOK | ECHOPRT },
	{ "-crt", FIELD_LFLAG, ECHOK, ECHOE | ECHOKE | ECHOCTL },
	{ "newcrt", FIELD_LFLAG, ECHOE | ECHOKE | ECHOCTL, ECHOK | ECHOPRT },
	{ "-newcrt", FIELD_LFLAG, ECHOK, ECHOE | ECHOKE | ECHOCTL },
#endif

	{ "opost", FIELD_OFLAG, OPOST, 0 },
	{ "-opost", FIELD_OFLAG, 0, OPOST },
	{ "litout", FIELD_OFLAG, 0, OPOST },
	{ "-litout", FIELD_OFLAG, OPOST, 0 },
	{ "onlcr", FIELD_OFLAG, ONLCR, 0 },
	{ "-onlcr", FIELD_OFLAG, 0, ONLCR },
	{ "ocrnl", FIELD_OFLAG, OCRNL, 0 },
	{ "-ocrnl", FIELD_OFLAG, 0, OCRNL },
#ifdef TABDLY
#ifdef TAB0
	{ "tabs", FIELD_OFLAG, TAB0, TABDLY },
#endif
#ifdef TAB3
	{ "-tabs", FIELD_OFLAG, TAB3, TABDLY },
	{ "oxtabs", FIELD_OFLAG, TAB3, TABDLY },
#endif
#ifdef TAB0
	{ "-oxtabs", FIELD_OFLAG, TAB0, TABDLY },
	{ "tab0", FIELD_OFLAG, TAB0, TABDLY },
#endif
#ifdef TAB3
	{ "tab3", FIELD_OFLAG, TAB3, TABDLY },
#endif
#endif
	{ "onocr", FIELD_OFLAG, ONOCR, 0 },
	{ "-onocr", FIELD_OFLAG, 0, ONOCR },
	{ "onlret", FIELD_OFLAG, ONLRET, 0 },
	{ "-onlret", FIELD_OFLAG, 0, ONLRET },
};

static const struct print_flag local_print_flags[] = {
	{ "-icanon", FIELD_LFLAG, ICANON },
	{ "-isig", FIELD_LFLAG, ISIG },
	{ "-iexten", FIELD_LFLAG, IEXTEN },
	{ "-echo", FIELD_LFLAG, ECHO },
	{ "-echoe", FIELD_LFLAG, ECHOE },
	{ "-echok", FIELD_LFLAG, ECHOK },
#ifdef ECHOKE
	{ "-echoke", FIELD_LFLAG, ECHOKE },
#endif
	{ "-echonl", FIELD_LFLAG, ECHONL },
#ifdef ECHOCTL
	{ "-echoctl", FIELD_LFLAG, ECHOCTL },
#endif
#ifdef ECHOPRT
	{ "-echoprt", FIELD_LFLAG, ECHOPRT },
#endif
	{ "-noflsh", FIELD_LFLAG, NOFLSH },
	{ "-tostop", FIELD_LFLAG, TOSTOP },
#ifdef FLUSHO
	{ "-flusho", FIELD_LFLAG, FLUSHO },
#endif
#ifdef PENDIN
	{ "-pendin", FIELD_LFLAG, PENDIN },
#endif
#ifdef EXTPROC
	{ "-extproc", FIELD_LFLAG, EXTPROC },
#endif
};

static const struct print_flag input_print_flags[] = {
	{ "-istrip", FIELD_IFLAG, ISTRIP },
	{ "-icrnl", FIELD_IFLAG, ICRNL },
	{ "-inlcr", FIELD_IFLAG, INLCR },
	{ "-igncr", FIELD_IFLAG, IGNCR },
	{ "-ixon", FIELD_IFLAG, IXON },
	{ "-ixoff", FIELD_IFLAG, IXOFF },
	{ "-ixany", FIELD_IFLAG, IXANY },
#ifdef IMAXBEL
	{ "-imaxbel", FIELD_IFLAG, IMAXBEL },
#endif
	{ "-ignbrk", FIELD_IFLAG, IGNBRK },
	{ "-brkint", FIELD_IFLAG, BRKINT },
	{ "-inpck", FIELD_IFLAG, INPCK },
	{ "-ignpar", FIELD_IFLAG, IGNPAR },
	{ "-parmrk", FIELD_IFLAG, PARMRK },
#ifdef IUTF8
	{ "-iutf8", FIELD_IFLAG, IUTF8 },
#endif
};

static const struct print_flag output_print_flags[] = {
	{ "-opost", FIELD_OFLAG, OPOST },
	{ "-onlcr", FIELD_OFLAG, ONLCR },
	{ "-ocrnl", FIELD_OFLAG, OCRNL },
	{ "-onocr", FIELD_OFLAG, ONOCR },
	{ "-onlret", FIELD_OFLAG, ONLRET },
};

static const struct print_flag control_print_flags[] = {
	{ "-cread", FIELD_CFLAG, CREAD },
	{ "-parenb", FIELD_CFLAG, PARENB },
	{ "-parodd", FIELD_CFLAG, PARODD },
	{ "-hupcl", FIELD_CFLAG, HUPCL },
	{ "-clocal", FIELD_CFLAG, CLOCAL },
	{ "-cstopb", FIELD_CFLAG, CSTOPB },
#ifdef CRTSCTS
	{ "-crtscts", FIELD_CFLAG, CRTSCTS },
#endif
};

static const struct control_char control_chars[] = {
#ifdef VDISCARD
#ifdef CDISCARD
	{ "discard", VDISCARD, CDISCARD, false },
#else
	{ "discard", VDISCARD, _POSIX_VDISABLE, false },
#endif
#endif
#ifdef VEOF
	{ "eof", VEOF, CEOF, false },
#endif
#ifdef VEOL
#ifdef CEOL
	{ "eol", VEOL, CEOL, false },
#else
	{ "eol", VEOL, _POSIX_VDISABLE, false },
#endif
#endif
#ifdef VEOL2
#ifdef CEOL
	{ "eol2", VEOL2, CEOL, false },
#else
	{ "eol2", VEOL2, _POSIX_VDISABLE, false },
#endif
#endif
#ifdef VERASE
	{ "erase", VERASE, CERASE, false },
#endif
#ifdef VINTR
	{ "intr", VINTR, CINTR, false },
#endif
#ifdef VKILL
	{ "kill", VKILL, CKILL, false },
#endif
#ifdef VLNEXT
#ifdef CLNEXT
	{ "lnext", VLNEXT, CLNEXT, false },
#else
	{ "lnext", VLNEXT, _POSIX_VDISABLE, false },
#endif
#endif
#ifdef VMIN
	{ "min", VMIN, 1, true },
#endif
#ifdef VQUIT
	{ "quit", VQUIT, CQUIT, false },
#endif
#ifdef VREPRINT
#ifdef CREPRINT
	{ "reprint", VREPRINT, CREPRINT, false },
#else
	{ "reprint", VREPRINT, _POSIX_VDISABLE, false },
#endif
#endif
#ifdef VSTART
	{ "start", VSTART, CSTART, false },
#endif
#ifdef VSTOP
	{ "stop", VSTOP, CSTOP, false },
#endif
#ifdef VSUSP
	{ "susp", VSUSP, CSUSP, false },
#endif
#ifdef VTIME
	{ "time", VTIME, 0, true },
#endif
#ifdef VWERASE
#ifdef CWERASE
	{ "werase", VWERASE, CWERASE, false },
#else
	{ "werase", VWERASE, _POSIX_VDISABLE, false },
#endif
#endif
};

static const struct named_command commands[] = {
	{ "all", false, false, CMD_PRINT_BSD },
	{ "everything", false, false, CMD_PRINT_BSD },
	{ "cooked", false, false, CMD_SANE },
	{ "sane", false, false, CMD_SANE },
	{ "cbreak", true, false, CMD_CBREAK },
	{ "cols", false, true, CMD_COLUMNS },
	{ "columns", false, true, CMD_COLUMNS },
	{ "rows", false, true, CMD_ROWS },
	{ "size", false, false, CMD_SIZE },
	{ "speed", false, true, CMD_SPEED },
	{ "ispeed", false, true, CMD_ISPEED },
	{ "ospeed", false, true, CMD_OSPEED },
	{ "dec", false, false, CMD_DEC },
	{ "tty", false, false, CMD_TTY },
	{ "new", false, false, CMD_TTY },
	{ "old", false, false, CMD_TTY },
	{ "nl", true, false, CMD_NL },
	{ "raw", true, false, CMD_RAW },
	{ "ek", false, false, CMD_EK },
};

int
main(int argc, char *argv[])
{
	enum output_format format;
	const char *path;
	int index;
	bool format_set;

	format = FORMAT_DEFAULT;
	path = NULL;
	index = 1;
	format_set = false;

	while (index < argc) {
		const char *argument = argv[index];

		if (strcmp(argument, "--") == 0) {
			index++;
			break;
		}
		if (strcmp(argument, "-f") == 0) {
			index++;
			if (index >= argc)
				usage_error("option -f requires an argument");
			path = argv[index++];
			continue;
		}
		if (strncmp(argument, "-f", 2) == 0 && argument[2] != '\0') {
			path = argument + 2;
			index++;
			continue;
		}
		if (argument[0] != '-' || argument[1] == '\0' ||
		    strspn(argument + 1, "aeg") != strlen(argument + 1))
			break;

		for (size_t offset = 1; argument[offset] != '\0'; offset++) {
			enum output_format selected;

			switch (argument[offset]) {
			case 'a':
				selected = FORMAT_POSIX;
				break;
			case 'e':
				selected = FORMAT_BSD;
				break;
			case 'g':
				selected = FORMAT_GFMT;
				break;
			default:
				usage();
			}
			if (format_set && selected != format)
				usage_error("options -a, -e, and -g are mutually exclusive");
			format = selected;
			format_set = true;
		}
		index++;
	}

	struct state state;
	init_state(&state, path);

	if (format == FORMAT_BSD || format == FORMAT_POSIX || format == FORMAT_GFMT ||
	    index >= argc)
		print_state(&state, format);

	while (index < argc)
		apply_named_argument(&state, argv, argc, &index);

	if (state.termios_dirty && tcsetattr(state.fd, TCSANOW, &state.termios) != 0)
		die_errno("stty: tcsetattr failed for %s", state.label);
	if (state.winsize_dirty &&
	    ioctl(state.fd, TIOCSWINSZ, &state.winsize) != 0)
		die_errno("stty: TIOCSWINSZ failed for %s", state.label);
	if (state.close_fd)
		close(state.fd);
	return 0;
}

static void
init_state(struct state *state, const char *path)
{
	int fd;

	memset(state, 0, sizeof(*state));

	if (path == NULL) {
		state->fd = STDIN_FILENO;
		state->label = "stdin";
		state->close_fd = false;
	} else {
		fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
		if (fd < 0 && (errno == EACCES || errno == EPERM))
			fd = open(path, O_RDONLY | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
		if (fd < 0)
			die_errno("stty: cannot open %s", path);
		state->fd = fd;
		state->label = path;
		state->close_fd = true;
	}

	if (tcgetattr(state->fd, &state->termios) != 0) {
		if (errno == ENOTTY)
			die("stty: %s is not a terminal", state->label);
		die_errno("stty: tcgetattr failed for %s", state->label);
	}
	make_sane_termios(&state->sane, &state->termios);

	memset(&state->winsize, 0, sizeof(state->winsize));
	if (ioctl(state->fd, TIOCGWINSZ, &state->winsize) == 0)
		state->have_winsize = true;

#ifdef TIOCGETD
	if (ioctl(state->fd, TIOCGETD, &state->line_discipline) == 0)
		state->have_line_discipline = true;
#endif
}

static void
make_sane_termios(struct termios *sane, const struct termios *current)
{
	*sane = *current;

	sane->c_iflag = BRKINT | ICRNL | IXON;
#ifdef IMAXBEL
	sane->c_iflag |= IMAXBEL;
#endif
#ifdef IUTF8
	sane->c_iflag |= IUTF8;
#endif

	sane->c_oflag = OPOST | ONLCR;

	sane->c_cflag &= ~(CSIZE | PARENB | PARODD | CSTOPB);
#ifdef CRTSCTS
	sane->c_cflag &= ~CRTSCTS;
#endif
	sane->c_cflag |= CREAD | CS8;
#ifdef HUPCL
	sane->c_cflag |= HUPCL;
#endif
	sane->c_cflag |= current->c_cflag & CLOCAL;

	sane->c_lflag = ISIG | ICANON | IEXTEN | ECHO | ECHOE | ECHOK;
#ifdef ECHOKE
	sane->c_lflag |= ECHOKE;
#endif
#ifdef ECHOCTL
	sane->c_lflag |= ECHOCTL;
#endif

	memset(sane->c_cc, _POSIX_VDISABLE, sizeof(sane->c_cc));
	for (size_t i = 0; i < ARRAY_LEN(control_chars); i++)
		sane->c_cc[control_chars[i].index] = control_chars[i].default_value;

	if (cfsetispeed(sane, cfgetispeed(current)) != 0 ||
	    cfsetospeed(sane, cfgetospeed(current)) != 0)
		die_errno("stty: failed to preserve terminal speed");
}

static void
apply_named_argument(struct state *state, char **argv, int argc, int *index)
{
	const char *argument = argv[*index];
	const char *reason;

	if ((reason = unsupported_reason(argument)) != NULL)
		die_unsupported(argument, reason);
	if (maybe_apply_gfmt(state, argument)) {
		(*index)++;
		return;
	}
	if (maybe_apply_flag_action(state, argument)) {
		(*index)++;
		return;
	}
	if (maybe_apply_command(state, argv, argc, index))
		return;
	if (maybe_apply_control_char(state, argv, argc, index))
		return;
	if (maybe_apply_speed_token(state, argument)) {
		(*index)++;
		return;
	}
	die("stty: illegal argument: %s", argument);
}

static bool
maybe_apply_gfmt(struct state *state, const char *argument)
{
	char *cursor;
	char *text;

	if (strncmp(argument, "gfmt1:", 6) != 0)
		return false;

	text = strdup(argument + 6);
	if (text == NULL)
		die_errno("stty: strdup failed");

	for (cursor = text; cursor != NULL;) {
		char *token;
		char *equals;
		char *name;
		char *value;

		token = strsep(&cursor, ":");
		if (token == NULL || token[0] == '\0')
			break;
		equals = strchr(token, '=');
		if (equals == NULL) {
			free(text);
			die("stty: invalid gfmt1 token: %s", token);
		}
		*equals = '\0';
		name = token;
		value = equals + 1;

		if (strcmp(name, "cflag") == 0) {
			state->termios.c_cflag =
			    (tcflag_t)parse_unsigned(value, 16, UINT_MAX, "gfmt1 cflag");
			continue;
		}
		if (strcmp(name, "iflag") == 0) {
			state->termios.c_iflag =
			    (tcflag_t)parse_unsigned(value, 16, UINT_MAX, "gfmt1 iflag");
			continue;
		}
		if (strcmp(name, "lflag") == 0) {
			state->termios.c_lflag =
			    (tcflag_t)parse_unsigned(value, 16, UINT_MAX, "gfmt1 lflag");
			continue;
		}
		if (strcmp(name, "oflag") == 0) {
			state->termios.c_oflag =
			    (tcflag_t)parse_unsigned(value, 16, UINT_MAX, "gfmt1 oflag");
			continue;
		}
		if (strcmp(name, "ispeed") == 0) {
			set_speed(state, "ispeed", value, true, false);
			continue;
		}
		if (strcmp(name, "ospeed") == 0) {
			set_speed(state, "ospeed", value, false, true);
			continue;
		}

		const struct control_char *control = find_control_char(name);
		if (control == NULL) {
			free(text);
			die("stty: invalid gfmt1 token: %s", name);
		}
		state->termios.c_cc[control->index] =
		    (cc_t)parse_unsigned(value, 16, UCHAR_MAX, name);
	}

	free(text);
	state->termios_dirty = true;
	return true;
}

static bool
maybe_apply_flag_action(struct state *state, const char *argument)
{
	for (size_t i = 0; i < ARRAY_LEN(flag_actions); i++) {
		tcflag_t *field;

		if (strcmp(flag_actions[i].name, argument) != 0)
			continue;
		field = field_ptr(&state->termios, flag_actions[i].field);
		*field &= ~flag_actions[i].clear_mask;
		*field |= flag_actions[i].set_mask;
		state->termios_dirty = true;
		return true;
	}
	return false;
}

static bool
maybe_apply_command(struct state *state, char **argv, int argc, int *index)
{
	const struct named_command *command;
	const char *argument = argv[*index];
	const char *name = argument;
	const char *value = NULL;
	bool off = false;

	if (argument[0] == '-' && argument[1] != '\0') {
		name = argument + 1;
		off = true;
	}

	command = find_command(name);
	if (command == NULL)
		return false;
	if (off && !command->allow_off)
		die("stty: illegal argument: %s", argument);
	if (command->needs_argument) {
		if (*index + 1 >= argc)
			die("stty: %s requires an argument", name);
		value = argv[*index + 1];
	}

	switch (command->id) {
	case CMD_PRINT_BSD:
		print_state(state, FORMAT_BSD);
		break;
	case CMD_SANE:
		apply_sane(state);
		break;
	case CMD_CBREAK:
		apply_cbreak(state, off);
		break;
	case CMD_COLUMNS:
		set_rows_or_columns(&state->winsize.ws_col, name, value);
		state->winsize_dirty = true;
		break;
	case CMD_ROWS:
		set_rows_or_columns(&state->winsize.ws_row, name, value);
		state->winsize_dirty = true;
		break;
	case CMD_SIZE:
		if (!state->have_winsize && !state->winsize_dirty)
			die("stty: terminal window size is unavailable on %s", state->label);
		printf("%u %u\n", state->winsize.ws_row, state->winsize.ws_col);
		break;
	case CMD_SPEED:
		set_speed(state, "speed", value, true, true);
		break;
	case CMD_ISPEED:
		set_speed(state, "ispeed", value, true, false);
		break;
	case CMD_OSPEED:
		set_speed(state, "ospeed", value, false, true);
		break;
	case CMD_DEC:
		apply_dec(state);
		break;
	case CMD_TTY:
		apply_tty(state);
		break;
	case CMD_NL:
		apply_nl(state, off);
		break;
	case CMD_RAW:
		apply_raw(state, off);
		break;
	case CMD_EK:
		apply_ek(state);
		break;
	}

	*index += command->needs_argument ? 2 : 1;
	return true;
}

static bool
maybe_apply_control_char(struct state *state, char **argv, int argc, int *index)
{
	const struct control_char *control;

	control = find_control_char(argv[*index]);
	if (control == NULL)
		return false;
	if (*index + 1 >= argc)
		die("stty: %s requires an argument", control->name);

	state->termios.c_cc[control->index] =
	    parse_control_char_value(control, argv[*index + 1]);
	state->termios_dirty = true;
	*index += 2;
	return true;
}

static bool
maybe_apply_speed_token(struct state *state, const char *argument)
{
	if (!is_decimal_number(argument))
		return false;
	set_speed(state, "speed", argument, true, true);
	return true;
}

static void
print_state(const struct state *state, enum output_format format)
{
	unsigned int ispeed;
	unsigned int ospeed;

	ispeed = speed_to_baud(cfgetispeed(&state->termios));
	ospeed = speed_to_baud(cfgetospeed(&state->termios));

	if (format == FORMAT_GFMT) {
		printf("gfmt1:cflag=%lx:iflag=%lx:lflag=%lx:oflag=%lx:",
		    (unsigned long)state->termios.c_cflag,
		    (unsigned long)state->termios.c_iflag,
		    (unsigned long)state->termios.c_lflag,
		    (unsigned long)state->termios.c_oflag);
		for (size_t i = 0; i < ARRAY_LEN(control_chars); i++) {
			printf("%s=%x:", control_chars[i].name,
			    state->termios.c_cc[control_chars[i].index]);
		}
		printf("ispeed=%u:ospeed=%u\n", ispeed, ospeed);
		return;
	}

	if (state->have_line_discipline && state->line_discipline != N_TTY)
		printf("#%d disc; ", state->line_discipline);

	if (ispeed != ospeed)
		printf("ispeed %u baud; ospeed %u baud;", ispeed, ospeed);
	else
		printf("speed %u baud;", ispeed);
	if ((format == FORMAT_BSD || format == FORMAT_POSIX) && state->have_winsize)
		printf(" %u rows; %u columns;",
		    state->winsize.ws_row, state->winsize.ws_col);
	printf("\n");

	print_boolean_group(state, format, "lflags",
	    local_print_flags, ARRAY_LEN(local_print_flags));
	print_boolean_group(state, format, "iflags",
	    input_print_flags, ARRAY_LEN(input_print_flags));
	print_boolean_group(state, format, "oflags",
	    output_print_flags, ARRAY_LEN(output_print_flags));

	struct line_builder builder;
	line_builder_begin(&builder, "oflags");
#ifdef TABDLY
	{
		tcflag_t current = state->termios.c_oflag & TABDLY;
		tcflag_t sane = state->sane.c_oflag & TABDLY;

		if (format != FORMAT_DEFAULT || current != sane) {
#ifdef TAB3
			if (current == TAB3)
				line_builder_put(&builder, "tab3");
			else
#endif
#ifdef TAB0
				line_builder_put(&builder, "tab0");
#else
				line_builder_put(&builder, "tabs");
#endif
		}
	}
#endif
	line_builder_finish(&builder);

	print_boolean_group(state, format, "cflags",
	    control_print_flags, ARRAY_LEN(control_print_flags));

	line_builder_begin(&builder, "cflags");
	{
		tcflag_t current = state->termios.c_cflag & CSIZE;
		tcflag_t sane = state->sane.c_cflag & CSIZE;
		if (format != FORMAT_DEFAULT || current != sane) {
			switch (current) {
			case CS5:
				line_builder_put(&builder, "cs5");
				break;
			case CS6:
				line_builder_put(&builder, "cs6");
				break;
			case CS7:
				line_builder_put(&builder, "cs7");
				break;
			default:
				line_builder_put(&builder, "cs8");
				break;
			}
		}
	}
	line_builder_finish(&builder);

	print_control_chars(state, format);
}

static void
print_boolean_group(const struct state *state, enum output_format format,
    const char *label, const struct print_flag *flags, size_t count)
{
	struct line_builder builder;

	line_builder_begin(&builder, label);
	for (size_t i = 0; i < count; i++) {
		bool current;
		bool sane;

		current = (field_value(&state->termios, flags[i].field) & flags[i].mask) != 0;
		sane = (field_value(&state->sane, flags[i].field) & flags[i].mask) != 0;
		if (format == FORMAT_DEFAULT && current == sane)
			continue;
		line_builder_put(&builder, current ? flags[i].off_name + 1 : flags[i].off_name);
	}
	line_builder_finish(&builder);
}

static void
print_control_chars(const struct state *state, enum output_format format)
{
	char name_line[128];
	char value_line[128];
	char buffer[16];
	int count = 0;

	name_line[0] = '\0';
	value_line[0] = '\0';

	if (format == FORMAT_POSIX) {
		struct line_builder builder;

		line_builder_begin(&builder, "cchars");
		for (size_t i = 0; i < ARRAY_LEN(control_chars); i++) {
			char token[32];
			const struct control_char *control = &control_chars[i];

			snprintf(token, sizeof(token), "%s = %s;",
			    control->name,
			    format_control_char(control,
				state->termios.c_cc[control->index],
				buffer, sizeof(buffer)));
			line_builder_put(&builder, token);
		}
		line_builder_finish(&builder);
		return;
	}

	for (size_t i = 0; i < ARRAY_LEN(control_chars); i++) {
		const struct control_char *control = &control_chars[i];
		cc_t current = state->termios.c_cc[control->index];
		cc_t sane = state->sane.c_cc[control->index];
		const char *formatted;

		if (format == FORMAT_DEFAULT && current == sane)
			continue;

		formatted = format_control_char(control, current, buffer, sizeof(buffer));
		snprintf(name_line + count * 8, sizeof(name_line) - (size_t)count * 8,
		    "%-8s", control->name);
		snprintf(value_line + count * 8, sizeof(value_line) - (size_t)count * 8,
		    "%-8s", formatted);
		count++;
		if (count == 8) {
			printf("%s\n%s\n", name_line, value_line);
			count = 0;
			name_line[0] = '\0';
			value_line[0] = '\0';
		}
	}
	if (count != 0)
		printf("%s\n%s\n", name_line, value_line);
}

static void
line_builder_begin(struct line_builder *builder, const char *label)
{
	builder->label = label;
	builder->column = 0;
}

static void
line_builder_put(struct line_builder *builder, const char *token)
{
	size_t length = strlen(token);

	if (builder->column == 0) {
		if (builder->label != NULL)
			builder->column = (size_t)printf("%s: %s", builder->label, token);
		else
			builder->column = (size_t)printf("%s", token);
		return;
	}
	if (builder->column + 1 + length > OUTPUT_WIDTH) {
		printf("\n\t%s", token);
		builder->column = 8 + length;
		return;
	}
	printf(" %s", token);
	builder->column += 1 + length;
}

static void
line_builder_finish(struct line_builder *builder)
{
	if (builder->column != 0)
		printf("\n");
	builder->column = 0;
}

static tcflag_t *
field_ptr(struct termios *termios, enum termios_field field)
{
	switch (field) {
	case FIELD_IFLAG:
		return &termios->c_iflag;
	case FIELD_OFLAG:
		return &termios->c_oflag;
	case FIELD_CFLAG:
		return &termios->c_cflag;
	case FIELD_LFLAG:
		return &termios->c_lflag;
	}
	die("stty: internal error: unknown termios field");
	return &termios->c_iflag;
}

static tcflag_t
field_value(const struct termios *termios, enum termios_field field)
{
	switch (field) {
	case FIELD_IFLAG:
		return termios->c_iflag;
	case FIELD_OFLAG:
		return termios->c_oflag;
	case FIELD_CFLAG:
		return termios->c_cflag;
	case FIELD_LFLAG:
		return termios->c_lflag;
	}
	die("stty: internal error: unknown termios field");
	return 0;
}

static const struct control_char *
find_control_char(const char *name)
{
	if (strcmp(name, "brk") == 0)
		name = "eol";
	else if (strcmp(name, "flush") == 0)
		name = "discard";
	else if (strcmp(name, "rprnt") == 0)
		name = "reprint";

	for (size_t i = 0; i < ARRAY_LEN(control_chars); i++) {
		if (strcmp(control_chars[i].name, name) == 0)
			return &control_chars[i];
	}
	return NULL;
}

static const struct named_command *
find_command(const char *name)
{
	for (size_t i = 0; i < ARRAY_LEN(commands); i++) {
		if (strcmp(commands[i].name, name) == 0)
			return &commands[i];
	}
	return NULL;
}

static const char *
unsupported_reason(const char *name)
{
	if (strcmp(name, "altwerase") == 0 || strcmp(name, "-altwerase") == 0)
		return "Linux termios has no ALTWERASE local mode";
	if (strcmp(name, "mdmbuf") == 0 || strcmp(name, "-mdmbuf") == 0)
		return "Linux termios has no carrier-detect output flow-control flag";
	if (strcmp(name, "rtsdtr") == 0 || strcmp(name, "-rtsdtr") == 0)
		return "Linux termios cannot control BSD RTS/DTR-on-open semantics";
	if (strcmp(name, "kerninfo") == 0 || strcmp(name, "-kerninfo") == 0 ||
	    strcmp(name, "nokerninfo") == 0 || strcmp(name, "-nokerninfo") == 0)
		return "Linux has no STATUS control character or kernel tty status line";
	if (strcmp(name, "status") == 0)
		return "Linux termios has no VSTATUS control character";
	if (strcmp(name, "dsusp") == 0)
		return "Linux termios has no VDSUSP control character";
	if (strcmp(name, "erase2") == 0)
		return "Linux termios has no VERASE2 control character";
	return NULL;
}

static void
set_rows_or_columns(unsigned short *slot, const char *keyword, const char *value)
{
	*slot = (unsigned short)parse_unsigned(value, 10, USHRT_MAX, keyword);
}

static void
set_speed(struct state *state, const char *keyword, const char *value, bool set_input,
    bool set_output)
{
	bool found;
	speed_t symbol;
	unsigned int baud;

	baud = (unsigned int)parse_unsigned(value, 10, UINT_MAX, keyword);
	symbol = lookup_speed_symbol(baud, &found);
	if (!found)
		die("stty: unsupported baud rate on Linux termios API: %u", baud);
	if (set_input && cfsetispeed(&state->termios, symbol) != 0)
		die_errno("stty: cannot set input speed to %u", baud);
	if (set_output && cfsetospeed(&state->termios, symbol) != 0)
		die_errno("stty: cannot set output speed to %u", baud);
	state->termios_dirty = true;
}

static speed_t
lookup_speed_symbol(unsigned int baud, bool *found)
{
	for (size_t i = 0; i < ARRAY_LEN(speed_table); i++) {
		if (speed_table[i].baud == baud) {
			*found = true;
			return speed_table[i].symbol;
		}
	}
	*found = false;
	return (speed_t)0;
}

static unsigned int
speed_to_baud(speed_t symbol)
{
	for (size_t i = 0; i < ARRAY_LEN(speed_table); i++) {
		if (speed_table[i].symbol == symbol)
			return speed_table[i].baud;
	}
	return (unsigned int)symbol;
}

static unsigned long long
parse_unsigned(const char *text, int base, unsigned long long max_value, const char *what)
{
	char *end;
	unsigned long long value;

	errno = 0;
	value = strtoull(text, &end, base);
	if (errno == ERANGE || value > max_value)
		die("stty: %s requires a number between 0 and %llu", what, max_value);
	if (text[0] == '\0' || end == text || *end != '\0')
		die("stty: %s requires a number between 0 and %llu", what, max_value);
	return value;
}

static bool
is_decimal_number(const char *text)
{
	if (*text == '\0')
		return false;
	for (const unsigned char *p = (const unsigned char *)text; *p != '\0'; p++) {
		if (!isdigit(*p))
			return false;
	}
	return true;
}

static cc_t
parse_control_char_value(const struct control_char *control, const char *value)
{
	if (strcmp(value, "undef") == 0 || strcmp(value, "<undef>") == 0 ||
	    strcmp(value, "^-") == 0)
		return _POSIX_VDISABLE;
	if (control->numeric)
		return (cc_t)parse_unsigned(value, 10, UCHAR_MAX, control->name);
	if (value[0] == '^' && value[1] != '\0' && value[2] == '\0') {
		if (value[1] == '?')
			return 0177;
		if (value[1] == '-')
			return _POSIX_VDISABLE;
		return (cc_t)(value[1] & 037);
	}
	if (value[0] != '\0' && value[1] == '\0')
		return (cc_t)value[0];
	die("stty: control character %s requires a single character, ^X, ^?, ^-, or undef",
	    control->name);
	return 0;
}

static const char *
format_control_char(const struct control_char *control, cc_t value, char *buffer,
    size_t buffer_size)
{
	if (control->numeric) {
		snprintf(buffer, buffer_size, "%u", (unsigned int)value);
		return buffer;
	}
	if (value == _POSIX_VDISABLE)
		return "<undef>";
	if (value == 0177)
		return "^?";
	if (value < 32) {
		buffer[0] = '^';
		buffer[1] = (char)(value + '@');
		buffer[2] = '\0';
		return buffer;
	}
	if (isprint(value)) {
		buffer[0] = (char)value;
		buffer[1] = '\0';
		return buffer;
	}
	snprintf(buffer, buffer_size, "0x%02x", value);
	return buffer;
}

static void
apply_sane(struct state *state)
{
	state->termios = state->sane;
	state->termios_dirty = true;
}

static void
apply_cbreak(struct state *state, bool off)
{
	if (off) {
		apply_sane(state);
		return;
	}
	state->termios.c_iflag |= BRKINT | IXON;
#ifdef IMAXBEL
	state->termios.c_iflag |= IMAXBEL;
#endif
	state->termios.c_oflag |= OPOST;
	state->termios.c_lflag |= ISIG | IEXTEN;
	state->termios.c_lflag &= ~ICANON;
	state->termios_dirty = true;
}

static void
apply_dec(struct state *state)
{
#ifdef VERASE
	state->termios.c_cc[VERASE] = 0177;
#endif
#ifdef VKILL
	state->termios.c_cc[VKILL] = ('u' & 037);
#endif
#ifdef VINTR
	state->termios.c_cc[VINTR] = ('c' & 037);
#endif
	state->termios.c_iflag &= ~IXANY;
	state->termios.c_lflag |= ECHOE;
#ifdef ECHOKE
	state->termios.c_lflag |= ECHOKE;
#endif
#ifdef ECHOCTL
	state->termios.c_lflag |= ECHOCTL;
#endif
	state->termios.c_lflag &= ~ECHOK;
#ifdef ECHOPRT
	state->termios.c_lflag &= ~ECHOPRT;
#endif
	state->termios_dirty = true;
}

static void
apply_nl(struct state *state, bool off)
{
	if (off) {
		state->termios.c_iflag &= ~(ICRNL | INLCR | IGNCR);
		state->termios.c_oflag &= ~ONLCR;
	} else {
		state->termios.c_iflag |= ICRNL;
		state->termios.c_oflag |= ONLCR;
	}
	state->termios_dirty = true;
}

static void
apply_raw(struct state *state, bool off)
{
	if (off) {
		apply_sane(state);
		return;
	}
	cfmakeraw(&state->termios);
	state->termios.c_cflag &= ~(CSIZE | PARENB);
	state->termios.c_cflag |= CS8;
	state->termios_dirty = true;
}

static void
apply_tty(struct state *state)
{
#ifdef TIOCSETD
	int discipline = N_TTY;

	if (ioctl(state->fd, TIOCSETD, &discipline) != 0)
		die_errno("stty: TIOCSETD failed for %s", state->label);
	state->line_discipline = discipline;
	state->have_line_discipline = true;
#else
	(void)state;
	die_unsupported("tty", "Linux headers do not expose TIOCSETD");
#endif
}

static void
apply_ek(struct state *state)
{
#ifdef VERASE2
	state->termios.c_cc[VERASE2] = CERASE2;
#else
	die_unsupported("ek",
	    "Linux termios has no VERASE2 control character, so BSD ek semantics are incomplete");
#endif
#ifdef VERASE
	state->termios.c_cc[VERASE] = CERASE;
#endif
#ifdef VKILL
	state->termios.c_cc[VKILL] = CKILL;
#endif
	state->termios_dirty = true;
}

static noreturn void
usage(void)
{
	fprintf(stderr, "usage: stty [-a | -e | -g] [-f file] [arguments]\n");
	exit(1);
}

static noreturn void
usage_error(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	fputs("stty: ", stderr);
	vfprintf(stderr, fmt, ap);
	fputc('\n', stderr);
	va_end(ap);
	usage();
}

static noreturn void
die(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	fputc('\n', stderr);
	va_end(ap);
	exit(1);
}

static noreturn void
die_errno(const char *fmt, ...)
{
	va_list ap;
	int saved_errno = errno;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, ": %s\n", strerror(saved_errno));
	exit(1);
}

static noreturn void
die_unsupported(const char *name, const char *reason)
{
	die("stty: argument '%s' is not supported on Linux: %s", name, reason);
}
