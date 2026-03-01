#include <sys/types.h>
#include <sys/xattr.h>

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pax.h"
#include "extern.h"

struct pax_xattr {
	char *name;
	unsigned char *value;
	size_t value_len;
	struct pax_xattr *next;
};

static int pax_xattr_add(ARCHD *, const char *, const void *, size_t);
static void pax_xattr_clear_list(PAX_XATTR *);
static char *url_encode_component(const char *);
static char *url_decode_component(const char *);
static char *base64_encode(const unsigned char *, size_t);
static unsigned char *base64_decode(const char *, size_t *);
static int append_pax_record(char **, size_t *, size_t *, const char *,
    const char *);
static int pax_xattr_is_selinux_label(const PAX_XATTR *);

void
pax_xattr_free(ARCHD *arcn)
{
	if (arcn == NULL)
		return;
	pax_xattr_clear_list(arcn->xattr_head);
	arcn->xattr_head = NULL;
}

void
pax_xattr_preserve_and_reset(ARCHD *arcn)
{
	PAX_XATTR *xattrs;

	if (arcn == NULL)
		return;

	xattrs = arcn->xattr_head;
	memset(arcn, 0, sizeof(*arcn));
	arcn->xattr_head = xattrs;
}

int
pax_xattr_has_any(const ARCHD *arcn)
{
	return (arcn != NULL) && (arcn->xattr_head != NULL);
}

int
pax_xattr_format_supported(const FSUB *frmt)
{
	return (frmt != NULL) && (strcmp(frmt->name, "ustar") == 0);
}

int
pax_xattr_has_non_selinux(const ARCHD *arcn)
{
	PAX_XATTR *entry;

	if (arcn == NULL)
		return(0);
	for (entry = arcn->xattr_head; entry != NULL; entry = entry->next) {
		if (!pax_xattr_is_selinux_label(entry))
			return(1);
	}
	return(0);
}

int
pax_xattr_drop_selinux(ARCHD *arcn)
{
	PAX_XATTR *entry;
	PAX_XATTR *next;
	PAX_XATTR *prev;
	int dropped;

	if (arcn == NULL)
		return(0);

	dropped = 0;
	prev = NULL;
	entry = arcn->xattr_head;
	while (entry != NULL) {
		next = entry->next;
		if (pax_xattr_is_selinux_label(entry)) {
			if (prev == NULL)
				arcn->xattr_head = next;
			else
				prev->next = next;
			free(entry->name);
			free(entry->value);
			free(entry);
			dropped = 1;
		} else
			prev = entry;
		entry = next;
	}
	return(dropped);
}

int
pax_xattr_read_path(ARCHD *arcn, const char *path)
{
	PAX_XATTR *entry;
	char *namebuf;
	char *name;
	char *end;
	ssize_t namesz;
	ssize_t valsz;
	unsigned char *value;

	pax_xattr_free(arcn);

	errno = 0;
	namesz = llistxattr(path, NULL, 0);
	if (namesz == 0)
		return(0);
	if (namesz < 0) {
		if ((errno == ENOTSUP) || (errno == EOPNOTSUPP) ||
		    (errno == ENODATA))
			return(0);
		syswarn(1, errno,
		    "Unable to inspect extended attributes on %s", path);
		return(-1);
	}

	if ((namebuf = malloc((size_t)namesz)) == NULL) {
		paxwarn(1,
		    "Unable to allocate memory to inspect extended attributes on %s",
		    path);
		return(-1);
	}

	errno = 0;
	namesz = llistxattr(path, namebuf, (size_t)namesz);
	if (namesz < 0) {
		syswarn(1, errno,
		    "Unable to inspect extended attributes on %s", path);
		free(namebuf);
		return(-1);
	}

	end = namebuf + namesz;
	for (name = namebuf; name < end; name += strlen(name) + 1) {
		errno = 0;
		valsz = lgetxattr(path, name, NULL, 0);
		if (valsz < 0) {
			syswarn(1, errno, "Unable to read extended attribute %s on %s",
			    name, path);
			free(namebuf);
			pax_xattr_free(arcn);
			return(-1);
		}
		value = NULL;
		if (valsz > 0) {
			if ((value = malloc((size_t)valsz)) == NULL) {
				paxwarn(1,
				    "Unable to allocate memory for extended attribute %s on %s",
				    name, path);
				free(namebuf);
				pax_xattr_free(arcn);
				return(-1);
			}
			errno = 0;
			if (lgetxattr(path, name, value, (size_t)valsz) != valsz) {
				syswarn(1, errno,
				    "Unable to read extended attribute %s on %s", name,
				    path);
				free(value);
				free(namebuf);
				pax_xattr_free(arcn);
				return(-1);
			}
		}
		if (pax_xattr_add(arcn, name, value, (size_t)valsz) < 0) {
			free(value);
			free(namebuf);
			pax_xattr_free(arcn);
			return(-1);
		}
		free(value);
	}

	free(namebuf);

	/*
	 * Reverse the push-front insertion order so pax records stay stable.
	 */
	entry = arcn->xattr_head;
	arcn->xattr_head = NULL;
	while (entry != NULL) {
		PAX_XATTR *next = entry->next;
		entry->next = arcn->xattr_head;
		arcn->xattr_head = entry;
		entry = next;
	}
	return(0);
}

