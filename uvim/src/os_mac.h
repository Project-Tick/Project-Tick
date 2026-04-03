/* vi:set ts=8 sts=4 sw=4 noet:
 *
 * MNV - MNV is not Vim	by Bram Moolenaar
 *
 * Do ":help uganda"  in MNV to read copying and usage conditions.
 * Do ":help credits" in MNV to see a list of people who contributed.
 */

#ifndef OS_MAC__H
#define OS_MAC__H

// Before Including the MacOS specific files,
// let's set the OPAQUE_TOOLBOX_STRUCTS to 0 so we
// can access the internal structures.
// (Until fully Carbon compliant)
// TODO: Can we remove this? (Dany)
#if 0
# define OPAQUE_TOOLBOX_STRUCTS 0
#endif

// Include MAC_OS_X_VERSION_* macros
#ifdef HAVE_AVAILABILITYMACROS_H
# include <AvailabilityMacros.h>
#endif

/*
 * Unix interface
 */
#if defined(__APPLE_CC__) // for Project Builder and ...
# include <unistd.h>
// Get stat.h or something similar. Comment: How come some OS get in mnv.h
# include <sys/stat.h>
// && defined(HAVE_CURSE)
// The curses.h from MacOS X provides by default some BACKWARD compatibility
// definition which can cause us problem later on. So we undefine a few of them.
# include <curses.h>
# undef reg
# undef ospeed
// OK defined to 0 in MacOS X 10.2 curses!  Remove it, we define it to be 1.
# undef OK
#endif
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <dirent.h>

/*
 * Mach interface
 */
#include <mach/task.h>

/*
 * MacOS specific #define
 */

// This will go away when CMD_KEY fully tested
#define USE_CMD_KEY
// On MacOS X use the / not the :
// TODO: Should file such as ~/.mnvrc reside instead in
//       ~/Library/MNV or ~/Library/Preferences/org.mnv.mnv/ ? (Dany)
// When compiled under MacOS X (including CARBON version)
// we use the Unix File path style.  Also when UNIX is defined.
#define USE_UNIXFILENAME


/*
 * Generic MNV #define for Mac
 */

#define FEAT_SOURCE_FFS
#define FEAT_SOURCE_FF_MAC

#define USE_EXE_NAME		    // to find  $MNV
#define CASE_INSENSITIVE_FILENAME   // ignore case when comparing file names
#define SPACE_IN_FILENAME

#define USE_FNAME_CASE		// make ":e os_Mac.c" open the file in its
				// original case, as "os_mac.c"
#define BINARY_FILE_IO
#define EOL_DEFAULT EOL_MAC
#define HAVE_AVAIL_MEM

#ifndef HAVE_CONFIG_H
# define HAVE_STRING_H
# define HAVE_MEMSET
# define USE_TMPNAM		// use tmpnam() instead of mktemp()
# define HAVE_FCNTL_H
# define HAVE_QSORT
# define HAVE_ST_MODE		// have stat.st_mode
# define HAVE_MATH_H

# if defined(__DATE__) && defined(__TIME__)
#  define HAVE_DATE_TIME
# endif
# define HAVE_STRFTIME
#endif

/*
 * Names for the EXRC, HELP and temporary files.
 * Some of these may have been defined in the makefile.
 */

#ifndef SYS_MNVRC_FILE
# define SYS_MNVRC_FILE "$MNV/mnvrc"
#endif
#ifndef SYS_GMNVRC_FILE
# define SYS_GMNVRC_FILE "$MNV/gmnvrc"
#endif
#ifndef SYS_MENU_FILE
# define SYS_MENU_FILE	"$MNVRUNTIME/menu.mnv"
#endif
#ifndef SYS_OPTWIN_FILE
# define SYS_OPTWIN_FILE "$MNVRUNTIME/optwin.mnv"
#endif
#ifndef MNV_DEFAULTS_FILE
# define MNV_DEFAULTS_FILE "$MNVRUNTIME/defaults.mnv"
#endif
#ifndef EMNV_FILE
# define EMNV_FILE	"$MNVRUNTIME/emnv.mnv"
#endif

#ifdef FEAT_GUI
# ifndef USR_GMNVRC_FILE
#  define USR_GMNVRC_FILE "~/.gmnvrc"
# endif
# ifndef GMNVRC_FILE
#  define GMNVRC_FILE	"_gmnvrc"
# endif
#endif
#ifndef USR_MNVRC_FILE
# define USR_MNVRC_FILE	"~/.mnvrc"
#endif

#ifndef USR_EXRC_FILE
# define USR_EXRC_FILE	"~/.exrc"
#endif

#ifndef MNVRC_FILE
# define MNVRC_FILE	"_mnvrc"
#endif

#ifndef EXRC_FILE
# define EXRC_FILE	"_exrc"
#endif

#ifndef DFLT_HELPFILE
# define DFLT_HELPFILE	"$MNVRUNTIME/doc/help.txt"
#endif

