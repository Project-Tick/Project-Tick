/*
 * deep_helper.c – test helper for pwd's deep-path test
 *
 * Usage: deep_helper <depth> <segment> <pwd_binary>
 *
 * Creates `depth` nested directories each named `segment`, navigates
 * into the deepest one using chdir(2) (not the shell's cd builtin),
 * then exec(2)s `pwd_binary` as a child process.  The output of
 * pwd_binary goes to stdout.
 *
 * Exit codes:
 *   0  – pwd_binary exited 0
 *   1  – setup failure (mkdir/chdir/exec)
 *   N  – whatever pwd_binary exited with
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#define _GNU_SOURCE 1

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
	long depth;
	char *end;
	const char *segment;
	const char *pwd_bin;
	int i, status;
	pid_t child;

	if (argc != 4) {
		fprintf(stderr, "usage: deep_helper <depth> <segment> <pwd_binary>\n");
		return 1;
	}

	errno = 0;
	depth = strtol(argv[1], &end, 10);
	if (end == argv[1] || *end != '\0' || errno != 0 || depth < 0) {
		fprintf(stderr, "deep_helper: invalid depth: %s\n", argv[1]);
		return 1;
	}
	segment = argv[2];
	pwd_bin = argv[3];

	for (i = 0; i < (int)depth; i++) {
		if (mkdir(segment, 0755) != 0 && errno != EEXIST) {
			fprintf(stderr, "deep_helper: mkdir(%s) at depth %d: %s\n",
			    segment, i, strerror(errno));
			return 1;
		}
		if (chdir(segment) != 0) {
			/*
			 * chdir(2) with a single short component should not fail
			 * ENAMETOOLONG on Linux even when the cumulative path
			 * exceeds PATH_MAX.  If it does, report it clearly.
			 */
			fprintf(stderr, "deep_helper: chdir(%s) at depth %d: %s\n",
			    segment, i, strerror(errno));
			return 1;
		}
	}

	/* Unset PWD so pwd(1) falls back to getcwd(NULL, 0). */
	if (unsetenv("PWD") != 0) {
		fprintf(stderr, "deep_helper: unsetenv(PWD): %s\n",
		    strerror(errno));
		return 1;
	}

	/* Fork so we can capture the exit status of pwd_bin. */
	child = fork();
	if (child == -1) {
		fprintf(stderr, "deep_helper: fork: %s\n", strerror(errno));
		return 1;
	}
	if (child == 0) {
		/* Child: exec pwd_bin with no arguments (default -L) */
		char *args[] = { (char *)pwd_bin, "-P", NULL };
		execv(pwd_bin, args);
		fprintf(stderr, "deep_helper: execv(%s): %s\n",
		    pwd_bin, strerror(errno));
		_exit(1);
	}

	/* Parent: wait for child. */
	if (waitpid(child, &status, 0) == -1) {
		fprintf(stderr, "deep_helper: waitpid: %s\n", strerror(errno));
		return 1;
	}
	if (WIFEXITED(status))
		return WEXITSTATUS(status);

	fprintf(stderr, "deep_helper: pwd_bin killed by signal %d\n",
	    WTERMSIG(status));
	return 1;
}
