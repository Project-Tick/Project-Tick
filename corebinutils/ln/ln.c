/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1987, 1993, 1994
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

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

struct ln_options {
	bool force;
	bool remove_dir;
	bool no_target_follow;
	bool interactive;
	bool follow_source_symlink;
	bool symbolic;
	bool verbose;
	bool warn_missing;
	char linkch;
};

static const char *progname;

static void	error_errno(const char *fmt, ...);
static void	error_msg(const char *fmt, ...);
static char	*join_path(const char *dir, const char *name);
static int	link_usage(void);
static int	ln_usage(void);
static int	linkit(const struct ln_options *options, const char *source,
		    const char *target, bool target_is_dir);
static const char *path_basename_start(const char *path);
static size_t	path_basename_len(const char *path);
static char	*path_basename_dup(const char *path);
static char	*path_dirname_dup(const char *path);
static const char *program_name(const char *argv0);
static int	prompt_replace(const char *target);
static int	remove_existing_target(const struct ln_options *options,
		    const char *target, const struct stat *target_sb);
static int	samedirent(const char *path1, const char *path2);
static bool	should_append_basename(const struct ln_options *options,
		    const char *target, bool target_is_dir);
static int	stat_parent_dir(const char *path, struct stat *sb);
static void	warn_missing_symlink_source(const char *source,
		    const char *target);

static const char *
program_name(const char *argv0)
{
	const char *name;

	if (argv0 == NULL || argv0[0] == '\0')
		return ("ln");
	name = strrchr(argv0, '/');
	return (name == NULL ? argv0 : name + 1);
}

static void
verror_message(bool with_errno, const char *fmt, va_list ap)
{
	int saved_errno;

	saved_errno = errno;
	(void)fprintf(stderr, "%s: ", progname);
	(void)vfprintf(stderr, fmt, ap);
	if (with_errno)
		(void)fprintf(stderr, ": %s", strerror(saved_errno));
	(void)fputc('\n', stderr);
}

static void
error_errno(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	verror_message(true, fmt, ap);
	va_end(ap);
}

static void
error_msg(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	verror_message(false, fmt, ap);
	va_end(ap);
}

static void *
xmalloc(size_t size)
{
	void *ptr;

	ptr = malloc(size);
	if (ptr == NULL) {
		error_msg("out of memory");
		exit(1);
	}
	return (ptr);
}

static char *
xstrdup(const char *text)
{
	size_t len;
	char *copy;

	len = strlen(text) + 1;
	copy = xmalloc(len);
	memcpy(copy, text, len);
	return (copy);
}

static char *
path_basename_dup(const char *path)
{
	const char *start;
	size_t len;
	char *name;

	start = path_basename_start(path);
	len = path_basename_len(path);
	name = xmalloc(len + 1);
	memcpy(name, start, len);
	name[len] = '\0';
	return (name);
}

static const char *
path_basename_start(const char *path)
{
	const char *end;
	const char *start;

	if (path[0] == '\0')
		return (path);

	end = path + strlen(path);
	while (end > path && end[-1] == '/')
		end--;
	if (end == path)
		return (path);

	start = end;
	while (start > path && start[-1] != '/')
		start--;
	return (start);
}

static size_t
path_basename_len(const char *path)
{
	const char *end;
	const char *start;

	if (path[0] == '\0')
		return (1);

	end = path + strlen(path);
	while (end > path && end[-1] == '/')
		end--;
	if (end == path)
		return (1);

	start = path_basename_start(path);
	return ((size_t)(end - start));
}

static char *
path_dirname_dup(const char *path)
{
	const char *end;
	const char *slash;
	size_t len;
	char *dir;

	if (path[0] == '\0')
		return (xstrdup("."));

	end = path + strlen(path);
	while (end > path && end[-1] == '/')
		end--;
	if (end == path)
		return (xstrdup("/"));

	slash = end;
	while (slash > path && slash[-1] != '/')
		slash--;
	if (slash == path)
		return (xstrdup("."));

	while (slash > path && slash[-1] == '/')
		slash--;
	if (slash == path)
		return (xstrdup("/"));

	len = (size_t)(slash - path);
	dir = xmalloc(len + 1);
	memcpy(dir, path, len);
	dir[len] = '\0';
	return (dir);
}

static char *
join_path(const char *dir, const char *name)
{
	size_t dir_len;
	size_t name_len;
	bool need_sep;
	char *path;

	dir_len = strlen(dir);
	name_len = strlen(name);
	need_sep = dir_len != 0 && dir[dir_len - 1] != '/';

	path = xmalloc(dir_len + (need_sep ? 1U : 0U) + name_len + 1U);
	memcpy(path, dir, dir_len);
	if (need_sep)
		path[dir_len++] = '/';
	memcpy(path + dir_len, name, name_len);
	path[dir_len + name_len] = '\0';
	return (path);
}

