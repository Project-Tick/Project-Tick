/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2001 Chris D. Faulhaber
 * All rights reserved.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * Linux-native setfacl implementation.
 *
 * FreeBSD's original utility depends on sys/acl.h, acl_*_np(), and FTS.
 * This port applies POSIX ACLs by reading and writing Linux ACL xattrs
 * directly:
 *   - system.posix_acl_access
 *   - system.posix_acl_default
 *
 * NFSv4 ACL manipulation has no Linux POSIX ACL equivalent here and is
 * rejected explicitly instead of emulating partial behavior.
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/xattr.h>

#include <ctype.h>
#include <dirent.h>
#include <endian.h>
#include <errno.h>
#include <getopt.h>
#include <grp.h>
#include <inttypes.h>
#include <limits.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef le16toh
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define le16toh(x) (x)
#define le32toh(x) (x)
#define htole16(x) (x)
#define htole32(x) (x)
#else
#define le16toh(x) __builtin_bswap16(x)
#define le32toh(x) __builtin_bswap32(x)
#define htole16(x) __builtin_bswap16(x)
#define htole32(x) __builtin_bswap32(x)
#endif
#endif

#ifndef ENOATTR
#define ENOATTR ENODATA
#endif

#define ACL_XATTR_ACCESS "system.posix_acl_access"
#define ACL_XATTR_DEFAULT "system.posix_acl_default"

#define POSIX_ACL_XATTR_VERSION 0x0002U
#define ACL_UNDEFINED_ID ((uint32_t)-1)

#define ACL_READ 0x04U
#define ACL_WRITE 0x02U
#define ACL_EXECUTE 0x01U

#define ACL_USER_OBJ 0x01U
#define ACL_USER 0x02U
#define ACL_GROUP_OBJ 0x04U
#define ACL_GROUP 0x08U
#define ACL_MASK 0x10U
#define ACL_OTHER 0x20U

struct posix_acl_xattr_entry_linux {
	uint16_t e_tag;
	uint16_t e_perm;
	uint32_t e_id;
};

struct posix_acl_xattr_header_linux {
	uint32_t a_version;
};

enum acl_kind {
	ACL_KIND_ACCESS,
	ACL_KIND_DEFAULT,
};

enum op_type {
	OP_MODIFY,
	OP_REMOVE_SPEC,
	OP_REMOVE_INDEX,
	OP_REMOVE_ALL,
	OP_REMOVE_DEFAULT,
};

enum walk_mode {
	WALK_PHYSICAL,
	WALK_LOGICAL,
	WALK_HYBRID,
};

struct acl_entry_linux {
	uint16_t tag;
	uint16_t perm;
	uint32_t id;
};

struct acl_list {
	struct acl_entry_linux *entries;
	size_t count;
	size_t cap;
};

struct acl_spec {
	struct acl_list acl;
	bool explicit_mask;
};

struct operation {
	enum op_type type;
	enum acl_kind kind;
	struct acl_spec spec;
	size_t index;
};

struct operation_list {
	struct operation *ops;
	size_t count;
	size_t cap;
};

struct options {
	bool no_mask;
	bool recursive;
	bool no_follow;
	enum walk_mode walk_mode;
};

struct parser_state {
	enum acl_kind current_kind;
	bool stdin_reserved;
	bool stdin_path_error;
};

struct visited_dir {
	dev_t dev;
	ino_t ino;
};

struct visited_set {
	struct visited_dir *items;
	size_t count;
	size_t cap;
};

struct file_state {
	const char *path;
	bool follow;
	bool is_dir;
	struct stat st;
	bool access_loaded;
	bool access_dirty;
	struct acl_list access_acl;
	bool default_loaded;
	bool default_dirty;
	struct acl_list default_acl;
};

static const struct option long_options[] = {
	{ NULL, 0, NULL, 0 },
};

static void usage(void) __attribute__((noreturn));

static void *xcalloc(size_t nmemb, size_t size);
static void *xreallocarray(void *ptr, size_t nmemb, size_t size);
static char *xstrdup(const char *s);
static void report_error(const char *fmt, ...);
static void report_errno(const char *path);
static void report_path_error(const char *path, const char *fmt, ...);

static void acl_list_init(struct acl_list *acl);
static void acl_list_free(struct acl_list *acl);
static void acl_list_clear(struct acl_list *acl);
static void acl_list_push(struct acl_list *acl,
    const struct acl_entry_linux *entry);
static void acl_list_remove_at(struct acl_list *acl, size_t index);
static void op_list_init(struct operation_list *ops);
static void op_list_free(struct operation_list *ops);
static void op_list_push(struct operation_list *ops, const struct operation *op);

static void visited_init(struct visited_set *visited);
static void visited_free(struct visited_set *visited);
static bool visited_contains(const struct visited_set *visited, dev_t dev,
    ino_t ino);
static void visited_add(struct visited_set *visited, dev_t dev, ino_t ino);

static const char *xattr_name(enum acl_kind kind);
static int tag_sort_rank(uint16_t tag);
static int compare_acl_entries(const void *lhs, const void *rhs);
static void acl_sort(struct acl_list *acl);
static bool acl_has_named_entries(const struct acl_list *acl);
static bool acl_has_mask(const struct acl_list *acl);
static bool acl_is_base_only(const struct acl_list *acl);
static struct acl_entry_linux *acl_find_entry(struct acl_list *acl, uint16_t tag,
    uint32_t id);
static const struct acl_entry_linux *acl_find_entry_const(const struct acl_list *acl,
    uint16_t tag, uint32_t id);
static struct acl_entry_linux *acl_find_single(struct acl_list *acl, uint16_t tag);
static const struct acl_entry_linux *acl_find_single_const(const struct acl_list *acl,
    uint16_t tag);

static int validate_acl(const struct acl_list *acl, enum acl_kind kind,
    char *errbuf, size_t errbufsz);
static void synthesize_access_acl(mode_t mode, struct acl_list *acl);
static mode_t access_acl_to_mode(mode_t existing_mode, const struct acl_list *acl);
static void strip_access_acl(struct acl_list *acl);
static void recalculate_mask(struct acl_list *acl);

static char *trim_whitespace(char *s);
static bool looks_like_nfs4_acl(const char *text);
static int parse_perm_string(const char *text, uint16_t *perm_out,
    char *errbuf, size_t errbufsz);
static int resolve_user(const char *text, uint32_t *id_out, char *errbuf,
    size_t errbufsz);
static int resolve_group(const char *text, uint32_t *id_out, char *errbuf,
    size_t errbufsz);
static int parse_acl_entry(const char *text, enum acl_kind kind, bool for_remove,
    struct acl_entry_linux *entry_out, bool *has_perm_out, char *errbuf,
    size_t errbufsz);
static int parse_acl_text_list(const char *text, enum acl_kind kind, bool for_remove,
    struct acl_spec *spec, char *errbuf, size_t errbufsz);
static int parse_acl_file(const char *filename, enum acl_kind kind, bool for_remove,
    struct parser_state *parser, struct acl_spec *spec, char *errbuf,
    size_t errbufsz);

static int stat_path(const char *path, bool follow, struct stat *st);
static int load_xattr_blob(const char *path, enum acl_kind kind, bool follow,
    void **buf_out, size_t *size_out);
