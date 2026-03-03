#ifndef SH_SIGNAMES_H
#define SH_SIGNAMES_H

#include <stdbool.h>
#include <stddef.h>

bool sh_signal_name(int signum, char *buffer, size_t buffer_size);
bool sh_signal_number(const char *name, int *signum);
int sh_signal_max(void);

#endif
