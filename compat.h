#ifndef PAX_COMPAT_H
#define PAX_COMPAT_H

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif

#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <time.h>

#ifndef __aligned
#define __aligned(x) __attribute__((__aligned__(x)))
#endif

#ifndef __printflike
#define __printflike(fmtarg, firstvararg) \
	__attribute__((__format__(__printf__, fmtarg, firstvararg)))
#endif

#ifdef __GLIBC__
typedef unsigned int u_int;
typedef unsigned long u_long;
typedef uint64_t u_quad_t;
#endif

#ifndef _PATH_TMP
#define _PATH_TMP "/tmp/"
#endif

#ifndef _PATH_DEFTAPE
#define _PATH_DEFTAPE "/dev/tape"
#endif

#ifndef QUAD_MAX
#define QUAD_MAX LLONG_MAX
#endif

#ifndef setpassent
#define setpassent(stayopen) ((void)(stayopen), setpwent())
#endif

#ifndef setgroupent
#define setgroupent(stayopen) ((void)(stayopen), setgrent())
#endif

size_t pax_strlcpy(char *dst, const char *src, size_t dsize);
void pax_strmode(mode_t mode, char *buf);

#define strlcpy pax_strlcpy
#define strmode pax_strmode

#endif