static int parse_acl_blob(const void *buf, size_t size, enum acl_kind kind,
    struct acl_list *acl, char *errbuf, size_t errbufsz);
static int encode_acl_blob(const struct acl_list *acl, void **buf_out,
    size_t *size_out);

static int load_acl_kind(struct file_state *state, enum acl_kind kind,
    char *errbuf, size_t errbufsz);
static int persist_access_acl(struct file_state *state, char *errbuf,
    size_t errbufsz);
static int persist_default_acl(struct file_state *state, char *errbuf,
    size_t errbufsz);
static int persist_file_state(struct file_state *state, char *errbuf,
    size_t errbufsz);

static int apply_modify(struct acl_list *acl, const struct acl_spec *spec);
static int apply_remove_spec(struct acl_list *acl, const struct acl_spec *spec,
    char *errbuf, size_t errbufsz);
static int apply_remove_index(struct acl_list *acl, size_t index, char *errbuf,
    size_t errbufsz);
static int finalize_acl_after_change(struct acl_list *acl, enum acl_kind kind,
    bool explicit_mask, bool no_mask, char *errbuf, size_t errbufsz);
static int apply_operation(struct file_state *state, const struct operation *op,
    const struct options *opts, char *errbuf, size_t errbufsz);
static int process_single_path(const char *path, bool follow,
    const struct operation_list *ops, const struct options *opts);
static int process_path_recursive(const char *path, bool follow_root,
    bool follow_children, const struct operation_list *ops,
    const struct options *opts, struct visited_set *visited);

static char **read_path_operands_from_stdin(struct parser_state *parser);
static int process_operand(const char *path, const struct operation_list *ops,
    const struct options *opts, struct visited_set *visited);

static void
usage(void)
{

	fprintf(stderr,
	    "usage: setfacl [-R [-H | -L | -P]] [-bdhkn] "
	    "[-a position entries] [-m entries] [-M file] "
	    "[-x entries | position] [-X file] [file ...]\n");
	exit(1);
}

static void *
xcalloc(size_t nmemb, size_t size)
{
	void *ptr;

	if (nmemb != 0 && size > SIZE_MAX / nmemb) {
		report_error("setfacl: allocation overflow");
		exit(1);
	}
	ptr = calloc(nmemb, size);
	if (ptr == NULL) {
		report_errno("calloc");
		exit(1);
	}
	return (ptr);
}

static void *
xreallocarray(void *ptr, size_t nmemb, size_t size)
{
	void *newptr;

	if (nmemb != 0 && size > SIZE_MAX / nmemb) {
		report_error("setfacl: allocation overflow");
		exit(1);
	}
	newptr = realloc(ptr, nmemb * size);
	if (newptr == NULL) {
		report_errno("realloc");
		exit(1);
	}
	return (newptr);
}

static char *
xstrdup(const char *s)
{
	char *copy;

	copy = strdup(s);
	if (copy == NULL) {
		report_errno("strdup");
		exit(1);
	}
	return (copy);
}

static void
report_error(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
}

static void
report_errno(const char *path)
{

	fprintf(stderr, "setfacl: %s: %s\n", path, strerror(errno));
}

static void
report_path_error(const char *path, const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "setfacl: %s: ", path);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
}

static void
acl_list_init(struct acl_list *acl)
{

	acl->entries = NULL;
	acl->count = 0;
	acl->cap = 0;
}

static void
acl_list_free(struct acl_list *acl)
{

	free(acl->entries);
	acl->entries = NULL;
	acl->count = 0;
	acl->cap = 0;
}

static void
acl_list_clear(struct acl_list *acl)
{

	acl->count = 0;
}

static void
acl_list_push(struct acl_list *acl, const struct acl_entry_linux *entry)
{

	if (acl->count == acl->cap) {
		size_t newcap;

		newcap = (acl->cap == 0) ? 8 : acl->cap * 2;
		acl->entries = xreallocarray(acl->entries, newcap,
		    sizeof(*acl->entries));
		acl->cap = newcap;
	}
	acl->entries[acl->count++] = *entry;
}

static void
acl_list_remove_at(struct acl_list *acl, size_t index)
{

	if (index + 1 < acl->count) {
		memmove(&acl->entries[index], &acl->entries[index + 1],
		    (acl->count - index - 1) * sizeof(*acl->entries));
	}
	acl->count--;
}

static void
op_list_init(struct operation_list *ops)
{

	ops->ops = NULL;
	ops->count = 0;
	ops->cap = 0;
}

static void
op_list_free(struct operation_list *ops)
{
	size_t i;

	for (i = 0; i < ops->count; i++)
		acl_list_free(&ops->ops[i].spec.acl);
	free(ops->ops);
	ops->ops = NULL;
	ops->count = 0;
	ops->cap = 0;
}

static void
op_list_push(struct operation_list *ops, const struct operation *op)
{

	if (ops->count == ops->cap) {
		size_t newcap;

		newcap = (ops->cap == 0) ? 8 : ops->cap * 2;
		ops->ops = xreallocarray(ops->ops, newcap, sizeof(*ops->ops));
		ops->cap = newcap;
	}
	ops->ops[ops->count++] = *op;
}

static void
visited_init(struct visited_set *visited)
{

	visited->items = NULL;
	visited->count = 0;
	visited->cap = 0;
}

static void
visited_free(struct visited_set *visited)
{

	free(visited->items);
	visited->items = NULL;
	visited->count = 0;
	visited->cap = 0;
}

static bool
visited_contains(const struct visited_set *visited, dev_t dev, ino_t ino)
{
	size_t i;

	for (i = 0; i < visited->count; i++) {
		if (visited->items[i].dev == dev && visited->items[i].ino == ino)
			return (true);
	}
	return (false);
}

static void
visited_add(struct visited_set *visited, dev_t dev, ino_t ino)
{

	if (visited->count == visited->cap) {
		size_t newcap;

		newcap = (visited->cap == 0) ? 16 : visited->cap * 2;
		visited->items = xreallocarray(visited->items, newcap,
		    sizeof(*visited->items));
		visited->cap = newcap;
	}
	visited->items[visited->count].dev = dev;
	visited->items[visited->count].ino = ino;
	visited->count++;
}

static const char *
xattr_name(enum acl_kind kind)
{

	return (kind == ACL_KIND_DEFAULT) ? ACL_XATTR_DEFAULT : ACL_XATTR_ACCESS;
}

static int
tag_sort_rank(uint16_t tag)
{

	switch (tag) {
	case ACL_USER_OBJ:
		return (0);
	case ACL_USER:
		return (1);
	case ACL_GROUP_OBJ:
		return (2);
	case ACL_GROUP:
		return (3);
	case ACL_MASK:
		return (4);
	case ACL_OTHER:
		return (5);
	default:
		return (6);
	}
}

static int
compare_acl_entries(const void *lhs, const void *rhs)
{
	const struct acl_entry_linux *a;
	const struct acl_entry_linux *b;
	int rank_a;
	int rank_b;

	a = lhs;
	b = rhs;
	rank_a = tag_sort_rank(a->tag);
	rank_b = tag_sort_rank(b->tag);
	if (rank_a != rank_b)
		return (rank_a < rank_b ? -1 : 1);
	if (a->id < b->id)
		return (-1);
	if (a->id > b->id)
		return (1);
	return (0);
}

