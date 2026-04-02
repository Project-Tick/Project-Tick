#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if __has_include(<sysexits.h>)
#include <sysexits.h>
#endif

#ifndef EX_OK
#define EX_OK 0
#endif
#ifndef EX_USAGE
#define EX_USAGE 64
#endif
#ifndef EX_DATAERR
#define EX_DATAERR 65
#endif
#ifndef EX_UNAVAILABLE
#define EX_UNAVAILABLE 69
#endif
#ifndef EX_SOFTWARE
#define EX_SOFTWARE 70
#endif
#ifndef EX_OSERR
#define EX_OSERR 71
#endif
#ifndef EX_TEMPFAIL
#define EX_TEMPFAIL 75
#endif

#ifndef RMAIL_SENDMAIL_PATH
#define RMAIL_SENDMAIL_PATH "/usr/sbin/sendmail"
#endif

struct parsed_from_line {
	char *system;
	char *user;
};

static const char usage_message[] = "usage: rmail [-D domain] [-T] user ...";

static void
die_with_errno(int code, const char *prefix)
{
	int saved_errno;

	saved_errno = errno;
	if (saved_errno == 0)
		saved_errno = EIO;

	fprintf(stderr, "rmail: %s: %s\n", prefix, strerror(saved_errno));
	exit(code);
}

static void
die(int code, const char *fmt, ...)
{
	va_list ap;

	fputs("rmail: ", stderr);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	exit(code);
}

static void
usage(void)
{
	fprintf(stderr, "%s\n", usage_message);
	exit(EX_USAGE);
}

static void *
xmalloc(size_t size)
{
	void *ptr;

	if (size == 0)
		size = 1;
	ptr = malloc(size);
	if (ptr == NULL)
		die(EX_TEMPFAIL, "out of memory");
	return ptr;
}

static void *
xrealloc(void *ptr, size_t size)
{
	void *next;

	if (size == 0)
		size = 1;
	next = realloc(ptr, size);
	if (next == NULL)
		die(EX_TEMPFAIL, "out of memory");
	return next;
}

static char *
xstrdup(const char *text)
{
	size_t len;
	char *copy;

	len = strlen(text) + 1;
	copy = xmalloc(len);
	memcpy(copy, text, len);
	return copy;
}

static char *
xstrndup(const char *text, size_t len)
{
	char *copy;

	copy = xmalloc(len + 1);
	memcpy(copy, text, len);
	copy[len] = '\0';
	return copy;
}

static char *
xasprintf(const char *fmt, ...)
{
	va_list ap;
	va_list ap_copy;
	int needed;
	char *buffer;

	va_start(ap, fmt);
	va_copy(ap_copy, ap);
	needed = vsnprintf(NULL, 0, fmt, ap_copy);
	va_end(ap_copy);
	if (needed < 0) {
		va_end(ap);
		die(EX_SOFTWARE, "snprintf sizing failed");
	}

	buffer = xmalloc((size_t)needed + 1);
	if (vsnprintf(buffer, (size_t)needed + 1, fmt, ap) != needed) {
		va_end(ap);
		die(EX_SOFTWARE, "snprintf formatting failed");
	}
	va_end(ap);

	return buffer;
}

static void
set_owned_string(char **slot, char *value)
{
	free(*slot);
	*slot = value;
}

static void
append_path_component(char **path, size_t *path_len, const char *component)
{
	size_t component_len;

	component_len = strlen(component);
	*path = xrealloc(*path, *path_len + component_len + 2);
	memcpy(*path + *path_len, component, component_len);
	*path_len += component_len;
	(*path)[(*path_len)++] = '!';
	(*path)[*path_len] = '\0';
}

static const char *
skip_token_end(const char *text)
{
	while (*text != '\0' && !isspace((unsigned char)*text))
		text++;
	return text;
}

static bool
parse_from_line(const char *line, struct parsed_from_line *parsed)
{
	static const char remote_marker[] = " remote from ";
	const char *cursor;
	const char *token_end;
	const char *remote;
	char *token;
	char *last_bang;

	memset(parsed, 0, sizeof(*parsed));

	if (strncmp(line, "From ", 5) == 0)
		cursor = line + 5;
	else if (strncmp(line, ">From ", 6) == 0)
		cursor = line + 6;
	else
		return false;

	if (*cursor == '\0')
		die(EX_DATAERR, "corrupted From line: %s", line);

	token_end = skip_token_end(cursor);
	if (token_end == cursor)
		die(EX_DATAERR, "corrupted From line: %s", line);

	remote = strstr(cursor, remote_marker);
	if (remote != NULL) {
		const char *site_start;
		const char *site_end;

		parsed->user = xstrndup(cursor, (size_t)(token_end - cursor));

		site_start = remote + strlen(remote_marker);
		site_end = skip_token_end(site_start);
		if (site_end == site_start)
			die(EX_DATAERR, "corrupted From line: %s", line);
		parsed->system = xstrndup(site_start, (size_t)(site_end - site_start));
		return true;
	}

	token = xstrndup(cursor, (size_t)(token_end - cursor));
	if (token[0] == '!')
		die(EX_DATAERR, "bang starts address: %s", token);

	last_bang = strrchr(token, '!');
	if (last_bang == NULL) {
		parsed->user = token;
		return true;
	}

	*last_bang++ = '\0';
	if (*last_bang == '\0')
		die(EX_DATAERR, "corrupted From line: %s", line);

	parsed->system = xstrdup(token);
	parsed->user = xstrdup(last_bang);
	free(token);
	return true;
}

