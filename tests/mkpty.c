#define _XOPEN_SOURCE 700

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int
main(void)
{
	char buffer[256];
	char *path;
	int master;
	ssize_t bytes;

	master = posix_openpt(O_RDWR | O_NOCTTY | O_CLOEXEC);
	if (master < 0) {
		if (errno == ENOENT || errno == ENODEV || errno == ENOSYS ||
		    errno == EPERM || errno == EACCES)
			return 77;
		fprintf(stderr, "mkpty: posix_openpt: %s\n", strerror(errno));
		return 1;
	}
	if (grantpt(master) != 0) {
		fprintf(stderr, "mkpty: grantpt: %s\n", strerror(errno));
		close(master);
		return 1;
	}
	if (unlockpt(master) != 0) {
		fprintf(stderr, "mkpty: unlockpt: %s\n", strerror(errno));
		close(master);
		return 1;
	}
	path = ptsname(master);
	if (path == NULL) {
		fprintf(stderr, "mkpty: ptsname: %s\n", strerror(errno));
		close(master);
		return 1;
	}
	if (printf("%s\n", path) < 0 || fflush(stdout) != 0) {
		fprintf(stderr, "mkpty: failed to write slave path\n");
		close(master);
		return 1;
	}
	while ((bytes = read(STDIN_FILENO, buffer, sizeof(buffer))) > 0)
		;
	if (bytes < 0) {
		fprintf(stderr, "mkpty: read: %s\n", strerror(errno));
		close(master);
		return 1;
	}
	close(master);
	return 0;
}
