/* vi:set ts=8 sts=4 sw=4 noet:
 *
 * MNV - MNV is not Vim	by Bram Moolenaar
 *
 * Do ":help uganda"  in MNV to read copying and usage conditions.
 * Do ":help credits" in MNV to see a list of people who contributed.
 */

#ifdef __QNXNTO__
# include <sys/procmgr.h>
#endif

#define	USE_TMPNAM

#define POSIX	    // Used by pty.c

#if defined(FEAT_GUI_PHOTON)
extern int is_photon_available;
#endif