int
pax_xattr_apply_path(ARCHD *arcn, const char *path)
{
	PAX_XATTR *entry;
	unsigned char *current;
	ssize_t current_len;
	int rv = 0;

	if ((arcn == NULL) || (arcn->xattr_head == NULL))
		return(0);

	for (entry = arcn->xattr_head; entry != NULL; entry = entry->next) {
		current = NULL;
		errno = 0;
		current_len = lgetxattr(path, entry->name, NULL, 0);
		if (current_len >= 0) {
			if ((size_t)current_len == entry->value_len) {
				if (current_len > 0) {
					if ((current = malloc((size_t)current_len)) == NULL) {
						paxwarn(1,
						    "Unable to allocate memory while restoring extended attribute %s on %s",
						    entry->name, path);
						return(-1);
					}
					if (lgetxattr(path, entry->name, current,
					    (size_t)current_len) == current_len &&
					    memcmp(current, entry->value,
					    entry->value_len) == 0) {
						free(current);
						continue;
					}
					free(current);
				} else {
					continue;
				}
			}
		} else if ((errno == ENOTSUP) || (errno == EOPNOTSUPP) ||
		    (errno == EPERM) || (errno == EACCES)) {
			continue;
		}

		if (lsetxattr(path, entry->name, entry->value, entry->value_len, 0) < 0) {
			if ((errno == ENOTSUP) || (errno == EOPNOTSUPP) ||
			    (errno == EPERM) || (errno == EACCES))
				continue;
			syswarn(1, errno,
			    "Unable to restore extended attribute %s on %s",
			    entry->name, path);
			rv = -1;
		}
	}
	return(rv);
}

char *
pax_xattr_to_pax_records(const ARCHD *arcn, size_t *outlen)
{
	PAX_XATTR *entry;
	char *records;
	char *encoded_key;
	char *encoded_value;
	char *fullkey;
	const char *dot;
	const char *namespace;
	const char *key;
	size_t records_len;
	size_t records_cap;
	size_t nslen;
	size_t fullkey_len;
	size_t i;

	records = NULL;
	records_len = 0;
	records_cap = 0;

	for (entry = arcn->xattr_head; entry != NULL; entry = entry->next) {
		encoded_key = NULL;
		encoded_value = NULL;
		fullkey = NULL;

		dot = strchr(entry->name, '.');
		if (dot == NULL) {
			namespace = "user";
			nslen = 4;
			key = entry->name;
		} else {
			namespace = entry->name;
			nslen = (size_t)(dot - entry->name);
			key = dot + 1;
		}

		if (nslen == 0 || key[0] == '\0') {
			free(records);
			return(NULL);
		}

		if (dot != NULL) {
			if (strchr(dot + 1, '.') != NULL) {
				free(records);
				return(NULL);
			}
		}

		for (i = 0; i < nslen; i++) {
			if (!isalnum((unsigned char)namespace[i]) && namespace[i] != '_') {
				free(records);
				return(NULL);
			}
		}

		if ((encoded_key = url_encode_component(key)) == NULL ||
		    (encoded_value = base64_encode(entry->value,
		    entry->value_len)) == NULL) {
			free(encoded_key);
			free(encoded_value);
			free(records);
			return(NULL);
		}

		fullkey_len = sizeof("LIBARCHIVE.xattr.") - 1 + nslen + 1 +
		    strlen(encoded_key);
		if ((fullkey = malloc(fullkey_len + 1)) == NULL) {
			free(encoded_key);
			free(encoded_value);
			free(records);
			return(NULL);
		}
		(void)snprintf(fullkey, fullkey_len + 1,
		    "LIBARCHIVE.xattr.%.*s.%s", (int)nslen, namespace, encoded_key);

		free(encoded_key);
		if (append_pax_record(&records, &records_len, &records_cap, fullkey,
		    encoded_value) < 0) {
			free(fullkey);
			free(encoded_value);
			free(records);
			return(NULL);
		}
		free(fullkey);
		free(encoded_value);
	}

	if (outlen != NULL)
		*outlen = records_len;
	return(records);
}

