/* vi:set ts=8 sts=4 sw=4:
 *
 * MNV - MNV is not Vim	by Bram Moolenaar
 *    BeBox GUI support Copyright 1998 by Olaf Seibert.
 *		    All Rights Reserved.
 *
 * Do ":help uganda"  in MNV to read copying and usage conditions.
 * Do ":help credits" in MNV to see a list of people who contributed.
 *
 * Based on "GUI support for the Buzzword Enhanced Operating System."
 *
 * Ported to R4 by Richard Offer <richard@whitequeen.com> Jul 99
 *
 * Haiku support by Siarzhuk Zharski <imker@gmx.li> Apr-Mai 2009
 *
 */

/*
 * Structure of the Haiku GUI code:
 *
 * There are 3 threads.
 * 1. The initial thread. In gui_mch_prepare() this gets to run the
 *    BApplication message loop. But before it starts doing that,
 *    it creates thread 2
 * 2. The main() thread. This thread is created in gui_mch_prepare()
 *    and its purpose in life is to call main(argc, argv) again.
 *    This thread is doing the bulk of the work.
 * 3. Sooner or later, a window is opened by the main() thread. This
 *    causes a second message loop to be created: the window thread.
 *
 * == alternatively ===
 *
 * #if RUN_BAPPLICATION_IN_NEW_THREAD...
 *
 * 1. The initial thread. In gui_mch_prepare() this gets to spawn
 *    thread 2. After doing that, it returns to main() to do the
 *    bulk of the work, being the main() thread.
 * 2. Runs the BApplication.
 * 3. The window thread, just like in the first case.
 *
 * This second alternative is cleaner from MNV's viewpoint. However,
 * the BeBook seems to assume everywhere that the BApplication *must*
 * run in the initial thread. So perhaps doing otherwise is very wrong.
 *
 * However, from a B_SINGLE_LAUNCH viewpoint, the first is better.
 * If MNV is marked "Single Launch" in its application resources,
 * and a file is dropped on the MNV icon, and another MNV is already
 * running, the file is passed on to the earlier MNV. This happens
 * in BApplication::Run(). So we want MNV to terminate if
 * BApplication::Run() terminates. (See the BeBook, on BApplication.
 * However, it seems that the second copy of MNV isn't even started
 * in this case... which is for the better since I wouldn't know how
 * to detect this case.)
 *
 * Communication between these threads occurs mostly by translating
 * BMessages that come in and posting an appropriate translation on
 * the VDCMP (MNV Direct Communication Message Port). Therefore the
 * actions required for keypresses and window resizes, etc, are mostly
 * performed in the main() thread.
 *
 * A notable exception to this is the Draw() event. The redrawing of
 * the window contents is performed asynchronously from the window
 * thread. To make this work correctly, a locking protocol is used when
 * any thread is accessing the essential variables that are used by
 * the window thread.
 *
 * This locking protocol consists of locking MNV's window. This is both
 * convenient and necessary.
 */

extern "C" {

#include <assert.h>
#include <float.h>
#include <syslog.h>

#include "mnv.h"
#include "version.h"

}   // extern "C"

// ---------------- start of header part ----------------

//#include <Alert.h>
#include <Application.h>
#include <Beep.h>
#include <Bitmap.h>
#include <Box.h>
#include <Button.h>
#include <Clipboard.h>
#include <Debug.h>
//#include <Directory.h>
//#include <Entry.h>
#include <File.h>
#include <FilePanel.h>
#include <FindDirectory.h>
//#include <Font.h>
#include <IconUtils.h>
#include <Input.h>
#include <ListView.h>
#include <MenuBar.h>
#include <MenuItem.h>
//#include <MessageQueue.h>
//#include <OS.h>
#include <Path.h>
#include <PictureButton.h>
#include <PopUpMenu.h>
//#include <Region.h>
#include <Resources.h>
//#include <Roster.h>
#include <Screen.h>
#include <ScrollBar.h>
#include <ScrollView.h>
#include <String.h>
#include <StringView.h>
//#include <SupportDefs.h>
#include <TabView.h>
#include <TextControl.h>
#include <TextView.h>
#include <TranslationUtils.h>
#include <TranslatorFormats.h>
#include <View.h>
#include <Window.h>

class MNVApp;
class MNVFormView;
class MNVTextAreaView;
class MNVWindow;
class MNVToolbar;
class MNVTabLine;

extern key_map *keyMap;
extern char *keyMapChars;

extern int main(int argc, char **argv);

#ifndef B_MAX_PORT_COUNT
#define B_MAX_PORT_COUNT    255
#endif

// MNVApp seems comparable to the X "mnvShell"
class MNVApp: public BApplication
{
    typedef BApplication Inherited;
    public:
    MNVApp(const char *appsig);
    ~MNVApp();

    // callbacks:
#if 0
    virtual void DispatchMessage(BMessage *m, BHandler *h)
    {
	m->PrintToStream();
	Inherited::DispatchMessage(m, h);
    }
#endif
    virtual void ReadyToRun();
    virtual void ArgvReceived(int32 argc, char **argv);
    virtual void RefsReceived(BMessage *m);
    virtual bool QuitRequested();
    virtual void MessageReceived(BMessage *m);

    static void SendRefs(BMessage *m, bool changedir);

    sem_id	fFilePanelSem;
    BFilePanel*	fFilePanel;
    BPath	fBrowsedPath;
    private:
};

class MNVWindow: public BWindow
{
    typedef BWindow Inherited;
    public:
    MNVWindow();
    ~MNVWindow();

    virtual void DispatchMessage(BMessage *m, BHandler *h);
    virtual void WindowActivated(bool active);
    virtual bool QuitRequested();

    MNVFormView	    *formView;

    private:
    void init();

    bool is_fullscreen = false;
    BRect saved_frame;
    window_look saved_look;
};

class MNVFormView: public BView
{
    typedef BView Inherited;
    public:
    MNVFormView(BRect frame);
    ~MNVFormView();

    // callbacks:
    virtual void AllAttached();
    virtual void FrameResized(float new_width, float new_height);

#define MENUBAR_MARGIN	1
    float MenuHeight() const
    { return menuBar ? menuBar->Frame().Height() + MENUBAR_MARGIN: 0; }
    BMenuBar *MenuBar() const
    { return menuBar; }

    private:
    void init(BRect);

    BMenuBar	    *menuBar;
    MNVTextAreaView *textArea;

#ifdef FEAT_TOOLBAR
    public:
    float ToolbarHeight() const;
    MNVToolbar *ToolBar() const
    { return toolBar; }
    private:
    MNVToolbar	    *toolBar;
#endif

#ifdef FEAT_GUI_TABLINE
    public:
    MNVTabLine *TabLine() const	{ return tabLine; }
    bool IsShowingTabLine() const { return showingTabLine; }
    void SetShowingTabLine(bool showing) { showingTabLine = showing;	}
    float TablineHeight() const;
    private:
    MNVTabLine	*tabLine;
    int	showingTabLine;
#endif
};

class MNVTextAreaView: public BView
{
    typedef BView Inherited;
    public:
    MNVTextAreaView(BRect frame);
    ~MNVTextAreaView();

    // callbacks:
    virtual void Draw(BRect updateRect);
    virtual void KeyDown(const char *bytes, int32 numBytes);
    virtual void MouseDown(BPoint point);
    virtual void MouseUp(BPoint point);
    virtual void MouseMoved(BPoint point, uint32 transit, const BMessage *message);
    virtual void MessageReceived(BMessage *m);

    // own functions:
    int mchInitFont(char_u *name);
    void mchDrawString(int row, int col, char_u *s, int len, int flags);
    void mchClearBlock(int row1, int col1, int row2, int col2);
    void mchClearAll();
    void mchDeleteLines(int row, int num_lines);
    void mchInsertLines(int row, int num_lines);

    static void guiSendMouseEvent(int button, int x, int y, int repeated_click, int_u modifiers);
    static void guiMouseMoved(int x, int y);
    static void guiBlankMouse(bool should_hide);
    static int_u mouseModifiersToMNV(int32 beModifiers);

    int32 mouseDragEventCount;

#ifdef FEAT_MBYTE_IME
    void DrawIMString(void);
#endif

    private:
    void init(BRect);

    int_u	mnvMouseButton;
    int_u	mnvMouseModifiers;

#ifdef FEAT_MBYTE_IME
    struct {
	BMessenger* messenger;
	BMessage* message;
	BPoint location;
	int row;
	int col;
	int count;
    } IMData;
#endif
};

class MNVScrollBar: public BScrollBar
{
    typedef BScrollBar Inherited;
    public:
    MNVScrollBar(scrollbar_T *gsb, orientation posture);
    ~MNVScrollBar();

    virtual void ValueChanged(float newValue);
    virtual void MouseUp(BPoint where);
    void SetValue(float newval);
    scrollbar_T *getGsb()
    { return gsb; }

    int32	scrollEventCount;

    private:
    scrollbar_T *gsb;
    float   ignoreValue;
};


#ifdef FEAT_TOOLBAR

class MNVToolbar : public BBox
{
    static BBitmap *normalButtonsBitmap;
    static BBitmap *grayedButtonsBitmap;

    BBitmap *LoadMNVBitmap(const char* fileName);
    bool GetPictureFromBitmap(BPicture *pictureTo, int32 index, BBitmap *bitmapFrom, bool pressed);
    bool ModifyBitmapToGrayed(BBitmap *bitmap);

    BList fButtonsList;
    void InvalidateLayout();

    public:
    MNVToolbar(BRect frame, const char * name);
    ~MNVToolbar();

    bool PrepareButtonBitmaps();

    bool AddButton(int32 index, mnvmenu_T *menu);
    bool RemoveButton(mnvmenu_T *menu);
    bool GrayButton(mnvmenu_T *menu, int grey);

    float ToolbarHeight() const;
    virtual void AttachedToWindow();
};

BBitmap *MNVToolbar::normalButtonsBitmap  = NULL;
BBitmap *MNVToolbar::grayedButtonsBitmap  = NULL;

const float ToolbarMargin = 3.;
const float ButtonMargin  = 3.;

#endif //FEAT_TOOLBAR

#ifdef FEAT_GUI_TABLINE

class MNVTabLine : public BTabView
{
    public:
	class MNVTab : public BTab {
	    public:
		MNVTab() : BTab(new BView(BRect(), "-Empty-", 0, 0)) {}

	    virtual void Select(BView* owner);
	};

	MNVTabLine(BRect r) : BTabView(r, "mnvTabLine", B_WIDTH_FROM_LABEL,
	       B_FOLLOW_LEFT | B_FOLLOW_TOP | B_FOLLOW_RIGHT, B_WILL_DRAW | B_FRAME_EVENTS) {}

    float TablineHeight() const;
    virtual void MouseDown(BPoint point);
};

#endif //FEAT_GUI_TABLINE


// For caching the fonts that are used;
// MNV seems rather sloppy in this regard.
class MNVFont: public BFont
{
    typedef BFont Inherited;
    public:
    MNVFont();
    MNVFont(const MNVFont *rhs);
    MNVFont(const BFont *rhs);
    MNVFont(const MNVFont &rhs);
    ~MNVFont();

    MNVFont *next;
    int refcount;
    char_u *name;

    private:
    void init();
};

#if defined(FEAT_GUI_DIALOG)

class MNVDialog : public BWindow
{
    typedef BWindow Inherited;

    BButton* _CreateButton(int32 which, const char* label);

    public:

    class View : public BView {
	typedef BView Inherited;

	public:
	View(BRect frame);
	~View();

	virtual void Draw(BRect updateRect);
	void InitIcon(int32 type);

	private:
	BBitmap*    fIconBitmap;
    };

    MNVDialog(int type, const char *title, const char *message,
	    const char *buttons, int dfltbutton, const char *textfield,
	    int ex_cmd);
    ~MNVDialog();

    int Go();

    virtual void MessageReceived(BMessage *msg);

    private:
    sem_id	    fDialogSem;
    int		    fDialogValue;
    BList	    fButtonsList;
    BTextView*	    fMessageView;
    BTextControl*   fInputControl;
    const char*	    fInputValue;
};

class MNVSelectFontDialog : public BWindow
{
    typedef BWindow Inherited;

    void _CleanList(BListView* list);
    void _UpdateFontStyles();
    void _UpdateSizeInputPreview();
    void _UpdateFontPreview();
    bool _UpdateFromListItem(BListView* list, char* text, int textSize);
    public:

    MNVSelectFontDialog(font_family* family, font_style* style, float* size);
    ~MNVSelectFontDialog();

    bool Go();

    virtual void MessageReceived(BMessage *msg);

    private:
    status_t	    fStatus;
    sem_id	    fDialogSem;
    bool	    fDialogValue;
    font_family*    fFamily;
    font_style*	    fStyle;
    float*	    fSize;
    font_family	    fFontFamily;
    font_style	    fFontStyle;
    float	    fFontSize;
    BStringView*    fPreview;
    BListView*	    fFamiliesList;
    BListView*	    fStylesList;
    BListView*	    fSizesList;
    BTextControl*   fSizesInput;
};

#endif // FEAT_GUI_DIALOG

// ---------------- end of GUI classes ----------------

struct MainArgs {
    int	     argc;
    char    **argv;
};

// These messages are copied through the VDCMP.
// Therefore they ought not to have anything fancy.
// They must be of POD type (Plain Old Data)
// as the C++ standard calls them.

#define	KEY_MSG_BUFSIZ	7
#if KEY_MSG_BUFSIZ < MAX_KEY_CODE_LEN
#error Increase KEY_MSG_BUFSIZ!
#endif

struct MNVKeyMsg {
    char_u  length;
    char_u  chars[KEY_MSG_BUFSIZ];  // contains MNV encoding
    bool    csi_escape;
};

struct MNVResizeMsg {
    int	    width;
    int	    height;
};

struct MNVScrollBarMsg {
    MNVScrollBar *sb;
    long    value;
    int	    stillDragging;
};

struct MNVMenuMsg {
    mnvmenu_T	*guiMenu;
};

struct MNVMouseMsg {
    int	    button;
    int	    x;
    int	    y;
    int	    repeated_click;
    int_u   modifiers;
};

struct MNVMouseMovedMsg {
    int	    x;
    int	    y;
};

struct MNVFocusMsg {
    bool    active;
};

struct MNVRefsMsg {
    BMessage   *message;
    bool    changedir;
};

struct MNVTablineMsg {
    int	    index;
};

struct MNVTablineMenuMsg {
    int	    index;
    int	    event;
};

struct MNVMsg {
    enum MNVMsgType {
	Key, Resize, ScrollBar, Menu, Mouse, MouseMoved, Focus, Refs, Tabline, TablineMenu
    };

    union {
	struct MNVKeyMsg    Key;
	struct MNVResizeMsg NewSize;
	struct MNVScrollBarMsg	Scroll;
	struct MNVMenuMsg   Menu;
	struct MNVMouseMsg  Mouse;
	struct MNVMouseMovedMsg	MouseMoved;
	struct MNVFocusMsg  Focus;
	struct MNVRefsMsg   Refs;
	struct MNVTablineMsg	Tabline;
	struct MNVTablineMenuMsg    TablineMenu;
    } u;
};

#define RGB(r, g, b)	((char_u)(r) << 16 | (char_u)(g) << 8 | (char_u)(b) << 0)
#define GUI_TO_RGB(g)	{ (char_u)((g) >> 16), (char_u)((g) >> 8), (char_u)((g) >> 0), 255 }

// ---------------- end of header part ----------------

static struct specialkey
{
    uint16  BeKeys;
#define KEY(a,b)    ((a)<<8|(b))
#define K(a)	    KEY(0,a)		// for ASCII codes
#define F(b)	    KEY(1,b)		// for scancodes
    char_u  mnv_code0;
    char_u  mnv_code1;
} special_keys[] =
{
    {K(B_UP_ARROW),	'k', 'u'},
    {K(B_DOWN_ARROW),	    'k', 'd'},
    {K(B_LEFT_ARROW),	    'k', 'l'},
    {K(B_RIGHT_ARROW),	    'k', 'r'},
    {K(B_BACKSPACE),	    'k', 'b'},
    {K(B_INSERT),	'k', 'I'},
    {K(B_DELETE),	'k', 'D'},
    {K(B_HOME),		'k', 'h'},
    {K(B_END),		'@', '7'},
    {K(B_PAGE_UP),	'k', 'P'},	// XK_Prior
    {K(B_PAGE_DOWN),	    'k', 'N'},	    // XK_Next,

#define FIRST_FUNCTION_KEY  11
    {F(B_F1_KEY),	'k', '1'},
    {F(B_F2_KEY),	'k', '2'},
    {F(B_F3_KEY),	'k', '3'},
    {F(B_F4_KEY),	'k', '4'},
    {F(B_F5_KEY),	'k', '5'},
    {F(B_F6_KEY),	'k', '6'},
    {F(B_F7_KEY),	'k', '7'},
    {F(B_F8_KEY),	'k', '8'},
    {F(B_F9_KEY),	'k', '9'},
    {F(B_F10_KEY),	'k', ';'},

    {F(B_F11_KEY),	'F', '1'},
    {F(B_F12_KEY),	'F', '2'},
    //	{XK_F13,	    'F', '3'},	// would be print screen
    // sysreq
    {F(0x0F),		'F', '4'},	// scroll lock
    {F(0x10),		'F', '5'},	// pause/break
    //	{XK_F16,	'F', '6'},
    //	{XK_F17,	'F', '7'},
    //	{XK_F18,	'F', '8'},
    //	{XK_F19,	'F', '9'},
    //	 {XK_F20,	'F', 'A'},
    //	{XK_F21,	'F', 'B'},
    //	{XK_F22,	'F', 'C'},
    //	{XK_F23,	'F', 'D'},
    //	{XK_F24,	'F', 'E'},
    //	{XK_F25,	'F', 'F'},
    //	{XK_F26,	'F', 'G'},
    //	{XK_F27,	'F', 'H'},
    //	{XK_F28,	'F', 'I'},
    //	{XK_F29,	'F', 'J'},
    //	{XK_F30,	'F', 'K'},
    //	{XK_F31,	'F', 'L'},
    //	{XK_F32,	'F', 'M'},
    //	{XK_F33,	'F', 'N'},
    //	{XK_F34,	'F', 'O'},
    //	{XK_F35,	'F', 'P'},	// keysymdef.h defines up to F35

    //	{XK_Help,	'%', '1'},	// XK_Help
    {F(B_PRINT_KEY),	    '%', '9'},

#if 0
    // Keypad keys:
    {F(0x48),	    'k', 'l'},	    // XK_KP_Left
    {F(0x4A),	    'k', 'r'},	    // XK_KP_Right
    {F(0x38),	    'k', 'u'},	    // XK_KP_Up
    {F(0x59),	    'k', 'd'},	    // XK_KP_Down
    {F(0x64),	    'k', 'I'},	    // XK_KP_Insert
    {F(0x65),	    'k', 'D'},	    // XK_KP_Delete
    {F(0x37),	    'k', 'h'},	    // XK_KP_Home
    {F(0x58),	    '@', '7'},	    // XK_KP_End
    {F(0x39),	    'k', 'P'},	    // XK_KP_Prior
    {F(0x60),	    'k', 'N'},	    // XK_KP_Next
    {F(0x49),	    '&', '8'},	    // XK_Undo, keypad 5
#endif

    // End of list marker:
    {0,		    0, 0}
};

