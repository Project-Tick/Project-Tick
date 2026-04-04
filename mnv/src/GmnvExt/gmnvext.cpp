/* vi:set ts=8 sts=4 sw=4:
 *
 * MNV - MNV is not Vim	gmnvext by Tianmiao Hu
 *
 * Do ":help uganda"  in MNV to read copying and usage conditions.
 * Do ":help credits" in MNV to see a list of people who contributed.
 */

/*
 * gmnvext is a DLL which is used for the "Edit with MNV" context menu
 * extension.  It implements a MS defined interface with the Shell.
 *
 * If you have any questions or any suggestions concerning gmnvext, please
 * contact Tianmiao Hu: tianmiao@acm.org.
 */

#include "gmnvext.h"

static char *searchpath(char *name);

// Always get an error while putting the following stuff to the
// gmnvext.h file as class protected variables, give up and
// declare them as global stuff
FORMATETC fmte = {CF_HDROP,
		  (DVTARGETDEVICE FAR *)NULL,
		  DVASPECT_CONTENT,
		  -1,
		  TYMED_HGLOBAL
		 };
STGMEDIUM medium;
HRESULT hres = 0;
UINT cbFiles = 0;

/* The buffers size used to be MAX_PATH (260 bytes), but that's not always
 * enough */
#define BUFSIZE 1100

// The "Edit with MNV" shell extension provides these choices when
// a new instance of Gmnv is selected:
//   - use tabpages
//   - enable diff mode
//   - none of the above
#define EDIT_WITH_MNV_USE_TABPAGES (2)
#define EDIT_WITH_MNV_IN_DIFF_MODE (1)
#define EDIT_WITH_MNV_NO_OPTIONS   (0)

//
// Get the name of the Gmnv executable to use, with the path.
// When "runtime" is non-zero, we were called to find the runtime directory.
// Returns the path in name[BUFSIZE].  It's empty when it fails.
//
    static void
getGmnvName(char *name, int runtime)
{
    HKEY	keyhandle;
    DWORD	hlen;

    // Get the location of gmnv from the registry.  Try
    // HKEY_CURRENT_USER first, then fall back to HKEY_LOCAL_MACHINE if
    // not found.
    name[0] = 0;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\MNV\\Gmnv", 0,
				       KEY_READ, &keyhandle) == ERROR_SUCCESS)
    {
	hlen = BUFSIZE;
	if (RegQueryValueEx(keyhandle, "path", 0, NULL, (BYTE *)name, &hlen)
							     != ERROR_SUCCESS)
	    name[0] = 0;
	else
	    name[hlen] = 0;
	RegCloseKey(keyhandle);
    }

    if ((name[0] == 0) &&
	    (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\MNV\\Gmnv", 0,
				       KEY_READ, &keyhandle) == ERROR_SUCCESS))
    {
	hlen = BUFSIZE;
	if (RegQueryValueEx(keyhandle, "path", 0, NULL, (BYTE *)name, &hlen)
							     != ERROR_SUCCESS)
	    name[0] = 0;
	else
	    name[hlen] = 0;
	RegCloseKey(keyhandle);
    }

    // Registry didn't work, use the search path.
    if (name[0] == 0)
	strcpy(name, searchpath((char *)"gmnv.exe"));

    if (!runtime)
    {
	// Only when looking for the executable, not the runtime dir, we can
	// search for the batch file or a name without a path.
	if (name[0] == 0)
	    strcpy(name, searchpath((char *)"gmnv.bat"));
	if (name[0] == 0)
	    strcpy(name, "gmnv");	// finds gmnv.bat or gmnv.exe
    }
}

    static void
getGmnvInvocation(char *name, int runtime)
{
    getGmnvName(name, runtime);
    // avoid that MNV tries to expand wildcards in the file names
    strcat(name, " --literal");
}

    static void
getGmnvInvocationW(wchar_t *nameW)
{
    char *name;

    name = (char *)malloc(BUFSIZE);
    getGmnvInvocation(name, 0);
    mbstowcs(nameW, name, BUFSIZE);
    free(name);
}