static void
acl_sort(struct acl_list *acl)
{

	if (acl->count > 1)
		qsort(acl->entries, acl->count, sizeof(*acl->entries),
		    compare_acl_entries);
}

static bool
acl_has_named_entries(const struct acl_list *acl)
{
	size_t i;

	for (i = 0; i < acl->count; i++) {
		if (acl->entries[i].tag == ACL_USER || acl->entries[i].tag == ACL_GROUP)
			return (true);
	}
	return (false);
}

static bool
acl_has_mask(const struct acl_list *acl)
{

	return (acl_find_single_const(acl, ACL_MASK) != NULL);
}

static bool
acl_is_base_only(const struct acl_list *acl)
{
	size_t i;

	for (i = 0; i < acl->count; i++) {
		switch (acl->entries[i].tag) {
		case ACL_USER_OBJ:
		case ACL_GROUP_OBJ:
		case ACL_OTHER:
			break;
		default:
			return (false);
		}
	}
	return (true);
}

static struct acl_entry_linux *
acl_find_entry(struct acl_list *acl, uint16_t tag, uint32_t id)
{
	size_t i;

	for (i = 0; i < acl->count; i++) {
		if (acl->entries[i].tag == tag && acl->entries[i].id == id)
			return (&acl->entries[i]);
	}
	return (NULL);
}

static const struct acl_entry_linux *
acl_find_entry_const(const struct acl_list *acl, uint16_t tag, uint32_t id)
{
	size_t i;

	for (i = 0; i < acl->count; i++) {
		if (acl->entries[i].tag == tag && acl->entries[i].id == id)
			return (&acl->entries[i]);
	}
	return (NULL);
}

static struct acl_entry_linux *
acl_find_single(struct acl_list *acl, uint16_t tag)
{

	return (acl_find_entry(acl, tag, ACL_UNDEFINED_ID));
}

static const struct acl_entry_linux *
acl_find_single_const(const struct acl_list *acl, uint16_t tag)
{

	return (acl_find_entry_const(acl, tag, ACL_UNDEFINED_ID));
}

static int
validate_acl(const struct acl_list *acl, enum acl_kind kind, char *errbuf,
    size_t errbufsz)
{
	size_t i;
	bool saw_user_obj;
	bool saw_group_obj;
	bool saw_other;
	bool saw_mask;
	bool saw_named_user;
	bool saw_named_group;

	if (kind == ACL_KIND_DEFAULT && acl->count == 0)
		return (0);
	if (kind == ACL_KIND_ACCESS && acl->count == 0) {
		snprintf(errbuf, errbufsz,
		    "access ACL cannot be empty");
		return (-1);
	}

	saw_user_obj = false;
	saw_group_obj = false;
	saw_other = false;
	saw_mask = false;
	saw_named_user = false;
	saw_named_group = false;

	for (i = 0; i < acl->count; i++) {
		const struct acl_entry_linux *entry;

		entry = &acl->entries[i];
		if ((entry->perm & ~(ACL_READ | ACL_WRITE | ACL_EXECUTE)) != 0) {
			snprintf(errbuf, errbufsz,
			    "ACL contains invalid permission bits");
			return (-1);
		}
		if (i > 0 &&
		    compare_acl_entries(&acl->entries[i - 1], &acl->entries[i]) == 0) {
			snprintf(errbuf, errbufsz,
			    "ACL contains duplicate entries");
			return (-1);
		}

		switch (entry->tag) {
		case ACL_USER_OBJ:
			saw_user_obj = true;
			break;
		case ACL_USER:
			saw_named_user = true;
			break;
		case ACL_GROUP_OBJ:
			saw_group_obj = true;
			break;
		case ACL_GROUP:
			saw_named_group = true;
			break;
		case ACL_MASK:
			saw_mask = true;
			break;
		case ACL_OTHER:
			saw_other = true;
			break;
		default:
			snprintf(errbuf, errbufsz,
			    "ACL contains unsupported tag 0x%x", entry->tag);
			return (-1);
		}
	}

	if (!saw_user_obj) {
		snprintf(errbuf, errbufsz, "ACL is missing required user:: entry");
		return (-1);
	}
	if (!saw_group_obj) {
		snprintf(errbuf, errbufsz, "ACL is missing required group:: entry");
		return (-1);
	}
	if (!saw_other) {
		snprintf(errbuf, errbufsz, "ACL is missing required other:: entry");
		return (-1);
	}
	if ((saw_named_user || saw_named_group) && !saw_mask) {
		snprintf(errbuf, errbufsz,
		    "ACL with named user/group entries requires a mask entry");
		return (-1);
	}

	return (0);
}

static void
synthesize_access_acl(mode_t mode, struct acl_list *acl)
{
	struct acl_entry_linux entry;

	acl_list_clear(acl);

	entry.tag = ACL_USER_OBJ;
	entry.id = ACL_UNDEFINED_ID;
	entry.perm = ((mode & S_IRUSR) ? ACL_READ : 0) |
	    ((mode & S_IWUSR) ? ACL_WRITE : 0) |
	    ((mode & S_IXUSR) ? ACL_EXECUTE : 0);
	acl_list_push(acl, &entry);

	entry.tag = ACL_GROUP_OBJ;
	entry.perm = ((mode & S_IRGRP) ? ACL_READ : 0) |
	    ((mode & S_IWGRP) ? ACL_WRITE : 0) |
	    ((mode & S_IXGRP) ? ACL_EXECUTE : 0);
	acl_list_push(acl, &entry);

	entry.tag = ACL_OTHER;
	entry.perm = ((mode & S_IROTH) ? ACL_READ : 0) |
	    ((mode & S_IWOTH) ? ACL_WRITE : 0) |
	    ((mode & S_IXOTH) ? ACL_EXECUTE : 0);
	acl_list_push(acl, &entry);
}

static mode_t
access_acl_to_mode(mode_t existing_mode, const struct acl_list *acl)
{
	const struct acl_entry_linux *user_obj;
	const struct acl_entry_linux *group_obj;
	const struct acl_entry_linux *other;
	mode_t mode;

	user_obj = acl_find_single_const(acl, ACL_USER_OBJ);
	group_obj = acl_find_single_const(acl, ACL_GROUP_OBJ);
	other = acl_find_single_const(acl, ACL_OTHER);

	mode = existing_mode & ~0777;
	if (user_obj->perm & ACL_READ)
		mode |= S_IRUSR;
	if (user_obj->perm & ACL_WRITE)
		mode |= S_IWUSR;
	if (user_obj->perm & ACL_EXECUTE)
		mode |= S_IXUSR;
	if (group_obj->perm & ACL_READ)
		mode |= S_IRGRP;
	if (group_obj->perm & ACL_WRITE)
		mode |= S_IWGRP;
	if (group_obj->perm & ACL_EXECUTE)
		mode |= S_IXGRP;
	if (other->perm & ACL_READ)
		mode |= S_IROTH;
	if (other->perm & ACL_WRITE)
		mode |= S_IWOTH;
	if (other->perm & ACL_EXECUTE)
		mode |= S_IXOTH;
	return (mode);
}