#define NUM_SPECIAL_KEYS    ARRAY_LENGTH(special_keys)

// ---------------- MNVApp ----------------

    static void
docd(BPath &path)
{
    mch_chdir((char *)path.Path());
    // Do this to get the side effects of a :cd command
    do_cmdline_cmd((char_u *)"cd .");
}

	static void
drop_callback(void *cookie)
{
    // TODO here we could handle going to a specific position in the dropped
    // file (see src/gui_mac.c, deleted in 8.2.1422)
    // Update the screen display
    update_screen(UPD_NOT_VALID);
}

    // Really handle dropped files and folders.
	static void
RefsReceived(BMessage *m, bool changedir)
{
    uint32 type;
    int32 count;

    m->PrintToStream();
    switch (m->what) {
	case B_REFS_RECEIVED:
	case B_SIMPLE_DATA:
	    m->GetInfo("refs", &type, &count);
	    if (type != B_REF_TYPE)
		goto bad;
	    break;
	case B_ARGV_RECEIVED:
	    m->GetInfo("argv", &type, &count);
	    if (type != B_STRING_TYPE)
		goto bad;
	    if (changedir) {
		char *dirname;
		if (m->FindString("cwd", (const char **) &dirname) == B_OK) {
		    chdir(dirname);
		    do_cmdline_cmd((char_u *)"cd .");
		}
	    }
	    break;
	default:
bad:
	    /*fprintf(stderr, "bad!\n"); */
	    delete m;
	    return;
    }

#ifdef FEAT_VISUAL
    reset_VIsual();
#endif

    char_u  **fnames;
    fnames = (char_u **) alloc(count * sizeof(char_u *));
    int fname_index = 0;

    switch (m->what) {
	case B_REFS_RECEIVED:
	case B_SIMPLE_DATA:
	    // fprintf(stderr, "case B_REFS_RECEIVED\n");
	    for (int i = 0; i < count; ++i)
	    {
		entry_ref ref;
		if (m->FindRef("refs", i, &ref) == B_OK) {
		    BEntry entry(&ref, false);
		    BPath path;
		    entry.GetPath(&path);

		    // Change to parent directory?
		    if (changedir) {
			BPath parentpath;
			path.GetParent(&parentpath);
			docd(parentpath);
		    }

		    // Is it a directory? If so, cd into it.
		    BDirectory bdir(&ref);
		    if (bdir.InitCheck() == B_OK) {
			// don't cd if we already did it
			if (!changedir)
			    docd(path);
		    } else {
			mch_dirname(IObuff, IOSIZE);
			char_u *fname = shorten_fname((char_u *)path.Path(), IObuff);
			if (fname == NULL)
			    fname = (char_u *)path.Path();
			fnames[fname_index++] = mnv_strsave(fname);
			// fprintf(stderr, "%s\n", fname);
		    }

		    // Only do it for the first file/dir
		    changedir = false;
		}
	    }
	    break;
	case B_ARGV_RECEIVED:
	    // fprintf(stderr, "case B_ARGV_RECEIVED\n");
	    for (int i = 1; i < count; ++i)
	    {
		char *fname;

		if (m->FindString("argv", i, (const char **) &fname) == B_OK) {
		    fnames[fname_index++] = mnv_strsave((char_u *)fname);
		}
	    }
	    break;
	default:
	    // fprintf(stderr, "case default\n");
	    break;
    }

    delete m;

    // Handle the drop, :edit to get to the file
    if (fname_index > 0) {
	handle_drop(fname_index, fnames, FALSE, drop_callback, NULL);

	setcursor();
	out_flush();
    } else {
	mnv_free(fnames);
    }
}

MNVApp::MNVApp(const char *appsig):
    BApplication(appsig),
    fFilePanelSem(-1),
    fFilePanel(NULL)
{
}

MNVApp::~MNVApp()
{
}

    void
MNVApp::ReadyToRun()
{
    /*
     * Apparently signals are inherited by the created thread -
     * disable the most annoying ones.
     */
    mch_signal(SIGINT, SIG_IGN);
    mch_signal(SIGQUIT, SIG_IGN);
}

    void
MNVApp::ArgvReceived(int32 arg_argc, char **arg_argv)
{
    if (!IsLaunching()) {
	/*
	 * This can happen if we are set to Single or Exclusive
	 * Launch. Be nice and open the file(s).
	 */
	if (gui.mnvWindow)
	    gui.mnvWindow->Minimize(false);
	BMessage *m = CurrentMessage();
	DetachCurrentMessage();
	SendRefs(m, true);
    }
}

    void
MNVApp::RefsReceived(BMessage *m)
{
    // Horrible hack!!! XXX XXX XXX
    // The real problem is that b_start_ffc is set too late for
    // the initial empty buffer. As a result the window will be
    // split instead of abandoned.
    int limit = 15;
    while (--limit >= 0 && (curbuf == NULL || curbuf->b_start_ffc == 0))
	snooze(100000);    //  0.1 s
    if (gui.mnvWindow)
	gui.mnvWindow->Minimize(false);
    DetachCurrentMessage();
    SendRefs(m, true);
}

/*
 * Pass a BMessage on to the main() thread.
 * Caller must have detached the message.
 */
    void
MNVApp::SendRefs(BMessage *m, bool changedir)
{
    MNVRefsMsg rm;
    rm.message = m;
    rm.changedir = changedir;

    write_port(gui.vdcmp, MNVMsg::Refs, &rm, sizeof(rm));
    //	calls ::RefsReceived
}

    void
MNVApp::MessageReceived(BMessage *m)
{
    switch (m->what) {
	case 'save':
	    {
		entry_ref refDirectory;
		m->FindRef("directory", &refDirectory);
		fBrowsedPath.SetTo(&refDirectory);
		BString strName;
		m->FindString("name", &strName);
		fBrowsedPath.Append(strName.String());
	    }
	    break;
	case 'open':
	    {
		entry_ref ref;
		m->FindRef("refs", &ref);
		fBrowsedPath.SetTo(&ref);
	    }
	    break;
	case B_CANCEL:
	    {
		BFilePanel *panel;
		m->FindPointer("source", (void**)&panel);
		if (fFilePanelSem != -1 && panel == fFilePanel)
		{
		    delete_sem(fFilePanelSem);
		    fFilePanelSem = -1;
		}

	    }
	    break;
	default:
	    Inherited::MessageReceived(m);
	    break;
    }
}

    bool
MNVApp::QuitRequested()
{
    (void)Inherited::QuitRequested();
    return false;
}

// ---------------- MNVWindow ----------------

MNVWindow::MNVWindow():
    BWindow(BRect(40, 40, 150, 150),
	    "MNV",
	    B_TITLED_WINDOW,
	    0,
	    B_CURRENT_WORKSPACE)

{
    init();
}

MNVWindow::~MNVWindow()
{
    if (formView) {
	RemoveChild(formView);
	delete formView;
    }
    gui.mnvWindow = NULL;
}

    void
MNVWindow::init()
{
    // Attach the MNVFormView
    formView = new MNVFormView(Bounds());
    if (formView != NULL) {
	AddChild(formView);
    }
}

#if 0  //  disabled in zeta patch
    void
MNVWindow::DispatchMessage(BMessage *m, BHandler *h)
{
    /*
     * Route B_MOUSE_UP messages to MouseUp(), in
     * a manner that should be compatible with the
     * intended future system behaviour.
     */
    switch (m->what) {
	case B_MOUSE_UP:
	    //	if (!h) h = PreferredHandler();
	    //	gcc isn't happy without this extra set of braces, complains about
	    //	jump to case label crosses init of 'class BView * v'
	    //	richard@whitequeen.com jul 99
	    {
		BView *v = dynamic_cast<BView *>(h);
		if (v) {
		    // m->PrintToStream();
		    BPoint where;
		    m->FindPoint("where", &where);
		    v->MouseUp(where);
		} else {
		    Inherited::DispatchMessage(m, h);
		}
	    }
	    break;
	default:
	    Inherited::DispatchMessage(m, h);
    }
}
#endif

    void
MNVWindow::WindowActivated(bool active)
{
    Inherited::WindowActivated(active);
    // the textArea gets the keyboard action
    if (active && gui.mnvTextArea)
	gui.mnvTextArea->MakeFocus(true);

    struct MNVFocusMsg fm;
    fm.active = active;

    write_port(gui.vdcmp, MNVMsg::Focus, &fm, sizeof(fm));
}

    bool
MNVWindow::QuitRequested()
{
    struct MNVKeyMsg km;
    km.length = 5;
    memcpy((char *)km.chars, "\033:qa\r", km.length);
    km.csi_escape = false;
    write_port(gui.vdcmp, MNVMsg::Key, &km, sizeof(km));
    return false;
}
    void
MNVWindow::DispatchMessage(BMessage *m, BHandler *h)
{
    bool should_propagate = true;

    switch (m->what)
    {
	case B_KEY_DOWN:
	    {
		int32 scancode = 0;
		int32 beModifiers = 0;
		m->FindInt32("raw_char", &scancode);
		m->FindInt32("modifiers", &beModifiers);

		if (scancode == B_ENTER && (beModifiers & B_LEFT_COMMAND_KEY))
		    {
			should_propagate = false;
			if (this->is_fullscreen)
			    {
				this->is_fullscreen = false;
				ResizeTo(this->saved_frame.Width(), this->saved_frame.Height());
				MoveTo(this->saved_frame.left, this->saved_frame.top);
				SetLook(this->saved_look);
				SetFlags(Flags() & ~(B_NOT_RESIZABLE | B_NOT_MOVABLE));
			    } else {
				this->saved_frame = Frame();
				this->saved_look = Look();
				this->is_fullscreen = true;
				BScreen s(this);
				SetLook(B_NO_BORDER_WINDOW_LOOK);
				ResizeTo(s.Frame().Width() + 1, s.Frame().Height() + 1);
				MoveTo(s.Frame().left, s.Frame().top);
				SetFlags(Flags() | (B_NOT_RESIZABLE | B_NOT_MOVABLE));
			    }
		    }
	    }
    }

    if (should_propagate)
	Inherited::DispatchMessage(m, h);
}


// ---------------- MNVFormView ----------------

MNVFormView::MNVFormView(BRect frame):
    BView(frame, "MNVFormView", B_FOLLOW_ALL_SIDES,
	    B_WILL_DRAW | B_FRAME_EVENTS),
    menuBar(NULL),
#ifdef FEAT_TOOLBAR
    toolBar(NULL),
#endif
#ifdef FEAT_GUI_TABLINE
//  showingTabLine(false),
    tabLine(NULL),
#endif
    textArea(NULL)
{
    init(frame);
}

MNVFormView::~MNVFormView()
{
    if (menuBar) {
	RemoveChild(menuBar);
#ifdef never
	//  deleting the menuBar leads to SEGV on exit
	//  richard@whitequeen.com Jul 99
	delete menuBar;
#endif
    }

#ifdef FEAT_TOOLBAR
    delete toolBar;
#endif

#ifdef FEAT_GUI_TABLINE
    delete tabLine;
#endif

    if (textArea) {
	RemoveChild(textArea);
	delete textArea;
    }
    gui.mnvForm = NULL;
}

    void
MNVFormView::init(BRect frame)
{
    menuBar = new BMenuBar(BRect(0,0,-MENUBAR_MARGIN,-MENUBAR_MARGIN),
	    "MNVMenuBar");

    AddChild(menuBar);

#ifdef FEAT_TOOLBAR
    toolBar = new MNVToolbar(BRect(0,0,0,0), "MNVToolBar");
    toolBar->PrepareButtonBitmaps();
    AddChild(toolBar);
#endif

#ifdef FEAT_GUI_TABLINE
    tabLine = new MNVTabLine(BRect(0,0,0,0));
//  tabLine->PrepareButtonBitmaps();
    AddChild(tabLine);
#endif

    BRect remaining = frame;
    textArea = new MNVTextAreaView(remaining);
    AddChild(textArea);
    // The textArea will be resized later when menus are added

    gui.mnvForm = this;
}

#ifdef FEAT_TOOLBAR
    float
MNVFormView::ToolbarHeight() const
{
    return toolBar ? toolBar->ToolbarHeight() : 0.;
}
#endif

#ifdef FEAT_GUI_TABLINE
    float
MNVFormView::TablineHeight() const
{
    return (tabLine && IsShowingTabLine()) ? tabLine->TablineHeight() : 0.;
}
#endif

    void
MNVFormView::AllAttached()
{
    /*
     * Apparently signals are inherited by the created thread -
     * disable the most annoying ones.
     */
    mch_signal(SIGINT, SIG_IGN);
    mch_signal(SIGQUIT, SIG_IGN);

    if (menuBar && textArea) {
	/*
	 * Resize the textArea to fill the space left over by the menu.
	 * This is somewhat futile since it will be done again once
	 * menus are added to the menu bar.
	 */
	BRect remaining = Bounds();

#ifdef FEAT_MENU
	remaining.top += MenuHeight();
	menuBar->ResizeTo(remaining.right, remaining.top);
	gui.menu_height = (int) MenuHeight();
#endif

#ifdef FEAT_TOOLBAR
	toolBar->MoveTo(remaining.left, remaining.top);
	toolBar->ResizeTo(remaining.right, ToolbarHeight());
	remaining.top += ToolbarHeight();
	gui.toolbar_height = ToolbarHeight();
#endif

#ifdef FEAT_GUI_TABLINE
	tabLine->MoveTo(remaining.left, remaining.top);
	tabLine->ResizeTo(remaining.right + 1, TablineHeight());
	remaining.top += TablineHeight();
	gui.tabline_height = TablineHeight();
#endif

	textArea->ResizeTo(remaining.Width(), remaining.Height());
	textArea->MoveTo(remaining.left, remaining.top);
    }


    Inherited::AllAttached();
}

    void
MNVFormView::FrameResized(float new_width, float new_height)
{
    struct MNVResizeMsg sm;
    int adjust_h, adjust_w;

    new_width += 1;	//  adjust from width to number of pixels occupied
    new_height += 1;

    sm.width = (int) new_width;
    sm.height = (int) new_height;
    adjust_w = ((int)new_width - gui_get_base_width()) % gui.char_width;
    adjust_h = ((int)new_height - gui_get_base_height()) % gui.char_height;

    if (adjust_w > 0 || adjust_h > 0) {
	sm.width  -= adjust_w;
	sm.height -= adjust_h;
    }

    write_port(gui.vdcmp, MNVMsg::Resize, &sm, sizeof(sm));
    //	calls gui_resize_shell(new_width, new_height);

    return;

    /*
     * The area below the vertical scrollbar is erased to the colour
     * set with SetViewColor() automatically, because we had set
     * B_WILL_DRAW. Resizing the window tight around the vertical
     * scroll bar also helps to avoid debris.
     */
}

// ---------------- MNVTextAreaView ----------------

MNVTextAreaView::MNVTextAreaView(BRect frame):
    BView(frame, "MNVTextAreaView", B_FOLLOW_ALL_SIDES,
#ifdef FEAT_MBYTE_IME
	B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE | B_INPUT_METHOD_AWARE
#else
	B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE
#endif
	),
    mouseDragEventCount(0)
{
#ifdef FEAT_MBYTE_IME
    IMData.messenger = NULL;
    IMData.message = NULL;
#endif
    init(frame);
}

MNVTextAreaView::~MNVTextAreaView()
{
    gui.mnvTextArea = NULL;
}

    void
MNVTextAreaView::init(BRect frame)
{
    // set up global var for fast access
    gui.mnvTextArea = this;

    /*
     * Tell the app server not to erase the view: we will
     * fill it in completely by ourselves.
     * (Does this really work? Even if not, it won't harm either.)
     */
    SetViewColor(B_TRANSPARENT_32_BIT);
#define PEN_WIDTH   1
    SetPenSize(PEN_WIDTH);
#define W_WIDTH(curwin)   0
}

    void
MNVTextAreaView::Draw(BRect updateRect)
{
    /*
     * XXX Other ports call here:
     * out_flush();	 * make sure all output has been processed *
     * but we can't do that, since it involves too much information
     * that is owned by other threads...
     */

    /*
     *	No need to use gui.mnvWindow->Lock(): we are locked already.
     *	However, it would not hurt.
     */
    rgb_color rgb = GUI_TO_RGB(gui.back_pixel);
    SetLowColor(rgb);
    FillRect(updateRect, B_SOLID_LOW);
    gui_redraw((int) updateRect.left, (int) updateRect.top,
	    (int) (updateRect.Width() + PEN_WIDTH), (int) (updateRect.Height() + PEN_WIDTH));

    // Clear the border areas if needed
    SetLowColor(rgb);

    if (updateRect.left < FILL_X(0))	//  left border
	FillRect(BRect(updateRect.left, updateRect.top,
		    FILL_X(0)-PEN_WIDTH, updateRect.bottom), B_SOLID_LOW);
    if (updateRect.top < FILL_Y(0)) //	top border
	FillRect(BRect(updateRect.left, updateRect.top,
		    updateRect.right, FILL_Y(0)-PEN_WIDTH), B_SOLID_LOW);
    if (updateRect.right >= FILL_X(Columns)) //  right border
	FillRect(BRect(FILL_X((int)Columns), updateRect.top,
		    updateRect.right, updateRect.bottom), B_SOLID_LOW);
    if (updateRect.bottom >= FILL_Y(Rows))   //  bottom border
	FillRect(BRect(updateRect.left, FILL_Y((int)Rows),
		    updateRect.right, updateRect.bottom), B_SOLID_LOW);

#ifdef FEAT_MBYTE_IME
    DrawIMString();
#endif
}

    void
