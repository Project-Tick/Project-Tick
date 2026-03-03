#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "compat.h"
#include "signames.h"

struct signal_entry {
	const char *name;
	int number;
};

#define SIGNAL_ENTRY(name) { #name, SIG##name }
#define SIGNAL_ALIAS(name, signal) { name, signal }

static const struct signal_entry canonical_signals[] = {
#ifdef SIGHUP
	SIGNAL_ENTRY(HUP),
#endif
#ifdef SIGINT
	SIGNAL_ENTRY(INT),
#endif
#ifdef SIGQUIT
	SIGNAL_ENTRY(QUIT),
#endif
#ifdef SIGILL
	SIGNAL_ENTRY(ILL),
#endif
#ifdef SIGTRAP
	SIGNAL_ENTRY(TRAP),
#endif
#ifdef SIGABRT
	SIGNAL_ENTRY(ABRT),
#endif
#ifdef SIGBUS
	SIGNAL_ENTRY(BUS),
#endif
#ifdef SIGFPE
	SIGNAL_ENTRY(FPE),
#endif
#ifdef SIGKILL
	SIGNAL_ENTRY(KILL),
#endif
#ifdef SIGUSR1
	SIGNAL_ENTRY(USR1),
#endif
#ifdef SIGSEGV
	SIGNAL_ENTRY(SEGV),
#endif
#ifdef SIGUSR2
	SIGNAL_ENTRY(USR2),
#endif
#ifdef SIGPIPE
	SIGNAL_ENTRY(PIPE),
#endif
#ifdef SIGALRM
	SIGNAL_ENTRY(ALRM),
#endif
#ifdef SIGTERM
	SIGNAL_ENTRY(TERM),
#endif
#ifdef SIGSTKFLT
	SIGNAL_ENTRY(STKFLT),
#endif
#ifdef SIGCHLD
	SIGNAL_ENTRY(CHLD),
#endif
#ifdef SIGCONT
	SIGNAL_ENTRY(CONT),
#endif
#ifdef SIGSTOP
	SIGNAL_ENTRY(STOP),
#endif
#ifdef SIGTSTP
	SIGNAL_ENTRY(TSTP),
#endif
#ifdef SIGTTIN
	SIGNAL_ENTRY(TTIN),
#endif
#ifdef SIGTTOU
	SIGNAL_ENTRY(TTOU),
#endif
#ifdef SIGURG
	SIGNAL_ENTRY(URG),
#endif
#ifdef SIGXCPU
	SIGNAL_ENTRY(XCPU),
#endif
#ifdef SIGXFSZ
	SIGNAL_ENTRY(XFSZ),
#endif
#ifdef SIGVTALRM
	SIGNAL_ENTRY(VTALRM),
#endif
#ifdef SIGPROF
	SIGNAL_ENTRY(PROF),
#endif
#ifdef SIGWINCH
	SIGNAL_ENTRY(WINCH),
#endif
#ifdef SIGIO
	SIGNAL_ENTRY(IO),
#endif
#ifdef SIGPWR
	SIGNAL_ENTRY(PWR),
#endif
#ifdef SIGSYS
	SIGNAL_ENTRY(SYS),
#endif
};

static const struct signal_entry signal_aliases[] = {
#ifdef SIGIOT
	SIGNAL_ALIAS("IOT", SIGIOT),
#endif
#ifdef SIGCLD
	SIGNAL_ALIAS("CLD", SIGCLD),
#endif
#ifdef SIGPOLL
	SIGNAL_ALIAS("POLL", SIGPOLL),
#endif
#ifdef SIGUNUSED
	SIGNAL_ALIAS("UNUSED", SIGUNUSED),
#endif
};

static bool
parse_nonnegative_number(const char *text, int *value)
{
	long parsed;
	char *end;

	if (text[0] == '\0')
		return (false);

	errno = 0;
	parsed = strtol(text, &end, 10);
	if (errno != 0 || *end != '\0' || parsed < 0 || parsed > INT_MAX)
		return (false);

	*value = (int)parsed;
	return (true);
}

