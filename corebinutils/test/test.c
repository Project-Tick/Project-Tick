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

#include <sys/stat.h>
#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum token_type {
	TOKEN_UNARY = 0x100,
	TOKEN_BINARY = 0x200,
	TOKEN_BOOLEAN_UNARY = 0x300,
	TOKEN_BOOLEAN_BINARY = 0x400,
	TOKEN_PAREN = 0x500
};

enum token {
	TOKEN_EOI,
	TOKEN_OPERAND,
	TOKEN_FILRD = TOKEN_UNARY + 1,
	TOKEN_FILWR,
	TOKEN_FILEX,
	TOKEN_FILEXIST,
	TOKEN_FILREG,
	TOKEN_FILDIR,
	TOKEN_FILCDEV,
	TOKEN_FILBDEV,
	TOKEN_FILFIFO,
	TOKEN_FILSOCK,
	TOKEN_FILSYM,
	TOKEN_FILGZ,
	TOKEN_FILTT,
	TOKEN_FILSUID,
	TOKEN_FILSGID,
	TOKEN_FILSTCK,
	TOKEN_STREZ,
	TOKEN_STRNZ,
	TOKEN_FILUID,
	TOKEN_FILGID,
	TOKEN_FILNT = TOKEN_BINARY + 1,
	TOKEN_FILOT,
	TOKEN_FILEQ,
	TOKEN_STREQ,
	TOKEN_STRNE,
	TOKEN_STRLT,
	TOKEN_STRGT,
	TOKEN_INTEQ,
	TOKEN_INTNE,
	TOKEN_INTGE,
	TOKEN_INTGT,
	TOKEN_INTLE,
	TOKEN_INTLT,
	TOKEN_UNOT = TOKEN_BOOLEAN_UNARY + 1,
	TOKEN_BAND = TOKEN_BOOLEAN_BINARY + 1,
	TOKEN_BOR,
	TOKEN_LPAREN = TOKEN_PAREN + 1,
	TOKEN_RPAREN
};

#define TOKEN_FAMILY(token) ((token) & 0xff00)

struct operator {
	const char *text;
	enum token token;
};

struct parser {
	char **argv;
	int pos;
	int remaining;
	int paren_level;
};

static const struct operator ops_single[] = {
	{"=", TOKEN_STREQ},
	{"<", TOKEN_STRLT},
	{">", TOKEN_STRGT},
	{"!", TOKEN_UNOT},
	{"(", TOKEN_LPAREN},
	{")", TOKEN_RPAREN},
};

static const struct operator ops_dash_single[] = {
	{"r", TOKEN_FILRD},
	{"w", TOKEN_FILWR},
	{"x", TOKEN_FILEX},
	{"e", TOKEN_FILEXIST},
	{"f", TOKEN_FILREG},
	{"d", TOKEN_FILDIR},
	{"c", TOKEN_FILCDEV},
	{"b", TOKEN_FILBDEV},
	{"p", TOKEN_FILFIFO},
	{"u", TOKEN_FILSUID},
	{"g", TOKEN_FILSGID},
	{"k", TOKEN_FILSTCK},
	{"s", TOKEN_FILGZ},
	{"t", TOKEN_FILTT},
	{"z", TOKEN_STREZ},
	{"n", TOKEN_STRNZ},
	{"h", TOKEN_FILSYM},
	{"O", TOKEN_FILUID},
	{"G", TOKEN_FILGID},
	{"L", TOKEN_FILSYM},
	{"S", TOKEN_FILSOCK},
	{"a", TOKEN_BAND},
	{"o", TOKEN_BOR},
};

static const struct operator ops_double[] = {
	{"==", TOKEN_STREQ},
	{"!=", TOKEN_STRNE},
};

static const struct operator ops_dash_double[] = {
	{"eq", TOKEN_INTEQ},
	{"ne", TOKEN_INTNE},
	{"ge", TOKEN_INTGE},
	{"gt", TOKEN_INTGT},
	{"le", TOKEN_INTLE},
	{"lt", TOKEN_INTLT},
	{"nt", TOKEN_FILNT},
	{"ot", TOKEN_FILOT},
	{"ef", TOKEN_FILEQ},
};

static const char *program_name = "test";

__attribute__((format(printf, 1, 2), noreturn))
static void
die(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	fprintf(stderr, "%s: ", program_name);
	vfprintf(stderr, fmt, ap);
	fputc('\n', stderr);
	va_end(ap);
	exit(2);
}

static void
syntax_error(const char *op, const char *msg)
{
	if (op != NULL && *op != '\0')
		die("%s: %s", op, msg);
	die("%s", msg);
}