MNVTextAreaView::KeyDown(const char *bytes, int32 numBytes)
{
    struct MNVKeyMsg km;
    char_u *dest = km.chars;

    bool canHaveMNVModifiers = false;

    BMessage *msg = Window()->CurrentMessage();
    assert(msg);
    // msg->PrintToStream();

    /*
     * Convert special keys to MNV codes.
     * I think it is better to do it in the window thread
     * so we use at least a little bit of the potential
     * of our 2 CPUs. Besides, due to the fantastic mapping
     * of special keys to UTF-8, we have quite some work to
     * do...
     * TODO: I'm not quite happy with detection of special
     * keys. Perhaps I should use scan codes after all...
     */
    if (numBytes > 1) {
	// This cannot be a special key
	if (numBytes > KEY_MSG_BUFSIZ)
	    numBytes = KEY_MSG_BUFSIZ;	    //	should never happen... ???
	km.length = numBytes;
	memcpy((char *)dest, bytes, numBytes);
	km.csi_escape = true;
    } else {
	int32 scancode = 0;
	msg->FindInt32("key", &scancode);

	int32 beModifiers = 0;
	msg->FindInt32("modifiers", &beModifiers);

	char_u string[3];
	int len = 0;
	km.length = 0;

	/*
	 * For normal, printable ASCII characters, don't look them up
	 * to check if they might be a special key. They aren't.
	 */
	assert(B_BACKSPACE <= 0x20);
	assert(B_DELETE == 0x7F);
	if (((char_u)bytes[0] <= 0x20 || (char_u)bytes[0] == 0x7F) &&
		numBytes == 1) {
	    /*
	     * Due to the great nature of Be's mapping of special keys,
	     * viz. into the range of the control characters,
	     * we can only be sure it is *really* a special key if
	     * if it is special without using ctrl. So, only if ctrl is
	     * used, we need to check it unmodified.
	     */
	    if (beModifiers & B_CONTROL_KEY) {
		int index = keyMap->normal_map[scancode];
		int newNumBytes = keyMapChars[index];
		char_u *newBytes = (char_u *)&keyMapChars[index + 1];

		/*
		 * Check if still special without the control key.
		 * This is needed for BACKSPACE: that key does produce
		 * different values with modifiers (DEL).
		 * Otherwise we could simply have checked for equality.
		 */
		if (newNumBytes != 1 || (*newBytes > 0x20 &&
			    *newBytes != 0x7F )) {
		    goto notspecial;
		}
		bytes = (char *)newBytes;
	    }
	    canHaveMNVModifiers = true;

	    uint16 beoskey;
	    int first, last;

	    /*
	     * If numBytes == 0 that probably always indicates a special key.
	     * (does not happen yet)
	     */
	    if (numBytes == 0 || bytes[0] == B_FUNCTION_KEY) {
		beoskey = F(scancode);
		first = FIRST_FUNCTION_KEY;
		last = NUM_SPECIAL_KEYS;
	    } else if (*bytes == '\n' && scancode == 0x47) {
		// remap the (non-keypad) ENTER key from \n to \r.
		string[0] = '\r';
		len = 1;
		first = last = 0;
	    } else {
		beoskey = K(bytes[0]);
		first = 0;
		last = FIRST_FUNCTION_KEY;
	    }

	    for (int i = first; i < last; i++) {
		if (special_keys[i].BeKeys == beoskey) {
		    string[0] = CSI;
		    string[1] = special_keys[i].mnv_code0;
		    string[2] = special_keys[i].mnv_code1;
		    len = 3;
		}
	    }
	}
notspecial:
	if (len == 0) {
	    string[0] = bytes[0];
	    len = 1;
	}

	// Special keys (and a few others) may have modifiers
#if 0
	if (len == 3 ||
		bytes[0] == B_SPACE || bytes[0] == B_TAB ||
		bytes[0] == B_RETURN || bytes[0] == '\r' ||
		bytes[0] == B_ESCAPE)
#else
	    if (canHaveMNVModifiers)
#endif
	    {
		int modifiers;
		modifiers = 0;
		if (beModifiers & B_SHIFT_KEY)
		    modifiers |= MOD_MASK_SHIFT;
		if (beModifiers & B_CONTROL_KEY)
		    modifiers |= MOD_MASK_CTRL;
		if (beModifiers & B_OPTION_KEY)
		    modifiers |= MOD_MASK_ALT;

		/*
		 * For some keys a shift modifier is translated into another key
		 * code.  Do we need to handle the case where len != 1 and
		 * string[0] != CSI? (Not for BeOS, since len == 3 implies
		 * string[0] == CSI...)
		 */
		int key;
		if (string[0] == CSI && len == 3)
		    key = TO_SPECIAL(string[1], string[2]);
		else
		    key = string[0];
		key = simplify_key(key, &modifiers);
		if (IS_SPECIAL(key))
		{
		    string[0] = CSI;
		    string[1] = K_SECOND(key);
		    string[2] = K_THIRD(key);
		    len = 3;
		}
		else
		{
		    string[0] = key;
		    len = 1;
		}

		if (modifiers)
		{
		    *dest++ = CSI;
		    *dest++ = KS_MODIFIER;
		    *dest++ = modifiers;
		    km.length = 3;
		}
	    }
	memcpy((char *)dest, string, len);
	km.length += len;
	km.csi_escape = false;
    }

    write_port(gui.vdcmp, MNVMsg::Key, &km, sizeof(km));

    /*
     * blank out the pointer if necessary
     */
    if (p_mh && !gui.pointer_hidden)
    {
	guiBlankMouse(true);
	gui.pointer_hidden = TRUE;
    }
}
void
MNVTextAreaView::guiSendMouseEvent(
	int	button,
	int	x,
	int	y,
	int	repeated_click,
	int_u	modifiers)
{
    MNVMouseMsg mm;

    mm.button = button;
    mm.x = x;
    mm.y = y;
    mm.repeated_click = repeated_click;
    mm.modifiers = modifiers;

    write_port(gui.vdcmp, MNVMsg::Mouse, &mm, sizeof(mm));
    //	calls gui_send_mouse_event()

    /*
     * if our pointer is currently hidden, then we should show it.
     */
    if (gui.pointer_hidden)
    {
	guiBlankMouse(false);
	gui.pointer_hidden = FALSE;
    }
}

void
MNVTextAreaView::guiMouseMoved(
	int	x,
	int	y)
{
    MNVMouseMovedMsg mm;

    mm.x = x;
    mm.y = y;

    write_port(gui.vdcmp, MNVMsg::MouseMoved, &mm, sizeof(mm));

    if (gui.pointer_hidden)
    {
	guiBlankMouse(false);
	gui.pointer_hidden = FALSE;
    }
}

    void
MNVTextAreaView::guiBlankMouse(bool should_hide)
{
    if (should_hide) {
	// gui.mnvApp->HideCursor();
	gui.mnvApp->ObscureCursor();
	/*
	 * ObscureCursor() would even be easier, but then
	 * MNV's idea of mouse visibility does not necessarily
	 * correspond to reality.
	 */
    } else {
	// gui.mnvApp->ShowCursor();
    }
}

    int_u
MNVTextAreaView::mouseModifiersToMNV(int32 beModifiers)
{
    int_u mnv_modifiers = 0x0;

    if (beModifiers & B_SHIFT_KEY)
	mnv_modifiers |= MOUSE_SHIFT;
    if (beModifiers & B_CONTROL_KEY)
	mnv_modifiers |= MOUSE_CTRL;
    if (beModifiers & B_OPTION_KEY)	// Alt or Meta key
	mnv_modifiers |= MOUSE_ALT;

    return mnv_modifiers;
}

    void
MNVTextAreaView::MouseDown(BPoint point)
{
    BMessage *m = Window()->CurrentMessage();
    assert(m);

    int32 buttons = 0;
    m->FindInt32("buttons", &buttons);

    int mnvButton;

    if (buttons & B_PRIMARY_MOUSE_BUTTON)
	mnvButton = MOUSE_LEFT;
    else if (buttons & B_SECONDARY_MOUSE_BUTTON)
	mnvButton = MOUSE_RIGHT;
    else if (buttons & B_TERTIARY_MOUSE_BUTTON)
	mnvButton = MOUSE_MIDDLE;
    else
	return;		// Unknown button

    mnvMouseButton = 1;	    // don't care which one

    // Handle multiple clicks
    int32 clicks = 0;
    m->FindInt32("clicks", &clicks);

    int32 modifiers = 0;
    m->FindInt32("modifiers", &modifiers);

    mnvMouseModifiers = mouseModifiersToMNV(modifiers);

    guiSendMouseEvent(mnvButton, point.x, point.y,
	    clicks > 1 /* = repeated_click*/, mnvMouseModifiers);
}

    void
MNVTextAreaView::MouseUp(BPoint point)
{
    mnvMouseButton = 0;

    BMessage *m = Window()->CurrentMessage();
    assert(m);
    // m->PrintToStream();

    int32 modifiers = 0;
    m->FindInt32("modifiers", &modifiers);

    mnvMouseModifiers = mouseModifiersToMNV(modifiers);

    guiSendMouseEvent(MOUSE_RELEASE, point.x, point.y,
	    0 /* = repeated_click*/, mnvMouseModifiers);

    Inherited::MouseUp(point);
}

    void
MNVTextAreaView::MouseMoved(BPoint point, uint32 transit, const BMessage *message)
{
    /*
     * if our pointer is currently hidden, then we should show it.
     */
    if (gui.pointer_hidden)
    {
	guiBlankMouse(false);
	gui.pointer_hidden = FALSE;
    }

    if (!mnvMouseButton) {    // could also check m->"buttons"
	guiMouseMoved(point.x, point.y);
	return;
    }

    atomic_add(&mouseDragEventCount, 1);

    // Don't care much about "transit"
    guiSendMouseEvent(MOUSE_DRAG, point.x, point.y, 0, mnvMouseModifiers);
}

    void
MNVTextAreaView::MessageReceived(BMessage *m)
{
    switch (m->what) {
	case 'menu':
	    {
		MNVMenuMsg mm;
		mm.guiMenu = NULL;  // in case no pointer in msg
		m->FindPointer("MNVMenu", (void **)&mm.guiMenu);
		write_port(gui.vdcmp, MNVMsg::Menu, &mm, sizeof(mm));
	    }
	    break;
	case B_MOUSE_WHEEL_CHANGED:
	    {
		MNVScrollBar* scb = curwin->w_scrollbars[1].id;
		float small=0, big=0, dy=0;
		m->FindFloat("be:wheel_delta_y", &dy);
		scb->GetSteps(&small, &big);
		scb->SetValue(scb->Value()+small*dy*3);
		scb->ValueChanged(scb->Value());
#if 0
		scb = curwin->w_scrollbars[0].id;
		scb->GetSteps(&small, &big);
		scb->SetValue(scb->Value()+small*dy);
		scb->ValueChanged(scb->Value());
#endif
	    }
	    break;
#ifdef FEAT_MBYTE_IME
	case B_INPUT_METHOD_EVENT:
	    {
		int32 opcode;
		m->FindInt32("be:opcode", &opcode);
		switch(opcode)
		{
		    case B_INPUT_METHOD_STARTED:
			if (!IMData.messenger) delete IMData.messenger;
			IMData.messenger = new BMessenger();
			m->FindMessenger("be:reply_to", IMData.messenger);
			break;
		    case B_INPUT_METHOD_CHANGED:
			{
			    BString str;
			    bool confirmed;
			    if (IMData.message) *(IMData.message) = *m;
			    else	       IMData.message = new BMessage(*m);
			    DrawIMString();
			    m->FindBool("be:confirmed", &confirmed);
			    if (confirmed)
			    {
				m->FindString("be:string", &str);
				char_u *chars = (char_u*)str.String();
				struct MNVKeyMsg km;
				km.csi_escape = true;
				int clen;
				int i = 0;
				while (i < str.Length())
				{
				    clen = utf_ptr2len(chars+i);
				    memcpy(km.chars, chars+i, clen);
				    km.length = clen;
				    write_port(gui.vdcmp, MNVMsg::Key, &km, sizeof(km));
				    i += clen;
				}
			    }
			}
			break;
		    case B_INPUT_METHOD_LOCATION_REQUEST:
			{
			    BMessage msg(B_INPUT_METHOD_EVENT);
			    msg.AddInt32("be:opcode", B_INPUT_METHOD_LOCATION_REQUEST);
			    msg.AddPoint("be:location_reply", IMData.location);
			    msg.AddFloat("be:height_reply", FILL_Y(1));
			    IMData.messenger->SendMessage(&msg);
			}
			break;
		    case B_INPUT_METHOD_STOPPED:
			delete IMData.messenger;
			delete IMData.message;
			IMData.messenger = NULL;
			IMData.message = NULL;
			break;
		}
	    }
	    // TODO: sz: break here???
#endif
	default:
	    if (m->WasDropped()) {
		BWindow *w = Window();
		w->DetachCurrentMessage();
		w->Minimize(false);
		MNVApp::SendRefs(m, (modifiers() & B_SHIFT_KEY) != 0);
	    } else {
		Inherited::MessageReceived(m);
	    }
	    break;
    }
}

    int
MNVTextAreaView::mchInitFont(char_u *name)
{
    MNVFont *newFont = (MNVFont *)gui_mch_get_font(name, 1);
    if (newFont != NOFONT) {
	gui.norm_font = (GuiFont)newFont;
	gui_mch_set_font((GuiFont)newFont);
	if (name && STRCMP(name, "*") != 0)
	    hl_set_font_name(name);

	SetDrawingMode(B_OP_COPY);

	/*
	 * Try to load other fonts for bold, italic, and bold-italic.
	 * We should also try to work out what font to use for these when they are
	 * not specified by X resources, but we don't yet.
	 */
	return OK;
    }
    return FAIL;
}

    void
MNVTextAreaView::mchDrawString(int row, int col, char_u *s, int len, int flags)
{
    /*
     * First we must erase the area, because DrawString won't do
     * that for us. XXX Most of the time this is a waste of effort
     * since the bachground has been erased already... DRAW_TRANSP
     * should be set when appropriate!!!
     * (Rectangles include the bottom and right edge)
     */
    if (!(flags & DRAW_TRANSP)) {
	int cells;
	cells = 0;
	for (int i=0; i<len; i++) {
	    int cn = utf_ptr2cells((char_u *)(s+i));
	    if (cn<4) cells += cn;
	}

	BRect r(FILL_X(col), FILL_Y(row),
		FILL_X(col + cells) - PEN_WIDTH, FILL_Y(row + 1) - PEN_WIDTH);
	FillRect(r, B_SOLID_LOW);
    }

    BFont font;
    this->GetFont(&font);
    if (!font.IsFixed())
    {
	char* p = (char*)s;
	int32 clen, lastpos = 0;
	BPoint where;
	int cells;
	while ((p - (char*)s) < len) {
	    clen = utf_ptr2len((u_char*)p);
	    where.Set(TEXT_X(col+lastpos), TEXT_Y(row));
	    DrawString(p, clen, where);
	    if (flags & DRAW_BOLD) {
		where.x += 1.0;
		SetDrawingMode(B_OP_BLEND);
		DrawString(p, clen, where);
		SetDrawingMode(B_OP_COPY);
	    }
	    cells = utf_ptr2cells((char_u *)p);
	    if (cells<4) lastpos += cells;
	    else	lastpos++;
	    p += clen;
	}
    }
    else
    {
	BPoint where(TEXT_X(col), TEXT_Y(row));
	DrawString((char*)s, len, where);
	if (flags & DRAW_BOLD) {
	    where.x += 1.0;
	    SetDrawingMode(B_OP_BLEND);
	    DrawString((char*)s, len, where);
	    SetDrawingMode(B_OP_COPY);
	}
    }

    if (flags & DRAW_UNDERL) {
	int cells;
	cells = 0;
	for (int i=0; i<len; i++) {
	    int cn = utf_ptr2cells((char_u *)(s+i));
	    if (cn<4) cells += cn;
	}

	BPoint start(FILL_X(col), FILL_Y(row + 1) - PEN_WIDTH);
	BPoint end(FILL_X(col + cells) - PEN_WIDTH, start.y);

	StrokeLine(start, end);
    }
}

void
MNVTextAreaView::mchClearBlock(
	int	row1,
	int	col1,
	int	row2,
	int	col2)
{
    BRect r(FILL_X(col1), FILL_Y(row1),
	    FILL_X(col2 + 1) - PEN_WIDTH, FILL_Y(row2 + 1) - PEN_WIDTH);
    gui_mch_set_bg_color(gui.back_pixel);
    FillRect(r, B_SOLID_LOW);
}

    void
MNVTextAreaView::mchClearAll()
{
    gui_mch_set_bg_color(gui.back_pixel);
    FillRect(Bounds(), B_SOLID_LOW);
}

/*
 * mchDeleteLines() Lock()s the window by itself.
 */
    void
MNVTextAreaView::mchDeleteLines(int row, int num_lines)
{
    BRect source, dest;
    source.left = FILL_X(gui.scroll_region_left);
    source.top = FILL_Y(row + num_lines);
    source.right = FILL_X(gui.scroll_region_right + 1) - PEN_WIDTH;
    source.bottom = FILL_Y(gui.scroll_region_bot + 1) - PEN_WIDTH;

    dest.left = FILL_X(gui.scroll_region_left);
    dest.top = FILL_Y(row);
    dest.right = FILL_X(gui.scroll_region_right + 1) - PEN_WIDTH;
    dest.bottom = FILL_Y(gui.scroll_region_bot - num_lines + 1) - PEN_WIDTH;

    if (gui.mnvWindow->Lock()) {
	// Clear one column more for when bold has spilled over
	CopyBits(source, dest);
	gui_clear_block(gui.scroll_region_bot - num_lines + 1,
		gui.scroll_region_left,
		gui.scroll_region_bot, gui.scroll_region_right);


	gui.mnvWindow->Unlock();
	/*
	 * The Draw() callback will be called now if some of the source
	 * bits were not in the visible region.
	 */
    }
    // gui_x11_check_copy_area();
    //	}
}

/*
 * mchInsertLines() Lock()s the window by itself.
 */
    void
MNVTextAreaView::mchInsertLines(int row, int num_lines)
{
    BRect source, dest;

    // XXX Attempt at a hack:
    gui.mnvWindow->UpdateIfNeeded();
    source.left = FILL_X(gui.scroll_region_left);
    source.top = FILL_Y(row);
    source.right = FILL_X(gui.scroll_region_right + 1) - PEN_WIDTH;
    source.bottom = FILL_Y(gui.scroll_region_bot - num_lines + 1) - PEN_WIDTH;

    dest.left = FILL_X(gui.scroll_region_left);
    dest.top = FILL_Y(row + num_lines);
    dest.right = FILL_X(gui.scroll_region_right + 1) - PEN_WIDTH;
    dest.bottom = FILL_Y(gui.scroll_region_bot + 1) - PEN_WIDTH;

    if (gui.mnvWindow->Lock()) {
	// Clear one column more for when bold has spilled over
	CopyBits(source, dest);
	gui_clear_block(row, gui.scroll_region_left,
		row + num_lines - 1, gui.scroll_region_right);

	gui.mnvWindow->Unlock();
	/*
	 * The Draw() callback will be called now if some of the source
	 * bits were not in the visible region.
	 * However, if we scroll too fast it can't keep up and the
	 * update region gets messed up. This seems to be because copying
	 * un-Draw()n bits does not generate Draw() calls for the copy...
	 * I moved the hack to before the CopyBits() to reduce the
	 * amount of additional waiting needed.
	 */

	// gui_x11_check_copy_area();

    }
}

#ifdef FEAT_MBYTE_IME
/*
 * DrawIMString draws string with IMData.message.
 */
void MNVTextAreaView::DrawIMString(void)
{
    static const rgb_color r_highlight = {255, 152, 152, 255},
		 b_highlight = {152, 203, 255, 255};
    BString str;
    const char* s;
    int len;
    BMessage* msg = IMData.message;
    if (!msg)
	return;
    gui_redraw_block(IMData.row, 0,
	    IMData.row + IMData.count, W_WIDTH(curwin), GUI_MON_NOCLEAR);
    bool confirmed = false;
    msg->FindBool("be:confirmed", &confirmed);
    if (confirmed)
	return;
    rgb_color hcolor = HighColor(), lcolor = LowColor();
    msg->FindString("be:string", &str);
    s = str.String();
    len = str.Length();
    SetHighColor(0, 0, 0);
    IMData.row = gui.row;
    IMData.col = gui.col;
    int32 sel_start = 0, sel_end = 0;
    msg->FindInt32("be:selection", 0, &sel_start);
    msg->FindInt32("be:selection", 1, &sel_end);
    int clen, cn;
    BPoint pos(IMData.col, 0);
    BRect r;
    BPoint where;
    IMData.location = ConvertToScreen(
	    BPoint(FILL_X(pos.x), FILL_Y(IMData.row + pos.y)));
    for (int i=0; i<len; i+=clen)
    {
	cn = utf_ptr2cells((char_u *)(s+i));
	clen = utf_ptr2len((char_u *)(s+i));
	if (pos.x + cn > W_WIDTH(curwin))
	{
	    pos.y++;
	    pos.x = 0;
	}
	if (sel_start<=i && i<sel_end)
	{
	    SetLowColor(r_highlight);
	    IMData.location = ConvertToScreen(
		    BPoint(FILL_X(pos.x), FILL_Y(IMData.row + pos.y)));
	}
	else
	{
	    SetLowColor(b_highlight);
	}
	r.Set(FILL_X(pos.x), FILL_Y(IMData.row + pos.y),
		FILL_X(pos.x + cn) - PEN_WIDTH,
		FILL_Y(IMData.row + pos.y + 1) - PEN_WIDTH);
	FillRect(r, B_SOLID_LOW);
	where.Set(TEXT_X(pos.x), TEXT_Y(IMData.row + pos.y));
	DrawString((s+i), clen, where);
	pos.x += cn;
    }
    IMData.count = (int)pos.y;

    SetHighColor(hcolor);
    SetLowColor(lcolor);
}
#endif
// ---------------- MNVScrollBar ----------------

