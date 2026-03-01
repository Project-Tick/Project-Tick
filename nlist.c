/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2026 Project Tick. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "ps.h"

fixpt_t	ccpu = 0;
int	nlistread = 0;
unsigned long mem_total_kb = 0;
int	fscale = 100;

/*
 * On Linux, we read memory stats from /proc/meminfo.
 * ccpu and fscale are used for CPU calculations; we'll use a simplified model.
 */
int
donlist(void)
{
	FILE *fp;
	char line[256];

	if (nlistread)
		return (0);

	fp = fopen("/proc/meminfo", "r");
	if (fp) {
		while (fgets(line, sizeof(line), fp)) {
			if (sscanf(line, "MemTotal: %lu", &mem_total_kb) == 1) {
				break;
			}
		}
		fclose(fp);
	}

	if (mem_total_kb == 0)
		mem_total_kb = 1024 * 256; /* fallback */

	/* 
	 * Linux fixed-point scale for load averages is usually 1 << 8, 
	 * but for ps pct calculation we just need a baseline.
	 */
	fscale = 100;
	nlistread = 1;
	return (0);
}
