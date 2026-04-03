/* vi:set ts=8 sts=4 sw=4 noet:
 *
 * MNV - MNV is not Vim	by Bram Moolenaar
 *
 * Do ":help uganda"  in MNV to read copying and usage conditions.
 * Do ":help credits" in MNV to see a list of people who contributed.
 * See README.txt for an overview of the MNV source code.
 */

/*
 * kword_test.c: Unittests for mnv_iswordc() and mnv_iswordp().
 */

#undef NDEBUG
#include <assert.h>

// Must include main.c because it contains much more than just main()
#define NO_MNV_MAIN
#include "main.c"

// This file has to be included because the tested functions are static
#include "charset.c"

/*
 * Test the results of mnv_iswordc() and mnv_iswordp() are matched.
 */
    static void
test_isword_funcs_utf8(void)
{
    buf_T buf;
    int c;

    CLEAR_FIELD(buf);
    p_enc = (char_u *)"utf-8";
    p_isi = (char_u *)"";
    p_isp = (char_u *)"";
    p_isf = (char_u *)"";
    buf.b_p_isk = (char_u *)"@,48-57,_,128-167,224-235";

    curbuf = &buf;
    mb_init(); // calls init_chartab()

    for (c = 0; c < 0x10000; ++c)
    {
	char_u p[4] = {0};
	int c1;
	int retc;
	int retp;

	utf_char2bytes(c, p);
	c1 = utf_ptr2char(p);
	if (c != c1)
	{
	    fprintf(stderr, "Failed: ");
	    fprintf(stderr,
		    "[c = %#04x, p = {%#02x, %#02x, %#02x}] ",
		    c, p[0], p[1], p[2]);
	    fprintf(stderr, "c != utf_ptr2char(p) (=%#04x)\n", c1);
	    abort();
	}
	retc = mnv_iswordc_buf(c, &buf);
	retp = mnv_iswordp_buf(p, &buf);
	if (retc != retp)
	{
	    fprintf(stderr, "Failed: ");
	    fprintf(stderr,
		    "[c = %#04x, p = {%#02x, %#02x, %#02x}] ",
		    c, p[0], p[1], p[2]);
	    fprintf(stderr, "mnv_iswordc(c) (=%d) != mnv_iswordp(p) (=%d)\n",
		    retc, retp);
	    abort();
	}
    }
}

    int
main(void)
{
    estack_init();
    test_isword_funcs_utf8();
    return 0;
}