//
// Get the MNV runtime directory into buf[BUFSIZE].
// The result is empty when it failed.
// When it works, the path ends in a slash or backslash.
//
    static void
getRuntimeDir(char *buf)
{
    int		idx;

    getGmnvName(buf, 1);
    if (buf[0] != 0)
    {
	// When no path found, use the search path to expand it.
	if (strchr(buf, '/') == NULL && strchr(buf, '\\') == NULL)
	    strcpy(buf, searchpath(buf));

	// remove "gmnv.exe" from the end
	for (idx = (int)strlen(buf) - 1; idx >= 0; idx--)
	    if (buf[idx] == '\\' || buf[idx] == '/')
	    {
		buf[idx + 1] = 0;
		break;
	    }
    }
}

    WCHAR *
utf8_to_utf16(const char *s)
{
    int size = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    WCHAR *buf = (WCHAR *)malloc(size * sizeof(WCHAR));
    MultiByteToWideChar(CP_UTF8, 0, s, -1, buf, size);
    return buf;
}

    HBITMAP
IconToBitmap(HICON hIcon, HBRUSH hBackground, int width, int height)
{
    HDC hDC = GetDC(NULL);
    HDC hMemDC = CreateCompatibleDC(hDC);
    HBITMAP hMemBmp = CreateCompatibleBitmap(hDC, width, height);
    HBITMAP hResultBmp = NULL;
    HGDIOBJ hOrgBMP = SelectObject(hMemDC, hMemBmp);

    DrawIconEx(hMemDC, 0, 0, hIcon, width, height, 0, hBackground, DI_NORMAL);

    hResultBmp = hMemBmp;
    hMemBmp = NULL;

    SelectObject(hMemDC, hOrgBMP);
    DeleteDC(hMemDC);
    ReleaseDC(NULL, hDC);
    DestroyIcon(hIcon);
    return hResultBmp;
}

//
// GETTEXT: removed — no translation support
//
#define _(x)	    x
#define W_impl(x) _wcsdup(L##x)
#define W(x)	    W_impl(x)
#define set_gettext_codeset()	    NULL
#define restore_gettext_codeset(x)

//
// Global variables
//
UINT      g_cRefThisDll = 0;    // Reference count of this DLL.
HINSTANCE g_hmodThisDll = NULL;	// Handle to this DLL itself.


//---------------------------------------------------------------------------
// DllMain
//---------------------------------------------------------------------------
extern "C" int APIENTRY
DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID  /* lpReserved */)
{
    switch (dwReason)
    {
    case DLL_PROCESS_ATTACH:
	// Extension DLL one-time initialization
	g_hmodThisDll = hInstance;
	break;

    case DLL_PROCESS_DETACH:
	break;
    }

    return 1;   // ok
}

    static void
inc_cRefThisDLL()
{
    InterlockedIncrement((LPLONG)&g_cRefThisDll);
}

    static void
dec_cRefThisDLL()
{
    InterlockedDecrement((LPLONG)&g_cRefThisDll);
}

//---------------------------------------------------------------------------
// DllCanUnloadNow
//---------------------------------------------------------------------------

STDAPI DllCanUnloadNow(void)
{
    return (g_cRefThisDll == 0 ? S_OK : S_FALSE);
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppvOut)
{
    *ppvOut = NULL;

    if (IsEqualIID(rclsid, CLSID_ShellExtension))
    {
	CShellExtClassFactory *pcf = new CShellExtClassFactory;

	return pcf->QueryInterface(riid, ppvOut);
    }

    return CLASS_E_CLASSNOTAVAILABLE;
}

CShellExtClassFactory::CShellExtClassFactory()
{
    m_cRef = 0L;

    inc_cRefThisDLL();
}

CShellExtClassFactory::~CShellExtClassFactory()
{
    dec_cRefThisDLL();
}

