#ifndef SH_COMPAT_H
#define SH_COMPAT_H

#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <time.h>

#ifndef __DECONST
#define __DECONST(type, var) ((type)(uintptr_t)(const void *)(var))
#endif

#ifndef nitems
#define nitems(array) (sizeof(array) / sizeof((array)[0]))
#endif

#ifndef ALIGNBYTES
#define ALIGNBYTES (sizeof(uintptr_t) - 1)
#endif

#ifndef ALIGN
#define ALIGN(value) (((value) + ALIGNBYTES) & ~ALIGNBYTES)
#endif

#ifndef _PATH_STDPATH
#define _PATH_STDPATH "/usr/bin:/bin"
#endif

#ifndef MAXLOGNAME
#ifdef LOGIN_NAME_MAX
#define MAXLOGNAME (LOGIN_NAME_MAX + 1)
#else
#define MAXLOGNAME 256
#endif
#endif

#ifndef CLOCK_UPTIME
#define CLOCK_UPTIME CLOCK_MONOTONIC
#endif

#ifndef timespecsub
#define timespecsub(a, b, result) do { \
	(result)->tv_sec = (a)->tv_sec - (b)->tv_sec; \
	(result)->tv_nsec = (a)->tv_nsec - (b)->tv_nsec; \
	if ((result)->tv_nsec < 0) { \
		(result)->tv_sec--; \
		(result)->tv_nsec += 1000000000L; \
	} \
} while (0)
#endif

#ifndef timespeccmp
#define timespeccmp(a, b, cmp) \
	(((a)->tv_sec == (b)->tv_sec) ? \
	 ((a)->tv_nsec cmp (b)->tv_nsec) : \
	 ((a)->tv_sec cmp (b)->tv_sec))
#endif

size_t sh_strlcpy(char *dst, const char *src, size_t dstsize);

#define strlcpy sh_strlcpy

#endif