static void
strip_access_acl(struct acl_list *acl)
{
	struct acl_entry_linux *group_obj;
	const struct acl_entry_linux *mask;
	uint16_t group_perm;
	size_t i;

	group_obj = acl_find_single(acl, ACL_GROUP_OBJ);
	mask = acl_find_single_const(acl, ACL_MASK);
	group_perm = (group_obj != NULL) ? group_obj->perm : 0;
	if (mask != NULL)
		group_perm &= mask->perm;

	for (i = 0; i < acl->count;) {
		switch (acl->entries[i].tag) {
		case ACL_USER_OBJ:
		case ACL_GROUP_OBJ:
		case ACL_OTHER:
			i++;
			break;
		default:
			acl_list_remove_at(acl, i);
			break;
		}
	}
	group_obj = acl_find_single(acl, ACL_GROUP_OBJ);
	if (group_obj != NULL)
		group_obj->perm = group_perm;
	acl_sort(acl);
}

static void
recalculate_mask(struct acl_list *acl)
{
	struct acl_entry_linux *mask;
	const struct acl_entry_linux *group_obj;
	uint16_t union_perm;
	size_t i;

	group_obj = acl_find_single_const(acl, ACL_GROUP_OBJ);
	if (group_obj == NULL)
		return;

	union_perm = group_obj->perm;
	for (i = 0; i < acl->count; i++) {
		if (acl->entries[i].tag == ACL_USER || acl->entries[i].tag == ACL_GROUP)
			union_perm |= acl->entries[i].perm;
	}

	mask = acl_find_single(acl, ACL_MASK);
	if (mask == NULL) {
		struct acl_entry_linux entry;

		entry.tag = ACL_MASK;
		entry.id = ACL_UNDEFINED_ID;
		entry.perm = union_perm;
		acl_list_push(acl, &entry);
	} else {
		mask->perm = union_perm;
	}
	acl_sort(acl);
}

static char *
trim_whitespace(char *s)
{
	char *end;

	while (*s != '\0' && isspace((unsigned char)*s))
		s++;
	end = s + strlen(s);
	while (end > s && isspace((unsigned char)end[-1]))
		end--;
	*end = '\0';
	return (s);
}

static bool
looks_like_nfs4_acl(const char *text)
{
	size_t colon_count;
	const char *p;

	if (strstr(text, ":allow") != NULL || strstr(text, ":deny") != NULL)
		return (true);
	if (strchr(text, '@') != NULL)
		return (true);
	colon_count = 0;
	for (p = text; *p != '\0'; p++) {
		if (*p == ':')
			colon_count++;
	}
	return (colon_count > 2);
}

static int
parse_perm_string(const char *text, uint16_t *perm_out, char *errbuf,
    size_t errbufsz)
{
	uint16_t perm;
	size_t i;
	bool saw_r;
	bool saw_w;
	bool saw_x;

	if (*text == '\0') {
		snprintf(errbuf, errbufsz, "missing ACL permissions");
		return (-1);
	}

	perm = 0;
	saw_r = false;
	saw_w = false;
	saw_x = false;
	for (i = 0; text[i] != '\0'; i++) {
		switch (text[i]) {
		case 'r':
			if (saw_r) {
				snprintf(errbuf, errbufsz,
				    "duplicate ACL permission 'r'");
				return (-1);
			}
			saw_r = true;
			perm |= ACL_READ;
			break;
		case 'w':
			if (saw_w) {
				snprintf(errbuf, errbufsz,
				    "duplicate ACL permission 'w'");
				return (-1);
			}
			saw_w = true;
			perm |= ACL_WRITE;
			break;
		case 'x':
			if (saw_x) {
				snprintf(errbuf, errbufsz,
				    "duplicate ACL permission 'x'");
				return (-1);
			}
			saw_x = true;
			perm |= ACL_EXECUTE;
			break;
		case '-':
			break;
		default:
			snprintf(errbuf, errbufsz,
			    "invalid ACL permission character '%c'", text[i]);
			return (-1);
		}
	}

	*perm_out = perm;
	return (0);
}

static int
resolve_user(const char *text, uint32_t *id_out, char *errbuf, size_t errbufsz)
{
	struct passwd *pwd;
	char *end;
	unsigned long value;

	errno = 0;
	value = strtoul(text, &end, 10);
	if (*text != '\0' && *end == '\0' && errno == 0 && value <= UINT32_MAX) {
		*id_out = (uint32_t)value;
		return (0);
	}

	pwd = getpwnam(text);
	if (pwd == NULL) {
		snprintf(errbuf, errbufsz, "unknown user '%s'", text);
		return (-1);
	}
	*id_out = (uint32_t)pwd->pw_uid;
	return (0);
}

static int
resolve_group(const char *text, uint32_t *id_out, char *errbuf, size_t errbufsz)
{
	struct group *grp;
	char *end;
	unsigned long value;

	errno = 0;
	value = strtoul(text, &end, 10);
	if (*text != '\0' && *end == '\0' && errno == 0 && value <= UINT32_MAX) {
		*id_out = (uint32_t)value;
		return (0);
	}

	grp = getgrnam(text);
	if (grp == NULL) {
		snprintf(errbuf, errbufsz, "unknown group '%s'", text);
		return (-1);
	}
	*id_out = (uint32_t)grp->gr_gid;
	return (0);
}

