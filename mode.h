#pragma once

#include <sys/types.h>

void *mode_compile(const char *mode_string);
mode_t mode_apply(const void *compiled, mode_t old_mode);
void mode_free(void *compiled);