static const char *
resolve_sendmail_path(void)
{
	const char *override;

	override = getenv("RMAIL_SENDMAIL");
	if (override != NULL && *override != '\0')
		return override;
	return RMAIL_SENDMAIL_PATH;
}

static void
debug_print_value(bool debug, const char *label, const char *value)
{
	if (!debug)
		return;
	fprintf(stderr, "%s: %s\n", label, value);
}

static void
write_stream_or_die(FILE *stream, const char *buffer, size_t length)
{
	if (length == 0)
		return;
	if (fwrite(buffer, 1, length, stream) != length)
		die_with_errno(EX_TEMPFAIL, "write");
}

int
main(int argc, char *argv[])
{
	bool debug;
	bool saw_header;
	bool stdin_is_regular;
	int ch;
	int pipe_fds[2];
	int status;
	pid_t child_pid;
	size_t arg_count;
	size_t path_len;
	char *domain;
	char *line;
	char *from_path;
	char *from_sys;
	char *from_user;
	char *protocol_arg;
	char *from_arg;
	char **sendmail_argv;
	const char *sendmail_path;
	FILE *child_stdin;
	struct stat stdin_stat;
	ssize_t line_size;
	size_t line_capacity;
	off_t body_offset;
	char *first_body_line;
	size_t first_body_length;

	debug = false;
	domain = "UUCP";
	while ((ch = getopt(argc, argv, "D:T")) != -1) {
		switch (ch) {
		case 'D':
			if (optarg == NULL || optarg[0] == '\0')
				die(EX_USAGE, "-D requires a non-empty domain");
			domain = optarg;
			break;
		case 'T':
			debug = true;
			break;
		case '?':
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;
	if (argc < 1)
		usage();

	for (int i = 0; i < argc; i++) {
		if (argv[i][0] == '-')
			die(EX_USAGE, "dash precedes argument: %s", argv[i]);
	}

	if (fstat(STDIN_FILENO, &stdin_stat) != 0)
		die_with_errno(EX_OSERR, "stdin");
	stdin_is_regular = S_ISREG(stdin_stat.st_mode);

	line = NULL;
	line_capacity = 0;
	body_offset = 0;
	first_body_line = NULL;
	first_body_length = 0;
	from_path = NULL;
	from_sys = NULL;
	from_user = NULL;
	path_len = 0;
	saw_header = false;

	for (;;) {
		char *logical_line;
		struct parsed_from_line parsed;

		errno = 0;
		line_size = getline(&line, &line_capacity, stdin);
		if (line_size < 0) {
			if (ferror(stdin))
				die_with_errno(EX_TEMPFAIL, "stdin");
			if (!saw_header)
				die(EX_DATAERR, "no data");
			die(EX_DATAERR, "no data");
		}
		if (line_size == 0 || line[(size_t)line_size - 1] != '\n')
			die(EX_DATAERR, "unterminated input line");

		logical_line = xstrndup(line, (size_t)line_size - 1);
		if (!parse_from_line(logical_line, &parsed)) {
			if (!saw_header)
				die(EX_DATAERR, "missing or empty From line: %s",
				    logical_line);
			first_body_line = xstrdup(line);
			first_body_length = (size_t)line_size;
			free(logical_line);
			break;
		}
		free(logical_line);
		saw_header = true;

		if (parsed.system != NULL) {
			debug_print_value(debug, strstr(line, " remote from ") != NULL ?
			    "remote from" : "bang", parsed.system);
			if (from_sys == NULL)
				from_sys = xstrdup(parsed.system);
			append_path_component(&from_path, &path_len, parsed.system);
		}

		if (parsed.user[0] == '\0')
			set_owned_string(&from_user, xstrdup("<>"));
		else
			set_owned_string(&from_user, parsed.user);
		parsed.user = NULL;

		debug_print_value(debug, "from_sys",
		    from_sys != NULL ? from_sys : "(none)");
		if (from_path != NULL)
			debug_print_value(debug, "from_path", from_path);
		debug_print_value(debug, "from_user", from_user);

		free(parsed.system);
		free(parsed.user);

		if (stdin_is_regular) {
			body_offset = ftello(stdin);
			if (body_offset < 0)
				die_with_errno(EX_TEMPFAIL, "stdin");
		} else if (body_offset == 0) {
			body_offset = 1;
		}
	}

	if (from_user == NULL)
		set_owned_string(&from_user, xstrdup("<>"));

#ifdef QUEUE_ONLY
	{
		const char *delivery_mode = "-odq";
#else
	{
		const char *delivery_mode = "-odi";
#endif
		protocol_arg = NULL;
		from_arg = NULL;
		sendmail_argv = NULL;

		if (from_sys == NULL) {
			protocol_arg = xasprintf("-p%s", domain);
		} else if (strchr(from_sys, '.') == NULL) {
			protocol_arg = xasprintf("-p%s:%s.%s",
			    domain, from_sys, domain);
		} else {
			protocol_arg = xasprintf("-p%s:%s", domain, from_sys);
		}

		from_arg = xasprintf("-f%s%s",
		    from_path != NULL ? from_path : "", from_user);

		arg_count = (size_t)argc + 8;
		sendmail_argv = xmalloc(sizeof(*sendmail_argv) * arg_count);
		sendmail_path = resolve_sendmail_path();

		sendmail_argv[0] = (char *)sendmail_path;
		sendmail_argv[1] = "-G";
		sendmail_argv[2] = "-oee";
		sendmail_argv[3] = (char *)delivery_mode;
		sendmail_argv[4] = "-oi";
		sendmail_argv[5] = protocol_arg;
		sendmail_argv[6] = from_arg;
		for (int i = 0; i < argc; i++) {
			if (strchr(argv[i], ',') != NULL && strchr(argv[i], '<') == NULL)
				sendmail_argv[7 + i] = xasprintf("<%s>", argv[i]);
			else
				sendmail_argv[7 + i] = argv[i];
		}
		sendmail_argv[7 + argc] = NULL;
	}

	if (debug) {
		fprintf(stderr, "Sendmail arguments:\n");
		for (size_t i = 0; sendmail_argv[i] != NULL; i++)
			fprintf(stderr, "\t%s\n", sendmail_argv[i]);
	}

	if (stdin_is_regular) {
		clearerr(stdin);
		if (lseek(STDIN_FILENO, body_offset, SEEK_SET) == (off_t)-1)
			die_with_errno(EX_TEMPFAIL, "stdin seek");
		execv(sendmail_path, sendmail_argv);
		die(EX_UNAVAILABLE, "%s: %s", sendmail_path, strerror(errno));
	}

	if (pipe(pipe_fds) != 0)
		die_with_errno(EX_OSERR, "pipe");

	child_pid = fork();
	if (child_pid < 0)
		die_with_errno(EX_OSERR, "fork");
	if (child_pid == 0) {
		if (dup2(pipe_fds[0], STDIN_FILENO) < 0) {
			fprintf(stderr, "rmail: dup2: %s\n", strerror(errno));
			_exit(EX_OSERR);
		}
		close(pipe_fds[0]);
		close(pipe_fds[1]);
		execv(sendmail_path, sendmail_argv);
		fprintf(stderr, "rmail: %s: %s\n", sendmail_path, strerror(errno));
		_exit(EX_UNAVAILABLE);
	}

	close(pipe_fds[0]);
	child_stdin = fdopen(pipe_fds[1], "w");
	if (child_stdin == NULL)
		die_with_errno(EX_OSERR, "fdopen");

	write_stream_or_die(child_stdin, first_body_line, first_body_length);
	while ((line_size = getline(&line, &line_capacity, stdin)) >= 0)
		write_stream_or_die(child_stdin, line, (size_t)line_size);
	if (ferror(stdin))
		die_with_errno(EX_TEMPFAIL, "stdin");
	if (fclose(child_stdin) != 0)
		die_with_errno(EX_OSERR, "pipe close");

	if (waitpid(child_pid, &status, 0) < 0)
		die_with_errno(EX_OSERR, sendmail_path);
	if (!WIFEXITED(status))
		die(EX_OSERR, "%s: did not terminate normally", sendmail_path);
	if (WEXITSTATUS(status) != 0)
		die(WEXITSTATUS(status), "%s: terminated with %d (non-zero) status",
		    sendmail_path, WEXITSTATUS(status));

	free(line);
	free(first_body_line);
	free(from_path);
	free(from_sys);
	free(from_user);

	return EX_OK;
}