STDMETHODIMP CShellExtClassFactory::QueryInterface(REFIID riid,
						   LPVOID FAR *ppv)
{
    *ppv = NULL;

    // any interface on this object is the object pointer

    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory))
    {
	*ppv = (LPCLASSFACTORY)this;

	AddRef();

	return NOERROR;
    }

    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CShellExtClassFactory::AddRef()
{
    return InterlockedIncrement((LPLONG)&m_cRef);
}

STDMETHODIMP_(ULONG) CShellExtClassFactory::Release()
{
    if (InterlockedDecrement((LPLONG)&m_cRef))
	return m_cRef;

    delete this;

    return 0L;
}

STDMETHODIMP CShellExtClassFactory::CreateInstance(LPUNKNOWN pUnkOuter,
						      REFIID riid,
						      LPVOID *ppvObj)
{
    *ppvObj = NULL;

    // Shell extensions typically don't support aggregation (inheritance)

    if (pUnkOuter)
	return CLASS_E_NOAGGREGATION;

    // Create the main shell extension object.  The shell will then call
    // QueryInterface with IID_IShellExtInit--this is how shell extensions are
    // initialized.

    LPCSHELLEXT pShellExt = new CShellExt();  // create the CShellExt object

    if (NULL == pShellExt)
	return E_OUTOFMEMORY;

    return pShellExt->QueryInterface(riid, ppvObj);
}


STDMETHODIMP CShellExtClassFactory::LockServer(BOOL  /* fLock */)
{
    return NOERROR;
}

// *********************** CShellExt *************************
CShellExt::CShellExt()
{
    m_cRef = 0L;
    m_pDataObj = NULL;

    inc_cRefThisDLL();

    LoadMenuIcon();
}

CShellExt::~CShellExt()
{
    if (m_pDataObj)
	m_pDataObj->Release();

    dec_cRefThisDLL();

    if (m_hMNVIconBitmap)
	DeleteObject(m_hMNVIconBitmap);
}

