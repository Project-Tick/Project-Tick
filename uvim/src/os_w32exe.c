/* vi:set ts=8 sts=4 sw=4 noet:
 *
 * MNV - MNV is not Vim		by Bram Moolenaar
 *				GUI support by Robert Webb
 *
 * Do ":help uganda"  in MNV to read copying and usage conditions.
 * Do ":help credits" in MNV to see a list of people who contributed.
 * See README.txt for an overview of the MNV source code.
 */
/*
 * Windows GUI/Console: main program (EXE) entry point:
 *
 * Ron Aaron <ronaharon@yahoo.com> wrote this and the DLL support code.
 * Adapted by Ken Takata.
 */
#include "mnv.h"

// cproto doesn't create a prototype for MNVMain()
#ifdef MNVDLL
__declspec(dllimport)
#endif
int MNVMain(int argc, char **argv);

#ifdef MNVDLL
# define SaveInst(hInst)    // Do nothing
#else
void SaveInst(HINSTANCE hInst);
#endif

#ifdef FEAT_GUI
    int WINAPI
wWinMain(
    HINSTANCE	hInstance,
    HINSTANCE	hPrevInst UNUSED,
    LPWSTR	lpszCmdLine UNUSED,
    int		nCmdShow UNUSED)
{
    SaveInst(hInstance);
    return MNVMain(0, NULL);
}
#else
    int
wmain(int argc UNUSED, wchar_t **argv UNUSED)
{
    SaveInst(GetModuleHandleW(NULL));
    return MNVMain(0, NULL);
}
#endif

#ifdef USE_OWNSTARTUP
// Use our own entry point and don't use the default CRT startup code to
// reduce the size of (g)mnv.exe.  This works only when MNVDLL is defined.
//
// For MSVC, the /GS- compiler option is needed to avoid the undefined symbol
// error.  (It disables the security check. However, it affects only this
// function and doesn't have any effect on MNV itself.)
// For MinGW, the -nostdlib compiler option and the --entry linker option are
// needed.
# ifdef FEAT_GUI
    void WINAPI
wWinMainCRTStartup(void)
{
    MNVMain(0, NULL);
}
# else
    void
wmainCRTStartup(void)
{
    MNVMain(0, NULL);
}
# endif
#endif	// USE_OWNSTARTUP


#if defined(MNVDLL) && defined(FEAT_MZSCHEME)

# if defined(_MSC_VER)
static __declspec(thread) void *tls_space;
extern intptr_t _tls_index;
# elif defined(__MINGW32__)
static __thread void *tls_space;
extern intptr_t _tls_index;
# endif

// Get TLS information that is needed for if_mzsch.
    __declspec(dllexport) void
get_tls_info(void ***ptls_space, intptr_t *ptls_index)
{
    *ptls_space = &tls_space;
    *ptls_index = _tls_index;
    return;
}
#endif
