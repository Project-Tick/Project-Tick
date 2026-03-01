/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2026
 *	Project Tick. All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ken Smith of The State University of New York at Buffalo.
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

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/xattr.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

#ifndef AT_NO_AUTOMOUNT
#define AT_NO_AUTOMOUNT 0
#endif

#define MV_EXIT_ERROR 1
#define MV_EXIT_USAGE 2
#define COPY_BUFFER_MIN (128U * 1024U)
#define COPY_BUFFER_MAX (2U * 1024U * 1024U)

struct mv_options {
	bool force;
	bool interactive;
	bool no_clobber;
	bool no_target_dir_follow;
	bool verbose;
};

struct move_target {
	bool treat_as_directory;
	char *path;
};

static const char *progname;

static int	append_child_basename(const char *directory, const char *source,
		    char **result);
static int	apply_existing_target_policy(const struct mv_options *options,
		    const char *target, bool *skip_move);
static int	apply_path_metadata(const char *target, const struct stat *source_sb,
		    bool nofollow, const char *source);
static size_t	basename_len(const char *path);
static const char *basename_start(const char *path);
static int	cleanup_failed_target(const char *target);
static int	copy_directory_tree(const char *source, const char *target,
		    const struct stat *source_sb);
static int	copy_file_data(int from_fd, int to_fd, const char *source,
		    const char *target);
static int	copy_file_xattrs(int source_fd, int dest_fd, const char *source,
		    const char *target);
static int	copy_move_fallback(const struct mv_options *options,
		    const char *source, const char *target,
		    const struct stat *source_sb, const struct stat *target_sb);
static int	copy_node(const char *source, const char *target,
		    const struct stat *source_sb);
static int	copy_non_regular(const char *source, const char *target,
		    const struct stat *source_sb);
static int	copy_path_xattrs(const char *source, const char *target,
		    bool nofollow);
static int	copy_regular_file(const char *source, const char *target,
		    const struct stat *source_sb);
static int	determine_target_mode(const struct mv_options *options, int argc,
		    char *const argv[], struct move_target *target);
static char	*dirname_dup(const char *path);
static void	error_errno(const char *fmt, ...);
static void	error_msg(const char *fmt, ...);
static int	file_list_xattrs(int fd, const char *source, char **names_buf,
		    ssize_t *names_len);
static int	handle_single_move(const struct mv_options *options,
		    const char *source, const char *target);
static int	is_dir_symlink(const char *path);
static int	is_mount_point(const char *path, const struct stat *source_sb,
		    bool *mount_point);
static char	*join_path(const char *dir, const char *name);
static int	path_list_xattrs(const char *source, bool nofollow,
		    char **names_buf, ssize_t *names_len);
static int	preserve_timestamps_fd(int fd, const struct stat *source_sb,
		    const char *target);
static int	preserve_timestamps_path(const char *target,
		    const struct stat *source_sb, bool nofollow);
static const char *program_name(const char *argv0);
static int	prompt_overwrite(const char *target);
static int	readlink_alloc(const char *path, char **targetp);
static int	remove_source_tree(const char *path, const struct stat *sb);
static int	remove_target_path(const char *target, const struct stat *target_sb);
static int	set_fd_ownership_and_mode(int fd, const struct stat *source_sb,
		    const char *target);
static int	set_path_ownership_and_mode(const char *target,
		    const struct stat *source_sb, bool nofollow);
static int	set_xattr_loop(const char *source, const char *target, char *names,
		    ssize_t names_len, bool nofollow, int source_fd, int dest_fd);
static int	should_prompt_for_permissions(const char *target);
static void	usage(void);
static void	*xmalloc(size_t size);
static void	*xrealloc(void *ptr, size_t size);
static char	*xstrdup(const char *text);
extern int	mknod(const char *path, mode_t mode, dev_t dev);

static const char *
program_name(const char *argv0)
{
	const char *name;

	if (argv0 == NULL || argv0[0] == '\0')
		return ("mv");
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
		exit(MV_EXIT_ERROR);
	}
	return (ptr);
}