STDMETHODIMP CShellExt::QueryInterface(REFIID riid, LPVOID FAR *ppv)
{
    *ppv = NULL;

    if (IsEqualIID(riid, IID_IShellExtInit) || IsEqualIID(riid, IID_IUnknown))
    {
	*ppv = (LPSHELLEXTINIT)this;
    }
    else if (IsEqualIID(riid, IID_IContextMenu))
    {
	*ppv = (LPCONTEXTMENU)this;
    }

    if (*ppv)
    {
	AddRef();

	return NOERROR;
    }

    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CShellExt::AddRef()
{
    return InterlockedIncrement((LPLONG)&m_cRef);
}

STDMETHODIMP_(ULONG) CShellExt::Release()
{

    if (InterlockedDecrement((LPLONG)&m_cRef))
	return m_cRef;

    delete this;

    return 0L;
}


//
//  FUNCTION: CShellExt::Initialize(LPCITEMIDLIST, LPDATAOBJECT, HKEY)
//
//  PURPOSE: Called by the shell when initializing a context menu or property
//	     sheet extension.
//
//  PARAMETERS:
//    pIDFolder - Specifies the parent folder
//    pDataObj  - Specifies the set of items selected in that folder.
//    hRegKey   - Specifies the type of the focused item in the selection.
//
//  RETURN VALUE:
//
//    NOERROR in all cases.
//
//  COMMENTS:   Note that at the time this function is called, we don't know
//		(or care) what type of shell extension is being initialized.
//		It could be a context menu or a property sheet.
//

STDMETHODIMP CShellExt::Initialize(LPCITEMIDLIST  /* pIDFolder */,
				   LPDATAOBJECT pDataObj,
				   HKEY  /* hRegKey */)
{
    // Initialize can be called more than once
    if (m_pDataObj)
	m_pDataObj->Release();

    // duplicate the object pointer and registry handle

    if (pDataObj)
    {
	m_pDataObj = pDataObj;
	pDataObj->AddRef();
    }

    return NOERROR;
}


//
//  FUNCTION: CShellExt::QueryContextMenu(HMENU, UINT, UINT, UINT, UINT)
//
//  PURPOSE: Called by the shell just before the context menu is displayed.
//	     This is where you add your specific menu items.
//
//  PARAMETERS:
//    hMenu      - Handle to the context menu
//    indexMenu  - Index of where to begin inserting menu items
//    idCmdFirst - Lowest value for new menu ID's
//    idCmtLast  - Highest value for new menu ID's
//    uFlags     - Specifies the context of the menu event
//
//  RETURN VALUE:
//
//
//  COMMENTS:
//

STDMETHODIMP CShellExt::QueryContextMenu(HMENU hMenu,
					 UINT indexMenu,
					 UINT idCmdFirst,
					 UINT  /* idCmdLast */,
					 UINT  /* uFlags */)
{
    UINT idCmd = idCmdFirst;

    hres = m_pDataObj->GetData(&fmte, &medium);
    if (medium.hGlobal)
	cbFiles = DragQueryFileW((HDROP)medium.hGlobal, (UINT)-1, 0, 0);

    // InsertMenu(hMenu, indexMenu++, MF_SEPARATOR|MF_BYPOSITION, 0, NULL);

    // Initialize m_cntOfHWnd to 0
    m_cntOfHWnd = 0;

    HKEY keyhandle;
    bool showExisting = true;
    bool showIcons = true;

    // Check whether "Edit with existing MNV" entries are disabled.
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\MNV\\Gmnv", 0,
				       KEY_READ, &keyhandle) == ERROR_SUCCESS)
    {
	if (RegQueryValueEx(keyhandle, "DisableEditWithExisting", 0, NULL,
						 NULL, NULL) == ERROR_SUCCESS)
	    showExisting = false;
	if (RegQueryValueEx(keyhandle, "DisableContextMenuIcons", 0, NULL,
						 NULL, NULL) == ERROR_SUCCESS)
	    showIcons = false;
	RegCloseKey(keyhandle);
    }

    // Use UTF-8 for gettext.
    char *prev = set_gettext_codeset();

    // Retrieve all the mnv instances, unless disabled.
    if (showExisting)
	EnumWindows(EnumWindowsProc, (LPARAM)this);

    MENUITEMINFOW mii = { sizeof(MENUITEMINFOW) };
    mii.fMask = MIIM_STRING | MIIM_ID;
    if (showIcons)
    {
	mii.fMask |= MIIM_BITMAP;
	mii.hbmpItem = m_hMNVIconBitmap;
    }

    if (cbFiles > 1)
    {
	mii.wID = idCmd++;
	mii.dwTypeData = W(_("Edit with MNV using &tabpages"));
	mii.cch = wcslen(mii.dwTypeData);
	InsertMenuItemW(hMenu, indexMenu++, TRUE, &mii);
	free(mii.dwTypeData);

	mii.wID = idCmd++;
	mii.dwTypeData = W(_("Edit with single &MNV"));
	mii.cch = wcslen(mii.dwTypeData);
	InsertMenuItemW(hMenu, indexMenu++, TRUE, &mii);
	free(mii.dwTypeData);

	if (cbFiles <= 4)
	{
	    // Can edit up to 4 files in diff mode
	    mii.wID = idCmd++;
	    mii.dwTypeData = W(_("Diff with MNV"));
	    mii.cch = wcslen(mii.dwTypeData);
	    InsertMenuItemW(hMenu, indexMenu++, TRUE, &mii);
	    free(mii.dwTypeData);
	    m_edit_existing_off = 3;
	}
	else
	    m_edit_existing_off = 2;

    }
    else
    {
	mii.wID = idCmd++;
	mii.dwTypeData = W(_("Edit with &MNV"));
	mii.cch = wcslen(mii.dwTypeData);
	InsertMenuItemW(hMenu, indexMenu++, TRUE, &mii);
	free(mii.dwTypeData);
	m_edit_existing_off = 1;
    }

    HMENU hSubMenu = NULL;
    if (m_cntOfHWnd > 1)
    {
	hSubMenu = CreatePopupMenu();
	mii.fMask |= MIIM_SUBMENU;
	mii.wID = idCmd;
	mii.dwTypeData = W(_("Edit with existing MNV"));
	mii.cch = wcslen(mii.dwTypeData);
	mii.hSubMenu = hSubMenu;
	InsertMenuItemW(hMenu, indexMenu++, TRUE, &mii);
	free(mii.dwTypeData);
	mii.fMask = mii.fMask & ~MIIM_SUBMENU;
	mii.hSubMenu = NULL;
    }
    // Now display all the mnv instances
    for (int i = 0; i < m_cntOfHWnd; i++)
    {
	WCHAR title[BUFSIZE];
	WCHAR temp[BUFSIZE];
	int index;
	HMENU hmenu;

	// Obtain window title, continue if can not
	if (GetWindowTextW(m_hWnd[i], title, BUFSIZE - 1) == 0)
	    continue;
	// Truncate the title before the path, keep the file name
	WCHAR *pos = wcschr(title, L'(');
	if (pos != NULL)
	{
	    if (pos > title && pos[-1] == L' ')
		--pos;
	    *pos = 0;
	}
	// Now concatenate
	if (m_cntOfHWnd > 1)
	    temp[0] = L'\0';
	else
	{
	    WCHAR *s = W(_("Edit with existing MNV - "));
	    wcsncpy(temp, s, BUFSIZE - 1);
	    temp[BUFSIZE - 1] = L'\0';
	    free(s);
	}
	wcsncat(temp, title, BUFSIZE - 1 - wcslen(temp));
	temp[BUFSIZE - 1] = L'\0';

	mii.wID = idCmd++;
	mii.dwTypeData = temp;
	mii.cch = wcslen(mii.dwTypeData);
	if (m_cntOfHWnd > 1)
	{
	    hmenu = hSubMenu;
	    index = i;
	}
	else
	{
	    hmenu = hMenu;
	    index = indexMenu++;
	}
	InsertMenuItemW(hmenu, index, TRUE, &mii);
    }
    // InsertMenu(hMenu, indexMenu++, MF_SEPARATOR|MF_BYPOSITION, 0, NULL);

    // Restore previous codeset.
    restore_gettext_codeset(prev);

    // Must return number of menu items we added.
    return ResultFromShort(idCmd-idCmdFirst);
}