int
pax_xattr_from_pax_records(ARCHD *arcn, const char *records, size_t records_len)
{
	const char *line;
	const char *line_end;
	const char *spc;
	const char *eq;
	const char *payload;
	char *key;
	char *value;
	char *full_name;
	char *decoded_key;
	unsigned char *decoded_value;
	size_t line_len;
	size_t value_len;
	size_t key_len;
	unsigned long long declen;
	char *endp;
	size_t full_name_len;
	size_t count = 0;
	size_t total_decoded_size = 0;

	pax_xattr_free(arcn);

	if (records_len > PAX_XHDR_MAX_SIZE || records_len == 0)
		return(-1);

	line = records;
	while ((size_t)(line - records) < records_len) {
		spc = memchr(line, ' ', records_len - (size_t)(line - records));
		if (spc == NULL)
			goto fail;
		
		errno = 0;
		declen = strtoull(line, &endp, 10);
		if ((errno != 0) || (endp != spc) || (declen < 5) ||
		    (declen > (unsigned long long)PAX_XHDR_MAX_SIZE))
			goto fail;
		
		line_len = (size_t)declen;
		if (line_len > records_len - (size_t)(line - records))
			goto fail;
		
		line_end = line + line_len;
		if ((line_end[-1] != '\n'))
			goto fail;

		if (memchr(line, '\0', line_len) != NULL)
			goto fail;

		payload = spc + 1;
		eq = memchr(payload, '=', (size_t)(line_end - payload));
		if ((eq == NULL))
			goto fail;
		
		key_len = (size_t)(eq - payload);
		if (key_len == 0 || key_len > PAX_XHDR_MAX_SIZE)
			goto fail;
		
		key = malloc(key_len + 1);
		if (key == NULL)
			goto fail;
		memcpy(key, payload, key_len);
		key[key_len] = '\0';
		
		value_len = (size_t)((line_end - 1) - (eq + 1));
		if (value_len > PAX_XHDR_MAX_SIZE) {
			free(key);
			goto fail;
		}

		value = malloc(value_len + 1);
		if (value == NULL) {
			free(key);
			goto fail;
		}
		memcpy(value, eq + 1, value_len);
		value[value_len] = '\0';

		if (strncmp(key, "LIBARCHIVE.xattr.", 17) == 0) {
			char *ns = key + 17;
			char *nsdot = strchr(ns, '.');
			
			if (nsdot == NULL || nsdot == ns || nsdot[1] == '\0') {
				free(key);
				free(value);
				goto fail;
			}

			if (strchr(nsdot + 1, '.') != NULL) {
				free(key);
				free(value);
				goto fail;
			}

			int valid_ns = 1;
			for (char *c = ns; c < nsdot; c++) {
				if (!isalnum((unsigned char)*c) && *c != '_') {
					valid_ns = 0;
					break;
				}
			}

			if (!valid_ns) {
				free(key);
				free(value);
				goto fail;
			}

			*nsdot = '\0';
			decoded_key = url_decode_component(nsdot + 1);
			decoded_value = base64_decode(value, &value_len);
			
			if ((decoded_key == NULL) || (decoded_value == NULL)) {
				free(decoded_key);
				free(decoded_value);
				free(key);
				free(value);
				goto fail;
			}

			if (value_len > PAX_XATTR_MAX_VALUE) {
				free(decoded_key);
				free(decoded_value);
				free(key);
				free(value);
				goto fail;
			}

			if (total_decoded_size > PAX_XATTR_MAX_TOTAL_RECORDS - value_len) {
				free(decoded_key);
				free(decoded_value);
				free(key);
				free(value);
				goto fail;
			}
			total_decoded_size += value_len;

			if (++count > PAX_XATTR_MAX_COUNT) {
				free(decoded_key);
				free(decoded_value);
				free(key);
				free(value);
				goto fail;
			}

			full_name_len = strlen(ns) + 1 + strlen(decoded_key);
			if (full_name_len > PAX_XHDR_MAX_SIZE ||
			    (full_name = malloc(full_name_len + 1)) == NULL) {
				free(decoded_key);
				free(decoded_value);
				free(key);
				free(value);
				goto fail;
			}
			(void)snprintf(full_name, full_name_len + 1, "%s.%s",
			    ns, decoded_key);
			free(decoded_key);
			
			if (pax_xattr_add(arcn, full_name, decoded_value,
			    value_len) < 0) {
				free(full_name);
				free(decoded_value);
				free(key);
				free(value);
				goto fail;
			}
			free(full_name);
			free(decoded_value);
		}

		free(key);
		free(value);
		line = line_end;
	}

	/*
	 * Reverse the push-front insertion order to match archive order.
	 */
	{
		PAX_XATTR *entry = arcn->xattr_head;
		arcn->xattr_head = NULL;
		while (entry != NULL) {
			PAX_XATTR *next = entry->next;
			entry->next = arcn->xattr_head;
			arcn->xattr_head = entry;
			entry = next;
		}
	}
	return(0);

fail:
	pax_xattr_free(arcn);
	return(-1);
}