static char *
normalize_signal_name(const char *token)
{
	size_t i;
	size_t length;
	size_t offset;
	char *name;

	length = strlen(token);
	name = malloc(length + 1);
	if (name == NULL)
		return (NULL);

	for (i = 0; i < length; i++) {
		unsigned char ch;

		ch = (unsigned char)token[i];
		if (ch >= 'a' && ch <= 'z')
			name[i] = (char)(ch - ('a' - 'A'));
		else
			name[i] = (char)ch;
	}
	name[length] = '\0';

	offset = 0;
	if (length > 3 && name[0] == 'S' && name[1] == 'I' && name[2] == 'G')
		offset = 3;
	if (offset != 0)
		memmove(name, name + offset, length - offset + 1);

	return (name);
}

bool
sh_signal_name(int signum, char *buffer, size_t buffer_size)
{
	size_t i;

	if (signum == 0) {
		if (buffer != NULL && buffer_size > 0) {
			buffer[0] = '0';
			if (buffer_size > 1)
				buffer[1] = '\0';
		}
		return (true);
	}

	for (i = 0; i < nitems(canonical_signals); i++) {
		if (canonical_signals[i].number != signum)
			continue;
		if (buffer != NULL && buffer_size > 0)
			strlcpy(buffer, canonical_signals[i].name, buffer_size);
		return (true);
	}

#ifdef SIGRTMIN
#ifdef SIGRTMAX
	if (signum == SIGRTMIN) {
		if (buffer != NULL && buffer_size > 0)
			strlcpy(buffer, "RTMIN", buffer_size);
		return (true);
	}
	if (signum == SIGRTMAX) {
		if (buffer != NULL && buffer_size > 0)
			strlcpy(buffer, "RTMAX", buffer_size);
		return (true);
	}
	if (signum > SIGRTMIN && signum < SIGRTMAX) {
		if (buffer != NULL && buffer_size > 0)
			snprintf(buffer, buffer_size, "RTMIN+%d", signum - SIGRTMIN);
		return (true);
	}
#endif
#endif

	return (false);
}

bool
sh_signal_number(const char *token, int *signum)
{
	char *name;
	char *end;
	int offset;
	size_t i;

	if (parse_nonnegative_number(token, signum))
		return (true);

	name = normalize_signal_name(token);
	if (name == NULL)
		return (false);

	for (i = 0; i < nitems(canonical_signals); i++) {
		if (strcmp(name, canonical_signals[i].name) == 0) {
			*signum = canonical_signals[i].number;
			free(name);
			return (true);
		}
	}

	for (i = 0; i < nitems(signal_aliases); i++) {
		if (strcmp(name, signal_aliases[i].name) == 0) {
			*signum = signal_aliases[i].number;
			free(name);
			return (true);
		}
	}

#ifdef SIGRTMIN
#ifdef SIGRTMAX
	if (strcmp(name, "RTMIN") == 0) {
		*signum = SIGRTMIN;
		free(name);
		return (true);
	}
	if (strncmp(name, "RTMIN+", 6) == 0) {
		errno = 0;
		offset = (int)strtol(name + 6, &end, 10);
		if (errno == 0 && *end == '\0' && offset >= 0 &&
		    SIGRTMIN + offset <= SIGRTMAX) {
			*signum = SIGRTMIN + offset;
			free(name);
			return (true);
		}
	}
	if (strcmp(name, "RTMAX") == 0) {
		*signum = SIGRTMAX;
		free(name);
		return (true);
	}
	if (strncmp(name, "RTMAX-", 6) == 0) {
		errno = 0;
		offset = (int)strtol(name + 6, &end, 10);
		if (errno == 0 && *end == '\0' && offset >= 0 &&
		    SIGRTMAX - offset >= SIGRTMIN) {
			*signum = SIGRTMAX - offset;
			free(name);
			return (true);
		}
	}
#endif
#endif

	free(name);
	return (false);
}

int
sh_signal_max(void)
{
	size_t i;
	int maxsig;

	maxsig = 0;
	for (i = 0; i < nitems(canonical_signals); i++) {
		if (canonical_signals[i].number > maxsig)
			maxsig = canonical_signals[i].number;
	}
#ifdef SIGRTMAX
	if (SIGRTMAX > maxsig)
		maxsig = SIGRTMAX;
#endif
	return (maxsig);
}
