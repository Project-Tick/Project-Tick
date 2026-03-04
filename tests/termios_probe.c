#define _DEFAULT_SOURCE 1
#define _XOPEN_SOURCE 700

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#ifndef N_TTY
#define N_TTY 0
#endif

struct speed_map {
	unsigned int baud;
	speed_t symbol;
};

static const struct speed_map speed_table[] = {
#ifdef B0
	{ 0U, B0 },
#endif
#ifdef B50
	{ 50U, B50 },
#endif
#ifdef B75
	{ 75U, B75 },
#endif
#ifdef B110
	{ 110U, B110 },
#endif
#ifdef B134
	{ 134U, B134 },
#endif
#ifdef B150
	{ 150U, B150 },
#endif
#ifdef B200
	{ 200U, B200 },
#endif
#ifdef B300
	{ 300U, B300 },
#endif
#ifdef B600
	{ 600U, B600 },
#endif
#ifdef B1200
	{ 1200U, B1200 },
#endif
#ifdef B1800
	{ 1800U, B1800 },
#endif
#ifdef B2400
	{ 2400U, B2400 },
#endif
#ifdef B4800
	{ 4800U, B4800 },
#endif
#ifdef B9600
	{ 9600U, B9600 },
#endif
#ifdef B19200
	{ 19200U, B19200 },
#endif
#ifdef B38400
	{ 38400U, B38400 },
#endif
#ifdef B57600
	{ 57600U, B57600 },
#endif
#ifdef B115200
	{ 115200U, B115200 },
#endif
#ifdef B230400
	{ 230400U, B230400 },
#endif
#ifdef B460800
	{ 460800U, B460800 },
#endif
#ifdef B500000
	{ 500000U, B500000 },
#endif
#ifdef B576000
	{ 576000U, B576000 },
#endif
#ifdef B921600
	{ 921600U, B921600 },
#endif
#ifdef B1000000
	{ 1000000U, B1000000 },
#endif
#ifdef B1152000
	{ 1152000U, B1152000 },
#endif
#ifdef B1500000
	{ 1500000U, B1500000 },
#endif
#ifdef B2000000
	{ 2000000U, B2000000 },
#endif
#ifdef B2500000
	{ 2500000U, B2500000 },
#endif
#ifdef B3000000
	{ 3000000U, B3000000 },
#endif
#ifdef B3500000
	{ 3500000U, B3500000 },
#endif
#ifdef B4000000
	{ 4000000U, B4000000 },
#endif
};

static unsigned int
speed_to_baud(speed_t symbol)
{
	for (size_t i = 0; i < sizeof(speed_table) / sizeof(speed_table[0]); i++) {
		if (speed_table[i].symbol == symbol)
			return speed_table[i].baud;
	}
	return (unsigned int)symbol;
}

static void
print_cc(const char *name, const struct termios *termios, size_t index)
{
	printf("%s=%u\n", name, (unsigned int)termios->c_cc[index]);
}

int
main(int argc, char *argv[])
{
	struct termios termios;
	struct winsize winsize;
	int fd;
	int line_discipline;

	if (argc != 2) {
		fprintf(stderr, "usage: %s path\n", argv[0]);
		return 1;
	}

	fd = open(argv[1], O_RDONLY | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
	if (fd < 0) {
		fprintf(stderr, "termios_probe: open: %s\n", strerror(errno));
		return 1;
	}
	if (tcgetattr(fd, &termios) != 0) {
		fprintf(stderr, "termios_probe: tcgetattr: %s\n", strerror(errno));
		close(fd);
		return 1;
	}
	memset(&winsize, 0, sizeof(winsize));
	if (ioctl(fd, TIOCGWINSZ, &winsize) != 0) {
		fprintf(stderr, "termios_probe: TIOCGWINSZ: %s\n", strerror(errno));
		close(fd);
		return 1;
	}

	printf("ispeed=%u\n", speed_to_baud(cfgetispeed(&termios)));
	printf("ospeed=%u\n", speed_to_baud(cfgetospeed(&termios)));
	printf("row=%u\n", winsize.ws_row);
	printf("col=%u\n", winsize.ws_col);

#ifdef TIOCGETD
	if (ioctl(fd, TIOCGETD, &line_discipline) == 0)
		printf("ldisc=%d\n", line_discipline);
#else
	printf("ldisc=%d\n", N_TTY);
#endif

	printf("echo=%d\n", (termios.c_lflag & ECHO) != 0);
	printf("icanon=%d\n", (termios.c_lflag & ICANON) != 0);
	printf("isig=%d\n", (termios.c_lflag & ISIG) != 0);
	printf("iexten=%d\n", (termios.c_lflag & IEXTEN) != 0);
#ifdef EXTPROC
	printf("extproc=%d\n", (termios.c_lflag & EXTPROC) != 0);
#endif
	printf("opost=%d\n", (termios.c_oflag & OPOST) != 0);
	printf("onlcr=%d\n", (termios.c_oflag & ONLCR) != 0);
	printf("ixany=%d\n", (termios.c_iflag & IXANY) != 0);
	printf("ixon=%d\n", (termios.c_iflag & IXON) != 0);
	printf("ixoff=%d\n", (termios.c_iflag & IXOFF) != 0);
	printf("icrnl=%d\n", (termios.c_iflag & ICRNL) != 0);
	printf("inlcr=%d\n", (termios.c_iflag & INLCR) != 0);
	printf("igncr=%d\n", (termios.c_iflag & IGNCR) != 0);
	printf("parenb=%d\n", (termios.c_cflag & PARENB) != 0);
	printf("parodd=%d\n", (termios.c_cflag & PARODD) != 0);
#ifdef CRTSCTS
	printf("crtscts=%d\n", (termios.c_cflag & CRTSCTS) != 0);
#endif

	switch (termios.c_cflag & CSIZE) {
	case CS5:
		printf("csize=5\n");
		break;
	case CS6:
		printf("csize=6\n");
		break;
	case CS7:
		printf("csize=7\n");
		break;
	default:
		printf("csize=8\n");
		break;
	}

#ifdef VINTR
	print_cc("intr", &termios, VINTR);
#endif
#ifdef VMIN
	print_cc("min", &termios, VMIN);
#endif
#ifdef VTIME
	print_cc("time", &termios, VTIME);
#endif

	close(fd);
	return 0;
}
