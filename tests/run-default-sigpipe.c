#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void
reset_sigpipe(void)
{
	struct sigaction sa;
	sigset_t set;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_DFL;
	sigemptyset(&sa.sa_mask);
	if (sigaction(SIGPIPE, &sa, NULL) != 0) {
		fprintf(stderr, "run-default-sigpipe: sigaction(SIGPIPE): %s\n",
		    strerror(errno));
		exit(125);
	}

	sigemptyset(&set);
	sigaddset(&set, SIGPIPE);
	if (sigprocmask(SIG_UNBLOCK, &set, NULL) != 0) {
		fprintf(stderr, "run-default-sigpipe: sigprocmask(SIGPIPE): %s\n",
		    strerror(errno));
		exit(125);
	}
}

int
main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: %s program [args...]\n", argv[0]);
		return (125);
	}

	reset_sigpipe();
	execv(argv[1], argv + 1);

	fprintf(stderr, "run-default-sigpipe: execv(%s): %s\n", argv[1],
	    strerror(errno));
	return (125);
}