/*
 * BUG: XXX
 * It seems that BScrollBar determine their direction not from
 * "posture" but from if they are "tall" or "wide" in shape...
 *
 * Also, place them out of sight, because MNV enables them before
 * they are positioned.
 */
MNVScrollBar::MNVScrollBar(scrollbar_T *g, orientation posture):
    BScrollBar(posture == B_HORIZONTAL ?  BRect(-100,-100,-10,-90) :
	    BRect(-100,-100,-90,-10),
	    "mnv scrollbar", (BView *)NULL,
	    0.0, 10.0, posture),
    ignoreValue(-1),
    scrollEventCount(0)
{
    gsb = g;
    SetResizingMode(B_FOLLOW_NONE);
}

MNVScrollBar::~MNVScrollBar()
{
}

    void
MNVScrollBar::ValueChanged(float newValue)
{
    if (ignoreValue >= 0.0 && newValue == ignoreValue) {
	ignoreValue = -1;
	return;
    }
    ignoreValue = -1;
    /*
     * We want to throttle the amount of scroll messages generated.
     * Normally I presume you won't get a new message before we've
     * handled the previous one, but because we're passing them on this
     * happens very quickly. So instead we keep a counter of how many
     * scroll events there are (or will be) in the VDCMP, and the
     * throttling happens at the receiving end.
     */
    atomic_add(&scrollEventCount, 1);

    struct MNVScrollBarMsg sm;

    sm.sb = this;
    sm.value = (long) newValue;
    sm.stillDragging = TRUE;

    write_port(gui.vdcmp, MNVMsg::ScrollBar, &sm, sizeof(sm));

    //	calls gui_drag_scrollbar(sb, newValue, TRUE);
}

/*
 * When the mouse goes up, report that scrolling has stopped.
 * MouseUp() is NOT called when the mouse-up occurs outside
 * the window, even though the thumb does move while the mouse
 * is outside... This has some funny effects... XXX
 * So we do special processing when the window de/activates.
 */
    void
MNVScrollBar::MouseUp(BPoint where)
{
    // BMessage *m = Window()->CurrentMessage();
    // m->PrintToStream();

    atomic_add(&scrollEventCount, 1);

    struct MNVScrollBarMsg sm;

    sm.sb = this;
    sm.value = (long) Value();
    sm.stillDragging = FALSE;

    write_port(gui.vdcmp, MNVMsg::ScrollBar, &sm, sizeof(sm));

    //	calls gui_drag_scrollbar(sb, newValue, FALSE);

    Inherited::MouseUp(where);
}

    void
MNVScrollBar::SetValue(float newValue)
{
    if (newValue == Value())
	return;

    ignoreValue = newValue;
    Inherited::SetValue(newValue);
}

// ---------------- MNVFont ----------------

MNVFont::MNVFont(): BFont()
{
    init();
}

MNVFont::MNVFont(const MNVFont *rhs): BFont(rhs)
{
    init();
}

MNVFont::MNVFont(const BFont *rhs): BFont(rhs)
{
    init();
}

MNVFont::MNVFont(const MNVFont &rhs): BFont(rhs)
{
    init();
}

MNVFont::~MNVFont()
{
}

    void
MNVFont::init()
{
    next = NULL;
    refcount = 1;
    name = NULL;
}

// ---------------- MNVDialog ----------------

#if defined(FEAT_GUI_DIALOG)

const unsigned int  kMNVDialogButtonMsg = 'VMDB';
const unsigned int  kMNVDialogIconStripeWidth = 30;
const unsigned int  kMNVDialogButtonsSpacingX = 9;
const unsigned int  kMNVDialogButtonsSpacingY = 4;
const unsigned int  kMNVDialogSpacingX = 6;
const unsigned int  kMNVDialogSpacingY = 10;
const unsigned int  kMNVDialogMinimalWidth  = 310;
const unsigned int  kMNVDialogMinimalHeight = 75;
const BRect	    kDefaultRect =
BRect(0, 0, kMNVDialogMinimalWidth, kMNVDialogMinimalHeight);

MNVDialog::MNVDialog(int type, const char *title, const char *message,
	const char *buttons, int dfltbutton, const char *textfield, int ex_cmd)
: BWindow(kDefaultRect, title, B_TITLED_WINDOW_LOOK, B_MODAL_APP_WINDOW_FEEL,
	B_NOT_CLOSABLE | B_NOT_RESIZABLE |
	B_NOT_ZOOMABLE | B_NOT_MINIMIZABLE | B_ASYNCHRONOUS_CONTROLS)
    , fDialogSem(-1)
    , fDialogValue(dfltbutton)
    , fMessageView(NULL)
    , fInputControl(NULL)
    , fInputValue(textfield)
{
    //	master view
    MNVDialog::View* view = new MNVDialog::View(Bounds());
    if (view == NULL)
	return;

    if (title == NULL)
	SetTitle("MNV " MNV_VERSION_MEDIUM);

    AddChild(view);

    //	icon
    view->InitIcon(type);

    //	buttons
    int32 which = 1;
    float maxButtonWidth  = 0;
    float maxButtonHeight = 0;
    float buttonsWidth	  = 0;
    float buttonsHeight   = 0;
    BString strButtons(buttons);
    strButtons.RemoveAll("&");
    do
    {
	int32 end = strButtons.FindFirst('\n');
	if (end != B_ERROR)
	    strButtons.SetByteAt(end, '\0');

	BButton *button = _CreateButton(which++, strButtons.String());
	view->AddChild(button);
	fButtonsList.AddItem(button);

	maxButtonWidth	= max_c(maxButtonWidth,  button->Bounds().Width());
	maxButtonHeight = max_c(maxButtonHeight, button->Bounds().Height());
	buttonsWidth   += button->Bounds().Width();
	buttonsHeight  += button->Bounds().Height();

	if (end == B_ERROR)
	    break;

	strButtons.Remove(0, end + 1);
    } while (true);

    int32 buttonsCount = fButtonsList.CountItems();
    buttonsWidth      += kMNVDialogButtonsSpacingX * (buttonsCount - 1);
    buttonsHeight     += kMNVDialogButtonsSpacingY * (buttonsCount - 1);
    float dialogWidth  = buttonsWidth + kMNVDialogIconStripeWidth +
	kMNVDialogSpacingX * 2;
    float dialogHeight = maxButtonHeight + kMNVDialogSpacingY * 3;

    // Check 'v' flag in 'guioptions': vertical button placement.
    bool vertical = (mnv_strchr(p_go, GO_VERTICAL) != NULL) ||
	dialogWidth >= gui.mnvWindow->Bounds().Width();
    if (vertical) {
	dialogWidth  -= buttonsWidth;
	dialogWidth  += maxButtonWidth;
	dialogHeight -= maxButtonHeight;
	dialogHeight += buttonsHeight;
    }

    dialogWidth  = max_c(dialogWidth,  kMNVDialogMinimalWidth);

    //	message view
    BRect rect(0, 0, dialogWidth, 0);
    rect.left  += kMNVDialogIconStripeWidth + 16 + kMNVDialogSpacingX;
    rect.top   += kMNVDialogSpacingY;
    rect.right -= kMNVDialogSpacingX;
    rect.bottom = rect.top;
    fMessageView = new BTextView(rect, "_tv_", rect.OffsetByCopy(B_ORIGIN),
	    B_FOLLOW_LEFT | B_FOLLOW_TOP, B_WILL_DRAW);

    fMessageView->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
    rgb_color textColor = ui_color(B_PANEL_TEXT_COLOR);
    fMessageView->SetFontAndColor(be_plain_font, B_FONT_ALL, &textColor);
    fMessageView->SetText(message);
    fMessageView->MakeEditable(false);
    fMessageView->MakeSelectable(false);
    fMessageView->SetWordWrap(true);
    AddChild(fMessageView);

    float messageHeight = fMessageView->TextHeight(0, fMessageView->CountLines());
    fMessageView->ResizeBy(0, messageHeight);
    fMessageView->SetTextRect(BRect(0, 0, rect.Width(), messageHeight));

    dialogHeight += messageHeight;

    //	input view
    if (fInputValue != NULL) {
	rect.top     =
	    rect.bottom += messageHeight + kMNVDialogSpacingY;
	fInputControl = new BTextControl(rect, "_iv_", NULL, fInputValue, NULL,
		B_FOLLOW_LEFT | B_FOLLOW_TOP, B_WILL_DRAW | B_NAVIGABLE |  B_PULSE_NEEDED);
	fInputControl->TextView()->SetText(fInputValue);
	fInputControl->TextView()->SetWordWrap(false);
	AddChild(fInputControl);

	float width = 0.f, height = 0.f;
	fInputControl->GetPreferredSize(&width, &height);
	fInputControl->MakeFocus(true);

	dialogHeight += height + kMNVDialogSpacingY * 1.5;
    }

    dialogHeight = max_c(dialogHeight, kMNVDialogMinimalHeight);

    ResizeTo(dialogWidth, dialogHeight);
    MoveTo((gui.mnvWindow->Bounds().Width() - dialogWidth) / 2,
	    (gui.mnvWindow->Bounds().Height() - dialogHeight) / 2);

    //	adjust layout of buttons
    float buttonWidth = max_c(maxButtonWidth, rect.Width() * 0.66);
    BPoint origin(dialogWidth, dialogHeight);
    origin.x -= kMNVDialogSpacingX + (vertical ? buttonWidth : buttonsWidth);
    origin.y -= kMNVDialogSpacingY + (vertical ? buttonsHeight	: maxButtonHeight);

    for (int32 i = 0 ; i < buttonsCount; i++) {
	BButton *button = (BButton*)fButtonsList.ItemAt(i);
	button->MoveTo(origin);
	if (vertical) {
	    origin.y += button->Frame().Height() + kMNVDialogButtonsSpacingY;
	    button->ResizeTo(buttonWidth, button->Frame().Height());
	} else
	    origin.x += button->Frame().Width() + kMNVDialogButtonsSpacingX;

	if (dfltbutton == i + 1) {
	    button->MakeDefault(true);
	    button->MakeFocus(fInputControl == NULL);
	}
    }
}

MNVDialog::~MNVDialog()
{
    if (fDialogSem > B_OK)
	delete_sem(fDialogSem);
}

    int
MNVDialog::Go()
{
    fDialogSem = create_sem(0, "MNVDialogSem");
    if (fDialogSem < B_OK) {
	Quit();
	return fDialogValue;
    }

    Show();

    while (acquire_sem(fDialogSem) == B_INTERRUPTED);

    int retValue = fDialogValue;
    if (fInputValue != NULL)
	mnv_strncpy((char_u*)fInputValue, (char_u*)fInputControl->Text(), IOSIZE - 1);

    if (Lock())
	Quit();

    return retValue;
}

void MNVDialog::MessageReceived(BMessage *msg)
{
    int32 which = 0;
    if (msg->what != kMNVDialogButtonMsg ||
	    msg->FindInt32("which", &which) != B_OK)
	return BWindow::MessageReceived(msg);

    fDialogValue = which;
    delete_sem(fDialogSem);
    fDialogSem = -1;
}

BButton* MNVDialog::_CreateButton(int32 which, const char* label)
{
    BMessage *message = new BMessage(kMNVDialogButtonMsg);
    message->AddInt32("which", which);

    BRect rect(0, 0, 0, 0);
    BString name;
    name << "_b" << which << "_";

    BButton* button = new BButton(rect, name.String(), label, message,
	    B_FOLLOW_RIGHT | B_FOLLOW_BOTTOM);

    float width = 0.f, height = 0.f;
    button->GetPreferredSize(&width, &height);
    button->ResizeTo(width, height);

    return button;
}

MNVDialog::View::View(BRect frame)
    :	BView(frame, "MNVDialogView", B_FOLLOW_ALL_SIDES, B_WILL_DRAW),
    fIconBitmap(NULL)
{
    SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
}

MNVDialog::View::~View()
{
    delete fIconBitmap;
}

void MNVDialog::View::Draw(BRect updateRect)
{
    BRect stripeRect = Bounds();
    stripeRect.right = kMNVDialogIconStripeWidth;
    SetHighColor(tint_color(ViewColor(), B_DARKEN_1_TINT));
    FillRect(stripeRect);

    if (fIconBitmap == NULL)
	return;

    SetDrawingMode(B_OP_ALPHA);
    SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
    DrawBitmapAsync(fIconBitmap, BPoint(18, 6));
}

void MNVDialog::View::InitIcon(int32 type)
{
    if (type == MNV_GENERIC)
	return;

    BPath path;
    status_t status = find_directory(B_BEOS_SERVERS_DIRECTORY, &path);
    if (status != B_OK) {
	fprintf(stderr, "Cannot retrieve app info:%s\n", strerror(status));
	return;
    }

    path.Append("app_server");

    BFile file(path.Path(), O_RDONLY);
    if (file.InitCheck() != B_OK) {
	fprintf(stderr, "App file assignment failed:%s\n",
		strerror(file.InitCheck()));
	return;
    }

    BResources resources(&file);
    if (resources.InitCheck() != B_OK) {
	fprintf(stderr, "App server resources assignment failed:%s\n",
		strerror(resources.InitCheck()));
	return;
    }

    const char *name = "";
    switch(type) {
	case MNV_ERROR:	    name = "stop"; break;
	case MNV_WARNING:   name = "warn"; break;
	case MNV_INFO:	    name = "info"; break;
	case MNV_QUESTION:  name = "idea"; break;
	default: return;
    }

    int32 iconSize = 32;
    fIconBitmap = new BBitmap(BRect(0, 0, iconSize - 1, iconSize - 1), 0, B_RGBA32);
    if (fIconBitmap == NULL || fIconBitmap->InitCheck() != B_OK) {
	fprintf(stderr, "Icon bitmap allocation failed:%s\n",
		(fIconBitmap == NULL) ? "null" : strerror(fIconBitmap->InitCheck()));
	return;
    }

    size_t size = 0;
    const uint8* iconData = NULL;
    //	try vector icon first?
    iconData = (const uint8*)resources.LoadResource(B_VECTOR_ICON_TYPE, name, &size);
    if (iconData != NULL && BIconUtils::GetVectorIcon(iconData, size, fIconBitmap) == B_OK)
	return;

    //	try bitmap icon now
    iconData = (const uint8*)resources.LoadResource(B_LARGE_ICON_TYPE, name, &size);
    if (iconData == NULL) {
	fprintf(stderr, "Bitmap icon resource not found\n");
	delete fIconBitmap;
	fIconBitmap = NULL;
	return;
    }

    if (fIconBitmap->ColorSpace() != B_CMAP8)
	BIconUtils::ConvertFromCMAP8(iconData, iconSize, iconSize, iconSize, fIconBitmap);
}

const unsigned int  kMNVDialogOKButtonMsg = 'FDOK';
const unsigned int  kMNVDialogCancelButtonMsg = 'FDCN';
const unsigned int  kMNVDialogSizeInputMsg = 'SICH';
const unsigned int  kMNVDialogFamilySelectMsg = 'MSFM';
const unsigned int  kMNVDialogStyleSelectMsg = 'MSST';
const unsigned int  kMNVDialogSizeSelectMsg = 'MSSZ';