static void *
xrealloc(void *ptr, size_t size)
{
	void *newptr;

	newptr = realloc(ptr, size);
	if (newptr == NULL) {
		error_msg("out of memory");
		exit(MV_EXIT_ERROR);
	}
	return (newptr);
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

static const char *
basename_start(const char *path)
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
basename_len(const char *path)
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

	start = basename_start(path);
	return ((size_t)(end - start));
}

static char *
dirname_dup(const char *path)
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
append_child_basename(const char *directory, const char *source, char **result)
{
	const char *base;
	size_t base_len;
	char *name;

	base = basename_start(source);
	base_len = basename_len(source);
	name = xmalloc(base_len + 1);
	memcpy(name, base, base_len);
	name[base_len] = '\0';
	*result = join_path(directory, name);
	free(name);
	return (0);
}

static void
usage(void)
{
	(void)fprintf(stderr, "%s\n%s\n",
	    "usage: mv [-f | -i | -n] [-hv] source target",
	    "       mv [-f | -i | -n] [-v] source ... directory");
	exit(MV_EXIT_USAGE);
}

static int
prompt_overwrite(const char *target)
{
	int ch;
	int first;

	(void)fprintf(stderr, "overwrite %s? (y/n [n]) ", target);
	first = getchar();
	ch = first;
	while (ch != '\n' && ch != EOF)
		ch = getchar();
	return (first == 'y' || first == 'Y');
}

static int
should_prompt_for_permissions(const char *target)
{
	struct stat sb;

	if (!isatty(STDIN_FILENO))
		return (0);
	if (access(target, W_OK) == 0)
		return (0);
	if (stat(target, &sb) != 0)
		return (0);
	return (1);
}

static int
apply_existing_target_policy(const struct mv_options *options, const char *target,
    bool *skip_move)
{
	int prompt;

	*skip_move = false;
	if (options->force)
		return (0);
	if (options->no_clobber) {
		if (options->verbose)
			(void)printf("%s not overwritten\n", target);
		*skip_move = true;
		return (0);
	}

	prompt = options->interactive || should_prompt_for_permissions(target);
	if (!prompt)
		return (0);

	if (!prompt_overwrite(target)) {
		(void)fprintf(stderr, "not overwritten\n");
		*skip_move = true;
	}
	return (0);
}

static int
readlink_alloc(const char *path, char **targetp)
{
	size_t size;
	ssize_t len;
	char *buf;

	size = 128;
	buf = xmalloc(size);
	for (;;) {
		len = readlink(path, buf, size);
		if (len < 0) {
			free(buf);
			return (-1);
		}
		if ((size_t)len < size) {
			buf[len] = '\0';
			*targetp = buf;
			return (0);
		}
		size *= 2;
		buf = xrealloc(buf, size);
	}
}

static int
file_list_xattrs(int fd, const char *source, char **names_buf, ssize_t *names_len)
{
	ssize_t len;
	char *buf;

	len = flistxattr(fd, NULL, 0);
	if (len < 0) {
		if (errno == ENOTSUP || errno == EOPNOTSUPP) {
			*names_buf = NULL;
			*names_len = 0;
			return (0);
		}
		error_errno("list xattrs for %s", source);
		return (-1);
	}
	if (len == 0) {
		*names_buf = NULL;
		*names_len = 0;
		return (0);
	}

	buf = xmalloc((size_t)len);
	if (flistxattr(fd, buf, (size_t)len) != len) {
		error_errno("list xattrs for %s", source);
		free(buf);
		return (-1);
	}
	*names_buf = buf;
	*names_len = len;
	return (0);
}

static int
path_list_xattrs(const char *source, bool nofollow, char **names_buf, ssize_t *names_len)
{
	ssize_t len;
	char *buf;

	if (nofollow)
		len = llistxattr(source, NULL, 0);
	else
		len = listxattr(source, NULL, 0);
	if (len < 0) {
		if (errno == ENOTSUP || errno == EOPNOTSUPP) {
			*names_buf = NULL;
			*names_len = 0;
			return (0);
		}
		error_errno("list xattrs for %s", source);
		return (-1);
	}
	if (len == 0) {
		*names_buf = NULL;
		*names_len = 0;
		return (0);
	}

	buf = xmalloc((size_t)len);
	if ((nofollow ? llistxattr(source, buf, (size_t)len) :
	    listxattr(source, buf, (size_t)len)) != len) {
		error_errno("list xattrs for %s", source);
		free(buf);
		return (-1);
	}
	*names_buf = buf;
	*names_len = len;
	return (0);
}