static int
pax_xattr_add(ARCHD *arcn, const char *name, const void *value, size_t value_len)
{
	PAX_XATTR *entry;

	if ((entry = calloc(1, sizeof(*entry))) == NULL)
		return(-1);
	if ((entry->name = strdup(name)) == NULL) {
		free(entry);
		return(-1);
	}
	if (value_len > 0) {
		if ((entry->value = malloc(value_len)) == NULL) {
			free(entry->name);
			free(entry);
			return(-1);
		}
		memcpy(entry->value, value, value_len);
	}
	entry->value_len = value_len;
	entry->next = arcn->xattr_head;
	arcn->xattr_head = entry;
	return(0);
}

static int
pax_xattr_is_selinux_label(const PAX_XATTR *entry)
{
	return (entry != NULL) && (strcmp(entry->name, "security.selinux") == 0);
}

static void
pax_xattr_clear_list(PAX_XATTR *head)
{
	PAX_XATTR *next;

	while (head != NULL) {
		next = head->next;
		free(head->name);
		free(head->value);
		free(head);
		head = next;
	}
}

static char *
url_encode_component(const char *src)
{
	char *dst;
	char *out;
	size_t len;
	size_t i;

	len = 0;
	for (i = 0; src[i] != '\0'; ++i) {
		if (isalnum((unsigned char)src[i]) || (src[i] == '.') ||
		    (src[i] == '_') || (src[i] == '-') || (src[i] == '~'))
			++len;
		else
			len += 3;
	}
	if ((dst = malloc(len + 1)) == NULL)
		return(NULL);
	out = dst;
	for (i = 0; src[i] != '\0'; ++i) {
		if (isalnum((unsigned char)src[i]) || (src[i] == '.') ||
		    (src[i] == '_') || (src[i] == '-') || (src[i] == '~'))
			*out++ = src[i];
		else {
			(void)sprintf(out, "%%%02X", (unsigned char)src[i]);
			out += 3;
		}
	}
	*out = '\0';
	return(dst);
}

static char *
url_decode_component(const char *src)
{
	char *dst;
	char *out;
	int hi;
	int lo;

	if ((dst = malloc(strlen(src) + 1)) == NULL)
		return(NULL);
	out = dst;
	while (*src != '\0') {
		if (*src == '%') {
			if (isxdigit((unsigned char)src[1]) &&
			    isxdigit((unsigned char)src[2])) {
				hi = isdigit((unsigned char)src[1]) ? src[1] - '0' :
				    (tolower((unsigned char)src[1]) - 'a' + 10);
				lo = isdigit((unsigned char)src[2]) ? src[2] - '0' :
				    (tolower((unsigned char)src[2]) - 'a' + 10);
				*out++ = (char)((hi << 4) | lo);
				src += 3;
			} else {
				free(dst);
				return(NULL);
			}
			continue;
		}
		*out++ = *src++;
	}
	*out = '\0';
	return(dst);
}

static char *
base64_encode(const unsigned char *src, size_t srclen)
{
	static const char table[] =
	    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	char *dst;
	size_t i;
	size_t j;
	size_t dstlen;
	uint32_t chunk;

	dstlen = ((srclen + 2) / 3) * 4;
	if (srclen > SIZE_MAX - 2 || (dst = malloc(dstlen + 1)) == NULL)
		return(NULL);

	for (i = 0, j = 0; i < srclen; i += 3) {
		chunk = (uint32_t)src[i] << 16;
		if ((i + 1) < srclen)
			chunk |= (uint32_t)src[i + 1] << 8;
		if ((i + 2) < srclen)
			chunk |= src[i + 2];
		dst[j++] = table[(chunk >> 18) & 0x3f];
		dst[j++] = table[(chunk >> 12) & 0x3f];
		dst[j++] = ((i + 1) < srclen) ? table[(chunk >> 6) & 0x3f] : '=';
		dst[j++] = ((i + 2) < srclen) ? table[chunk & 0x3f] : '=';
	}
	dst[j] = '\0';
	return(dst);
}