static const char *
base_name(const char *path)
{
	const char *slash;

	slash = strrchr(path, '/');
	return slash == NULL ? path : slash + 1;
}

static const char *
current_arg(const struct parser *parser)
{
	if (parser->remaining <= 0)
		return NULL;
	return parser->argv[parser->pos];
}

static const char *
peek_arg(const struct parser *parser, int offset)
{
	if (offset < 0 || offset >= parser->remaining)
		return NULL;
	return parser->argv[parser->pos + offset];
}

static const char *
advance_arg(struct parser *parser)
{
	if (parser->remaining > 0) {
		parser->pos++;
		parser->remaining--;
	}
	return current_arg(parser);
}

static void
rewind_arg(struct parser *parser)
{
	if (parser->pos > 0) {
		parser->pos--;
		parser->remaining++;
	}
}

static enum token
lookup_operator(const struct operator *ops, size_t count, const char *text)
{
	size_t i;

	for (i = 0; i < count; i++) {
		if (strcmp(text, ops[i].text) == 0)
			return ops[i].token;
	}
	return TOKEN_OPERAND;
}

static enum token
find_operator(const char *text)
{
	size_t length;

	if (text == NULL || text[0] == '\0')
		return TOKEN_OPERAND;

	length = strlen(text);
	if (length == 1)
		return lookup_operator(ops_single,
		    sizeof(ops_single) / sizeof(ops_single[0]), text);
	if (length == 2) {
		if (text[0] == '-')
			return lookup_operator(ops_dash_single,
			    sizeof(ops_dash_single) / sizeof(ops_dash_single[0]),
			    text + 1);
		return lookup_operator(ops_double,
		    sizeof(ops_double) / sizeof(ops_double[0]), text);
	}
	if (length == 3 && text[0] == '-') {
		return lookup_operator(ops_dash_double,
		    sizeof(ops_dash_double) / sizeof(ops_dash_double[0]),
		    text + 1);
	}
	return TOKEN_OPERAND;
}

static bool
is_unary_operand(const struct parser *parser)
{
	const char *next;
	const char *after_next;
	enum token next_token;

	if (parser->remaining == 1)
		return true;

	next = peek_arg(parser, 1);
	if (parser->remaining == 2)
		return parser->paren_level == 1 && strcmp(next, ")") == 0;

	after_next = peek_arg(parser, 2);
	next_token = find_operator(next);
	return TOKEN_FAMILY(next_token) == TOKEN_BINARY &&
	    (parser->paren_level == 0 || strcmp(after_next, ")") != 0);
}

static bool
is_left_paren_operand(const struct parser *parser)
{
	const char *next;
	enum token next_token;

	if (parser->remaining == 1)
		return true;

	next = peek_arg(parser, 1);
	if (parser->remaining == 2)
		return parser->paren_level == 1 && strcmp(next, ")") == 0;
	if (parser->remaining != 3)
		return false;

	next_token = find_operator(next);
	return TOKEN_FAMILY(next_token) == TOKEN_BINARY;
}

static bool
is_right_paren_operand(const struct parser *parser)
{
	const char *next;

	if (parser->remaining == 1)
		return false;

	next = peek_arg(parser, 1);
	if (parser->remaining == 2)
		return parser->paren_level == 1 && strcmp(next, ")") == 0;
	return false;
}

static enum token
lex_token(struct parser *parser, const char *text)
{
	enum token token;

	if (text == NULL)
		return TOKEN_EOI;

	token = find_operator(text);
	if (((TOKEN_FAMILY(token) == TOKEN_UNARY ||
	      TOKEN_FAMILY(token) == TOKEN_BOOLEAN_UNARY) &&
	     is_unary_operand(parser)) ||
	    (token == TOKEN_LPAREN && is_left_paren_operand(parser)) ||
	    (token == TOKEN_RPAREN && is_right_paren_operand(parser))) {
		return TOKEN_OPERAND;
	}
	return token;
}

static int
parse_int(const char *text)
{
	char *end;
	intmax_t value;

	errno = 0;
	value = strtoimax(text, &end, 10);
	if (end == text)
		die("%s: bad number", text);
	if (errno == ERANGE || value < INT_MIN || value > INT_MAX)
		die("%s: out of range", text);

	while (*end != '\0' && isspace((unsigned char)*end))
		end++;
	if (*end != '\0')
		die("%s: bad number", text);

	return (int)value;
}