static int
set_xattr_loop(const char *source, const char *target, char *names,
    ssize_t names_len, bool nofollow, int source_fd, int dest_fd)
{
	char *name;
	char *limit;

	limit = names + names_len;
	for (name = names; name < limit; name += strlen(name) + 1) {
		ssize_t value_len;
		void *value;
		int set_result;

		if (source_fd >= 0)
			value_len = fgetxattr(source_fd, name, NULL, 0);
		else if (nofollow)
			value_len = lgetxattr(source, name, NULL, 0);
		else
			value_len = getxattr(source, name, NULL, 0);
		if (value_len < 0) {
			error_errno("read xattr '%s' from %s", name, source);
			return (-1);
		}

		value = xmalloc(value_len == 0 ? 1U : (size_t)value_len);
		if (value_len > 0) {
			if (source_fd >= 0)
				value_len = fgetxattr(source_fd, name, value,
				    (size_t)value_len);
			else if (nofollow)
				value_len = lgetxattr(source, name, value,
				    (size_t)value_len);
			else
				value_len = getxattr(source, name, value,
				    (size_t)value_len);
			if (value_len < 0) {
				error_errno("read xattr '%s' from %s", name, source);
				free(value);
				return (-1);
			}
		}

		if (dest_fd >= 0)
			set_result = fsetxattr(dest_fd, name, value,
			    (size_t)value_len, 0);
		else if (nofollow)
			set_result = lsetxattr(target, name, value,
			    (size_t)value_len, 0);
		else
			set_result = setxattr(target, name, value,
			    (size_t)value_len, 0);
		if (set_result != 0) {
			error_errno("set xattr '%s' on %s", name, target);
			free(value);
			return (-1);
		}
		free(value);
	}
	return (0);
}

static int
copy_file_xattrs(int source_fd, int dest_fd, const char *source, const char *target)
{
	char *names;
	ssize_t names_len;
	int rc;

	names = NULL;
	names_len = 0;
	rc = file_list_xattrs(source_fd, source, &names, &names_len);
	if (rc != 0)
		return (rc);
	if (names_len == 0)
		return (0);

	rc = set_xattr_loop(source, target, names, names_len, false, source_fd,
	    dest_fd);
	free(names);
	return (rc);
}

static int
copy_path_xattrs(const char *source, const char *target, bool nofollow)
{
	char *names;
	ssize_t names_len;
	int rc;

	names = NULL;
	names_len = 0;
	rc = path_list_xattrs(source, nofollow, &names, &names_len);
	if (rc != 0)
		return (rc);
	if (names_len == 0)
		return (0);

	rc = set_xattr_loop(source, target, names, names_len, nofollow, -1, -1);
	free(names);
	return (rc);
}

static int
preserve_timestamps_fd(int fd, const struct stat *source_sb, const char *target)
{
	struct timespec times[2];

	times[0] = source_sb->st_atim;
	times[1] = source_sb->st_mtim;
	if (futimens(fd, times) != 0) {
		error_errno("set times on %s", target);
		return (-1);
	}
	return (0);
}

static int
preserve_timestamps_path(const char *target, const struct stat *source_sb,
    bool nofollow)
{
	struct timespec times[2];
	int flags;

	times[0] = source_sb->st_atim;
	times[1] = source_sb->st_mtim;
	flags = nofollow ? AT_SYMLINK_NOFOLLOW : 0;
	if (utimensat(AT_FDCWD, target, times, flags) != 0) {
		error_errno("set times on %s", target);
		return (-1);
	}
	return (0);
}

