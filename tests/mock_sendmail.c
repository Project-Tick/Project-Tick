#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *
require_env(const char *name)
{
	const char *value;

	value = getenv(name);
	if (value == NULL || value[0] == '\0') {
		fprintf(stderr, "mock_sendmail: missing %s\n", name);
		exit(111);
	}
	return value;
}

int
main(int argc, char *argv[])
{
	const char *argv_path;
	const char *stdin_path;
	const char *exit_env;
	FILE *argv_file;
	FILE *stdin_file;
	char buffer[4096];
	size_t nread;

	argv_path = require_env("RMAIL_SENDMAIL_LOG");
	stdin_path = require_env("RMAIL_SENDMAIL_INPUT");

	argv_file = fopen(argv_path, "w");
	if (argv_file == NULL) {
		fprintf(stderr, "mock_sendmail: fopen(%s): %s\n",
		    argv_path, strerror(errno));
		return 111;
	}
	for (int i = 0; i < argc; i++) {
		if (fprintf(argv_file, "%s\n", argv[i]) < 0) {
			fprintf(stderr, "mock_sendmail: fprintf(%s): %s\n",
			    argv_path, strerror(errno));
			fclose(argv_file);
			return 111;
		}
	}
	if (fclose(argv_file) != 0) {
		fprintf(stderr, "mock_sendmail: fclose(%s): %s\n",
		    argv_path, strerror(errno));
		return 111;
	}

	stdin_file = fopen(stdin_path, "w");
	if (stdin_file == NULL) {
		fprintf(stderr, "mock_sendmail: fopen(%s): %s\n",
		    stdin_path, strerror(errno));
		return 111;
	}
	while ((nread = fread(buffer, 1, sizeof(buffer), stdin)) > 0) {
		if (fwrite(buffer, 1, nread, stdin_file) != nread) {
			fprintf(stderr, "mock_sendmail: fwrite(%s): %s\n",
			    stdin_path, strerror(errno));
			fclose(stdin_file);
			return 111;
		}
	}
	if (ferror(stdin)) {
		fprintf(stderr, "mock_sendmail: fread(stdin): %s\n",
		    strerror(errno));
		fclose(stdin_file);
		return 111;
	}
	if (fclose(stdin_file) != 0) {
		fprintf(stderr, "mock_sendmail: fclose(%s): %s\n",
		    stdin_path, strerror(errno));
		return 111;
	}

	exit_env = getenv("RMAIL_SENDMAIL_EXIT");
	if (exit_env != NULL && exit_env[0] != '\0')
		return (int)strtol(exit_env, NULL, 10);
	return 0;
}