static int
samedirent(const char *path1, const char *path2)
{
	const char *base1;
	const char *base2;
	struct stat sb1;
	struct stat sb2;
	size_t base1_len;
	size_t base2_len;

	if (strcmp(path1, path2) == 0)
		return (1);

	base1 = path_basename_start(path1);
	base2 = path_basename_start(path2);
	base1_len = path_basename_len(path1);
	base2_len = path_basename_len(path2);
	if (base1_len != base2_len || memcmp(base1, base2, base1_len) != 0)
		return (0);

	if (stat_parent_dir(path1, &sb1) != 0 || stat_parent_dir(path2, &sb2) != 0)
		return (0);

	return (sb1.st_dev == sb2.st_dev && sb1.st_ino == sb2.st_ino);
}

static bool
should_append_basename(const struct ln_options *options, const char *target,
    bool target_is_dir)
{
	struct stat sb;
	const char *base;

	base = strrchr(target, '/');
	base = base == NULL ? target : base + 1;
	if (base[0] == '\0' || (base[0] == '.' && base[1] == '\0'))
		return (true);

	if (options->remove_dir)
		return (false);
	if (target_is_dir)
		return (true);
	if (lstat(target, &sb) == 0 && S_ISDIR(sb.st_mode))
		return (true);
	if (options->no_target_follow)
		return (false);
	if (stat(target, &sb) == 0 && S_ISDIR(sb.st_mode))
		return (true);
	return (false);
}

static int
stat_parent_dir(const char *path, struct stat *sb)
{
	char *dir;
	size_t dir_len;
	const char *base;

	base = path_basename_start(path);
	if (base == path)
		return (stat(".", sb));

	dir_len = (size_t)(base - path);
	while (dir_len > 1 && path[dir_len - 1] == '/')
		dir_len--;
	if (dir_len == 1 && path[0] == '/')
		return (stat("/", sb));

	dir = xmalloc(dir_len + 1);
	memcpy(dir, path, dir_len);
	dir[dir_len] = '\0';
	if (stat(dir, sb) != 0) {
		free(dir);
		return (-1);
	}
	free(dir);
	return (0);
}

static int
remove_existing_target(const struct ln_options *options, const char *target,
    const struct stat *target_sb)
{
	if (options->remove_dir && S_ISDIR(target_sb->st_mode)) {
		if (rmdir(target) != 0) {
			error_errno("%s", target);
			return (1);
		}
		return (0);
	}

	if (unlink(target) != 0) {
		error_errno("%s", target);
		return (1);
	}
	return (0);
}

static void
warn_missing_symlink_source(const char *source, const char *target)
{
	char *dir;
	char *resolved;
	struct stat st;

	if (source[0] == '/') {
		if (stat(source, &st) != 0)
			error_errno("warning: %s", source);
		return;
	}

	dir = path_dirname_dup(target);
	resolved = join_path(dir, source);
	if (stat(resolved, &st) != 0)
		error_errno("warning: %s", source);
	free(dir);
	free(resolved);
}

static int
prompt_replace(const char *target)
{
	char answer[16];
	bool stdin_is_tty;
	int ch;

	stdin_is_tty = isatty(STDIN_FILENO) == 1;
	(void)stdin_is_tty;

	(void)fflush(stdout);
	(void)fprintf(stderr, "replace %s? ", target);
	if (fgets(answer, sizeof(answer), stdin) == NULL) {
		if (ferror(stdin)) {
			error_errno("stdin");
			return (-1);
		}
		if (!stdin_is_tty && feof(stdin)) {
			(void)fprintf(stderr, "not replaced\n");
			return (1);
		}
		(void)fprintf(stderr, "not replaced\n");
		return (1);
	}

	if (strchr(answer, '\n') == NULL) {
		while ((ch = getchar()) != '\n' && ch != EOF)
			continue;
		if (ferror(stdin)) {
			error_errno("stdin");
			return (-1);
		}
	}

	if (answer[0] != 'y' && answer[0] != 'Y') {
		(void)fprintf(stderr, "not replaced\n");
		return (1);
	}
	return (0);
}

