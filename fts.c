/*-
 * SPDX-License-Identifier: BSD-3-Clause
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

#include "fts.h"

#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

enum {
	FTS_STATE_PRE = 0,
	FTS_STATE_CHILDREN,
	FTS_STATE_DNR,
	FTS_STATE_POST,
	FTS_STATE_DONE,
};

static FTSENT *fts_make_entry(FTS *ftsp, FTSENT *parent, const char *path,
    int level, int is_root);
static int fts_prepare_children(FTS *ftsp, FTSENT *parent);
static FTSENT *fts_advance(FTS *ftsp, FTSENT *ent);
static void fts_free_entry(FTSENT *ent);
static const char *fts_basename(const char *path);
static char *fts_join_path(const char *parent, const char *name);
static int fts_should_follow(const FTS *ftsp, int is_root);
static int fts_is_cycle(const FTSENT *parent, const struct stat *st);
static const FTSENT *fts_root(const FTSENT *ent);

static const char *
fts_basename(const char *path)
{
	const char *end;
	const char *base;

	end = path + strlen(path);
	while (end > path + 1 && end[-1] == '/')
		end--;
	base = end;
	while (base > path && base[-1] != '/')
		base--;
	if (base == end)
		return ("/");
	return (base);
}

static char *
fts_join_path(const char *parent, const char *name)
{
	size_t parent_len, name_len;
	int need_sep;
	char *path;

	parent_len = strlen(parent);
	name_len = strlen(name);
	need_sep = parent_len > 0 && parent[parent_len - 1] != '/';
	path = malloc(parent_len + need_sep + name_len + 1);
	if (path == NULL)
		return (NULL);
	memcpy(path, parent, parent_len);
	if (need_sep)
		path[parent_len++] = '/';
	memcpy(path + parent_len, name, name_len + 1);
	return (path);
}

static int
fts_should_follow(const FTS *ftsp, int is_root)
{
	if (ftsp->fts_options & FTS_LOGICAL)
		return (1);
	if (is_root && (ftsp->fts_options & FTS_COMFOLLOW))
		return (1);
	return (0);
}

static const FTSENT *
fts_root(const FTSENT *ent)
{
	while (ent != NULL && ent->fts_parent != NULL)
		ent = ent->fts_parent;
	return (ent);
}

static int
fts_is_cycle(const FTSENT *parent, const struct stat *st)
{
	const FTSENT *cur;

	for (cur = parent; cur != NULL; cur = cur->fts_parent) {
		if (cur->fts_statp != NULL &&
		    cur->fts_statp->st_dev == st->st_dev &&
		    cur->fts_statp->st_ino == st->st_ino)
			return (1);
	}
	return (0);
}

static FTSENT *
fts_make_entry(FTS *ftsp, FTSENT *parent, const char *path, int level, int is_root)
{
	FTSENT *ent;
	struct stat st;
	int follow;

	ent = calloc(1, sizeof(*ent));
	if (ent == NULL)
		return (NULL);
	ent->fts_path = strdup(path);
	ent->fts_accpath = ent->fts_path;
	ent->fts_name = strdup(fts_basename(path));
	ent->fts_parent = parent;
	ent->fts_level = level;
	ent->fts_statp = malloc(sizeof(*ent->fts_statp));
	if (ent->fts_path == NULL || ent->fts_name == NULL || ent->fts_statp == NULL) {
		fts_free_entry(ent);
		return (NULL);
	}

	follow = fts_should_follow(ftsp, is_root);
	if ((follow ? stat(path, &st) : lstat(path, &st)) != 0) {
		if (lstat(path, &st) == 0 && S_ISLNK(st.st_mode)) {
			ent->fts_info = FTS_SL;
			*ent->fts_statp = st;
		} else {
			ent->fts_info = FTS_NS;
			ent->fts_errno = errno;
			memset(ent->fts_statp, 0, sizeof(*ent->fts_statp));
		}
		return (ent);
	}

	*ent->fts_statp = st;
	if (S_ISDIR(st.st_mode)) {
		if (fts_is_cycle(parent, &st)) {
			ent->fts_info = FTS_DC;
		} else {
			const FTSENT *root;

			ent->fts_info = FTS_D;
			root = fts_root(ent);
			if ((ftsp->fts_options & FTS_XDEV) != 0 && root != NULL &&
			    root->fts_statp != NULL && root != ent &&
			    root->fts_statp->st_dev != st.st_dev)
				ent->fts_instr = FTS_SKIP;
		}
	} else if (S_ISLNK(st.st_mode) && !follow) {
		ent->fts_info = FTS_SL;
	} else {
		ent->fts_info = FTS_F;
	}

	return (ent);
}

static int
fts_prepare_children(FTS *ftsp, FTSENT *parent)
{
	struct dirent *dp;
	DIR *dirp;
	char **names;
	char *child_path;
	FTSENT *child;
	size_t cap, count, i;

	dirp = opendir(parent->fts_accpath);
	if (dirp == NULL) {
		parent->fts_errno = errno;
		parent->fts_info = FTS_DNR;
		parent->_state = FTS_STATE_DNR;
		return (-1);
	}

	names = NULL;
	cap = count = 0;
	while ((dp = readdir(dirp)) != NULL) {
		char *name;

		if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
			continue;
		if (count == cap) {
			size_t new_cap;
			char **new_names;

			new_cap = cap == 0 ? 16 : cap * 2;
			new_names = realloc(names, new_cap * sizeof(*names));
			if (new_names == NULL)
				goto fail;
			names = new_names;
			cap = new_cap;
		}
		name = strdup(dp->d_name);
		if (name == NULL)
			goto fail;
		names[count++] = name;
	}
	closedir(dirp);

	if (count == 0) {
		free(names);
		parent->_children = NULL;
		parent->_child_count = 0;
		parent->_child_index = 0;
		return (0);
	}

	if (ftsp->fts_compar != NULL) {
		FTSENT **cmp_entries;

		cmp_entries = calloc(count, sizeof(*cmp_entries));
		if (cmp_entries == NULL)
			goto fail_names;
		for (i = 0; i < count; i++) {
			child_path = fts_join_path(parent->fts_path, names[i]);
			if (child_path == NULL) {
				while (i > 0)
					fts_free_entry(cmp_entries[--i]);
				free(cmp_entries);
				goto fail_names;
			}
			cmp_entries[i] = fts_make_entry(ftsp, parent, child_path,
			    parent->fts_level + 1, 0);
			free(child_path);
			if (cmp_entries[i] == NULL) {
				while (i > 0)
					fts_free_entry(cmp_entries[--i]);
				free(cmp_entries);
				goto fail_names;
			}
		}
		qsort(cmp_entries, count, sizeof(*cmp_entries),
		    (int (*)(const void *, const void *))ftsp->fts_compar);
		parent->_children = cmp_entries;
		parent->_child_count = count;
		parent->_child_index = 0;
		for (i = 0; i < count; i++)
			free(names[i]);
		free(names);
		return (0);
	}

	parent->_children = calloc(count, sizeof(*parent->_children));
	if (parent->_children == NULL)
		goto fail_names;
	for (i = 0; i < count; i++) {
		child_path = fts_join_path(parent->fts_path, names[i]);
		if (child_path == NULL)
			goto fail_children;
		child = fts_make_entry(ftsp, parent, child_path, parent->fts_level + 1, 0);
		free(child_path);
		if (child == NULL)
			goto fail_children;
		parent->_children[i] = child;
	}
	parent->_child_count = count;
	parent->_child_index = 0;
	for (i = 0; i < count; i++)
		free(names[i]);
	free(names);
	return (0);

fail_children:
	while (i > 0)
		fts_free_entry(parent->_children[--i]);
	free(parent->_children);
	parent->_children = NULL;
fail_names:
	for (i = 0; i < count; i++)
		free(names[i]);
	free(names);
	parent->fts_errno = ENOMEM;
	parent->fts_info = FTS_ERR;
	parent->_state = FTS_STATE_DONE;
	return (-1);
fail:
	closedir(dirp);
	free(names);
	parent->fts_errno = ENOMEM;
	parent->fts_info = FTS_ERR;
	parent->_state = FTS_STATE_DONE;
	return (-1);
}

static FTSENT *
fts_advance(FTS *ftsp, FTSENT *ent)
{
	FTSENT *parent;

	for (;;) {
		if (ent == NULL)
			break;

		if (ent->fts_info == FTS_D && ent->_state == FTS_STATE_PRE) {
			if (ent->fts_instr == FTS_SKIP || ent->fts_info == FTS_DC) {
				ent->fts_info = FTS_DP;
				ent->_state = FTS_STATE_POST;
				return (ent);
			}
			if (fts_prepare_children(ftsp, ent) != 0) {
				if (ent->fts_info == FTS_DNR || ent->fts_info == FTS_ERR)
					return (ent);
			}
			ent->_state = FTS_STATE_CHILDREN;
			if (ent->_child_count > 0)
				return (ent->_children[ent->_child_index++]);
			ent->fts_info = FTS_DP;
			ent->_state = FTS_STATE_POST;
			return (ent);
		}

		if (ent->_state == FTS_STATE_DNR) {
			ent->fts_info = FTS_DP;
			ent->_state = FTS_STATE_POST;
			return (ent);
		}

		if (ent->_state == FTS_STATE_CHILDREN) {
			if (ent->_child_index < ent->_child_count)
				return (ent->_children[ent->_child_index++]);
			ent->fts_info = FTS_DP;
			ent->_state = FTS_STATE_POST;
			return (ent);
		}

		ent->_state = FTS_STATE_DONE;
		parent = ent->fts_parent;
		if (parent != NULL) {
			ent = parent;
			continue;
		}
		if (++ftsp->_root_index < ftsp->_root_count)
			return (ftsp->_roots[ftsp->_root_index]);
		break;
	}

	return (NULL);
}

FTS *
fts_open(char * const *paths, int options,
    int (*compar)(const FTSENT * const *, const FTSENT * const *))
{
	FTS *ftsp;
	size_t count, i;

	count = 0;
	while (paths[count] != NULL)
		count++;

	ftsp = calloc(1, sizeof(*ftsp));
	if (ftsp == NULL)
		return (NULL);
	ftsp->_roots = calloc(count, sizeof(*ftsp->_roots));
	if (ftsp->_roots == NULL) {
		free(ftsp);
		return (NULL);
	}
	ftsp->_root_count = count;
	ftsp->fts_options = options;
	ftsp->fts_compar = compar;

	for (i = 0; i < count; i++) {
		ftsp->_roots[i] = fts_make_entry(ftsp, NULL, paths[i], FTS_ROOTLEVEL, 1);
		if (ftsp->_roots[i] == NULL) {
			while (i > 0)
				fts_free_entry(ftsp->_roots[--i]);
			free(ftsp->_roots);
			free(ftsp);
			return (NULL);
		}
	}

	return (ftsp);
}

FTSENT *
fts_read(FTS *ftsp)
{
	if (ftsp == NULL || ftsp->_root_count == 0)
		return (NULL);
	if (ftsp->_current == NULL) {
		ftsp->_root_index = 0;
		ftsp->_current = ftsp->_roots[0];
		return (ftsp->_current);
	}
	ftsp->_current = fts_advance(ftsp, ftsp->_current);
	if (ftsp->_current == NULL)
		errno = 0;
	return (ftsp->_current);
}

int
fts_set(FTS *ftsp, FTSENT *f, int instr)
{
	(void)ftsp;
	if (f == NULL)
		return (-1);
	f->fts_instr = instr;
	return (0);
}

static void
fts_free_entry(FTSENT *ent)
{
	size_t i;

	if (ent == NULL)
		return;
	for (i = 0; i < ent->_child_count; i++)
		fts_free_entry(ent->_children[i]);
	free(ent->_children);
	free(ent->fts_statp);
	free(ent->fts_name);
	free(ent->fts_path);
	free(ent);
}

int
fts_close(FTS *ftsp)
{
	size_t i;

	if (ftsp == NULL)
		return (0);
	for (i = 0; i < ftsp->_root_count; i++)
		fts_free_entry(ftsp->_roots[i]);
	free(ftsp->_roots);
	free(ftsp);
	return (0);
}