//
//  FUNCTION: CShellExt::InvokeCommand(LPCMINVOKECOMMANDINFO)
//
//  PURPOSE: Called by the shell after the user has selected on of the
//	     menu items that was added in QueryContextMenu().
//
//  PARAMETERS:
//    lpcmi - Pointer to an CMINVOKECOMMANDINFO structure
//
//  RETURN VALUE:
//
//
//  COMMENTS:
//

STDMETHODIMP CShellExt::InvokeCommand(LPCMINVOKECOMMANDINFO lpcmi)
{
    HRESULT hr = E_INVALIDARG;
    int gmnvExtraOptions;

    // If HIWORD(lpcmi->lpVerb) then we have been called programmatically
    // and lpVerb is a command that should be invoked.  Otherwise, the shell
    // has called us, and LOWORD(lpcmi->lpVerb) is the menu ID the user has
    // selected.  Actually, it's (menu ID - idCmdFirst) from QueryContextMenu().
    if (!HIWORD(lpcmi->lpVerb))
    {
	UINT idCmd = LOWORD(lpcmi->lpVerb);

	if (idCmd >= m_edit_existing_off)
	{
	    // Existing with mnv instance
	    hr = PushToWindow(lpcmi->hwnd,
		    lpcmi->lpDirectory,
		    lpcmi->lpVerb,
		    lpcmi->lpParameters,
		    lpcmi->nShow,
		    idCmd - m_edit_existing_off);
	}
	else
	{
	    switch (idCmd)
	    {
		case 0:
		    gmnvExtraOptions = EDIT_WITH_MNV_USE_TABPAGES;
		    break;
		case 1:
		    gmnvExtraOptions = EDIT_WITH_MNV_NO_OPTIONS;
		    break;
		case 2:
		    gmnvExtraOptions = EDIT_WITH_MNV_IN_DIFF_MODE;
		    break;
		default:
		    // If execution reaches this point we likely have an
		    // inconsistency between the code that setup the menus
		    // and this code that determines what the user
		    // selected.  This should be detected and fixed during 
		    // development.
		    return E_FAIL;
	    }

            LPCMINVOKECOMMANDINFOEX lpcmiex = (LPCMINVOKECOMMANDINFOEX)lpcmi;
            LPCWSTR currentDirectory = lpcmi->cbSize == sizeof(CMINVOKECOMMANDINFOEX) ? lpcmiex->lpDirectoryW : NULL;

	    hr = InvokeSingleGmnv(lpcmi->hwnd,
		    currentDirectory,
		    lpcmi->lpVerb,
		    lpcmi->lpParameters,
		    lpcmi->nShow,
		    gmnvExtraOptions);
	}
    }
    return hr;
}