static int
parse_acl_entry(const char *text, enum acl_kind kind, bool for_remove,
    struct acl_entry_linux *entry_out, bool *has_perm_out, char *errbuf,
    size_t errbufsz)
{
	char *copy;
	char *body;
	char *first;
	char *second;
	char *third;
	char *tag_text;
	char *qual_text;
	char *perm_text;
	int ret;

	copy = xstrdup(text);
	body = trim_whitespace(copy);
	if (strncmp(body, "default:", 8) == 0) {
		if (kind != ACL_KIND_DEFAULT) {
			snprintf(errbuf, errbufsz,
			    "default ACL entry requires -d");
			free(copy);
			return (-1);
		}
		body += 8;
		body = trim_whitespace(body);
	}

	if (looks_like_nfs4_acl(body)) {
		snprintf(errbuf, errbufsz,
		    "NFSv4 ACL entries are not supported on Linux");
		free(copy);
		return (-1);
	}

	first = strchr(body, ':');
	if (first == NULL) {
		snprintf(errbuf, errbufsz, "ACL entry '%s' is missing ':' fields",
		    body);
		free(copy);
		return (-1);
	}
	second = strchr(first + 1, ':');
	if (second == NULL && !for_remove) {
		snprintf(errbuf, errbufsz, "ACL entry '%s' is missing permission field",
		    body);
		free(copy);
		return (-1);
	}
	third = (second != NULL) ? strchr(second + 1, ':') : NULL;
	if (third != NULL) {
		snprintf(errbuf, errbufsz,
		    "NFSv4 ACL entries are not supported on Linux");
		free(copy);
		return (-1);
	}

	*first = '\0';
	if (second != NULL)
		*second = '\0';
	tag_text = trim_whitespace(body);
	qual_text = trim_whitespace(first + 1);
	perm_text = (second != NULL) ? trim_whitespace(second + 1) : (char *)"";

	if (strcmp(tag_text, "u") == 0 || strcmp(tag_text, "user") == 0) {
		if (*qual_text == '\0') {
			entry_out->tag = ACL_USER_OBJ;
			entry_out->id = ACL_UNDEFINED_ID;
		} else {
			entry_out->tag = ACL_USER;
			if (resolve_user(qual_text, &entry_out->id, errbuf, errbufsz) != 0) {
				free(copy);
				return (-1);
			}
		}
	} else if (strcmp(tag_text, "g") == 0 || strcmp(tag_text, "group") == 0) {
		if (*qual_text == '\0') {
			entry_out->tag = ACL_GROUP_OBJ;
			entry_out->id = ACL_UNDEFINED_ID;
		} else {
			entry_out->tag = ACL_GROUP;
			if (resolve_group(qual_text, &entry_out->id, errbuf, errbufsz) != 0) {
				free(copy);
				return (-1);
			}
		}
	} else if (strcmp(tag_text, "o") == 0 || strcmp(tag_text, "other") == 0) {
		if (*qual_text != '\0') {
			snprintf(errbuf, errbufsz,
			    "other:: ACL entry does not accept a qualifier");
			free(copy);
			return (-1);
		}
		entry_out->tag = ACL_OTHER;
		entry_out->id = ACL_UNDEFINED_ID;
	} else if (strcmp(tag_text, "m") == 0 || strcmp(tag_text, "mask") == 0) {
		if (*qual_text != '\0') {
			snprintf(errbuf, errbufsz,
			    "mask:: ACL entry does not accept a qualifier");
			free(copy);
			return (-1);
		}
		entry_out->tag = ACL_MASK;
		entry_out->id = ACL_UNDEFINED_ID;
	} else {
		snprintf(errbuf, errbufsz, "unsupported ACL tag '%s'", tag_text);
		free(copy);
		return (-1);
	}

	if (*perm_text == '\0') {
		if (!for_remove) {
			snprintf(errbuf, errbufsz,
			    "ACL entry '%s' is missing permissions", text);
			free(copy);
			return (-1);
		}
		*has_perm_out = false;
		entry_out->perm = 0;
		free(copy);
		return (0);
	}

	ret = parse_perm_string(perm_text, &entry_out->perm, errbuf, errbufsz);
	free(copy);
	if (ret != 0)
		return (-1);
	*has_perm_out = true;
	return (0);
}

static int
parse_acl_text_list(const char *text, enum acl_kind kind, bool for_remove,
    struct acl_spec *spec, char *errbuf, size_t errbufsz)
{
	char *copy;
	char *cursor;

	copy = xstrdup(text);
	cursor = copy;
	while (*cursor != '\0') {
		char *next;
		char *token;
		struct acl_entry_linux entry;
		bool has_perm;

		next = strchr(cursor, ',');
		if (next != NULL)
			*next = '\0';
		token = trim_whitespace(cursor);
		if (*token == '\0') {
			snprintf(errbuf, errbufsz, "empty ACL entry");
			free(copy);
			return (-1);
		}
		if (parse_acl_entry(token, kind, for_remove, &entry, &has_perm,
		    errbuf, errbufsz) != 0) {
			free(copy);
			return (-1);
		}
		acl_list_push(&spec->acl, &entry);
		if (entry.tag == ACL_MASK)
			spec->explicit_mask = true;
		if (next == NULL)
			break;
		cursor = next + 1;
	}
	free(copy);
	return (0);
}

static int
parse_acl_file(const char *filename, enum acl_kind kind, bool for_remove,
    struct parser_state *parser, struct acl_spec *spec, char *errbuf,
    size_t errbufsz)
{
	FILE *fp;
	char *line;
	size_t linecap;
	ssize_t linelen;

	if (strcmp(filename, "-") == 0) {
		if (parser->stdin_reserved) {
			snprintf(errbuf, errbufsz,
			    "cannot specify more than one stdin source");
			return (-1);
		}
		parser->stdin_reserved = true;
		fp = stdin;
	} else {
		fp = fopen(filename, "r");
		if (fp == NULL) {
			snprintf(errbuf, errbufsz, "%s: %s", filename, strerror(errno));
			return (-1);
		}
	}

	line = NULL;
	linecap = 0;
	while ((linelen = getline(&line, &linecap, fp)) != -1) {
		char *comment;
		char *text;

		(void)linelen;
		comment = strchr(line, '#');
		if (comment != NULL)
			*comment = '\0';
		text = trim_whitespace(line);
		if (*text == '\0')
			continue;
		if (parse_acl_text_list(text, kind, for_remove, spec, errbuf,
		    errbufsz) != 0) {
			free(line);
			if (fp != stdin)
				fclose(fp);
			return (-1);
		}
	}

	if (ferror(fp) != 0) {
		snprintf(errbuf, errbufsz, "%s: %s", filename, strerror(errno));
		free(line);
		if (fp != stdin)
			fclose(fp);
		return (-1);
	}

	free(line);
	if (fp != stdin)
		fclose(fp);
	return (0);
}

static int
stat_path(const char *path, bool follow, struct stat *st)
{

	if (follow)
		return (stat(path, st));
	return (lstat(path, st));
}

static int
load_xattr_blob(const char *path, enum acl_kind kind, bool follow, void **buf_out,
    size_t *size_out)
{
	const char *name;
	ssize_t size;
	void *buf;

	*buf_out = NULL;
	*size_out = 0;
	name = xattr_name(kind);

	for (;;) {
		if (follow)
			size = getxattr(path, name, NULL, 0);
		else
			size = lgetxattr(path, name, NULL, 0);
		if (size >= 0)
			break;
		if (errno == ENODATA || errno == ENOATTR || errno == ENOTSUP ||
		    errno == EOPNOTSUPP)
			return (0);
		return (-1);
	}
	if (size == 0)
		return (0);

	buf = xcalloc((size_t)size, 1);
	for (;;) {
		ssize_t nread;

		if (follow)
			nread = getxattr(path, name, buf, (size_t)size);
		else
			nread = lgetxattr(path, name, buf, (size_t)size);
		if (nread >= 0) {
			*buf_out = buf;
			*size_out = (size_t)nread;
			return (1);
		}
		if (errno != ERANGE) {
			free(buf);
			if (errno == ENODATA || errno == ENOATTR || errno == ENOTSUP ||
			    errno == EOPNOTSUPP)
				return (0);
			return (-1);
		}
		free(buf);
		if (follow)
			size = getxattr(path, name, NULL, 0);
		else
			size = lgetxattr(path, name, NULL, 0);
		if (size <= 0)
			return (0);
		buf = xcalloc((size_t)size, 1);
	}
}

static int
parse_acl_blob(const void *buf, size_t size, enum acl_kind kind,
    struct acl_list *acl, char *errbuf, size_t errbufsz)
{
	const struct posix_acl_xattr_header_linux *header;
	const struct posix_acl_xattr_entry_linux *src;
	size_t count;
	size_t i;

	if (size < sizeof(*header)) {
		snprintf(errbuf, errbufsz, "ACL xattr is truncated");
		return (-1);
	}
	if ((size - sizeof(*header)) % sizeof(*src) != 0) {
		snprintf(errbuf, errbufsz, "ACL xattr has invalid length");
		return (-1);
	}

	header = buf;
	if (le32toh(header->a_version) != POSIX_ACL_XATTR_VERSION) {
		snprintf(errbuf, errbufsz, "unsupported ACL xattr version %" PRIu32,
		    le32toh(header->a_version));
		return (-1);
	}

