/* vi:set ts=8 sts=4 sw=4 noet:
 *
 * MNV - MNV is not Vim	by Bram Moolenaar
 *
 * Do ":help uganda"  in MNV to read copying and usage conditions.
 * Do ":help credits" in MNV to see a list of people who contributed.
 */

/*
 * Amiga Machine-dependent things
 */

#define CASE_INSENSITIVE_FILENAME   // ignore case when comparing file names
#define SPACE_IN_FILENAME
#define USE_FNAME_CASE		    // adjust case of file names
#define USE_TERM_CONSOLE
#define HAVE_AVAIL_MEM

#ifndef HAVE_CONFIG_H
# if defined(AZTEC_C) || defined(__amigaos4__)
#  define HAVE_STAT_H
# endif
# define HAVE_LOCALE_H
# define HAVE_STDLIB_H
# define HAVE_STRING_H
# define HAVE_FCNTL_H
# define HAVE_STRICMP
# define HAVE_STRNICMP
# define HAVE_STRFTIME	    // guessed
# define HAVE_SETENV
# define HAVE_MEMSET
# define HAVE_QSORT
# if defined(__DATE__) && defined(__TIME__)
#  define HAVE_DATE_TIME
# endif

#endif // HAVE_CONFIG_H

#ifndef	DFLT_ERRORFILE
# define DFLT_ERRORFILE		"AztecC.Err"	// Should this change?
#endif

#ifndef	DFLT_RUNTIMEPATH
# define DFLT_RUNTIMEPATH "MNV:mnvfiles,MNV:,MNV:mnvfiles/after"
#endif
#ifndef	CLEAN_RUNTIMEPATH
# define CLEAN_RUNTIMEPATH "MNV:mnvfiles,MNV:,MNV:mnvfiles/after"
#endif

#ifndef	BASENAMELEN
# define BASENAMELEN	26	// Amiga
#endif

#ifndef	TEMPNAME
# define TEMPNAME	"t:v?XXXXXX"
# define TEMPNAMELEN	12
#endif

#include <exec/types.h>
#include <libraries/dos.h>
#include <libraries/dosextens.h>

// Currently, all Amiga compilers except AZTEC C have these...
#ifndef AZTEC_C
# include <proto/exec.h>
# include <proto/dos.h>
# include <proto/intuition.h>
#endif

#define FNAME_ILLEGAL ";*?`#%" // illegal characters in a file name

/*
 * Manx doesn't have off_t, define it here.
 */
#ifdef AZTEC_C
typedef long off_t;
#endif

#ifdef LATTICE
# define USE_TMPNAM	// use tmpnam() instead of mktemp()
#endif

#ifdef __GNUC__
# include <sys/stat.h>
# include <unistd.h>
# include <limits.h>
# include <errno.h>
# include <pwd.h>
# include <grp.h>
# include <dirent.h>
#endif

#include <time.h>	// for strftime() and others

/*
 * arpbase.h must be included before functions.h
 */
#ifdef FEAT_ARP
# include <libraries/arpbase.h>
#endif

/*
 * This won't be needed if you have a version of Lattice 4.01 without broken
 * break signal handling.
 */
#include <signal.h>

/*
 * Names for the EXRC, HELP and temporary files.
 * Some of these may have been defined in the makefile.
 */
#ifndef SYS_MNVRC_FILE
# define SYS_MNVRC_FILE "MNV:mnvrc"
#endif
#ifndef SYS_GMNVRC_FILE
# define SYS_GMNVRC_FILE "MNV:gmnvrc"
#endif
#ifndef SYS_MENU_FILE
# define SYS_MENU_FILE	"MNV:menu.mnv"
#endif
#ifndef DFLT_HELPFILE
# define DFLT_HELPFILE	"MNV:doc/help.txt"
#endif
#ifndef SYNTAX_FNAME
# define SYNTAX_FNAME	"MNV:syntax/%s.mnv"
#endif

#ifndef USR_EXRC_FILE
# define USR_EXRC_FILE	"HOME:.exrc"
#endif
#ifndef USR_EXRC_FILE2
# define USR_EXRC_FILE2	"MNV:.exrc"
#endif

#ifndef USR_MNVRC_FILE
# define USR_MNVRC_FILE	"HOME:.mnvrc"
#endif
#ifndef USR_MNVRC_FILE2
# define USR_MNVRC_FILE2 "MNV:.mnvrc"
#endif
#ifndef USR_MNVRC_FILE3
# define USR_MNVRC_FILE3 "HOME:mnvfiles/mnvrc"
#endif
#ifndef USR_MNVRC_FILE4
# define USR_MNVRC_FILE4 "S:.mnvrc"
#endif
#ifndef MNV_DEFAULTS_FILE
# define MNV_DEFAULTS_FILE "MNV:defaults.mnv"
#endif
#ifndef EMNV_FILE
# define EMNV_FILE	"MNV:emnv.mnv"
#endif

#ifndef USR_GMNVRC_FILE
# define USR_GMNVRC_FILE "HOME:.gmnvrc"
#endif
#ifndef USR_GMNVRC_FILE2
# define USR_GMNVRC_FILE2 "MNV:.gmnvrc"
#endif
#ifndef USR_GMNVRC_FILE3
# define USR_GMNVRC_FILE3 "HOME:mnvfiles/gmnvrc"
#endif
#ifndef USR_GMNVRC_FILE4
# define USR_GMNVRC_FILE4 "S:.gmnvrc"
#endif

#ifdef FEAT_MNVINFO
# ifndef MNVINFO_FILE
#  define MNVINFO_FILE	"MNV:.mnvinfo"
# endif
#endif

#ifndef EXRC_FILE
# define EXRC_FILE	".exrc"
#endif

#ifndef MNVRC_FILE
# define MNVRC_FILE	".mnvrc"
#endif

#ifndef GMNVRC_FILE
# define GMNVRC_FILE	".gmnvrc"
#endif

#ifndef DFLT_BDIR
# define DFLT_BDIR	"T:"		// default for 'backupdir'
#endif

#ifndef DFLT_DIR
# define DFLT_DIR	"T:"		// default for 'directory'
#endif

#ifndef DFLT_VDIR
# define DFLT_VDIR	"HOME:mnvfiles/view"	// default for 'viewdir'
#endif

#ifndef DFLT_MAXMEM
# define DFLT_MAXMEM	256		// use up to 256Kbyte for buffer
#endif
#ifndef DFLT_MAXMEMTOT
# define DFLT_MAXMEMTOT	0		// decide in set_init
#endif

#if defined(SASC)
int setenv(const char *, const char *);
#endif

#define mch_remove(x) remove((char *)(x))
#define mch_rename(src, dst) rename(src, dst)
#define mch_chdir(s) chdir(s)
#define mnv_mkdir(x, y) mch_mkdir(x)