STDMETHODIMP CShellExt::PushToWindow(HWND  /* hParent */,
				   LPCSTR  /* pszWorkingDir */,
				   LPCSTR  /* pszCmd */,
				   LPCSTR  /* pszParam */,
				   int  /* iShowCmd */,
				   int idHWnd)
{
    HWND hWnd = m_hWnd[idHWnd];

    // Show and bring mnv instance to foreground
    if (IsIconic(hWnd) != 0)
	ShowWindow(hWnd, SW_RESTORE);
    else
	ShowWindow(hWnd, SW_SHOW);
    SetForegroundWindow(hWnd);

    // Post the selected files to the mnv instance
    PostMessage(hWnd, WM_DROPFILES, (WPARAM)medium.hGlobal, 0);

    return NOERROR;
}

STDMETHODIMP CShellExt::GetCommandString(UINT_PTR  /* idCmd */,
					 UINT uFlags,
					 UINT FAR * /* reserved */,
					 LPSTR pszName,
					 UINT cchMax)
{
    // Use UTF-8 for gettext.
    char *prev = set_gettext_codeset();

    WCHAR *s = W(_("Edits the selected file(s) with MNV"));
    if (uFlags == GCS_HELPTEXTW && cchMax > wcslen(s))
	wcscpy((WCHAR *)pszName, s);
    free(s);

    // Restore previous codeset.
    restore_gettext_codeset(prev);

    return NOERROR;
}

BOOL CALLBACK CShellExt::EnumWindowsProc(HWND hWnd, LPARAM lParam)
{
    char temp[BUFSIZE];

    // First do a bunch of check
    // No invisible window
    if (!IsWindowVisible(hWnd))
	return TRUE;
    // No child window ???
    // if (GetParent(hWnd)) return TRUE;
    // Class name should be MNV, if failed to get class name, return
    if (GetClassName(hWnd, temp, sizeof(temp)) == 0)
	return TRUE;
    // Compare class name to that of mnv, if not, return
    if (_strnicmp(temp, "mnv", sizeof("mnv")) != 0)
	return TRUE;
    // First check if the number of mnv instance exceeds MAX_HWND
    CShellExt *cs = (CShellExt*) lParam;
    if (cs->m_cntOfHWnd >= MAX_HWND)
	return FALSE;	// stop enumeration
    // Now we get the mnv window, put it into some kind of array
    cs->m_hWnd[cs->m_cntOfHWnd] = hWnd;
    cs->m_cntOfHWnd ++;

    return TRUE; // continue enumeration (otherwise this would be false)
}

BOOL CShellExt::LoadMenuIcon()
{
    char mnvExeFile[BUFSIZE];
    getGmnvName(mnvExeFile, 1);
    if (mnvExeFile[0] == '\0')
	return FALSE;
    HICON hMNVIcon;
    if (ExtractIconEx(mnvExeFile, 0, NULL, &hMNVIcon, 1) == 0)
	return FALSE;
    m_hMNVIconBitmap = IconToBitmap(hMNVIcon,
	    GetSysColorBrush(COLOR_MENU),
	    GetSystemMetrics(SM_CXSMICON),
	    GetSystemMetrics(SM_CYSMICON));
    return TRUE;
}

    static char *
