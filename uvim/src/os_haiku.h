/* vi:set ts=8 sts=4 sw=4:
 *
 * MNV - MNV is not Vim	by Bram Moolenaar
 *	  Haiku port	by Siarzhuk Zharski
 *
 * Do ":help uganda"  in MNV to read copying and usage conditions.
 * Do ":help credits" in MNV to see a list of people who contributed.
 */

/*
 * os_haiku.h
 */

#define USE_TERM_CONSOLE

#define USR_MNV_DIR "$BE_USER_SETTINGS/mnv"

#define USR_EXRC_FILE	USR_MNV_DIR "/exrc"
#define USR_EXRC_FILE2	USR_MNV_DIR "/mnv/exrc"
#define USR_MNVRC_FILE	USR_MNV_DIR "/mnvrc"
#define USR_MNVRC_FILE2	USR_MNV_DIR "/mnv/mnvrc"
#define USR_GMNVRC_FILE	USR_MNV_DIR "/gmnvrc"
#define USR_GMNVRC_FILE2	USR_MNV_DIR "/mnv/gmnvrc"
#define MNVINFO_FILE	USR_MNV_DIR "/mnvinfo"

#ifdef RUNTIME_GLOBAL
# ifdef RUNTIME_GLOBAL_AFTER
#  define DFLT_RUNTIMEPATH	USR_MNV_DIR "," RUNTIME_GLOBAL ",$MNVRUNTIME," RUNTIME_GLOBAL_AFTER "," USR_MNV_DIR "/after"
#  define XDG_RUNTIMEPATH	"$XDG_CONFIG_HOME/mnv," RUNTIME_GLOBAL ",$MNVRUNTIME," RUNTIME_GLOBAL_AFTER ",$XDG_CONFIG_HOME/mnv/after"
#  define XDG_RUNTIMEPATH_FB	"~/config/settings/mnv," RUNTIME_GLOBAL ",$MNVRUNTIME," RUNTIME_GLOBAL_AFTER ",~/config/settings/mnv/after"
#  define CLEAN_RUNTIMEPATH	RUNTIME_GLOBAL ",$MNVRUNTIME," RUNTIME_GLOBAL_AFTER
# else
#  define DFLT_RUNTIMEPATH	USR_MNV_DIR "," RUNTIME_GLOBAL ",$MNVRUNTIME," RUNTIME_GLOBAL "/after," USR_MNV_DIR "/after"
#  define XDG_RUNTIMEPATH	"$XDG_CONFIG_HOME/mnv," RUNTIME_GLOBAL ",$MNVRUNTIME," RUNTIME_GLOBAL "/after,$XDG_CONFIG_HOME/mnv/after"
#  define XDG_RUNTIMEPATH_FB	"~/config/settings/mnv," RUNTIME_GLOBAL ",$MNVRUNTIME," RUNTIME_GLOBAL "/after,~/config/settings/mnv/after"
#  define CLEAN_RUNTIMEPATH	RUNTIME_GLOBAL ",$MNVRUNTIME," RUNTIME_GLOBAL "/after"
# endif
#else
# define DFLT_RUNTIMEPATH	USR_MNV_DIR ",$MNV/mnvfiles,$MNVRUNTIME,$MNV/mnvfiles/after," USR_MNV_DIR "/after"
# define XDG_RUNTIMEPATH	"$XDG_CONFIG_HOME/mnv,$MNV/mnvfiles,$MNVRUNTIME,$MNV/mnvfiles/after,$XDG_CONFIG_HOME/mnv/after"
# define XDG_RUNTIMEPATH_FB	"~/config/settings/mnv,$MNV/mnvfiles,$MNVRUNTIME,$MNV/mnvfiles/after,~/config/settings/mnv/after"
# define CLEAN_RUNTIMEPATH	"$MNV/mnvfiles,$MNVRUNTIME,$MNV/mnvfiles/after"
#endif
