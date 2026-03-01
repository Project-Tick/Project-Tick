#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/prctl.h>

/*
 * Small helper to create processes with predictable names/properties for ps tests.
 */

static void
handle_sigterm(int sig)
{
	(void)sig;
	exit(0);
}

int
main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: %s <name> [arg...]\n", argv[0]);
		return (1);
	}

	/* Set name for /proc/self/comm (visible in ps -c) */
	prctl(PR_SET_NAME, argv[1], 0, 0, 0);

	signal(SIGTERM, handle_sigterm);

	/* Busy wait in background to consume some CPU time if needed */
	if (argc > 2 && strcmp(argv[2], "busy") == 0) {
		while (1) {
			/* Do nothing, just spin */
		}
	}

	/* Normal idle wait */
	while (1) {
		pause();
	}

	return (0);
}