static int
set_fd_ownership_and_mode(int fd, const struct stat *source_sb, const char *target)
{
	struct stat current_sb;
	mode_t mode;

	if (fstat(fd, &current_sb) != 0) {
		error_errno("stat %s", target);
		return (-1);
	}

	mode = source_sb->st_mode & 07777;
	if (current_sb.st_uid != source_sb->st_uid ||
	    current_sb.st_gid != source_sb->st_gid) {
		if (fchown(fd, source_sb->st_uid, source_sb->st_gid) != 0) {
			if (errno != EPERM) {
				error_errno("set owner on %s", target);
				return (-1);
			}
			mode &= ~(S_ISUID | S_ISGID);
		}
	}

	if ((current_sb.st_mode & 07777) != mode && fchmod(fd, mode) != 0) {
		error_errno("set mode on %s", target);
		return (-1);
	}

	return (0);
}

static int
set_path_ownership_and_mode(const char *target, const struct stat *source_sb,
    bool nofollow)
{
	struct stat current_sb;
	mode_t mode;
	int flags;

	flags = nofollow ? AT_SYMLINK_NOFOLLOW : 0;
	if ((nofollow ? lstat(target, &current_sb) : stat(target, &current_sb)) != 0) {
		error_errno("stat %s", target);
		return (-1);
	}

	mode = source_sb->st_mode & 07777;
	if (current_sb.st_uid != source_sb->st_uid ||
	    current_sb.st_gid != source_sb->st_gid) {
		if (fchownat(AT_FDCWD, target, source_sb->st_uid, source_sb->st_gid,
		    flags) != 0) {
			if (errno != EPERM) {
				error_errno("set owner on %s", target);
				return (-1);
			}
			mode &= ~(S_ISUID | S_ISGID);
		}
	}

	if (!nofollow && (current_sb.st_mode & 07777) != mode &&
	    fchmodat(AT_FDCWD, target, mode, 0) != 0) {
		error_errno("set mode on %s", target);
		return (-1);
	}

return (0);
}

static int
apply_path_metadata(const char *target, const struct stat *source_sb,
    bool nofollow, const char *source)
{
	if (set_path_ownership_and_mode(target, source_sb, nofollow) != 0)
		return (-1);
	if (copy_path_xattrs(source, target, nofollow) != 0)
		return (-1);
	return (preserve_timestamps_path(target, source_sb, nofollow));
}

static size_t
copy_buffer_size(void)
{
	long pages;
	long pagesize;
	size_t size;

	pages = sysconf(_SC_PHYS_PAGES);
	pagesize = sysconf(_SC_PAGESIZE);
	if (pages > 0 && pagesize > 0) {
		uint64_t total;

		total = (uint64_t)pages * (uint64_t)pagesize;
		if (total >= (uint64_t)(512U * 1024U * 1024U))
			return (COPY_BUFFER_MAX);
	}
	size = COPY_BUFFER_MIN;
	return (size);
}

static int
copy_file_data(int from_fd, int to_fd, const char *source, const char *target)
{
	char *buffer;
	size_t buffer_size;

	buffer_size = copy_buffer_size();
	buffer = xmalloc(buffer_size);
	for (;;) {
		ssize_t read_count;
		char *outp;

		read_count = read(from_fd, buffer, buffer_size);
		if (read_count == 0)
			break;
		if (read_count < 0) {
			if (errno == EINTR)
				continue;
			error_errno("read %s", source);
			free(buffer);
			return (-1);
		}

		outp = buffer;
		while (read_count > 0) {
			ssize_t write_count;

			write_count = write(to_fd, outp, (size_t)read_count);
			if (write_count < 0) {
				if (errno == EINTR)
					continue;
				error_errno("write %s", target);
				free(buffer);
				return (-1);
			}
			outp += write_count;
			read_count -= write_count;
		}
	}
	free(buffer);
	return (0);
}