MNVSelectFontDialog::MNVSelectFontDialog(font_family* family, font_style* style, float* size)
: BWindow(kDefaultRect, "Font Selection", B_TITLED_WINDOW_LOOK, B_MODAL_APP_WINDOW_FEEL,
	B_NOT_CLOSABLE | B_NOT_RESIZABLE |
	B_NOT_ZOOMABLE | B_NOT_MINIMIZABLE | B_ASYNCHRONOUS_CONTROLS)
    , fStatus(B_NO_INIT)
    , fDialogSem(-1)
    , fDialogValue(false)
    , fFamily(family)
    , fStyle(style)
    , fSize(size)
    , fFontSize(*size)
    , fPreview(0)
    , fFamiliesList(0)
    , fStylesList(0)
    , fSizesList(0)
    , fSizesInput(0)
{
    strncpy(fFontFamily, *family, B_FONT_FAMILY_LENGTH);
    strncpy(fFontStyle, *style, B_FONT_STYLE_LENGTH);

    //	"client" area view
    BBox *clientBox = new BBox(Bounds(), B_EMPTY_STRING, B_FOLLOW_ALL_SIDES,
		    B_WILL_DRAW | B_FRAME_EVENTS | B_NAVIGABLE_JUMP | B_PULSE_NEEDED,
		    B_PLAIN_BORDER);
    AddChild(clientBox);

    //	client view
    BRect RC = clientBox->Bounds();
    RC.InsetBy(kMNVDialogSpacingX, kMNVDialogSpacingY);
    BRect rc(RC.LeftTop(), RC.LeftTop());

    //	at first create all controls
    fPreview = new BStringView(rc, "preview", "DejaVu Sans Mono");
    clientBox->AddChild(fPreview);

    BBox* boxDivider = new BBox(rc, B_EMPTY_STRING,
	    B_FOLLOW_NONE, B_WILL_DRAW, B_FANCY_BORDER);
    clientBox->AddChild(boxDivider);

    BStringView *labelFamily = new BStringView(rc, "labelFamily", "Family:");
    clientBox->AddChild(labelFamily);
    labelFamily->ResizeToPreferred();

    BStringView *labelStyle = new BStringView(rc, "labelStyle", "Style:");
    clientBox->AddChild(labelStyle);
    labelStyle->ResizeToPreferred();

    BStringView *labelSize = new BStringView(rc, "labelSize", "Size:");
    clientBox->AddChild(labelSize);
    labelSize->ResizeToPreferred();

    fFamiliesList = new BListView(rc, "listFamily",
	    B_SINGLE_SELECTION_LIST, B_FOLLOW_ALL_SIDES);
    BScrollView *scrollFamilies = new BScrollView("scrollFamily",
	    fFamiliesList, B_FOLLOW_LEFT_RIGHT, 0, false, true);
    clientBox->AddChild(scrollFamilies);

    fStylesList= new BListView(rc, "listStyles",
	    B_SINGLE_SELECTION_LIST, B_FOLLOW_ALL_SIDES);
    BScrollView *scrollStyles = new BScrollView("scrollStyle",
	    fStylesList, B_FOLLOW_LEFT_RIGHT, 0, false, true);
    clientBox->AddChild(scrollStyles);

    fSizesInput = new BTextControl(rc, "inputSize", NULL, "???",
	    new BMessage(kMNVDialogSizeInputMsg));
    clientBox->AddChild(fSizesInput);
    fSizesInput->ResizeToPreferred();

    fSizesList = new BListView(rc, "listSizes",
	    B_SINGLE_SELECTION_LIST, B_FOLLOW_ALL_SIDES);
    BScrollView *scrollSizes = new BScrollView("scrollSize",
	    fSizesList, B_FOLLOW_LEFT_RIGHT, 0, false, true);
    clientBox->AddChild(scrollSizes);

    BButton *buttonOK = new BButton(rc, "buttonOK", "OK",
			new BMessage(kMNVDialogOKButtonMsg));
    clientBox->AddChild(buttonOK);
    buttonOK->ResizeToPreferred();

    BButton *buttonCancel = new BButton(rc, "buttonCancel", "Cancel",
			new BMessage(kMNVDialogCancelButtonMsg));
    clientBox->AddChild(buttonCancel);
    buttonCancel->ResizeToPreferred();

    //	layout controls
    float lineHeight = labelFamily->Bounds().Height();
    float previewHeight = lineHeight * 3;
    float offsetYLabels = previewHeight + kMNVDialogSpacingY;
    float offsetYLists = offsetYLabels + lineHeight + kMNVDialogSpacingY / 2;
    float offsetYSizes = offsetYLists + fSizesInput->Bounds().Height() + kMNVDialogSpacingY / 2;
    float listsHeight = lineHeight * 9;
    float offsetYButtons = offsetYLists + listsHeight +  kMNVDialogSpacingY;
    float maxControlsHeight = offsetYButtons + buttonOK->Bounds().Height();
    float familiesWidth = labelFamily->Bounds().Width() * 5;
    float offsetXStyles = familiesWidth + kMNVDialogSpacingX;
    float stylesWidth = labelStyle->Bounds().Width() * 4;
    float offsetXSizes = offsetXStyles + stylesWidth + kMNVDialogSpacingX;
    float sizesWidth = labelSize->Bounds().Width() * 2;
    float maxControlsWidth = offsetXSizes + sizesWidth;

    ResizeTo(maxControlsWidth + kMNVDialogSpacingX * 2,
	maxControlsHeight + kMNVDialogSpacingY * 2);

    BRect rcMNV = gui.mnvWindow->Frame();
    MoveTo(rcMNV.left + (rcMNV.Width() - Frame().Width()) / 2,
	    rcMNV.top + (rcMNV.Height() - Frame().Height()) / 2);

    fPreview->ResizeTo(maxControlsWidth, previewHeight);
    fPreview->SetAlignment(B_ALIGN_CENTER);

    boxDivider->MoveBy(0.f, previewHeight + kMNVDialogSpacingY / 2);
    boxDivider->ResizeTo(maxControlsWidth, 1.f);

    labelFamily->MoveBy(0.f, offsetYLabels);
    labelStyle->MoveBy(offsetXStyles, offsetYLabels);
    labelSize->MoveBy(offsetXSizes, offsetYLabels);

    //	text control alignment issues
    float insetX = fSizesInput->TextView()->Bounds().Width() - fSizesInput->Bounds().Width();
    float insetY = fSizesInput->TextView()->Bounds().Width() - fSizesInput->Bounds().Width();

    scrollFamilies->MoveBy(0.f, offsetYLists);
    scrollStyles->MoveBy(offsetXStyles, offsetYLists);
    fSizesInput->MoveBy(offsetXSizes + insetX / 2, offsetYLists + insetY / 2);
    scrollSizes->MoveBy(offsetXSizes, offsetYSizes);

    fSizesInput->SetAlignment(B_ALIGN_CENTER, B_ALIGN_CENTER);

    scrollFamilies->ResizeTo(familiesWidth, listsHeight);
    scrollStyles->ResizeTo(stylesWidth, listsHeight);
    fSizesInput->ResizeTo(sizesWidth, fSizesInput->Bounds().Height());
    scrollSizes->ResizeTo(sizesWidth,
	    listsHeight - (offsetYSizes - offsetYLists));

    buttonOK->MoveBy(maxControlsWidth - buttonOK->Bounds().Width(), offsetYButtons);
    buttonCancel->MoveBy(maxControlsWidth - buttonOK->Bounds().Width()
	    - buttonCancel->Bounds().Width() - kMNVDialogSpacingX, offsetYButtons);

    //	fill lists
    int selIndex = -1;
    int count = count_font_families();
    for (int i = 0; i < count; i++) {
	font_family family;
	if (get_font_family(i, &family ) == B_OK) {
	    fFamiliesList->AddItem(new BStringItem((const char*)family));
	    if (strncmp(family, fFontFamily, B_FONT_FAMILY_LENGTH) == 0)
		selIndex = i;
	}
    }

    if (selIndex >= 0) {
	fFamiliesList->Select(selIndex);
	fFamiliesList->ScrollToSelection();
    }

    _UpdateFontStyles();

    selIndex = -1;
    for (int size = 8, index = 0; size <= 18; size++, index++) {
	BString str;
	str << size;
	fSizesList->AddItem(new BStringItem(str));
	if (size == fFontSize)
	    selIndex = index;

    }

    if (selIndex >= 0) {
	fSizesList->Select(selIndex);
	fSizesList->ScrollToSelection();
    }

    fFamiliesList->SetSelectionMessage(new BMessage(kMNVDialogFamilySelectMsg));
    fStylesList->SetSelectionMessage(new BMessage(kMNVDialogStyleSelectMsg));
    fSizesList->SetSelectionMessage(new BMessage(kMNVDialogSizeSelectMsg));
    fSizesInput->SetModificationMessage(new BMessage(kMNVDialogSizeInputMsg));

    _UpdateSizeInputPreview();
    _UpdateFontPreview();

    fStatus = B_OK;
}

MNVSelectFontDialog::~MNVSelectFontDialog()
{
    _CleanList(fFamiliesList);
    _CleanList(fStylesList);
    _CleanList(fSizesList);

    if (fDialogSem > B_OK)
	delete_sem(fDialogSem);
}

    void
MNVSelectFontDialog::_CleanList(BListView* list)
{
    while (0 < list->CountItems())
	delete (dynamic_cast<BStringItem*>(list->RemoveItem((int32)0)));
}

    bool
MNVSelectFontDialog::Go()
{
    if (fStatus != B_OK) {
	Quit();
	return NOFONT;
    }

    fDialogSem = create_sem(0, "MNVFontSelectDialogSem");
    if (fDialogSem < B_OK) {
	Quit();
	return fDialogValue;
    }

    Show();

    while (acquire_sem(fDialogSem) == B_INTERRUPTED);

    bool retValue = fDialogValue;

    if (Lock())
	Quit();

    return retValue;
}


void MNVSelectFontDialog::_UpdateFontStyles()
{
    _CleanList(fStylesList);

    int32 selIndex = -1;
    int32 count = count_font_styles(fFontFamily);
    for (int32 i = 0; i < count; i++) {
	font_style style;
	uint32 flags = 0;
	if (get_font_style(fFontFamily, i, &style, &flags) == B_OK) {
	    fStylesList->AddItem(new BStringItem((const char*)style));
	    if (strncmp(style, fFontStyle, B_FONT_STYLE_LENGTH) == 0)
		selIndex = i;
	}
    }

    if (selIndex >= 0) {
	fStylesList->Select(selIndex);
	fStylesList->ScrollToSelection();
    } else
	fStylesList->Select(0);
}


void MNVSelectFontDialog::_UpdateSizeInputPreview()
{
    char buf[10] = {0};
    mnv_snprintf(buf, sizeof(buf), (char*)"%.0f", fFontSize);
    fSizesInput->SetText(buf);
}


void MNVSelectFontDialog::_UpdateFontPreview()
{
    BFont font;
    fPreview->GetFont(&font);
    font.SetSize(fFontSize);
    font.SetFamilyAndStyle(fFontFamily, fFontStyle);
    fPreview->SetFont(&font, B_FONT_FAMILY_AND_STYLE | B_FONT_SIZE);

    BString str;
    str << fFontFamily << " " << fFontStyle << ", " << (int)fFontSize << " pt.";
    fPreview->SetText(str);
}


    bool
MNVSelectFontDialog::_UpdateFromListItem(BListView* list, char* text, int textSize)
{
    int32 index = list->CurrentSelection();
    if (index < 0)
	return false;
    BStringItem* item = (BStringItem*)list->ItemAt(index);
    if (item == NULL)
	return false;
    strncpy(text, item->Text(), textSize);
    return true;
}


void MNVSelectFontDialog::MessageReceived(BMessage *msg)
{
    switch (msg->what) {
	case kMNVDialogOKButtonMsg:
	    strncpy(*fFamily, fFontFamily, B_FONT_FAMILY_LENGTH);
	    strncpy(*fStyle, fFontStyle, B_FONT_STYLE_LENGTH);
	    *fSize = fFontSize;
	    fDialogValue = true;
	case kMNVDialogCancelButtonMsg:
	    delete_sem(fDialogSem);
	    fDialogSem = -1;
	    return;
	case B_KEY_UP:
	    {
		int32 key = 0;
		if (msg->FindInt32("raw_char", &key) == B_OK
			&& key == B_ESCAPE) {
		    delete_sem(fDialogSem);
		    fDialogSem = -1;
		}
	    }
	    break;

	case kMNVDialogFamilySelectMsg:
	    if (_UpdateFromListItem(fFamiliesList,
		    fFontFamily, B_FONT_FAMILY_LENGTH)) {
		_UpdateFontStyles();
		_UpdateFontPreview();
	    }
	    break;
	case kMNVDialogStyleSelectMsg:
	    if (_UpdateFromListItem(fStylesList,
		    fFontStyle, B_FONT_STYLE_LENGTH))
		_UpdateFontPreview();
	    break;
	case kMNVDialogSizeSelectMsg:
	    {
		char buf[10] = {0};
		if (_UpdateFromListItem(fSizesList, buf, sizeof(buf))) {
		    float size = atof(buf);
		    if (size > 0.f) {
			fFontSize = size;
			_UpdateSizeInputPreview();
			_UpdateFontPreview();
		    }
		}
	    }
	    break;
	case kMNVDialogSizeInputMsg:
	    {
		float size = atof(fSizesInput->Text());
		if (size > 0.f) {
		    fFontSize = size;
		    _UpdateFontPreview();
		}
	    }
	    break;
	default:
	    break;
    }
    return BWindow::MessageReceived(msg);
}

#endif // FEAT_GUI_DIALOG

#ifdef FEAT_TOOLBAR

//  some forward declaration required by toolbar functions...
static BMessage * MenuMessage(mnvmenu_T *menu);

MNVToolbar::MNVToolbar(BRect frame, const char *name) :
    BBox(frame, name, B_FOLLOW_LEFT | B_FOLLOW_TOP, B_WILL_DRAW | B_FRAME_EVENTS, B_PLAIN_BORDER)
{
}

MNVToolbar::~MNVToolbar()
{
    int32 count = fButtonsList.CountItems();
    for (int32 i = 0; i < count; i++)
	delete (BPictureButton*)fButtonsList.ItemAt(i);
    fButtonsList.MakeEmpty();

    delete normalButtonsBitmap;
    delete grayedButtonsBitmap;
    normalButtonsBitmap    = NULL;
    grayedButtonsBitmap  = NULL;
}

    void
MNVToolbar::AttachedToWindow()
{
    BBox::AttachedToWindow();

    SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
}

    float
MNVToolbar::ToolbarHeight() const
{
    float size = NULL == normalButtonsBitmap ? 18. : normalButtonsBitmap->Bounds().Height();
    return size + ToolbarMargin * 2 + ButtonMargin * 2 + 1;
}

    bool
MNVToolbar::ModifyBitmapToGrayed(BBitmap *bitmap)
{
    float height = bitmap->Bounds().Height();
    float width  = bitmap->Bounds().Width();

    rgb_color *bits = (rgb_color*)bitmap->Bits();
    int32 pixels = bitmap->BitsLength() / 4;
    for (int32 i = 0; i < pixels; i++) {
	bits[i].red = bits[i].green =
	bits[i].blue = ((uint32)bits[i].red + bits[i].green + bits[i].blue) / 3;
	bits[i].alpha /= 4;
    }

    return true;
}

    bool
MNVToolbar::PrepareButtonBitmaps()
{
    //	first try to load potentially customized $VIRUNTIME/bitmaps/builtin-tools.png
    normalButtonsBitmap = LoadMNVBitmap("builtin-tools.png");
    if (normalButtonsBitmap == NULL)
	//  customized not found? dig application resources for "builtin-tools" one
	normalButtonsBitmap = BTranslationUtils::GetBitmap(B_PNG_FORMAT, "builtin-tools");

    if (normalButtonsBitmap == NULL)
	return false;

    BMessage archive;
    normalButtonsBitmap->Archive(&archive);

    grayedButtonsBitmap = new BBitmap(&archive);
    if (grayedButtonsBitmap == NULL)
	return false;

    //	modify grayed bitmap
    ModifyBitmapToGrayed(grayedButtonsBitmap);

    return true;
}

BBitmap *MNVToolbar::LoadMNVBitmap(const char* fileName)
{
    BBitmap *bitmap = NULL;

    int mustfree = 0;
    char_u* runtimePath = mnv_getenv((char_u*)"MNVRUNTIME", &mustfree);
    if (runtimePath != NULL && fileName != NULL) {
	BString strPath((char*)runtimePath);
	strPath << "/bitmaps/" << fileName;
	bitmap = BTranslationUtils::GetBitmap(strPath.String());
    }

    if (mustfree)
	mnv_free(runtimePath);

    return bitmap;
}

    bool
MNVToolbar::GetPictureFromBitmap(BPicture *pictureTo, int32 index, BBitmap *bitmapFrom, bool pressed)
{
    float size = bitmapFrom->Bounds().Height() + 1.;

    BView view(BRect(0, 0, size, size), "", 0, 0);

    AddChild(&view);
    view.BeginPicture(pictureTo);

    view.SetHighColor(ui_color(B_PANEL_BACKGROUND_COLOR));
    view.FillRect(view.Bounds());
    view.SetDrawingMode(B_OP_OVER);

    BRect source(0, 0, size - 1, size - 1);
    BRect destination(source);

    source.OffsetBy(size * index, 0);
    destination.OffsetBy(ButtonMargin, ButtonMargin);

    view.DrawBitmap(bitmapFrom, source, destination);

    if (pressed)	{
	rgb_color shineColor  = ui_color(B_SHINE_COLOR);
	rgb_color shadowColor = ui_color(B_SHADOW_COLOR);
	size += ButtonMargin * 2 - 1;
	view.BeginLineArray(4);
	view.AddLine(BPoint(0, 0),	 BPoint(size, 0),    shadowColor);
	view.AddLine(BPoint(size, 0),	 BPoint(size, size), shineColor);
	view.AddLine(BPoint(size, size), BPoint(0, size),    shineColor);
	view.AddLine(BPoint(0, size),	 BPoint(0, 0),	     shadowColor);
	view.EndLineArray();
    }

    view.EndPicture();
    RemoveChild(&view);

    return true;
}

    bool
MNVToolbar::AddButton(int32 index, mnvmenu_T *menu)
{
    BPictureButton *button = NULL;
    if (!menu_is_separator(menu->name)) {
	float size = normalButtonsBitmap ?
	    normalButtonsBitmap->Bounds().Height() + 1. + ButtonMargin * 2 : 18.;
	BRect frame(0, 0, size, size);
	BPicture pictureOn;
	BPicture pictureOff;
	BPicture pictureGray;

	if (menu->iconfile == NULL && menu->iconidx >= 0 && normalButtonsBitmap) {
	    GetPictureFromBitmap(&pictureOn,  menu->iconidx, normalButtonsBitmap, true);
	    GetPictureFromBitmap(&pictureOff, menu->iconidx, normalButtonsBitmap, false);
	    GetPictureFromBitmap(&pictureGray, menu->iconidx, grayedButtonsBitmap, false);
	} else {

	    char_u buffer[MAXPATHL] = {0};
	    BBitmap *bitmap = NULL;

	    if (menu->iconfile) {
		gui_find_iconfile(menu->iconfile, buffer, (char*)"png");
		bitmap = BTranslationUtils::GetBitmap((char*)buffer);
	    }

	    if (bitmap == NULL && gui_find_bitmap(menu->name, buffer, (char*)"png") == OK)
		bitmap = BTranslationUtils::GetBitmap((char*)buffer);

	    if (bitmap == NULL)
		bitmap = new BBitmap(BRect(0, 0, size, size), B_RGB32);

	    GetPictureFromBitmap(&pictureOn,   0, bitmap, true);
	    GetPictureFromBitmap(&pictureOff,  0, bitmap, false);
	    ModifyBitmapToGrayed(bitmap);
	    GetPictureFromBitmap(&pictureGray, 0, bitmap, false);

	    delete bitmap;
	}

	button = new BPictureButton(frame, (char*)menu->name,
		    &pictureOff, &pictureOn, MenuMessage(menu));

	button->SetDisabledOn(&pictureGray);
	button->SetDisabledOff(&pictureGray);

	button->SetTarget(gui.mnvTextArea);

	AddChild(button);

	menu->button = button;
    }

    bool result = fButtonsList.AddItem(button, index);
    InvalidateLayout();
    return result;
}

    bool
MNVToolbar::RemoveButton(mnvmenu_T *menu)
{
    if (menu->button) {
	if (fButtonsList.RemoveItem(menu->button)) {
	    delete menu->button;
	    menu->button = NULL;
	}
    }
    return true;
}

    bool
MNVToolbar::GrayButton(mnvmenu_T *menu, int grey)
{
    if (menu->button) {
	int32 index = fButtonsList.IndexOf(menu->button);
	if (index >= 0)
	    menu->button->SetEnabled(grey ? false : true);
    }
    return true;
}

    void
MNVToolbar::InvalidateLayout()
{
    int32 offset = ToolbarMargin;
    int32 count = fButtonsList.CountItems();
    for (int32 i = 0; i < count; i++) {
	BPictureButton *button = (BPictureButton *)fButtonsList.ItemAt(i);
	if (button) {
	    button->MoveTo(offset, ToolbarMargin);
	    offset += button->Bounds().Width() + ToolbarMargin;
	} else
	    offset += ToolbarMargin * 3;
    }
}

#endif /*FEAT_TOOLBAR*/

#if defined(FEAT_GUI_TABLINE)

    float
MNVTabLine::TablineHeight() const
{
//  float size = NULL == normalButtonsBitmap ? 18. : normalButtonsBitmap->Bounds().Height();
//  return size + ToolbarMargin * 2 + ButtonMargin * 2 + 1;
    return TabHeight(); //  + ToolbarMargin;
}