#ifndef SYNTAX_FNAME
# define SYNTAX_FNAME	"$MNVRUNTIME/syntax/%s.mnv"
#endif

#ifdef FEAT_MNVINFO
# ifndef MNVINFO_FILE
#  define MNVINFO_FILE	"~/.mnvinfo"
# endif
#endif // FEAT_MNVINFO

#ifndef DFLT_BDIR
# define DFLT_BDIR	"."	// default for 'backupdir'
#endif

#ifndef DFLT_DIR
# define DFLT_DIR	"."	// default for 'directory'
#endif

#ifndef DFLT_VDIR
# define DFLT_VDIR	"$MNV/mnvfiles/view"	// default for 'viewdir'
#endif

#define DFLT_ERRORFILE		"errors.err"

#ifndef DFLT_RUNTIMEPATH
# define DFLT_RUNTIMEPATH	"~/.mnv,$MNV/mnvfiles,$MNVRUNTIME,$MNV/mnvfiles/after,~/.mnv/after"
#endif
#ifndef CLEAN_RUNTIMEPATH
# define CLEAN_RUNTIMEPATH	"$MNV/mnvfiles,$MNVRUNTIME,$MNV/mnvfiles/after"
#endif

/*
 * Macintosh has plenty of memory, use large buffers
 */
#define CMDBUFFSIZE 1024	// size of the command processing buffer

#ifndef DFLT_MAXMEM
# define DFLT_MAXMEM	512	// use up to  512 Kbyte for buffer
#endif

#ifndef DFLT_MAXMEMTOT
# define DFLT_MAXMEMTOT	2048	// use up to 2048 Kbyte for MNV
#endif

#define WILDCHAR_LIST "*?[{`$"

/**************/
#define mch_rename(src, dst) rename(src, dst)
#define mch_remove(x) unlink((char *)(x))
#ifndef mch_getenv
# if defined(__APPLE_CC__)
#  define mch_getenv(name)  ((char_u *)getenv((char *)(name)))
#  define mch_setenv(name, val, x) setenv(name, val, x)
# else
  // mnv_getenv() is in pty.c
#  define USE_MNVPTY_GETENV
#  define mch_getenv(x) mnvpty_getenv(x)
#  define mch_setenv(name, val, x) setenv(name, val, x)
# endif
#endif

#ifndef HAVE_CONFIG_H
# ifdef __APPLE_CC__
// Assuming compiling for MacOS X
// Trying to take advantage of the prebinding
#  define HAVE_TGETENT
#  define OSPEED_EXTERN
#  define UP_BC_PC_EXTERN
# endif
#endif

// Some "prep work" definition to be able to compile the MacOS X
// version with os_unix.c instead of os_mac.c. Based on the result
// of ./configure for console MacOS X.

#ifndef SIGPROTOARG
# define SIGPROTOARG	(int)
#endif
#ifndef SIGDEFARG
# define SIGDEFARG(s)	(s) int s UNUSED;
#endif
#ifndef SIGDUMMYARG
# define SIGDUMMYARG	0
#endif
#undef  HAVE_AVAIL_MEM
#ifndef HAVE_CONFIG_H
//# define USE_SYSTEM  // Output ship do debugger :(, but not compile
# define HAVE_SYS_WAIT_H 1 // Attempt
# define HAVE_TERMIOS_H 1
# define SYS_SELECT_WITH_SYS_TIME 1
# define HAVE_SELECT 1
# define HAVE_SYS_SELECT_H 1
# define HAVE_PUTENV
# define HAVE_SETENV
# define HAVE_RENAME
#endif

#if !defined(HAVE_CONFIG_H)
# define HAVE_PUTENV
#endif

// A Mac constant causing big problem to syntax highlighting
#define UNKNOWN_CREATOR '\?\?\?\?'

#ifdef FEAT_RELTIME

# include <dispatch/dispatch.h>

# if !defined(MAC_OS_X_VERSION_10_12) \
	|| (MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_12)
typedef int clockid_t;
# endif
# ifndef CLOCK_REALTIME
#  define CLOCK_REALTIME 0
# endif
# ifndef CLOCK_MONOTONIC
#  define CLOCK_MONOTONIC 1
# endif

struct itimerspec
{
    struct timespec it_interval;  // timer period
    struct timespec it_value;	  // initial expiration
};

struct sigevent;

struct macos_timer
{
    dispatch_queue_t tim_queue;
    dispatch_source_t tim_timer;
    void (*tim_func)(union sigval);
    void *tim_arg;
};

typedef struct macos_timer *timer_t;

extern int timer_create(
    clockid_t clockid,
    struct sigevent *sevp,
    timer_t *timerid);

extern int timer_delete(timer_t timerid);

extern int timer_settime(
    timer_t timerid, int flags,
    const struct itimerspec *new_value,
    struct itimerspec *unused);

#endif // FEAT_RELTIME

#endif // OS_MAC__H