	count = (size - sizeof(*header)) / sizeof(*src);
	acl_list_clear(acl);
	src = (const struct posix_acl_xattr_entry_linux *)((const char *)buf +
	    sizeof(*header));
	for (i = 0; i < count; i++) {
		struct acl_entry_linux entry;

		entry.tag = le16toh(src[i].e_tag);
		entry.perm = le16toh(src[i].e_perm);
		entry.id = le32toh(src[i].e_id);
		if (entry.tag != ACL_USER && entry.tag != ACL_GROUP)
			entry.id = ACL_UNDEFINED_ID;
		acl_list_push(acl, &entry);
	}
	acl_sort(acl);
	return (validate_acl(acl, kind, errbuf, errbufsz));
}

static int
encode_acl_blob(const struct acl_list *acl, void **buf_out, size_t *size_out)
{
	struct posix_acl_xattr_header_linux *header;
	struct posix_acl_xattr_entry_linux *dst;
	void *buf;
	size_t size;
	size_t i;

	size = sizeof(*header) + acl->count * sizeof(*dst);
	buf = xcalloc(size, 1);
	header = buf;
	header->a_version = htole32(POSIX_ACL_XATTR_VERSION);
	dst = (struct posix_acl_xattr_entry_linux *)((char *)buf + sizeof(*header));
	for (i = 0; i < acl->count; i++) {
		dst[i].e_tag = htole16(acl->entries[i].tag);
		dst[i].e_perm = htole16(acl->entries[i].perm);
		dst[i].e_id = htole32(acl->entries[i].id);
	}
	*buf_out = buf;
	*size_out = size;
	return (0);
}

static int
load_acl_kind(struct file_state *state, enum acl_kind kind, char *errbuf,
    size_t errbufsz)
{
	struct acl_list *acl;
	bool *loaded;
	void *buf;
	size_t size;
	int xattr_state;

	if (kind == ACL_KIND_DEFAULT) {
		loaded = &state->default_loaded;
		acl = &state->default_acl;
	} else {
		loaded = &state->access_loaded;
		acl = &state->access_acl;
	}
	if (*loaded)
		return (0);

	buf = NULL;
	size = 0;
	xattr_state = load_xattr_blob(state->path, kind, state->follow, &buf, &size);
	if (xattr_state < 0) {
		snprintf(errbuf, errbufsz, "%s", strerror(errno));
		return (-1);
	}
	if (xattr_state == 0) {
		if (kind == ACL_KIND_ACCESS)
			synthesize_access_acl(state->st.st_mode, acl);
		else
			acl_list_clear(acl);
		*loaded = true;
		return (0);
	}

	if (parse_acl_blob(buf, size, kind, acl, errbuf, errbufsz) != 0) {
		free(buf);
		return (-1);
	}
	free(buf);
	*loaded = true;
	return (0);
}

static int
persist_access_acl(struct file_state *state, char *errbuf, size_t errbufsz)
{
	const char *name;
	mode_t new_mode;

	name = xattr_name(ACL_KIND_ACCESS);
	if (acl_is_base_only(&state->access_acl)) {
		new_mode = access_acl_to_mode(state->st.st_mode, &state->access_acl);
		if (chmod(state->path, new_mode) != 0) {
			snprintf(errbuf, errbufsz, "%s", strerror(errno));
			return (-1);
		}
		state->st.st_mode = (state->st.st_mode & ~0777) | (new_mode & 0777);
		if (state->follow) {
			if (removexattr(state->path, name) != 0 &&
			    errno != ENODATA && errno != ENOATTR &&
			    errno != ENOTSUP && errno != EOPNOTSUPP) {
				snprintf(errbuf, errbufsz, "%s", strerror(errno));
				return (-1);
			}
		} else {
			if (lremovexattr(state->path, name) != 0 &&
			    errno != ENODATA && errno != ENOATTR &&
			    errno != ENOTSUP && errno != EOPNOTSUPP) {
				snprintf(errbuf, errbufsz, "%s", strerror(errno));
				return (-1);
			}
		}
		return (0);
	}

	{
		void *buf;
		size_t size;
		int ret;

		buf = NULL;
		size = 0;
		encode_acl_blob(&state->access_acl, &buf, &size);
		if (state->follow)
			ret = setxattr(state->path, name, buf, size, 0);
		else
			ret = lsetxattr(state->path, name, buf, size, 0);
		free(buf);
		if (ret != 0) {
			snprintf(errbuf, errbufsz, "%s", strerror(errno));
			return (-1);
		}
	}

	return (0);
}

static int
persist_default_acl(struct file_state *state, char *errbuf, size_t errbufsz)
{
	const char *name;

	name = xattr_name(ACL_KIND_DEFAULT);
	if (state->default_acl.count == 0) {
		int ret;

		if (state->follow)
			ret = removexattr(state->path, name);
		else
			ret = lremovexattr(state->path, name);
		if (ret != 0 && errno != ENODATA && errno != ENOATTR &&
		    errno != ENOTSUP && errno != EOPNOTSUPP) {
			snprintf(errbuf, errbufsz, "%s", strerror(errno));
			return (-1);
		}
		return (0);
	}

	{
		void *buf;
		size_t size;
		int ret;

		buf = NULL;
		size = 0;
		encode_acl_blob(&state->default_acl, &buf, &size);
		if (state->follow)
			ret = setxattr(state->path, name, buf, size, 0);
		else
			ret = lsetxattr(state->path, name, buf, size, 0);
		free(buf);
		if (ret != 0) {
			snprintf(errbuf, errbufsz, "%s", strerror(errno));
			return (-1);
		}
	}

	return (0);
}

static int
persist_file_state(struct file_state *state, char *errbuf, size_t errbufsz)
{

	if (state->access_dirty &&
	    persist_access_acl(state, errbuf, errbufsz) != 0)
		return (-1);
	if (state->default_dirty &&
	    persist_default_acl(state, errbuf, errbufsz) != 0)
		return (-1);
	return (0);
}

static int
apply_modify(struct acl_list *acl, const struct acl_spec *spec)
{
	size_t i;

	for (i = 0; i < spec->acl.count; i++) {
		const struct acl_entry_linux *entry;
		struct acl_entry_linux *existing;

		entry = &spec->acl.entries[i];
		existing = acl_find_entry(acl, entry->tag, entry->id);
		if (existing != NULL)
			*existing = *entry;
		else
			acl_list_push(acl, entry);
	}
	acl_sort(acl);
	return (0);
}

static int
apply_remove_spec(struct acl_list *acl, const struct acl_spec *spec, char *errbuf,
    size_t errbufsz)
{
	size_t i;

	for (i = 0; i < spec->acl.count; i++) {
		const struct acl_entry_linux *entry;
		size_t j;
		bool found;

		entry = &spec->acl.entries[i];
		found = false;
		for (j = 0; j < acl->count; j++) {
			if (acl->entries[j].tag == entry->tag &&
			    acl->entries[j].id == entry->id) {
				acl_list_remove_at(acl, j);
				found = true;
				break;
			}
		}
		if (!found) {
			snprintf(errbuf, errbufsz,
			    "cannot remove non-existent ACL entry");
			return (-1);
		}
	}
	acl_sort(acl);
	return (0);
}