void
MNVTabLine::MouseDown(BPoint point)
{
    if (!gui_mch_showing_tabline())
	return;

    BMessage *m = Window()->CurrentMessage();
    assert(m);

    int32 buttons = 0;
    m->FindInt32("buttons", &buttons);

    int32 clicks = 0;
    m->FindInt32("clicks", &clicks);

    int index = 0; //  0 means here - no tab found
    for (int i = 0; i < CountTabs(); i++) {
	if (TabFrame(i).Contains(point)) {
	    index = i + 1; //  indexes are 1-based
	    break;
	}
    }

    int event = -1;

    if ((buttons & B_PRIMARY_MOUSE_BUTTON) && clicks > 1)
	//  left button double click on - create new tab
	event = TABLINE_MENU_NEW;

    else if (buttons & B_TERTIARY_MOUSE_BUTTON)
	//  middle button click - close the pointed tab
	//  or create new one in case empty space
	event = index > 0 ? TABLINE_MENU_CLOSE : TABLINE_MENU_NEW;

    else if (buttons & B_SECONDARY_MOUSE_BUTTON) {
	//  right button click - show context menu
	BPopUpMenu* popUpMenu = new BPopUpMenu("tabLineContextMenu", false, false);
	popUpMenu->AddItem(new BMenuItem(_("Close tabi R"), new BMessage(TABLINE_MENU_CLOSE)));
	popUpMenu->AddItem(new BMenuItem(_("New tab    T"), new BMessage(TABLINE_MENU_NEW)));
	popUpMenu->AddItem(new BMenuItem(_("Open tab..."), new BMessage(TABLINE_MENU_OPEN)));

	ConvertToScreen(&point);
	BMenuItem* item = popUpMenu->Go(point);
	if (item != NULL) {
	    event = item->Command();
	}

	delete popUpMenu;

    } else {
	//  default processing
	BTabView::MouseDown(point);
	return;
    }

    if (event < 0)
	return;

    MNVTablineMenuMsg tmm;
    tmm.index = index;
    tmm.event = event;
    write_port(gui.vdcmp, MNVMsg::TablineMenu, &tmm, sizeof(tmm));
}

void
MNVTabLine::MNVTab::Select(BView* owner)
{
    BTab::Select(owner);

    MNVTabLine *tabLine = gui.mnvForm->TabLine();
    if (tabLine != NULL) {

	int32 i = 0;
	for (; i < tabLine->CountTabs(); i++)
	    if (this == tabLine->TabAt(i))
		break;

//	printf("%d:%d:%s\n", i, tabLine->CountTabs(), tabLine->TabAt(i)->Label());
	if (i < tabLine->CountTabs()) {
	    MNVTablineMsg tm;
	    tm.index = i + 1;
	    write_port(gui.vdcmp, MNVMsg::Tabline, &tm, sizeof(tm));
	}
    }
}

#endif //  defined(FEAT_GUI_TABLINE)

// ---------------- ----------------

//  some global variables
static char appsig[] = "application/x-vnd.Haiku-MNV-8";
key_map *keyMap;
char *keyMapChars;
int main_exitcode = 127;

    status_t
gui_haiku_process_event(bigtime_t timeout)
{
    struct MNVMsg vm;
    int32 what;
    ssize_t size;

    size = read_port_etc(gui.vdcmp, &what, &vm, sizeof(vm),
	    B_TIMEOUT, timeout);

    if (size >= 0) {
	switch (what) {
	    case MNVMsg::Key:
		{
		    char_u *string = vm.u.Key.chars;
		    int len = vm.u.Key.length;
		    if (len == 1 && string[0] == Ctrl_chr('C')) {
			trash_input_buf();
			got_int = TRUE;
		    }

		    if (vm.u.Key.csi_escape)
#ifndef FEAT_MBYTE_IME
		    {
			int	i;
			char_u	buf[2];

			for (i = 0; i < len; ++i)
			{
			    add_to_input_buf(string + i, 1);
			    if (string[i] == CSI)
			    {
				// Turn CSI into K_CSI.
				buf[0] = KS_EXTRA;
				buf[1] = (int)KE_CSI;
				add_to_input_buf(buf, 2);
			    }
			}
		    }
#else
			add_to_input_buf_csi(string, len);
#endif
		    else
			add_to_input_buf(string, len);
		}
		break;
	    case MNVMsg::Resize:
		gui_resize_shell(vm.u.NewSize.width, vm.u.NewSize.height);
		break;
	    case MNVMsg::ScrollBar:
		{
		    /*
		     * If loads of scroll messages queue up, use only the last
		     * one. Always report when the scrollbar stops dragging.
		     * This is not perfect yet anyway: these events are queued
		     * yet again, this time in the keyboard input buffer.
		     */
		    int32 oldCount =
			atomic_add(&vm.u.Scroll.sb->scrollEventCount, -1);
		    if (oldCount <= 1 || !vm.u.Scroll.stillDragging)
			gui_drag_scrollbar(vm.u.Scroll.sb->getGsb(),
				vm.u.Scroll.value, vm.u.Scroll.stillDragging);
		}
		break;
#if defined(FEAT_MENU)
	    case MNVMsg::Menu:
		gui_menu_cb(vm.u.Menu.guiMenu);
		break;
#endif
	    case MNVMsg::Mouse:
		{
		    int32 oldCount;
		    if (vm.u.Mouse.button == MOUSE_DRAG)
			oldCount =
			    atomic_add(&gui.mnvTextArea->mouseDragEventCount, -1);
		    else
			oldCount = 0;
		    if (oldCount <= 1)
			gui_send_mouse_event(vm.u.Mouse.button, vm.u.Mouse.x,
				vm.u.Mouse.y, vm.u.Mouse.repeated_click,
				vm.u.Mouse.modifiers);
		}
		break;
	    case MNVMsg::MouseMoved:
		{
		    gui_mouse_moved(vm.u.MouseMoved.x, vm.u.MouseMoved.y);
		}
		break;
	    case MNVMsg::Focus:
		gui.in_focus = vm.u.Focus.active;
		// XXX Signal that scrollbar dragging has stopped?
		// This is needed because we don't get a MouseUp if
		// that happens while outside the window... :-(
		if (gui.dragged_sb) {
		    gui.dragged_sb = SBAR_NONE;
		}
		//  gui_update_cursor(TRUE, FALSE);
		break;
	    case MNVMsg::Refs:
		::RefsReceived(vm.u.Refs.message, vm.u.Refs.changedir);
		break;
	    case MNVMsg::Tabline:
		send_tabline_event(vm.u.Tabline.index);
		break;
	    case MNVMsg::TablineMenu:
		send_tabline_menu_event(vm.u.TablineMenu.index, vm.u.TablineMenu.event);
		break;
	    default:
		//  unrecognised message, ignore it
		break;
	}
    }

    /*
     * If size < B_OK, it is an error code.
     */
    return size;
}

/*
 * Here are some functions to protect access to ScreenLines[] and
 * LineOffset[]. These are used from the window thread to respond
 * to a Draw() callback. When that occurs, the window is already
 * locked by the system.
 *
 * Other code that needs to lock is any code that changes these
 * variables. Other read-only access, or access merely to the
 * contents of the screen buffer, need not be locked.
 *
 * If there is no window, don't call Lock() but do succeed.
 */

    int
mnv_lock_screen()
{
    return !gui.mnvWindow || gui.mnvWindow->Lock();
}

    void
mnv_unlock_screen()
{
    if (gui.mnvWindow)
	gui.mnvWindow->Unlock();
}

#define RUN_BAPPLICATION_IN_NEW_THREAD	0

#if RUN_BAPPLICATION_IN_NEW_THREAD

    int32
run_mnvapp(void *args)
{
    MNVApp app(appsig);

    gui.mnvApp = &app;
    app.Run();		    // Run until Quit() called

    return 0;
}

#else

    int32
call_main(void *args)
{
    struct MainArgs *ma = (MainArgs *)args;

    return main(ma->argc, ma->argv);
}
#endif

/*
 * Parse the GUI related command-line arguments.  Any arguments used are
 * deleted from argv, and *argc is decremented accordingly.  This is called
 * when mnv is started, whether or not the GUI has been started.
 */
    void
gui_mch_prepare(
	int	*argc,
	char	**argv)
{
    /*
     * We don't have any command line arguments for the BeOS GUI yet,
     * but this is an excellent place to create our Application object.
     */
    if (!gui.mnvApp) {
	thread_info tinfo;
	get_thread_info(find_thread(NULL), &tinfo);

	// May need the port very early on to process RefsReceived()
	gui.vdcmp = create_port(B_MAX_PORT_COUNT, "mnv VDCMP");

#if RUN_BAPPLICATION_IN_NEW_THREAD
	thread_id tid = spawn_thread(run_mnvapp, "mnv MNVApp",
		tinfo.priority, NULL);
	if (tid >= B_OK) {
	    resume_thread(tid);
	} else {
	    getout(1);
	}
#else
	MainArgs ma = { *argc, argv };
	thread_id tid = spawn_thread(call_main, "mnv main()",
		tinfo.priority, &ma);
	if (tid >= B_OK) {
	    MNVApp app(appsig);

	    gui.mnvApp = &app;
	    resume_thread(tid);
	    /*
	     * This is rather horrible.
	     * call_main will call main() again...
	     * There will be no infinite recursion since
	     * gui.mnvApp is set now.
	     */
	    app.Run();		    // Run until Quit() called
	    // fprintf(stderr, "app.Run() returned...\n");
	    status_t dummy_exitcode;
	    (void)wait_for_thread(tid, &dummy_exitcode);

	    /*
	     * This path should be the normal one taken to exit MNV.
	     * The main() thread calls mch_exit() which calls
	     * gui_mch_exit() which terminates its thread.
	     */
	    exit(main_exitcode);
	}
#endif
    }
    // Don't fork() when starting the GUI. Spawned threads are not
    // duplicated with a fork(). The result is a mess.
    gui.dofork = FALSE;
    /*
     * XXX Try to determine whether we were started from
     * the Tracker or the terminal.
     * It would be nice to have this work, because the Tracker
     * follows symlinks, so even if you double-click on gmnv,
     * when it is a link to mnv it will still pass a command name
     * of mnv...
     * We try here to see if stdin comes from /dev/null. If so,
     * (or if there is an error, which should never happen) start the GUI.
     * This does the wrong thing for mnv - </dev/null, and we're
     * too early to see the command line parsing. Tough.
     * On the other hand, it starts the gui for mnv file & which is nice.
     */
    if (!isatty(0)) {
	struct stat stat_stdin, stat_dev_null;

	if (fstat(0, &stat_stdin) == -1 ||
		stat("/dev/null", &stat_dev_null) == -1 ||
		(stat_stdin.st_dev == stat_dev_null.st_dev &&
		 stat_stdin.st_ino == stat_dev_null.st_ino))
	    gui.starting = TRUE;
    }
}

/*
 * Check if the GUI can be started.  Called before gmnvrc is sourced.
 * Return OK or FAIL.
 */
    int
gui_mch_init_check(void)
{
    return OK;	    // TODO: GUI can always be started?
}

/*
 * Initialise the GUI.	Create all the windows, set up all the call-backs
 * etc.
 */
    int
gui_mch_init()
{
    display_errors();
    gui.def_norm_pixel = RGB(0x00, 0x00, 0x00);	//  black
    gui.def_back_pixel = RGB(0xFF, 0xFF, 0xFF);	//  white
    gui.norm_pixel = gui.def_norm_pixel;
    gui.back_pixel = gui.def_back_pixel;

    gui.scrollbar_width = (int) B_V_SCROLL_BAR_WIDTH;
    gui.scrollbar_height = (int) B_H_SCROLL_BAR_HEIGHT;
#ifdef FEAT_MENU
    gui.menu_height = 19;   //	initial guess -
    //	correct for my default settings
#endif
    gui.border_offset = 3;  //	coordinates are inside window borders

    if (gui.vdcmp < B_OK)
	return FAIL;
    get_key_map(&keyMap, &keyMapChars);

    gui.mnvWindow = new MNVWindow();	// hidden and locked
    if (!gui.mnvWindow)
	return FAIL;

    gui.mnvWindow->Run();	// Run() unlocks but does not show

    // Get the colors from the "Normal" group (set in syntax.c or in a mnvrc
    // file)
    set_normal_colors();

    /*
     * Check that none of the colors are the same as the background color
     */
    gui_check_colors();

    // Get the colors for the highlight groups (gui_check_colors() might have
    // changed them)
    highlight_gui_started();	    // re-init colors and fonts

    gui_mch_new_colors();	// window must exist for this

    return OK;
}

/*
 * Called when the foreground or background color has been changed.
 */
    void
gui_mch_new_colors()
{
    rgb_color rgb = GUI_TO_RGB(gui.back_pixel);

    if (gui.mnvWindow->Lock()) {
	gui.mnvForm->SetViewColor(rgb);
	//  Does this not have too much effect for those small rectangles?
	gui.mnvForm->Invalidate();
	gui.mnvWindow->Unlock();
    }
}

/*
 * Open the GUI window which was created by a call to gui_mch_init().
 */
    int
gui_mch_open()
{
    if (gui_win_x != -1 && gui_win_y != -1)
	gui_mch_set_winpos(gui_win_x, gui_win_y);

    // Actually open the window
    if (gui.mnvWindow->Lock()) {
	gui.mnvWindow->Show();
	gui.mnvWindow->Unlock();
	return OK;
    }

    return FAIL;
}

    void
gui_mch_exit(int mnv_exitcode)
{
    if (gui.mnvWindow) {
	thread_id tid = gui.mnvWindow->Thread();
	gui.mnvWindow->Lock();
	gui.mnvWindow->Quit();
	// Wait until it is truly gone
	int32 exitcode;
	wait_for_thread(tid, &exitcode);
    }
    delete_port(gui.vdcmp);
#if !RUN_BAPPLICATION_IN_NEW_THREAD
    /*
     * We are in the main() thread - quit the App thread and
     * quit ourselves (passing on the exitcode). Use a global since the
     * value from exit_thread() is only used if wait_for_thread() is
     * called in time (race condition).
     */
#endif
    if (gui.mnvApp) {
	MNVTextAreaView::guiBlankMouse(false);

	main_exitcode = mnv_exitcode;
#if RUN_BAPPLICATION_IN_NEW_THREAD
	thread_id tid = gui.mnvApp->Thread();
	int32 exitcode;
	gui.mnvApp->Lock();
	gui.mnvApp->Quit();
	gui.mnvApp->Unlock();
	wait_for_thread(tid, &exitcode);
#else
	gui.mnvApp->Lock();
	gui.mnvApp->Quit();
	gui.mnvApp->Unlock();
	// suicide
	exit_thread(mnv_exitcode);
#endif
    }
    // If we are somehow still here, let mch_exit() handle things.
}

/*
 * Get the position of the top left corner of the window.
 */
    int
gui_mch_get_winpos(int *x, int *y)
{
    if (gui.mnvWindow->Lock()) {
	BRect r;
	r = gui.mnvWindow->Frame();
	gui.mnvWindow->Unlock();
	*x = (int)r.left;
	*y = (int)r.top;
	return OK;
    }
    else
	return FAIL;
}

/*
 * Set the position of the top left corner of the window to the given
 * coordinates.
 */
    void
gui_mch_set_winpos(int x, int y)
{
    if (gui.mnvWindow->Lock()) {
	gui.mnvWindow->MoveTo(x, y);
	gui.mnvWindow->Unlock();
    }
}

/*
 * Set the size of the window to the given width and height in pixels.
 */
void
gui_mch_set_shellsize(
	int	width,
	int	height,
	int	min_width,
	int	min_height,
	int	base_width,
	int	base_height,
	int	direction) // TODO: utilize?
{
    /*
     * We are basically given the size of the MNVForm, if I understand
     * correctly. Since it fills the window completely, this will also
     * be the size of the window.
     */
    if (gui.mnvWindow->Lock()) {
	gui.mnvWindow->ResizeTo(width - PEN_WIDTH, height - PEN_WIDTH);

	// set size limits
	float minWidth, maxWidth, minHeight, maxHeight;

	gui.mnvWindow->GetSizeLimits(&minWidth, &maxWidth,
		&minHeight, &maxHeight);
	gui.mnvWindow->SetSizeLimits(min_width, maxWidth,
		min_height, maxHeight);

	/*
	 * Set the resizing alignment depending on font size.
	 */
	gui.mnvWindow->SetWindowAlignment(
		B_PIXEL_ALIGNMENT,	//  window_alignment mode,
		1,		//  int32 h,
		0,		//  int32 hOffset = 0,
		gui.char_width,	    //	int32 width = 0,
		base_width,	    //	int32 widthOffset = 0,
		1,		//  int32 v = 0,
		0,		//  int32 vOffset = 0,
		gui.char_height,	//  int32 height = 0,
		base_height	    //	int32 heightOffset = 0
		);

	gui.mnvWindow->Unlock();
    }
}

void
gui_mch_get_screen_dimensions(
	int	*screen_w,
	int	*screen_h)
{
    BRect frame;

    {
	BScreen screen(gui.mnvWindow);

	if (screen.IsValid()) {
	    frame = screen.Frame();
	} else {
	    frame.right = 640;
	    frame.bottom = 480;
	}
    }

    // XXX approximations...
    *screen_w = (int) frame.right - 2 * gui.scrollbar_width - 20;
    *screen_h = (int) frame.bottom - gui.scrollbar_height
#ifdef FEAT_MENU
	- gui.menu_height
#endif
	- 30;
}

void
gui_mch_set_text_area_pos(
	int	x,
	int	y,
	int	w,
	int	h)
{
    if (!gui.mnvTextArea)
	return;

    if (gui.mnvWindow->Lock()) {
	gui.mnvTextArea->MoveTo(x, y);
	gui.mnvTextArea->ResizeTo(w - PEN_WIDTH, h - PEN_WIDTH);

#ifdef FEAT_GUI_TABLINE
	if (gui.mnvForm->TabLine() != NULL) {
	    gui.mnvForm->TabLine()->ResizeTo(w, gui.mnvForm->TablineHeight());
	}
#endif // FEAT_GUI_TABLINE

	gui.mnvWindow->Unlock();
    }
}


/*
 * Scrollbar stuff:
 */

void
gui_mch_enable_scrollbar(
	scrollbar_T *sb,
	int	flag)
{
    MNVScrollBar *vsb = sb->id;
    if (gui.mnvWindow->Lock()) {
	/*
	 * This function is supposed to be idempotent, but Show()/Hide()
	 * is not. Therefore we test if they are needed.
	 */
	if (flag) {
	    if (vsb->IsHidden()) {
		vsb->Show();
	    }
	} else {
	    if (!vsb->IsHidden()) {
		vsb->Hide();
	    }
	}
	gui.mnvWindow->Unlock();
    }
}

void
gui_mch_set_scrollbar_thumb(
	scrollbar_T *sb,
	int	val,
	int	size,
	int	max)
{
    if (gui.mnvWindow->Lock()) {
	MNVScrollBar *s = sb->id;
	if (max == 0) {
	    s->SetValue(0);
	    s->SetRange(0.0, 0.0);
	} else {
	    s->SetProportion((float)size / (max + 1.0));
	    s->SetSteps(1.0, size > 5 ? size - 2 : size);
#ifndef SCROLL_PAST_END	    //	really only defined in gui.c...
	    max = max + 1 - size;
#endif
	    if (max < s->Value()) {
		/*
		 * If the new maximum is lower than the current value,
		 * setting it would cause the value to be clipped and
		 * therefore a ValueChanged() call.
		 * We avoid this by setting the value first, because
		 * it presumably is <= max.
		 */
		s->SetValue(val);
		s->SetRange(0.0, max);
	    } else {
		/*
		 * In the other case, set the range first, since the
		 * new value might be higher than the current max.
		 */
		s->SetRange(0.0, max);
		s->SetValue(val);
	    }
	}
	gui.mnvWindow->Unlock();
    }
}