static intmax_t
parse_intmax(const char *text)
{
	char *end;
	intmax_t value;

	errno = 0;
	value = strtoimax(text, &end, 10);
	if (end == text)
		die("%s: bad number", text);
	if (errno == ERANGE)
		die("%s: out of range", text);

	while (*end != '\0' && isspace((unsigned char)*end))
		end++;
	if (*end != '\0')
		die("%s: bad number", text);

	return value;
}

static int
effective_access(const char *path, int mode)
{
	if (faccessat(AT_FDCWD, path, mode, AT_EACCESS) == 0)
		return 0;
	if (errno == EINVAL || errno == ENOSYS)
		die("Linux effective access checks require faccessat(AT_EACCESS)");
	return -1;
}

static int
compare_mtime(const struct stat *lhs, const struct stat *rhs)
{
	if (lhs->st_mtim.tv_sec > rhs->st_mtim.tv_sec)
		return 1;
	if (lhs->st_mtim.tv_sec < rhs->st_mtim.tv_sec)
		return -1;
	if (lhs->st_mtim.tv_nsec > rhs->st_mtim.tv_nsec)
		return 1;
	if (lhs->st_mtim.tv_nsec < rhs->st_mtim.tv_nsec)
		return -1;
	return 0;
}

static int
newer_file(const char *lhs, const char *rhs)
{
	struct stat lhs_stat;
	struct stat rhs_stat;

	if (stat(lhs, &lhs_stat) != 0 || stat(rhs, &rhs_stat) != 0)
		return 0;
	return compare_mtime(&lhs_stat, &rhs_stat) > 0;
}

static int
older_file(const char *lhs, const char *rhs)
{
	return newer_file(rhs, lhs);
}

static int
same_file(const char *lhs, const char *rhs)
{
	struct stat lhs_stat;
	struct stat rhs_stat;

	return stat(lhs, &lhs_stat) == 0 &&
	    stat(rhs, &rhs_stat) == 0 &&
	    lhs_stat.st_dev == rhs_stat.st_dev &&
	    lhs_stat.st_ino == rhs_stat.st_ino;
}

