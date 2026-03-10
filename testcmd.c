/*
 * Reuse the standalone test(1) parser as the shell builtin entry point.
 * The external source now exports main() and exits on syntax errors, so
 * provide a wrapper that maps its process-exit path back to a builtin
 * return value.
 */

#include <setjmp.h>
#include <stdio.h>

static jmp_buf testcmd_jmp;
static int testcmd_status;

static void
testcmd_exit(int status)
{
	testcmd_status = status;
	fflush(NULL);
	longjmp(testcmd_jmp, 1);
}

#define exit testcmd_exit
#define main testcmd_main
#include "../../bin/test/test.c"
#undef main
#undef exit

int
testcmd(int argc, char **argv)
{
	if (setjmp(testcmd_jmp) != 0)
		return testcmd_status;
	return testcmd_main(argc, argv);
}