static int
linkit(const struct ln_options *options, const char *source, const char *target,
    bool target_is_dir)
{
	struct stat source_sb;
	struct stat target_sb;
	char *resolved_target;
	bool exists;
	int flags;
	int ret;

	resolved_target = NULL;
	ret = 1;
	if (!options->symbolic) {
		if ((options->follow_source_symlink ? stat : lstat)(source,
		    &source_sb) != 0) {
			error_errno("%s", source);
			goto cleanup;
		}
		if (S_ISDIR(source_sb.st_mode)) {
			errno = EISDIR;
			error_errno("%s", source);
			goto cleanup;
		}
	}

	if (should_append_basename(options, target, target_is_dir)) {
		char *base;

		base = path_basename_dup(source);
		resolved_target = join_path(target, base);
		free(base);
	} else {
		resolved_target = xstrdup(target);
	}

	if (options->symbolic && options->warn_missing)
		warn_missing_symlink_source(source, resolved_target);

	exists = lstat(resolved_target, &target_sb) == 0;
	if (exists && !options->symbolic && samedirent(source, resolved_target)) {
		error_msg("%s and %s are the same directory entry", source,
		    resolved_target);
		goto cleanup;
	}

	if (exists && options->force) {
		if (remove_existing_target(options, resolved_target, &target_sb) != 0)
			goto cleanup;
	} else if (exists && options->interactive) {
		ret = prompt_replace(resolved_target);
		if (ret != 0)
			goto cleanup;
		if (remove_existing_target(options, resolved_target, &target_sb) != 0)
			goto cleanup;
	}

	flags = options->follow_source_symlink ? AT_SYMLINK_FOLLOW : 0;
	if (options->symbolic) {
		if (symlink(source, resolved_target) != 0) {
			error_errno("%s", resolved_target);
			goto cleanup;
		}
	} else if (linkat(AT_FDCWD, source, AT_FDCWD, resolved_target, flags) != 0) {
		error_errno("%s", resolved_target);
		goto cleanup;
	}

	if (options->verbose)
		(void)printf("%s %c> %s\n", resolved_target, options->linkch, source);

	ret = 0;
cleanup:
	free(resolved_target);
	return (ret);
}

static int
link_usage(void)
{
	(void)fprintf(stderr, "usage: link source_file target_file\n");
	return (2);
}

static int
ln_usage(void)
{
	(void)fprintf(stderr,
	    "usage: ln [-s [-F] | -L | -P] [-f | -i] [-hnvw] "
	    "source_file [target_file]\n");
	(void)fprintf(stderr,
	    "       ln [-s [-F] | -L | -P] [-f | -i] [-hnvw] "
	    "source_file ... target_dir\n");
	return (2);
}

int
main(int argc, char *argv[])
{
	struct ln_options options;
	struct stat sb;
	char *targetdir;
	int ch;
	int exitval;

	progname = program_name(argv[0]);
	memset(&options, 0, sizeof(options));
	/* FreeBSD hard-link semantics default to -L, unlike GNU ln's -P. */
	options.follow_source_symlink = true;

	if (strcmp(progname, "link") == 0) {
		opterr = 0;
		while ((ch = getopt(argc, argv, "")) != -1)
			return (link_usage());
		argc -= optind;
		argv += optind;
		if (argc != 2)
			return (link_usage());
		if (lstat(argv[1], &sb) == 0) {
			errno = EEXIST;
			error_errno("%s", argv[1]);
			return (1);
		}
		return (linkit(&options, argv[0], argv[1], false));
	}

	opterr = 0;
	while ((ch = getopt(argc, argv, "FLPfhinsvw")) != -1) {
		switch (ch) {
		case 'F':
			options.remove_dir = true;
			break;
		case 'L':
			options.follow_source_symlink = true;
			break;
		case 'P':
			options.follow_source_symlink = false;
			break;
		case 'f':
			options.force = true;
			options.interactive = false;
			options.warn_missing = false;
			break;
		case 'h':
		case 'n':
			options.no_target_follow = true;
			break;
		case 'i':
			options.interactive = true;
			options.force = false;
			break;
		case 's':
			options.symbolic = true;
			break;
		case 'v':
			options.verbose = true;
			break;
		case 'w':
			options.warn_missing = true;
			break;
		case '?':
		default:
			if (optopt != 0)
				error_msg("unknown option -- %c", optopt);
			return (ln_usage());
		}
	}

	argv += optind;
	argc -= optind;

	options.linkch = options.symbolic ? '-' : '=';
	if (!options.symbolic)
		options.remove_dir = false;
	if (options.remove_dir && !options.interactive) {
		options.force = true;
		options.warn_missing = false;
	}

	switch (argc) {
	case 0:
		return (ln_usage());
	case 1:
		return (linkit(&options, argv[0], ".", true));
	case 2:
		return (linkit(&options, argv[0], argv[1], false));
	default:
		break;
	}

	targetdir = argv[argc - 1];
	if (options.no_target_follow && lstat(targetdir, &sb) == 0 &&
	    S_ISLNK(sb.st_mode)) {
		errno = ENOTDIR;
		error_errno("%s", targetdir);
		return (1);
	}
	if (stat(targetdir, &sb) != 0) {
		error_errno("%s", targetdir);
		return (1);
	}
	if (!S_ISDIR(sb.st_mode))
		return (ln_usage());

	exitval = 0;
	for (int i = 0; i < argc - 1; i++)
		exitval |= linkit(&options, argv[i], targetdir, true);
	return (exitval);
}
