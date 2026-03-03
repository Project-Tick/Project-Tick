#include "mode.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct bitcmd {
	char cmd;
	char cmd2;
	mode_t bits;
} bitcmd_t;

#define SET_LEN 6
#define SET_LEN_INCR 4

#define CMD2_CLR   0x01
#define CMD2_SET   0x02
#define CMD2_GBITS 0x04
#define CMD2_OBITS 0x08
#define CMD2_UBITS 0x10

#define STANDARD_BITS (S_ISUID | S_ISGID | S_IRWXU | S_IRWXG | S_IRWXO)

static mode_t get_current_umask(void);
static bitcmd_t *addcmd(bitcmd_t *set, mode_t op, mode_t who, mode_t oparg,
    mode_t mask);
static void compress_mode(bitcmd_t *set);

mode_t
mode_apply(const void *compiled, mode_t old_mode)
{
	const bitcmd_t *set;
	mode_t clear_value;
	mode_t new_mode;
	mode_t value;

	set = compiled;
	new_mode = old_mode;
	for (value = 0;; set++) {
		switch (set->cmd) {
		case 'u':
			value = (new_mode & S_IRWXU) >> 6;
			goto common;
		case 'g':
			value = (new_mode & S_IRWXG) >> 3;
			goto common;
		case 'o':
			value = new_mode & S_IRWXO;
common:
			if (set->cmd2 & CMD2_CLR) {
				clear_value = (set->cmd2 & CMD2_SET) ? S_IRWXO : value;
				if (set->cmd2 & CMD2_UBITS)
					new_mode &= ~((clear_value << 6) & set->bits);
				if (set->cmd2 & CMD2_GBITS)
					new_mode &= ~((clear_value << 3) & set->bits);
				if (set->cmd2 & CMD2_OBITS)
					new_mode &= ~(clear_value & set->bits);
			}
			if (set->cmd2 & CMD2_SET) {
				if (set->cmd2 & CMD2_UBITS)
					new_mode |= (value << 6) & set->bits;
				if (set->cmd2 & CMD2_GBITS)
					new_mode |= (value << 3) & set->bits;
				if (set->cmd2 & CMD2_OBITS)
					new_mode |= value & set->bits;
			}
			break;
		case '+':
			new_mode |= set->bits;
			break;
		case '-':
			new_mode &= ~set->bits;
			break;
		case 'X':
			if (old_mode & (S_IFDIR | S_IXUSR | S_IXGRP | S_IXOTH))
				new_mode |= set->bits;
			break;
		case '\0':
		default:
			return (new_mode);
		}
	}
}