static int
copy_regular_file(const char *source, const char *target, const struct stat *source_sb)
{
	int source_fd;
	int target_fd;
	int saved_errno;
	int rc;

	source_fd = open(source, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
	if (source_fd < 0) {
		error_errno("%s", source);
		return (-1);
	}

	target_fd = open(target, O_WRONLY | O_CREAT | O_EXCL | O_TRUNC | O_CLOEXEC,
	    0600);
	if (target_fd < 0) {
		error_errno("%s", target);
		(void)close(source_fd);
		return (-1);
	}

	rc = copy_file_data(source_fd, target_fd, source, target);
	if (rc == 0 && set_fd_ownership_and_mode(target_fd, source_sb, target) != 0)
		rc = -1;
	if (rc == 0 && copy_file_xattrs(source_fd, target_fd, source, target) != 0)
		rc = -1;
	if (rc == 0 && preserve_timestamps_fd(target_fd, source_sb, target) != 0)
		rc = -1;

	saved_errno = errno;
	if (close(target_fd) != 0 && rc == 0) {
		error_errno("%s", target);
		rc = -1;
		saved_errno = errno;
	}
	(void)close(source_fd);

	if (rc != 0) {
		errno = saved_errno;
		(void)unlink(target);
		return (-1);
	}

	return (0);
}

static int
copy_non_regular(const char *source, const char *target, const struct stat *source_sb)
{
	char *link_target;

	if (S_ISLNK(source_sb->st_mode)) {
		link_target = NULL;
		if (readlink_alloc(source, &link_target) != 0) {
			error_errno("readlink %s", source);
			return (-1);
		}
		if (symlink(link_target, target) != 0) {
			error_errno("symlink %s", target);
			free(link_target);
			return (-1);
		}
		free(link_target);
		if (apply_path_metadata(target, source_sb, true, source) != 0) {
			(void)unlink(target);
			return (-1);
		}
		return (0);
	}

	if (S_ISFIFO(source_sb->st_mode)) {
		if (mkfifo(target, source_sb->st_mode & 07777) != 0) {
			error_errno("mkfifo %s", target);
			return (-1);
		}
		if (apply_path_metadata(target, source_sb, false, source) != 0) {
			(void)unlink(target);
			return (-1);
		}
		return (0);
	}

	if (S_ISCHR(source_sb->st_mode) || S_ISBLK(source_sb->st_mode)) {
		if (mknod(target, source_sb->st_mode, source_sb->st_rdev) != 0) {
			error_errno("mknod %s", target);
			return (-1);
		}
		if (apply_path_metadata(target, source_sb, false, source) != 0) {
			(void)unlink(target);
			return (-1);
		}
		return (0);
	}

	if (S_ISSOCK(source_sb->st_mode))
		error_msg("cannot move socket across filesystems: %s", source);
	else
		error_msg("unsupported file type for cross-filesystem move: %s",
		    source);
	return (-1);
}

static int
copy_directory_tree(const char *source, const char *target, const struct stat *source_sb)
{
	DIR *dirp;
	struct dirent *entry;
	mode_t mode;
	int rc;

	mode = (source_sb->st_mode & 07777) | S_IRWXU;
	if (mkdir(target, mode) != 0) {
		error_errno("mkdir %s", target);
		return (-1);
	}

	dirp = opendir(source);
	if (dirp == NULL) {
		error_errno("opendir %s", source);
		(void)rmdir(target);
		return (-1);
	}

	rc = 0;
	while ((entry = readdir(dirp)) != NULL) {
		struct stat child_sb;
		char *child_source;
		char *child_target;

		if ((entry->d_name[0] == '.' && entry->d_name[1] == '\0') ||
		    (entry->d_name[0] == '.' && entry->d_name[1] == '.' &&
		    entry->d_name[2] == '\0'))
			continue;

		child_source = join_path(source, entry->d_name);
		child_target = join_path(target, entry->d_name);
		if (lstat(child_source, &child_sb) != 0) {
			error_errno("%s", child_source);
			free(child_source);
			free(child_target);
			rc = -1;
			break;
		}
		if (copy_node(child_source, child_target, &child_sb) != 0) {
			free(child_source);
			free(child_target);
			rc = -1;
			break;
		}
		free(child_source);
		free(child_target);
	}

	if (closedir(dirp) != 0 && rc == 0) {
		error_errno("closedir %s", source);
		rc = -1;
	}

	if (rc != 0)
		return (-1);

	if (apply_path_metadata(target, source_sb, false, source) != 0)
		return (-1);

	return (0);
}

static int
copy_node(const char *source, const char *target, const struct stat *source_sb)
{
	if (S_ISREG(source_sb->st_mode))
		return (copy_regular_file(source, target, source_sb));
	if (S_ISDIR(source_sb->st_mode))
		return (copy_directory_tree(source, target, source_sb));
	return (copy_non_regular(source, target, source_sb));
}

static int
remove_source_tree(const char *path, const struct stat *sb)
{
	DIR *dirp;
	struct dirent *entry;
	int rc;

	if (!S_ISDIR(sb->st_mode)) {
		if (unlink(path) != 0) {
			error_errno("remove %s", path);
			return (-1);
		}
		return (0);
	}

	dirp = opendir(path);
	if (dirp == NULL) {
		error_errno("opendir %s", path);
		return (-1);
	}

	rc = 0;
	while ((entry = readdir(dirp)) != NULL) {
		struct stat child_sb;
		char *child_path;

		if ((entry->d_name[0] == '.' && entry->d_name[1] == '\0') ||
		    (entry->d_name[0] == '.' && entry->d_name[1] == '.' &&
		    entry->d_name[2] == '\0'))
			continue;

		child_path = join_path(path, entry->d_name);
		if (lstat(child_path, &child_sb) != 0) {
			error_errno("%s", child_path);
			free(child_path);
			rc = -1;
			break;
		}
		if (remove_source_tree(child_path, &child_sb) != 0) {
			free(child_path);
			rc = -1;
			break;
		}
		free(child_path);
	}

	if (closedir(dirp) != 0 && rc == 0) {
		error_errno("closedir %s", path);
		rc = -1;
	}
	if (rc != 0)
		return (-1);

	if (rmdir(path) != 0) {
		error_errno("remove %s", path);
		return (-1);
	}
	return (0);
}

static int
cleanup_failed_target(const char *target)
{
	struct stat sb;

	if (lstat(target, &sb) != 0) {
		if (errno == ENOENT)
			return (0);
		error_errno("%s", target);
		return (-1);
	}
	return (remove_source_tree(target, &sb));
}

static int
remove_target_path(const char *target, const struct stat *target_sb)
{
	if (S_ISDIR(target_sb->st_mode)) {
		if (rmdir(target) != 0) {
			error_errno("remove %s", target);
			return (-1);
		}
		return (0);
	}
	if (unlink(target) != 0) {
		error_errno("remove %s", target);
		return (-1);
	}
	return (0);
}

static int
is_mount_point(const char *path, const struct stat *source_sb, bool *mount_point)
{
	char *parent;
	struct stat parent_sb;

	*mount_point = false;
	parent = dirname_dup(path);
	if (lstat(parent, &parent_sb) != 0) {
		error_errno("%s", parent);
		free(parent);
		return (-1);
	}
	if (source_sb->st_dev != parent_sb.st_dev)
		*mount_point = true;

	if (!*mount_point && lstat(parent, &parent_sb) == 0 &&
	    source_sb->st_dev == parent_sb.st_dev &&
	    source_sb->st_ino == parent_sb.st_ino)
		*mount_point = true;

	free(parent);
	return (0);
}

static int
copy_move_fallback(const struct mv_options *options, const char *source,
    const char *target, const struct stat *source_sb, const struct stat *target_sb)
{
	bool mount_point;

	if (S_ISDIR(source_sb->st_mode)) {
		if (is_mount_point(source, source_sb, &mount_point) != 0)
			return (-1);
		if (mount_point) {
			error_msg("cannot move mount point across filesystems: %s",
			    source);
			return (-1);
		}
	}

	if (target_sb != NULL && remove_target_path(target, target_sb) != 0)
		return (-1);

	if (copy_node(source, target, source_sb) != 0) {
		(void)cleanup_failed_target(target);
		return (-1);
	}

	if (remove_source_tree(source, source_sb) != 0)
		return (-1);

	if (options->verbose)
		(void)printf("%s -> %s\n", source, target);
	return (0);
}

static int
handle_single_move(const struct mv_options *options, const char *source,
    const char *target)
{
	struct stat source_sb;
	struct stat target_sb;
	bool target_exists;
	bool skip_move;

	if (lstat(source, &source_sb) != 0) {
		error_errno("%s", source);
		return (MV_EXIT_ERROR);
	}

	target_exists = false;
	if (lstat(target, &target_sb) == 0)
		target_exists = true;
	else if (errno != ENOENT) {
		error_errno("%s", target);
		return (MV_EXIT_ERROR);
	}

	if (target_exists) {
		if (apply_existing_target_policy(options, target, &skip_move) != 0)
			return (MV_EXIT_ERROR);
		if (skip_move)
			return (0);

		if (S_ISDIR(source_sb.st_mode) && !S_ISDIR(target_sb.st_mode)) {
			errno = ENOTDIR;
			error_errno("%s", target);
			return (MV_EXIT_ERROR);
		}
		if (!S_ISDIR(source_sb.st_mode) && S_ISDIR(target_sb.st_mode)) {
			errno = EISDIR;
			error_errno("%s", target);
			return (MV_EXIT_ERROR);
		}
	}

	if (rename(source, target) == 0) {
		if (options->verbose)
			(void)printf("%s -> %s\n", source, target);
		return (0);
	}

	if (errno != EXDEV) {
		error_errno("rename %s to %s", source, target);
		return (MV_EXIT_ERROR);
	}

	if (copy_move_fallback(options, source, target, &source_sb,
	    target_exists ? &target_sb : NULL) != 0)
		return (MV_EXIT_ERROR);

	return (0);
}

static int
is_dir_symlink(const char *path)
{
	struct stat lst;
	struct stat st;

	if (lstat(path, &lst) != 0)
		return (0);
	if (!S_ISLNK(lst.st_mode))
		return (0);
	if (stat(path, &st) != 0)
		return (0);
	return (S_ISDIR(st.st_mode) ? 1 : 0);
}

static int
determine_target_mode(const struct mv_options *options, int argc, char *const argv[],
    struct move_target *target)
{
	struct stat sb;
	int stat_rc;
	bool treat_as_directory;

	treat_as_directory = false;
	stat_rc = stat(argv[argc - 1], &sb);
	if (!(options->no_target_dir_follow && argc == 2 &&
	    is_dir_symlink(argv[argc - 1])) &&
	    stat_rc == 0 && S_ISDIR(sb.st_mode))
		treat_as_directory = true;

	if (!treat_as_directory && argc > 2) {
		if (stat_rc != 0 && errno != ENOENT)
			error_errno("%s", argv[argc - 1]);
		else
			error_msg("%s is not a directory", argv[argc - 1]);
		return (-1);
	}

	target->treat_as_directory = treat_as_directory;
	target->path = argv[argc - 1];
	return (0);
}

int
main(int argc, char *argv[])
{
	struct move_target target;
	struct mv_options options;
	int ch;
	int rval;

	progname = program_name(argv[0]);
	memset(&options, 0, sizeof(options));

	while ((ch = getopt(argc, argv, "+fhinv")) != -1) {
		switch (ch) {
		case 'f':
			options.force = true;
			options.interactive = false;
			options.no_clobber = false;
			break;
		case 'h':
			options.no_target_dir_follow = true;
			break;
		case 'i':
			options.interactive = true;
			options.force = false;
			options.no_clobber = false;
			break;
		case 'n':
			options.no_clobber = true;
			options.force = false;
			options.interactive = false;
			break;
		case 'v':
			options.verbose = true;
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;
	if (argc < 2)
		usage();
	if (options.no_target_dir_follow && argc != 2)
		usage();
	if (determine_target_mode(&options, argc, argv, &target) != 0)
		return (MV_EXIT_ERROR);

	if (!target.treat_as_directory)
		return (handle_single_move(&options, argv[0], target.path));

	rval = 0;
	for (int i = 0; i < argc - 1; i++) {
		char *child_target;

		append_child_basename(target.path, argv[i], &child_target);
		if (handle_single_move(&options, argv[i], child_target) != 0)
			rval = MV_EXIT_ERROR;
		free(child_target);
	}
	return (rval);
}