static int
apply_remove_index(struct acl_list *acl, size_t index, char *errbuf,
    size_t errbufsz)
{

	if (index >= acl->count) {
		snprintf(errbuf, errbufsz, "ACL entry index %zu is out of range",
		    index);
		return (-1);
	}
	acl_list_remove_at(acl, index);
	return (0);
}

static int
finalize_acl_after_change(struct acl_list *acl, enum acl_kind kind,
    bool explicit_mask, bool no_mask, char *errbuf, size_t errbufsz)
{

	acl_sort(acl);
	if (kind == ACL_KIND_DEFAULT && acl->count == 0)
		return (0);
	if (!explicit_mask && !no_mask &&
	    (acl_has_named_entries(acl) || acl_has_mask(acl)))
		recalculate_mask(acl);
	if (no_mask && acl_has_named_entries(acl) && !acl_has_mask(acl)) {
		snprintf(errbuf, errbufsz,
		    "ACL requires a mask entry when -n is used");
		return (-1);
	}
	acl_sort(acl);
	return (validate_acl(acl, kind, errbuf, errbufsz));
}

static int
apply_operation(struct file_state *state, const struct operation *op,
    const struct options *opts, char *errbuf, size_t errbufsz)
{
	struct acl_list *acl;

	switch (op->type) {
	case OP_REMOVE_DEFAULT:
		if (!state->is_dir) {
			snprintf(errbuf, errbufsz,
			    "default ACL may only be set on a directory");
			return (-1);
		}
		if (load_acl_kind(state, ACL_KIND_DEFAULT, errbuf, errbufsz) != 0)
			return (-1);
		acl_list_clear(&state->default_acl);
		state->default_dirty = true;
		return (0);
	case OP_REMOVE_ALL:
		if (op->kind == ACL_KIND_DEFAULT) {
			if (!state->is_dir) {
				snprintf(errbuf, errbufsz,
				    "default ACL may only be set on a directory");
				return (-1);
			}
			if (load_acl_kind(state, ACL_KIND_DEFAULT, errbuf, errbufsz) != 0)
				return (-1);
			acl_list_clear(&state->default_acl);
			state->default_dirty = true;
			return (0);
		}
		if (load_acl_kind(state, ACL_KIND_ACCESS, errbuf, errbufsz) != 0)
			return (-1);
		strip_access_acl(&state->access_acl);
		if (finalize_acl_after_change(&state->access_acl, ACL_KIND_ACCESS,
		    false, opts->no_mask, errbuf, errbufsz) != 0)
			return (-1);
		state->access_dirty = true;
		return (0);
	default:
		break;
	}

	if (op->kind == ACL_KIND_DEFAULT) {
		if (!state->is_dir) {
			snprintf(errbuf, errbufsz,
			    "default ACL may only be set on a directory");
			return (-1);
		}
		if (load_acl_kind(state, ACL_KIND_DEFAULT, errbuf, errbufsz) != 0)
			return (-1);
		acl = &state->default_acl;
	} else {
		if (load_acl_kind(state, ACL_KIND_ACCESS, errbuf, errbufsz) != 0)
			return (-1);
		acl = &state->access_acl;
	}

	switch (op->type) {
	case OP_MODIFY:
		apply_modify(acl, &op->spec);
		if (finalize_acl_after_change(acl, op->kind, op->spec.explicit_mask,
		    opts->no_mask, errbuf, errbufsz) != 0)
			return (-1);
		break;
	case OP_REMOVE_SPEC:
		if (apply_remove_spec(acl, &op->spec, errbuf, errbufsz) != 0)
			return (-1);
		if (finalize_acl_after_change(acl, op->kind, false, opts->no_mask,
		    errbuf, errbufsz) != 0)
			return (-1);
		break;
	case OP_REMOVE_INDEX:
		if (apply_remove_index(acl, op->index, errbuf, errbufsz) != 0)
			return (-1);
		if (finalize_acl_after_change(acl, op->kind, false, opts->no_mask,
		    errbuf, errbufsz) != 0)
			return (-1);
		break;
	case OP_REMOVE_ALL:
	case OP_REMOVE_DEFAULT:
		break;
	}

	if (op->kind == ACL_KIND_DEFAULT)
		state->default_dirty = true;
	else
		state->access_dirty = true;
	return (0);
}

static int
process_single_path(const char *path, bool follow, const struct operation_list *ops,
    const struct options *opts)
{
	struct file_state state;
	char errbuf[256];
	size_t i;

	memset(&state, 0, sizeof(state));
	state.path = path;
	state.follow = follow;
	acl_list_init(&state.access_acl);
	acl_list_init(&state.default_acl);

	if (stat_path(path, follow, &state.st) != 0) {
		report_errno(path);
		goto fail;
	}
	state.is_dir = S_ISDIR(state.st.st_mode);
	if (!follow && S_ISLNK(state.st.st_mode)) {
		report_path_error(path,
		    "symbolic link ACLs are not supported on Linux");
		goto fail;
	}

	for (i = 0; i < ops->count; i++) {
		if (apply_operation(&state, &ops->ops[i], opts, errbuf,
		    sizeof(errbuf)) != 0) {
			report_path_error(path, "%s", errbuf);
			goto fail;
		}
	}

	if (persist_file_state(&state, errbuf, sizeof(errbuf)) != 0) {
		report_path_error(path, "%s", errbuf);
		goto fail;
	}

	acl_list_free(&state.access_acl);
	acl_list_free(&state.default_acl);
	return (0);

fail:
	acl_list_free(&state.access_acl);
	acl_list_free(&state.default_acl);
	return (1);
}

static int
process_path_recursive(const char *path, bool follow_root, bool follow_children,
    const struct operation_list *ops, const struct options *opts,
    struct visited_set *visited)
{
	struct stat st;
	int errors;

	errors = 0;
	if (stat_path(path, follow_root, &st) != 0) {
		report_errno(path);
		return (1);
	}

	if (visited_contains(visited, st.st_dev, st.st_ino)) {
		report_path_error(path, "recursive directory cycle detected");
		return (1);
	}
	if (S_ISDIR(st.st_mode))
		visited_add(visited, st.st_dev, st.st_ino);

	errors += process_single_path(path, follow_root, ops, opts);
	if (!S_ISDIR(st.st_mode))
		return (errors);

	{
		DIR *dir;
		struct dirent *de;

		dir = opendir(path);
		if (dir == NULL) {
			report_errno(path);
			return (errors + 1);
		}
		while ((de = readdir(dir)) != NULL) {
			char *child;
			size_t path_len;
			size_t name_len;
			size_t total;
			struct stat child_st;

			if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
				continue;
			path_len = strlen(path);
			name_len = strlen(de->d_name);
			total = path_len + 1 + name_len + 1;
			child = xcalloc(total, 1);
			memcpy(child, path, path_len);
			child[path_len] = '/';
			memcpy(child + path_len + 1, de->d_name, name_len);

			if (stat_path(child, follow_children, &child_st) != 0) {
				report_errno(child);
				errors++;
				free(child);
				continue;
			}
			if (S_ISDIR(child_st.st_mode)) {
				errors += process_path_recursive(child, follow_children,
				    follow_children, ops, opts, visited);
			} else {
				errors += process_single_path(child, follow_children,
				    ops, opts);
			}
			free(child);
		}
		if (closedir(dir) != 0) {
			report_errno(path);
			errors++;
		}
	}