void *
mode_compile(const char *mode_string)
{
	char op;
	char *endp;
	bitcmd_t *endset;
	bitcmd_t *saveset;
	bitcmd_t *set;
	mode_t mask;
	mode_t perm;
	mode_t perm_xbits;
	mode_t who;
	long perm_value;
	unsigned int setlen;
	int saved_errno;
	int equal_done;

	if (*mode_string == '\0') {
		errno = EINVAL;
		return (NULL);
	}

	mask = ~get_current_umask();
	setlen = SET_LEN + 2;
	set = malloc(setlen * sizeof(*set));
	if (set == NULL)
		return (NULL);
	saveset = set;
	endset = set + (setlen - 2);

	if (isdigit((unsigned char)*mode_string)) {
		errno = 0;
		perm_value = strtol(mode_string, &endp, 8);
		if (*endp != '\0') {
			errno = EINVAL;
			goto fail;
		}
		if (errno == ERANGE &&
		    (perm_value == LONG_MAX || perm_value == LONG_MIN)) {
			goto fail;
		}
		if (perm_value & ~(STANDARD_BITS | S_ISVTX)) {
			errno = EINVAL;
			goto fail;
		}
		perm = (mode_t)perm_value;
		set = addcmd(set, '=', STANDARD_BITS | S_ISVTX, perm, mask);
		set->cmd = '\0';
		return (saveset);
	}

	equal_done = 0;
	for (;;) {
		for (who = 0;; mode_string++) {
			switch (*mode_string) {
			case 'a':
				who |= STANDARD_BITS;
				break;
			case 'u':
				who |= S_ISUID | S_IRWXU;
				break;
			case 'g':
				who |= S_ISGID | S_IRWXG;
				break;
			case 'o':
				who |= S_IRWXO;
				break;
			default:
				goto getop;
			}
		}
getop:
		op = *mode_string++;
		if (op != '+' && op != '-' && op != '=') {
			errno = EINVAL;
			goto fail;
		}
		if (op == '=')
			equal_done = 0;

		who &= ~S_ISVTX;
		for (perm = 0, perm_xbits = 0;; mode_string++) {
			switch (*mode_string) {
			case 'r':
				perm |= S_IRUSR | S_IRGRP | S_IROTH;
				break;
			case 'w':
				perm |= S_IWUSR | S_IWGRP | S_IWOTH;
				break;
			case 'x':
				perm |= S_IXUSR | S_IXGRP | S_IXOTH;
				break;
			case 'X':
				perm_xbits = S_IXUSR | S_IXGRP | S_IXOTH;
				break;
			case 's':
				if (who == 0 || (who & ~S_IRWXO) != 0)
					perm |= S_ISUID | S_ISGID;
				break;
			case 't':
				if (who == 0 || (who & ~S_IRWXO) != 0) {
					who |= S_ISVTX;
					perm |= S_ISVTX;
				}
				break;
			case 'u':
			case 'g':
			case 'o':
				if (perm != 0) {
					set = addcmd(set, op, who, perm, mask);
					perm = 0;
				}
				if (op == '=')
					equal_done = 1;
				if (op == '+' && perm_xbits != 0) {
					set = addcmd(set, 'X', who, perm_xbits, mask);
					perm_xbits = 0;
				}
				set = addcmd(set, *mode_string, who, op, mask);
				break;
			default:
				if (perm != 0 || (op == '=' && !equal_done)) {
					if (op == '=')
						equal_done = 1;
					set = addcmd(set, op, who, perm, mask);
					perm = 0;
				}
				if (perm_xbits != 0) {
					set = addcmd(set, 'X', who, perm_xbits, mask);
					perm_xbits = 0;
				}
				goto apply;
			}

			if (set >= endset) {
				ptrdiff_t offset;
				bitcmd_t *newset;

				offset = set - saveset;
				setlen += SET_LEN_INCR;
				newset = realloc(saveset, setlen * sizeof(*newset));
				if (newset == NULL)
					goto fail;
				saveset = newset;
				set = newset + offset;
				endset = newset + (setlen - 2);
			}
		}
apply:
		if (*mode_string == '\0')
			break;
		if (*mode_string != ',') {
			errno = EINVAL;
			goto fail;
		}
		mode_string++;
	}

	set->cmd = '\0';
	compress_mode(saveset);
	return (saveset);

fail:
	saved_errno = errno;
	free(saveset);
	errno = saved_errno;
	return (NULL);
}

void
mode_free(void *compiled)
{
	free(compiled);
}

static mode_t
get_current_umask(void)
{
	mode_t mask;

	mask = umask(0);
	(void)umask(mask);
	return (mask);
}

static bitcmd_t *
addcmd(bitcmd_t *set, mode_t op, mode_t who, mode_t oparg, mode_t mask)
{
	switch (op) {
	case '=':
	case '+':
	case '-':
		if (who == 0)
			who = STANDARD_BITS & ~mask;
		set->cmd = op;
		set->bits = (who & oparg);
		set->cmd2 = 0;
		break;
	case 'u':
	case 'g':
	case 'o':
		if (who == 0)
			who = STANDARD_BITS & ~mask;
		set->cmd = op;
		set->bits = who;
		set->cmd2 = 0;
		if (oparg == '+')
			set->cmd2 = CMD2_SET;
		else if (oparg == '-')
			set->cmd2 = CMD2_CLR;
		else {
			set->cmd2 = CMD2_SET | CMD2_CLR;
		}
		if (who & (S_ISUID | S_IRWXU))
			set->cmd2 |= CMD2_UBITS;
		if (who & (S_ISGID | S_IRWXG))
			set->cmd2 |= CMD2_GBITS;
		if (who & S_IRWXO)
			set->cmd2 |= CMD2_OBITS;
		break;
	case 'X':
		if (who == 0)
			who = STANDARD_BITS & ~mask;
		set->cmd = 'X';
		set->bits = who & oparg;
		set->cmd2 = 0;
		break;
	}
	return (set + 1);
}

static void
compress_mode(bitcmd_t *set)
{
	bitcmd_t *nset;

	while (set->cmd != '\0') {
		if (set->cmd == '+' && set[1].cmd == '+') {
			set[1].bits |= set->bits;
			nset = set;
			do {
				*nset = nset[1];
			} while ((nset++)->cmd != '\0');
			continue;
		}
		set++;
	}
}
