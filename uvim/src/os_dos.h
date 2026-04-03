/* vi:set ts=8 sts=4 sw=4 noet:
 *
 * MNV - MNV is not Vim	by Bram Moolenaar
 *
 * Do ":help uganda"  in MNV to read copying and usage conditions.
 * Do ":help credits" in MNV to see a list of people who contributed.
 */

/*
 * Common MS-DOS and Win32 (Windows NT and Windows 95) defines.
 *
 * Names for the EXRC, HELP and temporary files.
 * Some of these may have been defined in the makefile or feature.h.
 */

#ifndef SYS_MNVRC_FILE
# define SYS_MNVRC_FILE		"$MNV\\mnvrc"
#endif
#ifndef USR_MNVRC_FILE
# define USR_MNVRC_FILE		"$HOME\\_mnvrc"
#endif
#ifndef USR_MNVRC_FILE2
# define USR_MNVRC_FILE2	"$HOME\\mnvfiles\\mnvrc"
#endif
#ifndef USR_MNVRC_FILE3
# define USR_MNVRC_FILE3	"$MNV\\_mnvrc"
#endif
#ifndef MNV_DEFAULTS_FILE
# define MNV_DEFAULTS_FILE	"$MNVRUNTIME\\defaults.mnv"
#endif
#ifndef EMNV_FILE
# define EMNV_FILE		"$MNVRUNTIME\\emnv.mnv"
#endif

#ifndef USR_EXRC_FILE
# define USR_EXRC_FILE		"$HOME\\_exrc"
#endif
#ifndef USR_EXRC_FILE2
# define USR_EXRC_FILE2		"$MNV\\_exrc"
#endif

#ifdef FEAT_GUI
# ifndef SYS_GMNVRC_FILE
#  define SYS_GMNVRC_FILE	"$MNV\\gmnvrc"
# endif
# ifndef USR_GMNVRC_FILE
#  define USR_GMNVRC_FILE	"$HOME\\_gmnvrc"
# endif
# ifndef USR_GMNVRC_FILE2
#  define USR_GMNVRC_FILE2	"$HOME\\mnvfiles\\gmnvrc"
# endif
# ifndef USR_GMNVRC_FILE3
#  define USR_GMNVRC_FILE3	"$MNV\\_gmnvrc"
# endif
# ifndef SYS_MENU_FILE
#  define SYS_MENU_FILE		"$MNVRUNTIME\\menu.mnv"
# endif
#endif

#ifndef SYS_OPTWIN_FILE
# define SYS_OPTWIN_FILE	"$MNVRUNTIME\\optwin.mnv"
#endif

#ifdef FEAT_MNVINFO
# ifndef MNVINFO_FILE
#  define MNVINFO_FILE		"$HOME\\_mnvinfo"
# endif
# ifndef MNVINFO_FILE2
#  define MNVINFO_FILE2		"$MNV\\_mnvinfo"
# endif
#endif

#ifndef MNVRC_FILE
# define MNVRC_FILE	"_mnvrc"
#endif

#ifndef EXRC_FILE
# define EXRC_FILE	"_exrc"
#endif

#ifdef FEAT_GUI
# ifndef GMNVRC_FILE
#  define GMNVRC_FILE	"_gmnvrc"
# endif
#endif

#ifndef DFLT_HELPFILE
# define DFLT_HELPFILE	"$MNVRUNTIME\\doc\\help.txt"
#endif

#ifndef SYNTAX_FNAME
# define SYNTAX_FNAME	"$MNVRUNTIME\\syntax\\%s.mnv"
#endif

#ifndef DFLT_BDIR
# define DFLT_BDIR	".,$TEMP,c:\\tmp,c:\\temp" // default for 'backupdir'
#endif

#ifndef DFLT_VDIR
# define DFLT_VDIR	"$HOME/mnvfiles/view"	// default for 'viewdir'
#endif

#ifndef DFLT_DIR
# define DFLT_DIR	".,$TEMP,c:\\tmp,c:\\temp" // default for 'directory'
#endif

#define DFLT_ERRORFILE		"errors.err"
#define DFLT_RUNTIMEPATH	"$HOME/mnvfiles,$MNV/mnvfiles,$MNVRUNTIME,$HOME/mnvfiles/after,$MNV/mnvfiles/after"
#define CLEAN_RUNTIMEPATH	"$MNV/mnvfiles,$MNVRUNTIME,$MNV/mnvfiles/after"

#define CASE_INSENSITIVE_FILENAME   // ignore case when comparing file names
#define SPACE_IN_FILENAME
#define BACKSLASH_IN_FILENAME
#define USE_CRNL		// lines end in CR-NL instead of NL
#define HAVE_DUP		// have dup()
#define HAVE_ST_MODE		// have stat.st_mode