static int
evaluate_file_test(const char *path, enum token token)
{
	struct stat st;
	int stat_result;

	stat_result = token == TOKEN_FILSYM ? lstat(path, &st) : stat(path, &st);
	if (stat_result != 0)
		return 0;

	switch (token) {
	case TOKEN_FILRD:
		return effective_access(path, R_OK) == 0;
	case TOKEN_FILWR:
		return effective_access(path, W_OK) == 0;
	case TOKEN_FILEX:
		if (effective_access(path, X_OK) != 0)
			return 0;
		if (S_ISDIR(st.st_mode) || geteuid() != 0)
			return 1;
		return (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0;
	case TOKEN_FILEXIST:
		return 1;
	case TOKEN_FILREG:
		return S_ISREG(st.st_mode);
	case TOKEN_FILDIR:
		return S_ISDIR(st.st_mode);
	case TOKEN_FILCDEV:
		return S_ISCHR(st.st_mode);
	case TOKEN_FILBDEV:
		return S_ISBLK(st.st_mode);
	case TOKEN_FILFIFO:
		return S_ISFIFO(st.st_mode);
	case TOKEN_FILSOCK:
		return S_ISSOCK(st.st_mode);
	case TOKEN_FILSYM:
		return S_ISLNK(st.st_mode);
	case TOKEN_FILSUID:
		return (st.st_mode & S_ISUID) != 0;
	case TOKEN_FILSGID:
		return (st.st_mode & S_ISGID) != 0;
	case TOKEN_FILSTCK:
		return (st.st_mode & S_ISVTX) != 0;
	case TOKEN_FILGZ:
		return st.st_size > 0;
	case TOKEN_FILUID:
		return st.st_uid == geteuid();
	case TOKEN_FILGID:
		return st.st_gid == getegid();
	default:
		return 0;
	}
}

static int
compare_integers(const char *lhs, const char *rhs)
{
	intmax_t lhs_value;
	intmax_t rhs_value;

	lhs_value = parse_intmax(lhs);
	rhs_value = parse_intmax(rhs);
	if (lhs_value > rhs_value)
		return 1;
	if (lhs_value < rhs_value)
		return -1;
	return 0;
}

static int parse_oexpr(struct parser *parser, enum token token);

static int
parse_binop(struct parser *parser, enum token token)
{
	const char *lhs;
	const char *op;
	const char *rhs;

	lhs = current_arg(parser);
	advance_arg(parser);
	op = current_arg(parser);
	advance_arg(parser);
	rhs = current_arg(parser);
	if (rhs == NULL)
		syntax_error(op, "argument expected");

	switch (token) {
	case TOKEN_STREQ:
		return strcmp(lhs, rhs) == 0;
	case TOKEN_STRNE:
		return strcmp(lhs, rhs) != 0;
	case TOKEN_STRLT:
		return strcmp(lhs, rhs) < 0;
	case TOKEN_STRGT:
		return strcmp(lhs, rhs) > 0;
	case TOKEN_INTEQ:
		return compare_integers(lhs, rhs) == 0;
	case TOKEN_INTNE:
		return compare_integers(lhs, rhs) != 0;
	case TOKEN_INTGE:
		return compare_integers(lhs, rhs) >= 0;
	case TOKEN_INTGT:
		return compare_integers(lhs, rhs) > 0;
	case TOKEN_INTLE:
		return compare_integers(lhs, rhs) <= 0;
	case TOKEN_INTLT:
		return compare_integers(lhs, rhs) < 0;
	case TOKEN_FILNT:
		return newer_file(lhs, rhs);
	case TOKEN_FILOT:
		return older_file(lhs, rhs);
	case TOKEN_FILEQ:
		return same_file(lhs, rhs);
	default:
		abort();
	}
}

static int
parse_primary(struct parser *parser, enum token token)
{
	enum token next_token;
	const char *operand;
	int result;

	if (token == TOKEN_EOI)
		return 0;

	if (token == TOKEN_LPAREN) {
		parser->paren_level++;
		next_token = lex_token(parser, advance_arg(parser));
		if (next_token == TOKEN_RPAREN) {
			parser->paren_level--;
			return 0;
		}
		result = parse_oexpr(parser, next_token);
		if (lex_token(parser, advance_arg(parser)) != TOKEN_RPAREN)
			syntax_error(NULL, "closing paren expected");
		parser->paren_level--;
		return result;
	}

	if (TOKEN_FAMILY(token) == TOKEN_UNARY) {
		if (parser->remaining <= 1)
			syntax_error(NULL, "argument expected");
		operand = advance_arg(parser);
		switch (token) {
		case TOKEN_STREZ:
			return operand[0] == '\0';
		case TOKEN_STRNZ:
			return operand[0] != '\0';
		case TOKEN_FILTT:
			return isatty(parse_int(operand));
		default:
			return evaluate_file_test(operand, token);
		}
	}

	next_token = lex_token(parser, peek_arg(parser, 1));
	if (TOKEN_FAMILY(next_token) == TOKEN_BINARY)
		return parse_binop(parser, next_token);

	return current_arg(parser)[0] != '\0';
}

static int
parse_nexpr(struct parser *parser, enum token token)
{
	if (token == TOKEN_UNOT)
		return !parse_nexpr(parser, lex_token(parser, advance_arg(parser)));
	return parse_primary(parser, token);
}

static int
parse_aexpr(struct parser *parser, enum token token)
{
	int result;

	result = parse_nexpr(parser, token);
	if (lex_token(parser, advance_arg(parser)) == TOKEN_BAND)
		return parse_aexpr(parser, lex_token(parser, advance_arg(parser))) &&
		    result;
	rewind_arg(parser);
	return result;
}

static int
parse_oexpr(struct parser *parser, enum token token)
{
	int result;

	result = parse_aexpr(parser, token);
	if (lex_token(parser, advance_arg(parser)) == TOKEN_BOR)
		return parse_oexpr(parser, lex_token(parser, advance_arg(parser))) ||
		    result;
	rewind_arg(parser);
	return result;
}

int
main(int argc, char **argv)
{
	struct parser parser;
	int result;

	program_name = base_name(argv[0]);
	if (strcmp(program_name, "[") == 0) {
		if (argc == 1 || strcmp(argv[argc - 1], "]") != 0)
			die("missing ']'");
		argc--;
		argv[argc] = NULL;
	}

	if (argc <= 1)
		return 1;

	parser.argv = argv + 1;
	parser.pos = 0;
	parser.remaining = argc - 1;
	parser.paren_level = 0;

	if (parser.remaining == 4 && strcmp(current_arg(&parser), "!") == 0) {
		advance_arg(&parser);
		result = parse_oexpr(&parser,
		    lex_token(&parser, current_arg(&parser)));
	} else {
		result = !parse_oexpr(&parser,
		    lex_token(&parser, current_arg(&parser)));
	}

	advance_arg(&parser);
	if (parser.remaining > 0)
		syntax_error(current_arg(&parser), "unexpected operator");

	return result;
}