searchpath(char *name)
{
    static char widename[2 * BUFSIZE];
    static char location[2 * BUFSIZE + 2];

    // There appears to be a bug in FindExecutableA() on Windows NT.
    // Use FindExecutableW() instead...
    MultiByteToWideChar(CP_ACP, 0, (LPCSTR)name, -1,
	    (LPWSTR)widename, BUFSIZE);
    if (FindExecutableW((LPCWSTR)widename, (LPCWSTR)"",
		(LPWSTR)location) > (HINSTANCE)32)
    {
	WideCharToMultiByte(CP_ACP, 0, (LPWSTR)location, -1,
		(LPSTR)widename, 2 * BUFSIZE, NULL, NULL);
	return widename;
    }
    return (char *)"";
}


STDMETHODIMP CShellExt::InvokeSingleGmnv(HWND hParent,
				   LPCWSTR  workingDir,
				   LPCSTR  /* pszCmd */,
				   LPCSTR  /* pszParam */,
				   int  /* iShowCmd */,
				   int gmnvExtraOptions)
{
    wchar_t	m_szFileUserClickedOn[BUFSIZE];
    wchar_t	*cmdStrW;
    size_t	cmdlen;
    size_t	len;
    UINT i;

    cmdlen = BUFSIZE;
    cmdStrW  = (wchar_t *) malloc(cmdlen * sizeof(wchar_t));
    if (cmdStrW == NULL)
	return E_FAIL;
    getGmnvInvocationW(cmdStrW);

    if (gmnvExtraOptions == EDIT_WITH_MNV_IN_DIFF_MODE)
	wcscat(cmdStrW, L" -d");
    else if (gmnvExtraOptions == EDIT_WITH_MNV_USE_TABPAGES)
	wcscat(cmdStrW, L" -p");
    for (i = 0; i < cbFiles; i++)
    {
	DragQueryFileW((HDROP)medium.hGlobal,
		i,
		m_szFileUserClickedOn,
		sizeof(m_szFileUserClickedOn));

	len = wcslen(cmdStrW) + wcslen(m_szFileUserClickedOn) + 4;
	if (len > cmdlen)
	{
	    cmdlen = len + BUFSIZE;
	    wchar_t *cmdStrW_new = (wchar_t *)realloc(cmdStrW, cmdlen * sizeof(wchar_t));
	    if (cmdStrW_new == NULL)
	    {
		free(cmdStrW);
		return E_FAIL;
	    }
	    cmdStrW = cmdStrW_new;
	}
	wcscat(cmdStrW, L" \"");
	wcscat(cmdStrW, m_szFileUserClickedOn);
	wcscat(cmdStrW, L"\"");
    }

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);

    // Start the child process.
    if (!CreateProcessW(NULL,	// No module name (use command line).
		cmdStrW,	// Command line.
		NULL,		// Process handle not inheritable.
		NULL,		// Thread handle not inheritable.
		FALSE,		// Set handle inheritance to FALSE.
		0,		// No creation flags.
		NULL,		// Use parent's environment block.
		workingDir,	// Use parent's starting directory.
		&si,		// Pointer to STARTUPINFO structure.
		&pi)		// Pointer to PROCESS_INFORMATION structure.
       )
    {
	// Use UTF-8 for gettext.
	char *prev = set_gettext_codeset();

	WCHAR *msg = W(_("Error creating process: Check if gmnv is in your path!"));
	WCHAR *title = W(_("gmnvext.dll error"));

	MessageBoxW(hParent, msg, title, MB_OK);

	free(msg);
	free(title);

	// Restore previous codeset.
	restore_gettext_codeset(prev);
    }
    else
    {
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
    }
    free(cmdStrW);

    return NOERROR;
}
