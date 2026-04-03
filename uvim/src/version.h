/* vi:set ts=8 sts=4 sw=4 noet:
 *
 * MNV - MNV is not Vim		by Bram Moolenaar
 *
 * Do ":help uganda"  in MNV to read copying and usage conditions.
 * Do ":help credits" in MNV to see a list of people who contributed.
 */

/*
 * Define the version number, name, etc.
 * The patchlevel is in included_patches[], in version.c.
 */

// Trick to turn a number into a string.
#define MNV_TOSTR_(a)			#a
#define MNV_TOSTR(a)			MNV_TOSTR_(a)

// Values that change for a new release.
#define MNV_VERSION_MAJOR		9
#define MNV_VERSION_MINOR		2
#define MNV_VERSION_BUILD		286
#define MNV_VERSION_BUILD_BCD		0x11e
#define MNV_VERSION_DATE_ONLY		"2026 Feb 14"

// Values based on the above
#define MNV_VERSION_MAJOR_STR		MNV_TOSTR(MNV_VERSION_MAJOR)
#define MNV_VERSION_MINOR_STR		MNV_TOSTR(MNV_VERSION_MINOR)
#define MNV_VERSION_100	    (MNV_VERSION_MAJOR * 100 + MNV_VERSION_MINOR)

#define MNV_VERSION_BUILD_STR		MNV_TOSTR(MNV_VERSION_BUILD)
#ifndef MNV_VERSION_PATCHLEVEL
# define MNV_VERSION_PATCHLEVEL		0
#endif

// Patchlevel with leading zeros
// For compatibility with the installer from "mnv-win32-installer" and WinGet.
// For details see https://github.com/Project-Tick/Project-Tick-win32-installer/pull/277
// and https://github.com/Project-Tick/Project-Tick-win32-installer/pull/285
#if MNV_VERSION_PATCHLEVEL < 10
# define LEADZERO(x) 000 ## x
#elif MNV_VERSION_PATCHLEVEL < 100
# define LEADZERO(x) 00 ## x
#elif MNV_VERSION_PATCHLEVEL < 1000
# define LEADZERO(x) 0 ## x
#else
# define LEADZERO(x) x
#endif

#define MNV_VERSION_PATCHLEVEL_STR	MNV_TOSTR(LEADZERO(MNV_VERSION_PATCHLEVEL))
// Used by MacOS port; should be one of: development, alpha, beta, final
#define MNV_VERSION_RELEASE		final

/*
 * MNV_VERSION_NODOT is used for the runtime directory name.
 * MNV_VERSION_SHORT is copied into the swap file (max. length is 6 chars).
 * MNV_VERSION_MEDIUM is used for the startup-screen.
 * MNV_VERSION_LONG is used for the ":version" command and "MNV -h".
 */
#define MNV_VERSION_NODOT     "mnv" MNV_VERSION_MAJOR_STR MNV_VERSION_MINOR_STR
#define MNV_VERSION_SHORT     MNV_VERSION_MAJOR_STR "." MNV_VERSION_MINOR_STR
#define MNV_VERSION_MEDIUM    MNV_VERSION_SHORT
#define MNV_VERSION_LONG_ONLY "MNV - MNV is not Vim " MNV_VERSION_MEDIUM
#define MNV_VERSION_LONG_HEAD MNV_VERSION_LONG_ONLY " (" MNV_VERSION_DATE_ONLY
#define MNV_VERSION_LONG      MNV_VERSION_LONG_HEAD ")"
#define MNV_VERSION_LONG_DATE MNV_VERSION_LONG_HEAD ", compiled "
