#include <string.h>

#include "compat.h"

size_t
sh_strlcpy(char *dst, const char *src, size_t dstsize)
{
	size_t srclen;

	srclen = strlen(src);
	if (dstsize != 0) {
		size_t copylen;

		copylen = srclen >= dstsize ? dstsize - 1 : srclen;
		memcpy(dst, src, copylen);
		dst[copylen] = '\0';
	}
	return (srclen);
}