void
gui_mch_set_scrollbar_pos(
	scrollbar_T *sb,
	int	x,
	int	y,
	int	w,
	int	h)
{
    if (gui.mnvWindow->Lock()) {
	BRect winb = gui.mnvWindow->Bounds();
	float vsbx = x, vsby = y;
	MNVScrollBar *vsb = sb->id;
	vsb->ResizeTo(w - PEN_WIDTH, h - PEN_WIDTH);
	if (winb.right-(x+w)<w) vsbx = winb.right - (w - PEN_WIDTH);
	vsb->MoveTo(vsbx, vsby);
	gui.mnvWindow->Unlock();
    }
}

    int
gui_mch_get_scrollbar_xpadding(void)
{
    // TODO: Calculate the padding for adjust scrollbar position when the
    // Window is maximized.
    return 0;
}

    int
gui_mch_get_scrollbar_ypadding(void)
{
    // TODO: Calculate the padding for adjust scrollbar position when the
    // Window is maximized.
    return 0;
}

void
gui_mch_create_scrollbar(
	scrollbar_T *sb,
	int	orient)	    // SBAR_VERT or SBAR_HORIZ
{
    orientation posture =
	(orient == SBAR_HORIZ) ? B_HORIZONTAL : B_VERTICAL;

    MNVScrollBar *vsb = sb->id = new MNVScrollBar(sb, posture);
    if (gui.mnvWindow->Lock()) {
	vsb->SetTarget(gui.mnvTextArea);
	vsb->Hide();
	gui.mnvForm->AddChild(vsb);
	gui.mnvWindow->Unlock();
    }
}

#if defined(FEAT_WINDOWS) || defined(FEAT_GUI_HAIKU)
void
gui_mch_destroy_scrollbar(
	scrollbar_T *sb)
{
    if (gui.mnvWindow->Lock()) {
	sb->id->RemoveSelf();
	delete sb->id;
	gui.mnvWindow->Unlock();
    }
}
#endif

/*
 * Cursor does not flash
 */
    int
gui_mch_is_blink_off(void)
{
    return FALSE;
}

/*
 * Cursor blink functions.
 *
 * This is a simple state machine:
 * BLINK_NONE	not blinking at all
 * BLINK_OFF	blinking, cursor is not shown
 * BLINK_ON blinking, cursor is shown
 */

#define BLINK_NONE  0
#define BLINK_OFF   1
#define BLINK_ON    2

static int	blink_state = BLINK_NONE;
static long_u	    blink_waittime = 700;
static long_u	    blink_ontime = 400;
static long_u	    blink_offtime = 250;
static int  blink_timer = 0;

void
gui_mch_set_blinking(
	long	waittime,
	long	on,
	long	off)
{
    // TODO
    blink_waittime = waittime;
    blink_ontime = on;
    blink_offtime = off;
}

/*
 * Stop the cursor blinking.  Show the cursor if it wasn't shown.
 */
    void
gui_mch_stop_blink(int may_call_gui_update_cursor)
{
    // TODO
    if (blink_timer != 0)
    {
	// XtRemoveTimeOut(blink_timer);
	blink_timer = 0;
    }
    if (blink_state == BLINK_OFF)
	gui_update_cursor(TRUE, FALSE);
    blink_state = BLINK_NONE;
}

/*
 * Start the cursor blinking.  If it was already blinking, this restarts the
 * waiting time and shows the cursor.
 */
    void
gui_mch_start_blink()
{
    // TODO
    if (blink_timer != 0)
	;// XtRemoveTimeOut(blink_timer);
    // Only switch blinking on if none of the times is zero
    if (blink_waittime && blink_ontime && blink_offtime && gui.in_focus)
    {
	blink_timer = 1; // XtAppAddTimeOut(app_context, blink_waittime,
	blink_state = BLINK_ON;
	gui_update_cursor(TRUE, FALSE);
    }
}

/*
 * Initialise mnv to use the font with the given name.	Return FAIL if the font
 * could not be loaded, OK otherwise.
 */
int
gui_mch_init_font(
	char_u	    *font_name,
	int	    fontset)
{
    if (gui.mnvWindow->Lock())
    {
	int rc = gui.mnvTextArea->mchInitFont(font_name);
	gui.mnvWindow->Unlock();

	return rc;
    }

    return FAIL;
}


    int
gui_mch_adjust_charsize()
{
    return FAIL;
}


    int
gui_mch_font_dialog(font_family* family, font_style* style, float* size)
{
#if defined(FEAT_GUI_DIALOG)
	// gui.mnvWindow->Unlock();
    MNVSelectFontDialog *dialog = new MNVSelectFontDialog(family, style, size);
    bool ret = dialog->Go();
	delete dialog;
	return ret;
#else
    return NOFONT;
#endif // FEAT_GUI_DIALOG
}


GuiFont
gui_mch_get_font(
	char_u	    *name,
	int	    giveErrorIfMissing)
{
    static MNVFont *fontList = NULL;

    if (!gui.in_use)	//  can't do this when GUI not running
	return NOFONT;

    //	storage for locally modified name;
    const int buff_size = B_FONT_FAMILY_LENGTH + B_FONT_STYLE_LENGTH + 20;
    static char font_name[buff_size] = {0};
    font_family family = {0};
    font_style	style  = {0};
    float size = 0.f;

    if (name == 0 && be_fixed_font == 0) {
	if (giveErrorIfMissing)
	    semsg(_(e_unknown_font_str), name);
	return NOFONT;
    }

    bool useSelectGUI = false;
    if (name != NULL)
	if (STRCMP(name, "*") == 0) {
	    useSelectGUI = true;
	    STRNCPY(font_name, hl_get_font_name(), buff_size);
	} else
	    STRNCPY(font_name, name, buff_size);

    if (font_name[0] == 0) {
	be_fixed_font->GetFamilyAndStyle(&family, &style);
	size = be_fixed_font->Size();
	mnv_snprintf(font_name, buff_size,
	    (char*)"%s/%s/%.0f", family, style, size);
    }

    //	replace underscores with spaces
    char* end = 0;
    while (end = strchr((char *)font_name, '_'))
	*end = ' ';

    //	store the name before strtok corrupt the buffer ;-)
    static char buff[buff_size] = {0};
    STRNCPY(buff, font_name, buff_size);
    STRNCPY(family, strtok(buff, "/\0"), B_FONT_FAMILY_LENGTH);
    char* style_s = strtok(0, "/\0");
    if (style_s != 0)
	STRNCPY(style, style_s, B_FONT_STYLE_LENGTH);
    size = atof((style_s != 0) ? strtok(0, "/\0") : "0");

    if (useSelectGUI) {
	if (gui_mch_font_dialog(&family, &style, &size) == NOFONT)
	    return FAIL;
	//  compose for further processing
	mnv_snprintf(font_name, buff_size,
		(char*)"%s/%s/%.0f", family, style, size);
	hl_set_font_name((char_u*)font_name);

	//  Set guifont to the name of the selected font.
	char_u* new_p_guifont = (char_u*)alloc(STRLEN(font_name) + 1);
	if (new_p_guifont != NULL) {
	    STRCPY(new_p_guifont, font_name);
	    mnv_free(p_guifont);
	    p_guifont = new_p_guifont;
	    //	Replace spaces in the font name with underscores.
	    for ( ; *new_p_guifont; ++new_p_guifont)
		if (*new_p_guifont == ' ')
		    *new_p_guifont = '_';
	}
    }

    MNVFont *flp;
    for (flp = fontList; flp; flp = flp->next) {
	if (STRCMP(font_name, flp->name) == 0) {
	    flp->refcount++;
	    return (GuiFont)flp;
	}
    }

    MNVFont *font = new MNVFont();
    font->name = mnv_strsave((char_u*)font_name);

    if (count_font_styles(family) <= 0) {
	if (giveErrorIfMissing)
	    semsg(_(e_unknown_font_str), font->name);
	delete font;
	return NOFONT;
    }

    //	Remember font in the static list for later use
    font->next = fontList;
    fontList = font;

    font->SetFamilyAndStyle(family, style);
    if (size > 0.f)
	font->SetSize(size);

    font->SetSpacing(B_FIXED_SPACING);
    font->SetEncoding(B_UNICODE_UTF8);

    return (GuiFont)font;
}

/*
 * Set the current text font.
 */
void
gui_mch_set_font(
	GuiFont	font)
{
    if (gui.mnvWindow->Lock()) {
	MNVFont *vf = (MNVFont *)font;

	gui.mnvTextArea->SetFont(vf);

	gui.char_width = (int) vf->StringWidth("n");
	font_height fh;
	vf->GetHeight(&fh);
	gui.char_height = (int)(fh.ascent + 0.9999)
	    + (int)(fh.descent + 0.9999) + (int)(fh.leading + 0.9999);
	gui.char_ascent = (int)(fh.ascent + 0.9999);

	gui.mnvWindow->Unlock();
    }
}

// XXX TODO This is apparently never called...
void
gui_mch_free_font(
	GuiFont	font)
{
    if (font == NOFONT)
	return;
    MNVFont *f = (MNVFont *)font;
    if (--f->refcount <= 0) {
	if (f->refcount < 0)
	    fprintf(stderr, "MNVFont: refcount < 0\n");
	delete f;
    }
}

    char_u *
gui_mch_get_fontname(GuiFont font, char_u *name)
{
    if (name == NULL)
	return NULL;
    return mnv_strsave(name);
}

/*
 * Adjust gui.char_height (after 'linespace' was changed).
 */
    int
gui_mch_adjust_charheight()
{

    // TODO: linespace support?

// #ifdef FEAT_XFONTSET
//  if (gui.fontset != NOFONTSET)
//  {
//  gui.char_height = fontset_height((XFontSet)gui.fontset) + p_linespace;
//  gui.char_ascent = fontset_ascent((XFontSet)gui.fontset)
//  + p_linespace / 2;
//  }
//  else
// #endif
    {
	MNVFont *font = (MNVFont *)gui.norm_font;
	font_height fh = {0};
	font->GetHeight(&fh);
	gui.char_height = (int)(fh.ascent + fh.descent + 0.5) + p_linespace;
	gui.char_ascent = (int)(fh.ascent + 0.5) + p_linespace / 2;
    }
    return OK;
}

    void
gui_mch_getmouse(int *x, int *y)
{
    fprintf(stderr, "gui_mch_getmouse");

    /*int	rootx, rooty, winx, winy;
      Window	root, child;
      unsigned int mask;

      if (gui.wid && XQueryPointer(gui.dpy, gui.wid, &root, &child,
      &rootx, &rooty, &winx, &winy, &mask)) {
     *x = winx;
     *y = winy;
     } else*/ {
	 *x = -1;
	 *y = -1;
     }
}

    void
gui_mch_mousehide(int hide)
{
    fprintf(stderr, "gui_mch_getmouse");
    //	TODO
}

/*
 * This function has been lifted from gui_w32.c and extended a bit.
 *
 * Return the Pixel value (color) for the given color name.
 * Return INVALCOLOR for error.
 */
guicolor_T
gui_mch_get_color(
	char_u	*name)
{
    return gui_get_color_cmn(name);
}

/*
 * Set the current text foreground color.
 */
void
gui_mch_set_fg_color(
	guicolor_T  color)
{
    rgb_color rgb = GUI_TO_RGB(color);
    if (gui.mnvWindow->Lock()) {
	gui.mnvTextArea->SetHighColor(rgb);
	gui.mnvWindow->Unlock();
    }
}

/*
 * Set the current text background color.
 */
void
gui_mch_set_bg_color(
	guicolor_T  color)
{
    rgb_color rgb = GUI_TO_RGB(color);
    if (gui.mnvWindow->Lock()) {
	gui.mnvTextArea->SetLowColor(rgb);
	gui.mnvWindow->Unlock();
    }
}

/*
 * Set the current text special color.
 */
    void
gui_mch_set_sp_color(guicolor_T	color)
{
    // prev_sp_color = color;
}

void
gui_mch_draw_string(
	int	row,
	int	col,
	char_u	*s,
	int	len,
	int	flags)
{
    if (gui.mnvWindow->Lock()) {
	gui.mnvTextArea->mchDrawString(row, col, s, len, flags);
	gui.mnvWindow->Unlock();
    }
}

	guicolor_T
gui_mch_get_rgb_color(int r, int g, int b)
{
    return gui_get_rgb_color_cmn(r, g, b);
}


// Return OK if the key with the termcap name "name" is supported.
int
gui_mch_haskey(
	char_u	*name)
{
    int i;

    for (i = 0; special_keys[i].BeKeys != 0; i++)
	if (name[0] == special_keys[i].mnv_code0 &&
		name[1] == special_keys[i].mnv_code1)
	    return OK;
    return FAIL;
}

    void
gui_mch_beep()
{
    ::beep();
}

    void
gui_mch_flash(int msec)
{
    // Do a visual beep by reversing the foreground and background colors

    if (gui.mnvWindow->Lock()) {
	BRect rect = gui.mnvTextArea->Bounds();

	gui.mnvTextArea->SetDrawingMode(B_OP_INVERT);
	gui.mnvTextArea->FillRect(rect);
	gui.mnvTextArea->Sync();
	snooze(msec * 1000);	 // wait for a few msec
	gui.mnvTextArea->FillRect(rect);
	gui.mnvTextArea->SetDrawingMode(B_OP_COPY);
	gui.mnvTextArea->Flush();
	gui.mnvWindow->Unlock();
    }
}

/*
 * Invert a rectangle from row r, column c, for nr rows and nc columns.
 */
void
gui_mch_invert_rectangle(
	int	r,
	int	c,
	int	nr,
	int	nc)
{
    BRect rect;
    rect.left = FILL_X(c);
    rect.top = FILL_Y(r);
    rect.right = rect.left + nc * gui.char_width - PEN_WIDTH;
    rect.bottom = rect.top + nr * gui.char_height - PEN_WIDTH;

    if (gui.mnvWindow->Lock()) {
	gui.mnvTextArea->SetDrawingMode(B_OP_INVERT);
	gui.mnvTextArea->FillRect(rect);
	gui.mnvTextArea->SetDrawingMode(B_OP_COPY);
	gui.mnvWindow->Unlock();
    }
}

/*
 * Iconify the GUI window.
 */
    void
gui_mch_iconify()
{
    if (gui.mnvWindow->Lock()) {
	gui.mnvWindow->Minimize(true);
	gui.mnvWindow->Unlock();
    }
}

#if defined(FEAT_EVAL)
/*
 * Bring the MNV window to the foreground.
 */
    void
gui_mch_set_foreground(void)
{
    // TODO
}
#endif

/*
 * Set the window title
 */
void
gui_mch_settitle(
	char_u	*title,
	char_u	*icon)
{
    if (gui.mnvWindow->Lock()) {
	gui.mnvWindow->SetTitle((char *)title);
	gui.mnvWindow->Unlock();
    }
}

/*
 * Draw a cursor without focus.
 */
    void
gui_mch_draw_hollow_cursor(guicolor_T color)
{
    gui_mch_set_fg_color(color);

    BRect r;
    r.left = FILL_X(gui.col);
    r.top = FILL_Y(gui.row);
    int cells = utf_off2cells(LineOffset[gui.row] + gui.col, 100); // TODO-TODO
    if (cells>=4) cells = 1;
    r.right = r.left + cells*gui.char_width - PEN_WIDTH;
    r.bottom = r.top + gui.char_height - PEN_WIDTH;

    if (gui.mnvWindow->Lock()) {
	gui.mnvTextArea->StrokeRect(r);
	gui.mnvWindow->Unlock();
	// gui_mch_flush();
    }
}

/*
 * Draw part of a cursor, only w pixels wide, and h pixels high.
 */
void
gui_mch_draw_part_cursor(
	int	w,
	int	h,
	guicolor_T  color)
{
    gui_mch_set_fg_color(color);

    BRect r;
    r.left =
#ifdef FEAT_RIGHTLEFT
	// vertical line should be on the right of current point
	CURSOR_BAR_RIGHT ? FILL_X(gui.col + 1) - w :
#endif
	FILL_X(gui.col);
    r.right = r.left + w - PEN_WIDTH;
    r.bottom = FILL_Y(gui.row + 1) - PEN_WIDTH;
    r.top = r.bottom - h + PEN_WIDTH;

    if (gui.mnvWindow->Lock()) {
	gui.mnvTextArea->FillRect(r);
	gui.mnvWindow->Unlock();
	// gui_mch_flush();
    }
}

/*
 * Catch up with any queued events.  This may put keyboard input into the
 * input buffer, call resize call-backs, trigger timers etc.  If there is
 * nothing in the event queue (& no timers pending), then we return
 * immediately.
 */
    void
gui_mch_update()
{
    gui_mch_flush();
    while (port_count(gui.vdcmp) > 0 &&
	    !mnv_is_input_buf_full() &&
	    gui_haiku_process_event(0) >= B_OK)
	/* nothing */ ;
}

/*
 * GUI input routine called by gui_wait_for_chars().  Waits for a character
 * from the keyboard.
 *  wtime == -1	    Wait forever.
 *  wtime == 0	    This should never happen.
 *  wtime > 0	    Wait wtime milliseconds for a character.
 * Returns OK if a character was found to be available within the given time,
 * or FAIL otherwise.
 */
int
gui_mch_wait_for_chars(
	int	wtime)
{
    int		focus;
    bigtime_t	until, timeout;
    status_t	st;

    if (wtime >= 0)
    {
	timeout = wtime * 1000;
	until = system_time() + timeout;
    }
    else
	timeout = B_INFINITE_TIMEOUT;

    focus = gui.in_focus;
    for (;;)
    {
	// Stop or start blinking when focus changes
	if (gui.in_focus != focus)
	{
	    if (gui.in_focus)
		gui_mch_start_blink();
	    else
		gui_mch_stop_blink(TRUE);
	    focus = gui.in_focus;
	}

	gui_mch_flush();

#ifdef MESSAGE_QUEUE
# ifdef FEAT_TIMERS
	did_add_timer = FALSE;
# endif
	parse_queued_messages();
# ifdef FEAT_TIMERS
	if (did_add_timer)
	    // Need to recompute the waiting time.
	    break;
# endif
# ifdef FEAT_JOB_CHANNEL
	if (has_any_channel())
	{
	    if (wtime < 0 || timeout > 20000)
		timeout = 20000;
	}
	else if (wtime < 0)
	    timeout = B_INFINITE_TIMEOUT;
# endif
#endif

	/*
	 * Don't use gui_mch_update() because then we will spin-lock until a
	 * char arrives, instead we use gui_haiku_process_event() to hang until
	 * an event arrives.  No need to check for input_buf_full because we
	 * are returning as soon as it contains a single char.
	 */
	st = gui_haiku_process_event(timeout);

	if (input_available())
	    return OK;
	if (st < B_OK)		// includes B_TIMED_OUT
	    return FAIL;

	/*
	 * Calculate how much longer we're willing to wait for the
	 * next event.
	 */
	if (wtime >= 0)
	{
	    timeout = until - system_time();
	    if (timeout < 0)
		break;
	}
    }
    return FAIL;

}

/*
 * Output routines.
 */