static unsigned char *
base64_decode(const char *src, size_t *outlen)
{
	static const signed char lookup[256] = {
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-2,-1,-1,
		-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
		-1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
	};
	unsigned char *dst;
	uint32_t chunk;
	size_t len, i, j;
	int b64[4], pad;

	len = strlen(src);
	if (len == 0) {
		if ((dst = malloc(1)) == NULL)
			return(NULL);
		dst[0] = '\0';
		*outlen = 0;
		return(dst);
	}

	if ((len % 4) != 0)
		return(NULL);
	if ((dst = malloc((len / 4) * 3 + 1)) == NULL)
		return(NULL);

	for (i = 0, j = 0; i < len; i += 4) {
		pad = 0;
		for (int k = 0; k < 4; k++) {
			int v = lookup[(unsigned char)src[i+k]];
			if (v == -2) { /* '=' */
				if (k < 2 || (k == 2 && src[i+3] != '=') || i + 4 < len) {
					free(dst);
					return(NULL);
				}
				pad++;
				b64[k] = 0;
			} else if (v == -1) {
				free(dst);
				return(NULL);
			} else {
				if (pad > 0) {
					free(dst);
					return(NULL);
				}
				b64[k] = v;
			}
		}
		chunk = (uint32_t)b64[0] << 18 | (uint32_t)b64[1] << 12 |
		        (uint32_t)b64[2] << 6  | (uint32_t)b64[3];
		dst[j++] = (chunk >> 16) & 0xff;
		if (pad < 2) dst[j++] = (chunk >> 8) & 0xff;
		if (pad < 1) dst[j++] = chunk & 0xff;
	}
	*outlen = j;
	return(dst);
}

static int
append_pax_record(char **buf, size_t *buflen, size_t *bufcap, const char *key,
    const char *value)
{
	char *out;
	char *record;
	size_t payload_len;
	size_t len;
	size_t digits;
	size_t need;
	size_t newcap;
	int written;
	size_t key_len;
	size_t val_len;

	key_len = strlen(key);
	val_len = strlen(value);

	if (key_len > PAX_XATTR_MAX_VALUE || val_len > PAX_XATTR_MAX_VALUE)
		return(-1);

	if (key_len > SIZE_MAX - val_len - 2)
		return(-1);

	payload_len = key_len + 1 + val_len + 1;

	if (payload_len > PAX_XATTR_MAX_TOTAL_RECORDS)
		return(-1);

	digits = 1;
	for (;;) {
		if (payload_len > SIZE_MAX - digits - 1)
			return(-1);
		len = digits + 1 + payload_len;
		if (len < 10)
			need = 1;
		else
			need = 0;
		for (size_t n = len; n > 0; n /= 10)
			++need;
		if (need == digits)
			break;
		digits = need;
	}

	if (len > PAX_XATTR_MAX_TOTAL_RECORDS)
		return(-1);

	if ((record = malloc(len + 1)) == NULL)
		return(-1);
	written = snprintf(record, len + 1, "%zu %s=%s\n", len, key, value);
	if ((written < 0) || ((size_t)written != len)) {
		free(record);
		return(-1);
	}

	if (*buflen + len > *bufcap) {
		if (len > SIZE_MAX - *buflen) {
			free(record);
			return(-1);
		}
		newcap = (*bufcap == 0) ? 128 : *bufcap;
		while (newcap < (*buflen + len)) {
			if (newcap > SIZE_MAX / 2) {
				newcap = *buflen + len;
				break;
			}
			newcap *= 2;
		}
		if (newcap > PAX_XATTR_MAX_TOTAL_RECORDS)
			newcap = PAX_XATTR_MAX_TOTAL_RECORDS;
		if (newcap < (*buflen + len)) {
			free(record);
			return(-1);
		}
		out = realloc(*buf, newcap);
		if (out == NULL) {
			free(record);
			return(-1);
		}
		*buf = out;
		*bufcap = newcap;
	}

	memcpy(*buf + *buflen, record, len);
	*buflen += len;
	free(record);
	return(0);
}