	return (errors);
}

static char **
read_path_operands_from_stdin(struct parser_state *parser)
{
	char **items;
	char *line;
	size_t cap;
	size_t count;
	size_t linecap;
	ssize_t linelen;

	if (parser->stdin_reserved) {
		report_error("setfacl: cannot specify more than one stdin source");
		exit(1);
	}
	parser->stdin_reserved = true;

	cap = 8;
	count = 0;
	items = xcalloc(cap, sizeof(*items));
	line = NULL;
	linecap = 0;
	while ((linelen = getline(&line, &linecap, stdin)) != -1) {
		char *text;

		while (linelen > 0 &&
		    (line[linelen - 1] == '\n' || line[linelen - 1] == '\r')) {
			line[--linelen] = '\0';
		}
		text = trim_whitespace(line);
		if (*text == '\0') {
			report_error("setfacl: stdin: empty pathname");
			parser->stdin_path_error = true;
			continue;
		}
		if (count + 1 >= cap) {
			cap *= 2;
			items = xreallocarray(items, cap, sizeof(*items));
		}
		items[count++] = xstrdup(text);
	}
	free(line);
	if (ferror(stdin) != 0) {
		report_errno("stdin");
		exit(1);
	}
	items[count] = NULL;
	return (items);
}

static int
process_operand(const char *path, const struct operation_list *ops,
    const struct options *opts, struct visited_set *visited)
{
	bool follow_root;
	bool follow_children;

	if (!opts->recursive)
		return (process_single_path(path, !opts->no_follow, ops, opts));

	switch (opts->walk_mode) {
	case WALK_LOGICAL:
		follow_root = true;
		follow_children = true;
		break;
	case WALK_HYBRID:
		follow_root = true;
		follow_children = false;
		break;
	case WALK_PHYSICAL:
	default:
		follow_root = false;
		follow_children = false;
		break;
	}

	return (process_path_recursive(path, follow_root, follow_children, ops, opts,
	    visited));
}

int
main(int argc, char *argv[])
{
	struct operation_list ops;
	struct parser_state parser;
	struct options opts;
	struct visited_set visited;
	char **paths;
	int ch;
	int errors;
	int i;

	op_list_init(&ops);
	parser.current_kind = ACL_KIND_ACCESS;
	parser.stdin_reserved = false;
	parser.stdin_path_error = false;
	memset(&opts, 0, sizeof(opts));
	opts.walk_mode = WALK_PHYSICAL;
	visited_init(&visited);

	while ((ch = getopt_long(argc, argv, "HLM:PRX:a:bdhkm:nx:", long_options,
	    NULL)) != -1) {
		struct operation op;
		char errbuf[256];
		char *end;

		memset(&op, 0, sizeof(op));
		acl_list_init(&op.spec.acl);
		op.kind = parser.current_kind;

		switch (ch) {
		case 'H':
			opts.walk_mode = WALK_HYBRID;
			break;
		case 'L':
			opts.walk_mode = WALK_LOGICAL;
			break;
		case 'M':
			op.type = OP_MODIFY;
			if (parse_acl_file(optarg, parser.current_kind, false, &parser,
			    &op.spec, errbuf, sizeof(errbuf)) != 0) {
				report_error("setfacl: %s", errbuf);
				acl_list_free(&op.spec.acl);
				op_list_free(&ops);
				visited_free(&visited);
				return (1);
			}
			op_list_push(&ops, &op);
			break;
		case 'P':
			opts.walk_mode = WALK_PHYSICAL;
			break;
		case 'R':
			opts.recursive = true;
			break;
		case 'X':
			op.type = OP_REMOVE_SPEC;
			if (parse_acl_file(optarg, parser.current_kind, true, &parser,
			    &op.spec, errbuf, sizeof(errbuf)) != 0) {
				report_error("setfacl: %s", errbuf);
				acl_list_free(&op.spec.acl);
				op_list_free(&ops);
				visited_free(&visited);
				return (1);
			}
			op_list_push(&ops, &op);
			break;
		case 'a':
			report_error("setfacl: option -a is not supported on Linux "
			    "(NFSv4 ACLs have no Linux POSIX ACL equivalent)");
			acl_list_free(&op.spec.acl);
			op_list_free(&ops);
			visited_free(&visited);
			return (1);
		case 'b':
			op.type = OP_REMOVE_ALL;
			op_list_push(&ops, &op);
			break;
		case 'd':
			parser.current_kind = ACL_KIND_DEFAULT;
			acl_list_free(&op.spec.acl);
			break;
		case 'h':
			opts.no_follow = true;
			break;
		case 'k':
			op.type = OP_REMOVE_DEFAULT;
			op.kind = ACL_KIND_DEFAULT;
			op_list_push(&ops, &op);
			break;
		case 'm':
			op.type = OP_MODIFY;
			if (parse_acl_text_list(optarg, parser.current_kind, false,
			    &op.spec, errbuf, sizeof(errbuf)) != 0) {
				report_error("setfacl: %s", errbuf);
				acl_list_free(&op.spec.acl);
				op_list_free(&ops);
				visited_free(&visited);
				return (1);
			}
			op_list_push(&ops, &op);
			break;
		case 'n':
			opts.no_mask = true;
			acl_list_free(&op.spec.acl);
			break;
		case 'x':
			errno = 0;
			op.index = strtoul(optarg, &end, 10);
			if (*optarg != '\0' && *end == '\0' && errno == 0) {
				op.type = OP_REMOVE_INDEX;
				op_list_push(&ops, &op);
				break;
			}
			op.type = OP_REMOVE_SPEC;
			if (parse_acl_text_list(optarg, parser.current_kind, true,
			    &op.spec, errbuf, sizeof(errbuf)) != 0) {
				report_error("setfacl: %s", errbuf);
				acl_list_free(&op.spec.acl);
				op_list_free(&ops);
				visited_free(&visited);
				return (1);
			}
			op_list_push(&ops, &op);
			break;
		default:
			acl_list_free(&op.spec.acl);
			op_list_free(&ops);
			visited_free(&visited);
			usage();
		}
	}

	if (opts.recursive && opts.no_follow) {
		report_error("setfacl: the -R and -h options may not be specified together");
		op_list_free(&ops);
		visited_free(&visited);
		return (1);
	}
	if (ops.count == 0) {
		op_list_free(&ops);
		visited_free(&visited);
		usage();
	}

	argc -= optind;
	argv += optind;
	if (argc == 0 || (argc == 1 && strcmp(argv[0], "-") == 0))
		paths = read_path_operands_from_stdin(&parser);
	else
		paths = argv;

	errors = 0;
	for (i = 0; paths[i] != NULL; i++) {
		visited.count = 0;
		errors += process_operand(paths[i], &ops, &opts, &visited);
	}
	if (parser.stdin_path_error)
		errors++;

	if (paths != argv) {
		for (i = 0; paths[i] != NULL; i++)
			free(paths[i]);
		free(paths);
	}
	op_list_free(&ops);
	visited_free(&visited);
	return (errors != 0 ? 1 : 0);
}