/*
 * Flush any output to the screen. This is typically called before
 * the app goes to sleep.
 */
    void
gui_mch_flush()
{
    //	does this need to lock the window? Apparently not but be safe.
    if (gui.mnvWindow->Lock()) {
	gui.mnvWindow->Flush();
	gui.mnvWindow->Unlock();
    }
    return;
}

/*
 * Clear a rectangular region of the screen from text pos (row1, col1) to
 * (row2, col2) inclusive.
 */
void
gui_mch_clear_block(
	int	row1,
	int	col1,
	int	row2,
	int	col2)
{
    if (gui.mnvWindow->Lock()) {
	gui.mnvTextArea->mchClearBlock(row1, col1, row2, col2);
	gui.mnvWindow->Unlock();
    }
}

    void
gui_mch_clear_all()
{
    if (gui.mnvWindow->Lock()) {
	gui.mnvTextArea->mchClearAll();
	gui.mnvWindow->Unlock();
    }
}

/*
 * Delete the given number of lines from the given row, scrolling up any
 * text further down within the scroll region.
 */
void
gui_mch_delete_lines(
	int	row,
	int	num_lines)
{
    gui.mnvTextArea->mchDeleteLines(row, num_lines);
}

/*
 * Insert the given number of lines before the given row, scrolling down any
 * following text within the scroll region.
 */
void
gui_mch_insert_lines(
	int	row,
	int	num_lines)
{
    gui.mnvTextArea->mchInsertLines(row, num_lines);
}

#if defined(FEAT_MENU)
/*
 * Menu stuff.
 */

void
gui_mch_enable_menu(
	int	flag)
{
    if (gui.mnvWindow->Lock())
    {
	BMenuBar *menubar = gui.mnvForm->MenuBar();
	menubar->SetEnabled(flag);
	gui.mnvWindow->Unlock();
    }
}

void
gui_mch_set_menu_pos(
	int	x,
	int	y,
	int	w,
	int	h)
{
    // It will be in the right place anyway
}

/*
 * Add a sub menu to the menu bar.
 */
void
gui_mch_add_menu(
	mnvmenu_T   *menu,
	int	idx)
{
    mnvmenu_T	*parent = menu->parent;

    //	popup menu - just create it unattached
    if (menu_is_popup(menu->name) && parent == NULL) {
	BPopUpMenu* popUpMenu = new BPopUpMenu((const char*)menu->name, false, false);
	menu->submenu_id = popUpMenu;
	menu->id = NULL;
	return;
    }

    if (!menu_is_menubar(menu->name)
	    || (parent != NULL && parent->submenu_id == NULL))
	return;

    if (gui.mnvWindow->Lock())
    {
	// Major re-write of the menu code, it was failing with memory corruption when
	// we started loading multiple files (the Buffer menu)
	//
	// Note we don't use the preference values yet, all are inserted into the
	// menubar on a first come-first served basis...
	//
	// richard@whitequeen.com jul 99

	BMenu *tmp;

	if ( parent )
	    tmp = parent->submenu_id;
	else
	    tmp = gui.mnvForm->MenuBar();
	//  make sure we don't try and add the same menu twice. The Buffers menu tries to
	//  do this and Be starts to crash...

	if ( ! tmp->FindItem((const char *) menu->dname)) {

	    BMenu *bmenu = new BMenu((char *)menu->dname);

	    menu->submenu_id = bmenu;

	    //	when we add a BMenu to another Menu, it creates the interconnecting BMenuItem
	    tmp->AddItem(bmenu);

	    //	Now it's safe to query the menu for the associated MenuItem...
	    menu->id = tmp->FindItem((const char *) menu->dname);

	}
	gui.mnvWindow->Unlock();
    }
}

    void
gui_mch_toggle_tearoffs(int enable)
{
    // no tearoff menus
}

    static BMessage *
MenuMessage(mnvmenu_T *menu)
{
    BMessage *m = new BMessage('menu');
    m->AddPointer("MNVMenu", (void *)menu);

    return m;
}

/*
 * Add a menu item to a menu
 */
void
gui_mch_add_menu_item(
	mnvmenu_T   *menu,
	int	idx)
{
    int	    mnemonic = 0;
    mnvmenu_T	*parent = menu->parent;

    // TODO: use menu->actext
    // This is difficult, since on Be, an accelerator must be a single char
    // and a lot of MNV ones are the standard VI commands.
    //
    // Punt for Now...
    // richard@whiequeen.com jul 99
    if (gui.mnvWindow->Lock())
    {
#ifdef FEAT_TOOLBAR
	if (menu_is_toolbar(parent->name)) {
	    MNVToolbar *toolbar = gui.mnvForm->ToolBar();
	    if (toolbar != NULL) {
		toolbar->AddButton(idx, menu);
	    }
	} else
#endif

	if (parent->submenu_id != NULL || menu_is_popup(parent->name)) {
	    if (menu_is_separator(menu->name)) {
		BSeparatorItem *item = new BSeparatorItem();
		parent->submenu_id->AddItem(item);
		menu->id = item;
		menu->submenu_id = NULL;
	    }
	    else {
		BMenuItem *item = new BMenuItem((char *)menu->dname,
			MenuMessage(menu));
		item->SetTarget(gui.mnvTextArea);
		item->SetTrigger((char) menu->mnemonic);
		parent->submenu_id->AddItem(item);
		menu->id = item;
		menu->submenu_id = NULL;
	    }
	}
	gui.mnvWindow->Unlock();
    }
}

/*
 * Destroy the machine specific menu widget.
 */
void
gui_mch_destroy_menu(
	mnvmenu_T   *menu)
{
    if (gui.mnvWindow->Lock())
    {
#ifdef FEAT_TOOLBAR
	if (menu->parent && menu_is_toolbar(menu->parent->name)) {
	    MNVToolbar *toolbar = gui.mnvForm->ToolBar();
	    if (toolbar != NULL) {
		toolbar->RemoveButton(menu);
	    }
	} else
#endif
	{
	    assert(menu->submenu_id == NULL || menu->submenu_id->CountItems() == 0);
	    /*
	     * Detach this menu from its parent, so that it is not deleted
	     * twice once we get to delete that parent.
	     * Deleting a BMenuItem also deletes the associated BMenu, if any
	     * (which does not have any items anymore since they were
	     * removed and deleted before).
	     */
	    BMenu *bmenu = menu->id->Menu();
	    if (bmenu)
	    {
		bmenu->RemoveItem(menu->id);
		/*
		 * If we removed the last item from the menu bar,
		 * resize it out of sight.
		 */
		if (bmenu == gui.mnvForm->MenuBar() && bmenu->CountItems() == 0)
		{
		    bmenu->ResizeTo(-MENUBAR_MARGIN, -MENUBAR_MARGIN);
		}
	    }
	    delete menu->id;
	    menu->id = NULL;
	    menu->submenu_id = NULL;

	    gui.menu_height = (int) gui.mnvForm->MenuHeight();
	}
	gui.mnvWindow->Unlock();
    }
}

/*
 * Make a menu either grey or not grey.
 */
void
gui_mch_menu_grey(
	mnvmenu_T   *menu,
	int	grey)
{
#ifdef FEAT_TOOLBAR
    if (menu->parent && menu_is_toolbar(menu->parent->name)) {
	if (gui.mnvWindow->Lock()) {
	    MNVToolbar *toolbar = gui.mnvForm->ToolBar();
	    if (toolbar != NULL) {
		toolbar->GrayButton(menu, grey);
	    }
	    gui.mnvWindow->Unlock();
	}
    } else
#endif
    if (menu->id != NULL)
	menu->id->SetEnabled(!grey);
}

/*
 * Make menu item hidden or not hidden
 */
void
gui_mch_menu_hidden(
	mnvmenu_T   *menu,
	int	hidden)
{
    if (menu->id != NULL)
	menu->id->SetEnabled(!hidden);
}

/*
 * This is called after setting all the menus to grey/hidden or not.
 */
    void
gui_mch_draw_menubar()
{
    // Nothing to do in BeOS
}

    void
gui_mch_show_popupmenu(mnvmenu_T *menu)
{
    if (!menu_is_popup(menu->name) || menu->submenu_id == NULL)
	return;

    BPopUpMenu* popupMenu = dynamic_cast<BPopUpMenu*>(menu->submenu_id);
    if (popupMenu == NULL)
	return;

    BPoint point;
    if (gui.mnvWindow->Lock()) {
	uint32 buttons = 0;
	gui.mnvTextArea->GetMouse(&point, &buttons);
	gui.mnvTextArea->ConvertToScreen(&point);
	gui.mnvWindow->Unlock();
    }
    popupMenu->Go(point, true);
}

#endif // FEAT_MENU

// Mouse stuff

#ifdef FEAT_CLIPBOARD
/*
 * Clipboard stuff, for cutting and pasting text to other windows.
 */
char textplain[] = "text/plain";
char mnvselectiontype[] = "application/x-vnd.Rhialto-MNV-selectiontype";

/*
 * Get the current selection and put it in the clipboard register.
 */
    void
clip_mch_request_selection(Clipboard_T *cbd)
{
    if (be_clipboard->Lock())
    {
	BMessage *m = be_clipboard->Data();
	// m->PrintToStream();

	char_u *string = NULL;
	ssize_t stringlen = -1;

	if (m->FindData(textplain, B_MIME_TYPE,
		    (const void **)&string, &stringlen) == B_OK
		|| m->FindString("text", (const char **)&string) == B_OK)
	{
	    if (stringlen == -1)
		stringlen = STRLEN(string);

	    int type;
	    char *seltype;
	    ssize_t seltypelen;

	    /*
	     * Try to get the special mnv selection type first
	     */
	    if (m->FindData(mnvselectiontype, B_MIME_TYPE,
			(const void **)&seltype, &seltypelen) == B_OK)
	    {
		switch (*seltype)
		{
		    default:
		    case 'L':	type = MLINE;	break;
		    case 'C':	type = MCHAR;	break;
#ifdef FEAT_VISUAL
		    case 'B':	type = MBLOCK;	break;
#endif
		}
	    }
	    else
	    {
		// Otherwise use heuristic as documented
		type = memchr(string, stringlen, '\n') ? MLINE : MCHAR;
	    }
	    clip_yank_selection(type, string, (long)stringlen, cbd);
	}
	be_clipboard->Unlock();
    }
}
/*
 * Make mnv the owner of the current selection.
 */
    void
clip_mch_lose_selection(Clipboard_T *cbd)
{
    // Nothing needs to be done here
}

/*
 * Make mnv the owner of the current selection.  Return OK upon success.
 */
    int
clip_mch_own_selection(Clipboard_T *cbd)
{
    /*
     * Never actually own the clipboard.  If another application sets the
     * clipboard, we don't want to think that we still own it.
     */
    return FAIL;
}

/*
 * Send the current selection to the clipboard.
 */
    void
clip_mch_set_selection(Clipboard_T *cbd)
{
    if (be_clipboard->Lock())
    {
	be_clipboard->Clear();
	BMessage *m = be_clipboard->Data();
	assert(m);

	// If the '*' register isn't already filled in, fill it in now
	cbd->owned = TRUE;
	clip_get_selection(cbd);
	cbd->owned = FALSE;

	char_u	*str = NULL;
	long_u	count;
	int type;

	type = clip_convert_selection(&str, &count, cbd);

	if (type < 0)
	    return;

	m->AddData(textplain, B_MIME_TYPE, (void *)str, count);

	// Add type of selection
	char	vtype;
	switch (type)
	{
	    default:
	    case MLINE:    vtype = 'L';    break;
	    case MCHAR:    vtype = 'C';    break;
#ifdef FEAT_VISUAL
	    case MBLOCK:   vtype = 'B';    break;
#endif
	}
	m->AddData(mnvselectiontype, B_MIME_TYPE, (void *)&vtype, 1);

	mnv_free(str);

	be_clipboard->Commit();
	be_clipboard->Unlock();
    }
}

#endif	// FEAT_CLIPBOARD

#ifdef FEAT_BROWSE
/*
 * Pop open a file browser and return the file selected, in allocated memory,
 * or NULL if Cancel is hit.
 *  saving  - TRUE if the file will be saved to, FALSE if it will be opened.
 *  title   - Title message for the file browser dialog.
 *  dflt    - Default name of file.
 *  ext     - Default extension to be added to files without extensions.
 *  initdir - directory in which to open the browser (NULL = current dir)
 *  filter  - Filter for matched files to choose from.
 *  Has a format like this:
 *  "C Files (*.c)\0*.c\0"
 *  "All Files\0*.*\0\0"
 *  If these two strings were concatenated, then a choice of two file
 *  filters will be selectable to the user.  Then only matching files will
 *  be shown in the browser.  If NULL, the default allows all files.
 *
 *  *NOTE* - the filter string must be terminated with TWO nulls.
 */
char_u *
gui_mch_browse(
	int saving,
	char_u *title,
	char_u *dflt,
	char_u *ext,
	char_u *initdir,
	char_u *filter)
{
    gui.mnvApp->fFilePanel = new BFilePanel((saving == TRUE) ? B_SAVE_PANEL : B_OPEN_PANEL,
	    NULL, NULL, 0, false,
	    new BMessage((saving == TRUE) ? 'save' : 'open'), NULL, true);

    gui.mnvApp->fBrowsedPath.Unset();

    gui.mnvApp->fFilePanel->Window()->SetTitle((char*)title);
    gui.mnvApp->fFilePanel->SetPanelDirectory((const char*)initdir);

    gui.mnvApp->fFilePanel->Show();

    gui.mnvApp->fFilePanelSem = create_sem(0, "FilePanelSem");

    while (acquire_sem(gui.mnvApp->fFilePanelSem) == B_INTERRUPTED);

    char_u *fileName = NULL;
    status_t result = gui.mnvApp->fBrowsedPath.InitCheck();
    if (result == B_OK) {
	fileName = mnv_strsave((char_u*)gui.mnvApp->fBrowsedPath.Path());
    } else
	if (result != B_NO_INIT) {
	    fprintf(stderr, "gui_mch_browse: BPath error: %#08x (%s)\n",
		    result, strerror(result));
	}

    delete gui.mnvApp->fFilePanel;
    gui.mnvApp->fFilePanel = NULL;

    return fileName;
}
#endif // FEAT_BROWSE


#if defined(FEAT_GUI_DIALOG)

/*
 * Create a dialog dynamically from the parameter strings.
 * type	    = type of dialog (question, alert, etc.)
 * title    = dialog title. may be NULL for default title.
 * message  = text to display. Dialog sizes to accommodate it.
 * buttons  = '\n' separated list of button captions, default first.
 * dfltbutton	= number of default button.
 *
 * This routine returns 1 if the first button is pressed,
 *	    2 for the second, etc.
 *
 *	    0 indicates Esc was pressed.
 *	    -1 for unexpected error
 *
 * If stubbing out this fn, return 1.
 */

int
gui_mch_dialog(
	int	 type,
	char_u	*title,
	char_u	*message,
	char_u	*buttons,
	int	 dfltbutton,
	char_u	*textfield,
	int ex_cmd)
{
    MNVDialog *dialog = new MNVDialog(type, (char*)title, (char*)message,
	    (char*)buttons, dfltbutton, (char*)textfield, ex_cmd);
    bool ret = dialog->Go();
    delete dialog;
	return ret;
}

#endif // FEAT_GUI_DIALOG


/*
 * Return the RGB value of a pixel as long.
 */
    guicolor_T
gui_mch_get_rgb(guicolor_T pixel)
{
    rgb_color rgb = GUI_TO_RGB(pixel);

    return ((rgb.red & 0xff) << 16) + ((rgb.green & 0xff) << 8)
	+ (rgb.blue & 0xff);
}

    void
gui_mch_setmouse(int x, int y)
{
    TRACE();
    // TODO
}

#ifdef FEAT_MBYTE_IME
    void
im_set_position(int row, int col)
{
    if (gui.mnvWindow->Lock())
    {
	gui.mnvTextArea->DrawIMString();
	gui.mnvWindow->Unlock();
    }
    return;
}
#endif

    void
gui_mch_show_toolbar(int showit)
{
    MNVToolbar *toolbar = gui.mnvForm->ToolBar();
    gui.toolbar_height = (toolbar && showit) ? toolbar->ToolbarHeight() : 0.;
}

    void
gui_mch_set_toolbar_pos(int x, int y, int w, int h)
{
    MNVToolbar *toolbar = gui.mnvForm->ToolBar();
    if (toolbar != NULL) {
	if (gui.mnvWindow->Lock()) {
	    toolbar->MoveTo(x, y);
	    toolbar->ResizeTo(w - 1, h - 1);
	    gui.mnvWindow->Unlock();
	}
    }
}

#if defined(FEAT_GUI_TABLINE)

/*
 * Show or hide the tabline.
 */
    void
gui_mch_show_tabline(int showit)
{
    MNVTabLine *tabLine = gui.mnvForm->TabLine();

    if (tabLine == NULL)
	return;

    if (!showit != !gui.mnvForm->IsShowingTabLine()) {
	gui.mnvForm->SetShowingTabLine(showit != 0);
	gui.tabline_height = gui.mnvForm->TablineHeight();
    }
}

    void
gui_mch_set_tabline_pos(int x, int y, int w, int h)
{
    MNVTabLine *tabLine = gui.mnvForm->TabLine();
    if (tabLine != NULL) {
	if (gui.mnvWindow->Lock()) {
	    tabLine->MoveTo(x, y);
	    tabLine->ResizeTo(w - 1, h - 1);
	    gui.mnvWindow->Unlock();
	}
    }
}

/*
 * Return TRUE when tabline is displayed.
 */
    int
gui_mch_showing_tabline()
{
    MNVTabLine *tabLine = gui.mnvForm->TabLine();
    return tabLine != NULL && gui.mnvForm->IsShowingTabLine();
}

/*
 * Update the labels of the tabline.
 */
    void
gui_mch_update_tabline()
{
    tabpage_T	*tp;
    int	    nr = 0;
    int	    curtabidx = 0;

    MNVTabLine *tabLine = gui.mnvForm->TabLine();

    if (tabLine == NULL)
	return;

    gui.mnvWindow->Lock();

    // Add a label for each tab page.  They all contain the same text area.
    for (tp = first_tabpage; tp != NULL; tp = tp->tp_next, ++nr) {
	if (tp == curtab)
	    curtabidx = nr;

	BTab* tab = tabLine->TabAt(nr);

	if (tab == NULL) {
	    tab = new MNVTabLine::MNVTab();
	    tabLine->AddTab(NULL, tab);
	}

	get_tabline_label(tp, FALSE);
	tab->SetLabel((const char*)NameBuff);
	tabLine->Invalidate();
    }

    // Remove any old labels.
    while (nr < tabLine->CountTabs())
	tabLine->RemoveTab(nr);

    if (tabLine->Selection() != curtabidx)
	tabLine->Select(curtabidx);

    gui.mnvWindow->Unlock();
}

/*
 * Set the current tab to "nr".  First tab is 1.
 */
    void
gui_mch_set_curtab(int nr)
{
    MNVTabLine *tabLine = gui.mnvForm->TabLine();
    if (tabLine == NULL)
	return;

    gui.mnvWindow->Lock();

    if (tabLine->Selection() != nr -1)
	tabLine->Select(nr -1);

    gui.mnvWindow->Unlock();
}

#endif // FEAT_GUI_TABLINE
