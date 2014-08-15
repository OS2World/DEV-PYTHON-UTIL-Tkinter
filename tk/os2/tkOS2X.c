/* 
 * tkOS2X.c --
 *
 *	This file contains OS/2 PM emulation procedures for X routines. 
 *
 * Copyright (c) 1996-1997 Illya Vaes
 * Copyright (c) 1995 Sun Microsystems, Inc.
 * Copyright (c) 1994 Software Research Associates, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */


#include "tkInt.h"
#include "tkOS2Int.h"

/*
 * The following declaration is a special purpose backdoor into the
 * Tcl notifier.  It is used to process events on the Tcl event queue,
 * without reentering the system event queue.
 */

extern void             TclOS2FlushEvents _ANSI_ARGS_((void));

/*
 * Declarations of static variables used in this file.
 */

static Display *os2Display;	/* Display that represents OS/2 PM screen. */
Tcl_HashTable windowTable;
				/* Table of child windows indexed by handle. */
static char os2ScreenName[] = "PM:0";
                                /* Default name of OS2 display. */

/*
 * Forward declarations of procedures used in this file.
 */

static void             DeleteWindow _ANSI_ARGS_((HWND hwnd));
static void 		TranslateEvent (HWND hwnd, ULONG message,
			    MPARAM param1, MPARAM param2);

/*
 *----------------------------------------------------------------------
 *
 * TkGetServerInfo --
 *
 *	Given a window, this procedure returns information about
 *	the window server for that window.  This procedure provides
 *	the guts of the "winfo server" command.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TkGetServerInfo(interp, tkwin)
    Tcl_Interp *interp;		/* The server information is returned in
				 * this interpreter's result. */
    Tk_Window tkwin;		/* Token for window;  this selects a
				 * particular display and server. */
{
    char buffer[50];
    ULONG info[QSV_MAX]= {0};	/* System Information Data Buffer */
    APIRET rc;

    /* Request all available system information */
    rc= DosQuerySysInfo (1L, QSV_MAX, (PVOID)info, sizeof(info));
    /* Hack for LX-versions above 2.11 */
    if (info[QSV_VERSION_MINOR - 1] > 11) {
        int major = (int) (info[QSV_VERSION_MINOR - 1] / 10);
        sprintf(buffer, "%s %d.%d", "OS/2", major,
                (int) (info[QSV_VERSION_MINOR - 1] - major * 10));
    } else {
        sprintf(buffer, "%s %d.%d", "OS/2",
                (int)(info[QSV_VERSION_MINOR - 1] / 10),
                (int)info[QSV_VERSION_MINOR - 1]);
    }
    Tcl_AppendResult(interp, buffer, (char *) NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * TkOS2GetTkModule --
 *
 *      This function returns the module handle for the Tk DLL.
 *
 * Results:
 *      Returns the library module handle.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

HMODULE
TkOS2GetTkModule()
{
#ifdef DEBUG
    printf("TkOS2GetTkModule\n");
#endif
    return dllHandle;
}

/*
 *----------------------------------------------------------------------
 *
 * TkOS2GetAppInstance --
 *
 *	Retrieves the global application instance handle.
 *
 * Results:
 *	Returns the global application instance handle.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

HAB
TkOS2GetAppInstance()
{
    return hab;
}

/*
 *----------------------------------------------------------------------
 *
 * TkOS2XInit --
 *
 *	Initialize Xlib emulation layer.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Sets up various data structures.
 *
 *----------------------------------------------------------------------
 */

void
TkOS2XInit( hInstance )
    HAB hInstance;
{
    BOOL success;
    static initialized = 0;

    if (initialized != 0) {
        return;
    }
    initialized = 1;

    /*
     * Register the Child window class.
     */

    /*
     * Don't use CS_SIZEREDRAW for the child, this will make vertical resizing
     * work incorrectly (keeping the same distance from the bottom instead of
     * from the top when using Tk's "pack ... -side top").
     */
    success = WinRegisterClass(hab, TOC_CHILD, TkOS2ChildProc, 0,
                               sizeof(ULONG));
    if (!success) {
        panic("Unable to register TkChild class");
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkGetDefaultScreenName --
 *
 *      Returns the name of the screen that Tk should use during
 *      initialization.
 *
 * Results:
 *      Returns a statically allocated string.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

char *
TkGetDefaultScreenName(interp, screenName)
    Tcl_Interp *interp;         /* Not used. */
    char *screenName;           /* If NULL, use default string. */
{
    char *DISPLAY = NULL;

#ifdef DEBUG
    printf("TkGetDefaultScreenName [%s] ", screenName);
#endif
    if ((screenName == NULL) || (screenName[0] == '\0')) {
        DISPLAY = getenv("DISPLAY");
        if (DISPLAY != NULL) {
            screenName = DISPLAY;
        } else {
            screenName = os2ScreenName;
        }
    }
#ifdef DEBUG
    printf("returns [%s]\n", screenName);
#endif
    return screenName;
}

/*
 *----------------------------------------------------------------------
 *
 * XOpenDisplay --
 *
 *	Create the Display structure and fill it with device
 *	specific information.
 *
 * Results:
 *	Returns a Display structure on success or NULL on failure.
 *
 * Side effects:
 *	Allocates a new Display structure.
 *
 *----------------------------------------------------------------------
 */

Display *
XOpenDisplay(display_name)
    _Xconst char *display_name;
{
    Screen *screen;
    TkOS2Drawable *todPtr;

    TkOS2PointerInit();

    Tcl_InitHashTable(&windowTable, TCL_ONE_WORD_KEYS);

    if (os2Display != NULL) {
#ifdef DEBUG
        printf("XOpenDisplay display_name [%s], os2Display->display_name [%s]\n",
               display_name, os2Display->display_name);
#endif
	if (strcmp(os2Display->display_name, display_name) == 0) {
	    return os2Display;
	} else {
	    panic("XOpenDisplay: tried to open multiple displays");
	    return NULL;
	}
    }

    os2Display = (Display *) ckalloc(sizeof(Display));
    if (!os2Display) {
        return (Display *)None;
    }
    os2Display->display_name = (char *) ckalloc(strlen(display_name)+1);
    if (!os2Display->display_name) {
	ckfree((char *)os2Display);
        return (Display *)None;
    }
    strcpy(os2Display->display_name, display_name);

    os2Display->cursor_font = 1;
    os2Display->nscreens = 1;
    os2Display->request = 1;
    os2Display->qlen = 0;

    screen = (Screen *) ckalloc(sizeof(Screen));
    if (!screen) {
	ckfree((char *)os2Display->display_name);
	ckfree((char *)os2Display);
	return (Display *)None;
    }
    screen->display = os2Display;

    screen->width = aDevCaps[CAPS_WIDTH];
    screen->height = yScreen;
    screen->mwidth = (screen->width * 1000) / aDevCaps[CAPS_HORIZONTAL_RESOLUTION];
    screen->mheight = (screen->width * 1000) / aDevCaps[CAPS_VERTICAL_RESOLUTION];

    /*
     * Set up the root window.
     */

    todPtr = (TkOS2Drawable*) ckalloc(sizeof(TkOS2Drawable));
    if (!todPtr) {
	ckfree((char *)os2Display->display_name);
	ckfree((char *)os2Display);
	ckfree((char *)screen);
	return (Display *)None;
    }
    todPtr->type = TOD_WINDOW;
    todPtr->window.winPtr = NULL;
    todPtr->window.handle = HWND_DESKTOP;
    screen->root = (Window)todPtr;

    screen->root_depth = aDevCaps[CAPS_COLOR_BITCOUNT];
    screen->root_visual = (Visual *) ckalloc(sizeof(Visual));
    if (!screen->root_visual) {
	ckfree((char *)os2Display->display_name);
	ckfree((char *)os2Display);
	ckfree((char *)screen);
	ckfree((char *)todPtr);
	return (Display *)None;
    }
    screen->root_visual->visualid = 0;
    if ( aDevCaps[CAPS_ADDITIONAL_GRAPHICS] & CAPS_PALETTE_MANAGER ) {
	screen->root_visual->map_entries = aDevCaps[CAPS_COLOR_INDEX]+1;
	screen->root_visual->class = PseudoColor;
    } else {
	if (screen->root_depth == 4) {
	    screen->root_visual->class = StaticColor;
	    screen->root_visual->map_entries = 16;
	} else if (screen->root_depth == 16) {
	    screen->root_visual->class = TrueColor;
	    screen->root_visual->map_entries = aDevCaps[CAPS_COLOR_INDEX]+1;
	    screen->root_visual->red_mask = 0xff;
	    screen->root_visual->green_mask = 0xff00;
	    screen->root_visual->blue_mask = 0xff0000;
	    /*
	    */
	} else if (screen->root_depth >= 24) {
	    screen->root_visual->class = TrueColor;
	    screen->root_visual->map_entries = 256;
	    screen->root_visual->red_mask = 0xff;
	    screen->root_visual->green_mask = 0xff00;
	    screen->root_visual->blue_mask = 0xff0000;
	}
    }
    screen->root_visual->bits_per_rgb = screen->root_depth;

    /*
     * Note that these pixel values are not palette relative.
     */

    screen->white_pixel = RGB(255, 255, 255);
    screen->black_pixel = RGB(0, 0, 0);

    os2Display->screens = screen;
    os2Display->nscreens = 1;
    os2Display->default_screen = 0;
    screen->cmap = XCreateColormap(os2Display, None, screen->root_visual,
	    AllocNone);

    return os2Display;
}

/*
 *----------------------------------------------------------------------
 *
 * XBell --
 *
 *	Generate a beep.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Plays a sounds out the system speakers.
 *
 *----------------------------------------------------------------------
 */

void
XBell(display, percent)
    Display* display;
    int percent;
{
    rc = DosBeep (770L, 300L);
}


MRESULT EXPENTRY
TkOS2FrameProc(hwnd, message, param1, param2)
    HWND hwnd;
    ULONG message;
    MPARAM param1;
    MPARAM param2;
{
    static inMoveSize = 0;

    if (inMoveSize) {
        TclOS2FlushEvents();
    }

    switch (message) {

	case WM_CREATE: {
            TkOS2Drawable *todPtr = (TkOS2Drawable *) PVOIDFROMMP(param1);
	    Tcl_HashEntry *hPtr;
	    int new;
	    BOOL rc;
#ifdef DEBUG
            printf("FrameProc: WM_CREATE hwnd %x, tod %x\n", hwnd, todPtr);
#endif

	    /*
	     * Add the window and handle to the window table.
	     */

            todPtr->window.handle = hwnd;
	    hPtr = Tcl_CreateHashEntry(&windowTable, (char *)hwnd, &new);
	    if (!new) {
		panic("Duplicate window handle: %p", hwnd);
	    }
	    Tcl_SetHashValue(hPtr, todPtr);

	    /*
	     * Store the pointer to the drawable structure passed into
	     * WinCreateWindow in the user data slot of the window.
	     */

	    rc= WinSetWindowULong(hwnd, QWL_USER, (ULONG)todPtr);
            break;
	}

	case WM_DESTROY: {
#ifdef DEBUG
            printf("FrameProc: WM_DESTROY hwnd %x\n", hwnd);
#endif
            DeleteWindow(hwnd);
	    return 0;
	}
	    

	case WM_QUERYTRACKINFO: {
	    TRACKINFO *track = (TRACKINFO *)PVOIDFROMMP(param2);
	    SWP pos;
	    BOOL rc;

#ifdef DEBUG
            printf("FrameProc: WM_QUERYTRACKINFO hwnd %x, trackinfo %x\n",
                   hwnd, track);
#endif
            inMoveSize = 1;
            /* Get present size as default max/min */
            rc = WinQueryWindowPos(hwnd, &pos);
#ifdef DEBUG
            if (rc!=TRUE) {
                printf("   WinQueryWindowPos ERROR %x\n", WinGetLastError(hab));
            } else {
                printf("   WinQueryWindowPos OK\n");
            }
#endif
            /* Fill in defaults */
            track->cxBorder = track->cyBorder = 4; /* 4 pixels tracking */
            track->cxGrid = track->cyGrid = 1; /* smooth tracking */
            track->cxKeyboard = track->cyKeyboard = 8; /* fast keyboardtracking */
            rc = WinSetRect(hab, &track->rclTrack, pos.x, pos.y,
                            pos.x + pos.cx, pos.y + pos.cy);
#ifdef DEBUG
            if (rc!=TRUE) {
                printf("    WinSetRect ERROR %x\n", WinGetLastError(hab));
            } else {
                printf("    WinSetRect OK\n");
            }
#endif
            rc = WinSetRect(hab, &track->rclBoundary, 0, 0, xScreen, yScreen);
#ifdef DEBUG
            if (rc!=TRUE) {
                printf("    WinSetRect ERROR %x\n", WinGetLastError(hab));
            } else {
                printf("    WinSetRect OK\n");
            }
#endif
            track->ptlMinTrackSize.x = 0;
            track->ptlMaxTrackSize.x = xScreen;
            track->ptlMinTrackSize.y = 0;
            track->ptlMaxTrackSize.y = yScreen;
            track->fs = SHORT1FROMMP(param1);
            /* Determine what Tk will allow */
            TkOS2WmSetLimits(hwnd, (TRACKINFO *) PVOIDFROMMP(param2));
            inMoveSize = 0;
            return (MRESULT)1;	/* continue sizing or moving */
	}

	case WM_REALIZEPALETTE:
	    /* Notifies that the input focus window has realized its logical
	     * palette, so realize ours and update client area(s)
	     * Must return 0
	     */
#ifdef DEBUG
            printf("FrameProc: WM_REALIZEPALETTE hwnd %x\n", hwnd);
#endif
	    TkOS2WmInstallColormaps(hwnd, WM_REALIZEPALETTE, FALSE);
            break;

	case WM_SETFOCUS:
	    /* 
             * If usfocus is true we translate the event *AND* install the
             * colormap, otherwise we only translate the event.
             */
#ifdef DEBUG
            printf("FrameProc: WM_SETFOCUS hwnd %x, usfocus %x\n", hwnd,
                   param2);
#endif
	    if ( LONGFROMMP(param2) == TRUE ) {
                HPS hps = WinGetPS(hwnd);
                ULONG colorsChanged;
                TkOS2Drawable *todPtr =
                    (TkOS2Drawable *) WinQueryWindowULong(hwnd, QWL_USER);

                if (TkOS2GetWinPtr(todPtr) != NULL) {
                    GpiSelectPalette(hps,
                       TkOS2GetPalette(TkOS2GetWinPtr(todPtr)->atts.colormap));
                    WinRealizePalette(hwnd, hps, &colorsChanged);
                    /*
	            TkOS2WmInstallColormaps(hwnd, WM_SETFOCUS, FALSE);
                    */
                }
	    }
	    TranslateEvent(hwnd, message, param1, param2);
	    break;

	case WM_WINDOWPOSCHANGED: {
	    SWP *pos = (SWP *) PVOIDFROMMP(param1);
	    TkOS2Drawable *todPtr = (TkOS2Drawable *)
	                            WinQueryWindowULong(hwnd, QWL_USER);
#ifdef DEBUG
				/*MM* add { */
            printf("FrameProc: WM_WINDOWPOSCHANGED hwnd %x (%d,%d) %dx%d,fl%x,\
                   Awp%x, tod %x\n", hwnd, pos->x, pos->y, pos->cx, pos->cy,
                   pos->fl, LONGFROMMP(param2), todPtr);
				/*MM* } */
/*MM* del            printf("FrameProc: WM_WINDOWPOSCHANGED hwnd %x (%d,%d) %dx%d,fl%x, */
/*MM* del                   Awp%x, tod %x\n", hwnd, pos->x, pos->y, pos->cx, pos->cy, */
/*MM* del                   pos->fl, LONGFROMMP(param2), todPtr); */
#endif
            TkOS2WmConfigure(TkOS2GetWinPtr(todPtr), pos);
            break;
	}

        /*
         * The frame sends the WM_CLOSE to the client (child) if it exists,
         * so we have to handle it there.
         * Also hand off mouse/button stoff to default procedure.
         */
	case WM_CLOSE:
	case WM_BUTTON1DOWN:
	case WM_BUTTON2DOWN:
	case WM_BUTTON3DOWN:
	case WM_BUTTON1UP:
	case WM_BUTTON2UP:
	case WM_BUTTON3UP:
	case WM_MOUSEMOVE:
            break;

	case WM_CHAR:
#ifdef DEBUG
           printf("FrameProc: WM_CHAR (First one pertinent)\n");
#endif
	    TranslateEvent(hwnd, message, param1, param2);
            return 0;

	case WM_DESTROYCLIPBOARD:
#ifdef DEBUG
            printf("FrameProc: WM_DESTROYCLIPBOARD hwnd %x\n", hwnd);
#endif
	    TranslateEvent(hwnd, message, param1, param2);

	    /*
	     * We need to pass these messages to the default window
	     * procedure in order to get the system menu to work.
	     */

	    break;
        default:
		  	;				/*MM*/
    }

    return oldFrameProc(hwnd, message, param1, param2);
}

/*
 *----------------------------------------------------------------------
 *
 * TkOS2ChildProc --
 *
 *	Callback from Presentation Manager whenever an event occurs on
 *	a child window.
 *
 * Results:
 *	Standard OS/2 PM return value.
 *
 * Side effects:
 *	Default window behavior.
 *
 *----------------------------------------------------------------------
 */

MRESULT EXPENTRY
TkOS2ChildProc(hwnd, message, param1, param2)
    HWND hwnd;
    ULONG message;
    MPARAM param1;
    MPARAM param2;
{
    switch (message) {

	case WM_CREATE: {
	    CREATESTRUCT *info = (CREATESTRUCT *) PVOIDFROMMP(param2);
	    Tcl_HashEntry *hPtr;
	    int new;
#ifdef DEBUG
            printf("Child: WM_CREATE hwnd %x, info %x\n", hwnd, info);
#endif

	    /*
	     * Add the window and handle to the window table.
	     */

	    hPtr = Tcl_CreateHashEntry(&windowTable, (char *)hwnd, &new);
	    if (!new) {
		panic("Duplicate window handle: %p", hwnd);
	    }
	    Tcl_SetHashValue(hPtr, info->pCtlData);

	    /*
	     * Store the pointer to the drawable structure passed into
	     * WinCreateWindow in the user data slot of the window.  Then
	     * set the Z stacking order so the window appears on top.
	     */
	    
	    WinSetWindowULong(hwnd, QWL_USER, (ULONG)info->pCtlData);
	    WinSetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_ZORDER);
	    return 0;
	}

	case WM_DESTROY:
#ifdef DEBUG
            printf("Child: WM_DESTROY hwnd %x\n", hwnd);
#endif
            DeleteWindow(hwnd);
	    return 0;
	    
	case WM_RENDERFMT: {
	    TkOS2Drawable *todPtr;
	    todPtr = (TkOS2Drawable *) WinQueryWindowULong(hwnd, QWL_USER);
#ifdef DEBUG
            printf("Child: WM_RENDERFMT hwnd %x, tod %x\n", hwnd, todPtr);
#endif
	    TkOS2ClipboardRender(TkOS2GetWinPtr(todPtr),
	                         (ULONG)(SHORT1FROMMP(param1)));
	    return 0;
	}

	case WM_BUTTON1DOWN:
#ifdef DEBUG
            printf("Child: WM_BUTTON1DOWN %d,%d\n", (LONG) SHORT1FROMMP(param1),
                   (LONG) SHORT2FROMMP(param1));
#endif
	case WM_BUTTON2DOWN:
	case WM_BUTTON3DOWN:
	case WM_BUTTON1UP:
#ifdef DEBUG
            printf("Child: WM_BUTTON1UP %d,%d\n", (LONG) SHORT1FROMMP(param1),
                   (LONG) SHORT2FROMMP(param1));
#endif
	case WM_BUTTON2UP:
	case WM_BUTTON3UP:
	    TranslateEvent(hwnd, message, param1, param2);
	    /*
	     * Pass on BUTTON stuff to default procedure, eg. for having the
	     * focus set to the frame window for us.
	     */
	    break;

	/*
	 * For double-clicks, generate a ButtonPress and ButtonRelease.
	 * Pass on BUTTON stuff to default procedure, eg. for having the
	 * focus set to the frame window for us.
	 */
	case WM_BUTTON1DBLCLK:
	    TranslateEvent(hwnd, WM_BUTTON1DOWN, param1, param2);
	    TranslateEvent(hwnd, WM_BUTTON1UP, param1, param2);
	    break;
	case WM_BUTTON2DBLCLK:
	    TranslateEvent(hwnd, WM_BUTTON2DOWN, param1, param2);
	    TranslateEvent(hwnd, WM_BUTTON2UP, param1, param2);
	    break;
	case WM_BUTTON3DBLCLK:
	    TranslateEvent(hwnd, WM_BUTTON3DOWN, param1, param2);
	    TranslateEvent(hwnd, WM_BUTTON3UP, param1, param2);
	    break;


	case WM_CLOSE:
	    /*
	     * The frame sends the WM_CLOSE to the client (child) if it exists,
	     * so we have to handle it here.
	     */
#ifdef DEBUG
            printf("Child: WM_CLOSE\n");
#endif
	case WM_PAINT:
#ifdef DEBUG
            printf("Child: WM_PAINT\n");
#endif
	case WM_DESTROYCLIPBOARD:
	case WM_MOUSEMOVE:
	case WM_CHAR:
	case WM_SETFOCUS:
	    TranslateEvent(hwnd, message, param1, param2);
	    /* Do not pass on to PM */
	    return 0;
    }

    return WinDefWindowProc(hwnd, message, param1, param2);
}

/*
 *----------------------------------------------------------------------
 *
 * TranslateEvent --
 *
 *	This function is called by the window procedures to handle
 *	the translation from OS/2 PM events to Tk events.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Queues a new Tk event.
 *
 *----------------------------------------------------------------------
 */

static void
TranslateEvent(hwnd, message, param1, param2)
    HWND hwnd;
    ULONG message;
    MPARAM param1;
    MPARAM param2;
{
    TkWindow *winPtr;
    XEvent event;
    TkOS2Drawable *todPtr;
    HWND hwndTop;

    /*
     * Retrieve the window information, and reset the hwnd pointer in
     * case the original window was a toplevel decorative frame.
     */

    todPtr = (TkOS2Drawable *) WinQueryWindowULong(hwnd, QWL_USER);
    if (todPtr == NULL) {
#ifdef DEBUG
        printf("TranslateEvent: NULL todPtr\n");
#endif
	return;
    }
    winPtr = TkOS2GetWinPtr(todPtr);

    /*
     * TranslateEvent may get called even after Tk has deleted the window.
     * So we must check for a dead window before proceeding.
     */

    if (winPtr == NULL || winPtr->window == None) {
#ifdef DEBUG
        printf("TranslateEvent: NULL winPtr or None Window (%x, %x)\n",
               winPtr, winPtr ? winPtr->window : 0);
#endif
	return;
    }

    hwndTop = hwnd;
    hwnd = TkOS2GetHWND(winPtr->window);

    event.xany.serial = winPtr->display->request++;
    event.xany.send_event = False;
#ifdef DEBUG
    printf("TranslateEvent display %x, hwnd %x\n", winPtr->display, hwnd);
#endif
    event.xany.display = winPtr->display;
    event.xany.window = (Window) winPtr->window;

    switch (message) {
	case WM_PAINT: {
	    HPS hps;
	    RECTL rectl;

	    event.type = Expose;
	    hps= WinBeginPaint(hwnd, NULLHANDLE, &rectl);
#ifdef DEBUG
            if (hps==NULLHANDLE) {
                printf("WinBeginPaint hwnd %x ERROR %x\n", hwnd,
                       WinGetLastError(hab));
            } else {
                printf("WinBeginPaint hwnd %x is %x\n", hwnd, hps);
            }
#endif
	    WinEndPaint(hps);
#ifdef DEBUG
            printf("TranslateEvent WM_PAINT hwnd %x, xL=%d, xR=%d, yT=%d, yB=%d\n",
                    hwnd, rectl.xLeft, rectl.xRight, rectl.yTop, rectl.yBottom);
#endif

	    event.xexpose.x = rectl.xLeft;
	    /* PM coordinates reversed (*/
	    event.xexpose.y = TkOS2WindowHeight(todPtr) - rectl.yTop;
	    event.xexpose.width = rectl.xRight - rectl.xLeft;
	    event.xexpose.height = rectl.yTop - rectl.yBottom;
#ifdef DEBUG
            printf("       event: x=%d, y=%d, w=%d, h=%d\n", event.xexpose.x,
                   event.xexpose.y, event.xexpose.width, event.xexpose.height);
#endif
	    event.xexpose.count = 0;
	    break;
	}

	case WM_CLOSE:
#ifdef DEBUG
            printf("TranslateEvent WM_CLOSE hwnd %x\n", hwnd);
#endif
	    event.type = ClientMessage;
	    event.xclient.message_type =
		Tk_InternAtom((Tk_Window) winPtr, "WM_PROTOCOLS");
	    event.xclient.format = 32;
	    event.xclient.data.l[0] =
		Tk_InternAtom((Tk_Window) winPtr, "WM_DELETE_WINDOW");
	    break;

	case WM_SETFOCUS:
#ifdef DEBUG
           printf("TranslateEvent WM_SETFOCUS hwnd %x\n", hwnd);
#endif
	    if ( (LOUSHORT(param2)) == TRUE ) {
	    	event.type = FocusIn;
	    } else {
	    	event.type = FocusOut;
	    }
	    event.xfocus.mode = NotifyNormal;
	    event.xfocus.detail = NotifyAncestor;
	    break;
	    
	case WM_DESTROYCLIPBOARD:
#ifdef DEBUG
           printf("TranslateEvent WM_DESTROYCLIPBOARD hwnd %x\n", hwnd);
#endif
	    event.type = SelectionClear;
	    event.xselectionclear.selection =
		Tk_InternAtom((Tk_Window)winPtr, "CLIPBOARD");
	    event.xselectionclear.time = WinGetCurrentTime(hab);
	    break;
	    
	case WM_BUTTON1DOWN:
	case WM_BUTTON2DOWN:
	case WM_BUTTON3DOWN:
	case WM_BUTTON1UP:
	case WM_BUTTON2UP:
	case WM_BUTTON3UP:
#ifdef DEBUG
           printf("TranslateEvent WM_BUTTON* %x hwnd %x fl %x (1D %x, 1U %x)\n",
                  message, hwnd, SHORT2FROMMP(param2), WM_BUTTON1DOWN,
                  WM_BUTTON1UP);
#endif
	case WM_MOUSEMOVE:
	case WM_CHAR:
			{
	    /*
	     * WM_CHAR and the others have different params, while the latter
	     * give only the flags of which keys were pressed (or none).
	     */
	    USHORT flags = (message == WM_CHAR) ? SHORT1FROMMP(param1)
	                                        : SHORT2FROMMP(param2);
	    unsigned int state = TkOS2GetModifierState(message, flags,
		    param1, param2);
	    Time time = WinGetCurrentTime(hab);
	    POINTL clientPoint;
	    POINTL rootPoint;
	    BOOL rc;

	    /*
	     * Compute the screen and window coordinates of the event.
	     */
	    
	    rc = WinQueryMsgPos(hab, &rootPoint);
	    clientPoint.x = rootPoint.x;
	    clientPoint.y = rootPoint.y;
	    rc= WinMapWindowPoints (HWND_DESKTOP, hwnd, &clientPoint, 1);
#ifdef DEBUG
            if (rc != TRUE) {
                printf("WinMapWindowPoints %d,%d (root %d,%d) ERROR %x\n",
                       clientPoint.x, clientPoint.y, rootPoint.x, rootPoint.y,
                       WinGetLastError(hab));
            } else {
                printf("WinMapWindowPoints %d,%d (root %d,%d) OK\n", clientPoint.x,
                       clientPoint.y, rootPoint.x, rootPoint.y);
            }
#endif

	    /*
	     * Set up the common event fields.
	     */

	    event.xbutton.root = RootWindow(winPtr->display,
		    winPtr->screenNum);
	    event.xbutton.subwindow = None;
	    event.xbutton.x = clientPoint.x;
	    /* PM coordinates reversed */
	    event.xbutton.y = TkOS2WindowHeight((TkOS2Drawable *)todPtr)
	                      - clientPoint.y;
	    event.xbutton.x_root = rootPoint.x;
	    event.xbutton.y_root = yScreen - rootPoint.y;
	    event.xbutton.state = state;
	    event.xbutton.time = time;
	    event.xbutton.same_screen = True;

	    /*
	     * Now set up event specific fields.
	     */

	    switch (message) {
		case WM_BUTTON1DOWN:
		    event.type = ButtonPress;
		    event.xbutton.button = Button1;
		    break;

		case WM_BUTTON2DOWN:
		    event.type = ButtonPress;
		    event.xbutton.button = Button2;
		    break;

		case WM_BUTTON3DOWN:
		    event.type = ButtonPress;
		    event.xbutton.button = Button3;
		    break;
	
		case WM_BUTTON1UP:
		    event.type = ButtonRelease;
		    event.xbutton.button = Button1;
		    break;
	
		case WM_BUTTON2UP:
		    event.type = ButtonRelease;
		    event.xbutton.button = Button2;
		    break;

		case WM_BUTTON3UP:
		    event.type = ButtonRelease;
		    event.xbutton.button = Button3;
		    break;
	
		case WM_MOUSEMOVE:
		    event.type = MotionNotify;
		    event.xmotion.is_hint = NotifyNormal;
#ifdef DEBUG
                    printf("WM_MOUSEMOVE %d,%d (root %d,%d)\n", clientPoint.x,
                           clientPoint.y, rootPoint.x, rootPoint.y);
#endif
		    break;

		    /*
		     * We don't check for translated characters on keyup
		     * because Tk won't know what to do with them.  Instead, we
		     * wait for the WM_CHAR messages which will follow.
		     */
		/*
		    event.type = KeyRelease;
		    event.xkey.keycode = param1;
		    event.xkey.nchars = 0;
		    break;
		*/

		case WM_CHAR: {
		    /*
		     * Careful: for each keypress, two of these messages are
		     * generated, one when pressed and one when released.
		     * When the keyboard-repeat is triggered, multiple key-
		     * down messages can be generated. If this goes faster
		     * than they are retrieved from the queue, they can be
		     * combined in one message.
		     */
		    USHORT flags= CHARMSG(&message)->fs;
		    UCHAR krepeat= CHARMSG(&message)->cRepeat;
		    USHORT charcode= CHARMSG(&message)->chr;
		    USHORT vkeycode= CHARMSG(&message)->vkey;
		    int loop;
		    /*
		     * Check for translated characters in the event queue.
		     * and more than 1 char in the message
		     */
		    if ( (flags & KC_ALT) && !(flags & KC_CTRL) &&
		         (vkeycode != VK_CTRL) && (vkeycode != VK_F10) ) {
		        /* Equivalent to Windows' WM_SYSKEY */
#ifdef DEBUG
                        printf("Equivalent of WM_SYSKEY...\n");
#endif
		    }
		    if ( flags & KC_KEYUP ) {
		    	/* Key Up */
#ifdef DEBUG
                        printf("KeyUp\n");
#endif
		    	event.type = KeyRelease;
		    } else {
		    	/* Key Down */
#ifdef DEBUG
                        printf("KeyDown\n");
#endif
		    	event.type = KeyPress;
		    }
		    if ( flags & KC_VIRTUALKEY ) {
		    	/* vkeycode is valid, should be given precedence */
		    	event.xkey.keycode = vkeycode;
#ifdef DEBUG
                        printf("virtual keycode %x\n", vkeycode);
#endif
		    } else {
		    	event.xkey.keycode = 0;
		    }
		    if ( flags & KC_CHAR ) {
		    	/* charcode is valid */
		    	event.xkey.nchars = krepeat;
		    	for ( loop=0; loop < krepeat; loop++ ) {
		    		event.xkey.trans_chars[loop] = charcode;
#ifdef DEBUG
                        printf("charcode %x\n", charcode);
#endif
		    	}
		    } else {
		        /*
		         * No KC_CHAR, but char can be valid with KC_ALT,
		         * KC_CTRL when it is not 0.
		         */
#ifdef DEBUG
                        printf("no KC_CHAR\n");
#endif
		    	if ( (flags & KC_ALT || flags & KC_CTRL)
		    	     && charcode != 0) {
#ifdef DEBUG
                            printf("KC_ALT/KC_CTRL and non-0 char %c (%x)\n",
                                   charcode, charcode);
#endif
		    	    event.xkey.keycode = charcode;
		    	}
		    	event.xkey.nchars = 0;
		   	event.xkey.trans_chars[0] = 0;
		    }
                    /*
                     * Synthesize both a KeyPress and a KeyRelease.
                    Tk_QueueWindowEvent(&event, TCL_QUEUE_TAIL);
                    event.type = KeyRelease;
                     */

		    break;
	    	    }
	    }

	    if ((event.type == MotionNotify)
		    || (event.type == ButtonPress)
		    || (event.type == ButtonRelease)) {
		TkOS2PointerEvent(&event, winPtr);
		return;
	    }
	    break;
	}

	default:
	    return;
    }
    Tk_QueueWindowEvent(&event, TCL_QUEUE_TAIL);
}

/*
 *----------------------------------------------------------------------
 *
 * TkOS2GetModifierState --
 *
 *	This function constructs a state mask for the mouse buttons 
 *	and modifier keys.
 *
 * Results:
 *	Returns a composite value of all the modifier and button state
 *	flags that were set at the time the event occurred.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

unsigned int
TkOS2GetModifierState(message, flags, param1, param2)
    ULONG message;		/* OS/2 PM message type */
    USHORT flags;		/* Applicable flags (in different param for
                                 * different messages
                                 */
    MPARAM param1;		/* param1 of message, used if key message */
    MPARAM param2;		/* param2 of message, used if key message */
{
    unsigned int state = 0;	/* accumulated state flags */
    int isKeyEvent = 0;		/* 1 if message is a key press or release */
    int prevState = 0;		/* 1 if key was previously down */
    USHORT vkeycode= SHORT2FROMMP(param2); /* function key */

#ifdef DEBUG
    UCHAR krepeat= CHAR3FROMMP(param1);
    UCHAR scancode= CHAR4FROMMP(param1);
    USHORT charcode= SHORT1FROMMP(param2);

    printf("TkOS2GetModifierState fl %x, krepeat %d, scan %x, char %x, VK %x\n",
           flags, krepeat, scancode, charcode, vkeycode);
    if (flags & KC_COMPOSITE) printf("KC_COMPOSITE\n");
    if (flags & KC_VIRTUALKEY) printf("KC_VIRTUALKEY\n");
    if (flags & KC_PREVDOWN) printf("KC_PREVDOWN\n");
    if (flags & KC_SHIFT) printf("KC_SHIFT\n");
    if (flags & KC_CTRL) printf("KC_CTRL\n");
    if (flags & KC_ALT) printf("KC_ALT\n");
    if (flags & KC_NONE) printf("KC_NONE\n");
    switch (vkeycode) {
    case VK_SHIFT:
        printf("    VK_SHIFT %s\n", ((flags & KC_ALT) && (flags & KC_CTRL)) ?
               "with CTRL and ALT" : (flags & KC_ALT ? "with ALT" :
               (flags & KC_CTRL ? "with CTRL" : "")));
        break;
    case VK_CTRL:
        printf("    VK_CTRL %s\n", flags & KC_ALT ? "with ALT" : "");
        break;
    case VK_ALT:
        printf("    VK_ALT %s\n", flags & KC_CTRL ? "with CTRL" : "");
        break;
    case VK_MENU:
        printf("    VK_MENU\n");
        break;
    }
#endif
    /*
     * If the event is a key press or release, we check for autorepeat.
     */
    if ( (flags & KC_CHAR) || (flags & KC_VIRTUALKEY) ) {
	isKeyEvent = TRUE;
	prevState = flags & KC_PREVDOWN;
#ifdef DEBUG
        printf("    isKeyEvent, prevState %x\n", prevState);
#endif
    }

    /*
     * If the key being pressed or released is a modifier key, then
     * we use its previous state, otherwise we look at the current state.
     */

    if (isKeyEvent && (vkeycode == VK_SHIFT)) {
	state |= prevState ? ShiftMask : 0;
    } else {
	/*
	state |= (WinGetKeyState(HWND_DESKTOP, VK_SHIFT) & 0x8000)
	*/
	state |= (flags & KC_SHIFT)
	         ? ShiftMask : 0;
    }
    if (isKeyEvent && (vkeycode == VK_CTRL)) {
	state |= prevState ? ControlMask : 0;
    } else {
	/*
	state |= (WinGetKeyState(HWND_DESKTOP, VK_CTRL) & 0x8000)
	*/
	state |= (flags & KC_CTRL)
	         ? ControlMask : 0;
    }
    /* Do NOT handle ALTGRAF specially, since that will ignore its char */
    if (isKeyEvent && (vkeycode == VK_ALT)) {
	state |= prevState ? (AnyModifier << 2) : 0;
    } else {
	/*
	state |= (WinGetKeyState(HWND_DESKTOP, VK_ALT) & 0x8000)
	*/
	state |= (flags & KC_ALT)
	         ? (AnyModifier << 2) : 0;
    }
    if (isKeyEvent && (vkeycode == VK_MENU)) {
	state |= prevState ? Mod2Mask : 0;
    } else {
	state |= (WinGetKeyState(HWND_DESKTOP, VK_MENU) & 0x8000)
	         ? Mod2Mask : 0;
    }
#ifdef DEBUG
    if (state & ShiftMask) printf("ShiftMask\n");
    if (state & ControlMask) printf("ControlMask\n");
    if (state & (AnyModifier << 2)) printf("AltMask\n");
#endif

    /*
     * For toggle keys, we have to check both the previous key state
     * and the current toggle state.  The result is the state of the
     * toggle before the event.
     */

    if ((vkeycode == VK_CAPSLOCK) && !( flags & KC_KEYUP)) {
	state = (prevState ^ (WinGetKeyState(HWND_DESKTOP, VK_CAPSLOCK) & 0x0001))
	        ? 0 : LockMask;
    } else {
	state |= (WinGetKeyState(HWND_DESKTOP, VK_CAPSLOCK) & 0x0001)
	         ? LockMask : 0;
    }
    if ((vkeycode == VK_NUMLOCK) && !( flags & KC_KEYUP)) {
	state = (prevState ^ (WinGetKeyState(HWND_DESKTOP, VK_NUMLOCK) & 0x0001))
	        ? 0 : Mod1Mask;
    } else {
	state |= (WinGetKeyState(HWND_DESKTOP, VK_NUMLOCK) & 0x0001)
	         ? Mod1Mask : 0;
    }
    if ((vkeycode == VK_SCRLLOCK) && !( flags & KC_KEYUP)) {
	state = (prevState ^ (WinGetKeyState(HWND_DESKTOP, VK_SCRLLOCK) & 0x0001))
	        ? 0 : Mod3Mask;
    } else {
	state |= (WinGetKeyState(HWND_DESKTOP, VK_SCRLLOCK) & 0x0001)
	         ? Mod3Mask : 0;
    }

    /*
     * If a mouse button is being pressed or released, we use the previous
     * state of the button.
     */

    if (message == WM_BUTTON1UP || (message != WM_BUTTON1DOWN
	    && WinGetKeyState(HWND_DESKTOP, VK_BUTTON1) & 0x8000)) {
	state |= Button1Mask;
    }
    if (message == WM_BUTTON2UP || (message != WM_BUTTON2DOWN
	    && WinGetKeyState(HWND_DESKTOP, VK_BUTTON2) & 0x8000)) {
	state |= Button2Mask;
    }
    if (message == WM_BUTTON3UP || (message != WM_BUTTON3DOWN
	    && WinGetKeyState(HWND_DESKTOP, VK_BUTTON3) & 0x8000)) {
	state |= Button3Mask;
    }
#ifdef DEBUG
    printf("    returning state %x\n", state);
#endif
    return state;
}

/*
 *----------------------------------------------------------------------
 *
 * TkOS2GetDrawableFromHandle --
 *
 *	Find the drawable associated with the given window handle.
 *
 * Results:
 *	Returns a drawable pointer if the window is managed by Tk.
 *	Otherwise it returns NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TkOS2Drawable *
TkOS2GetDrawableFromHandle(hwnd)
    HWND hwnd;			/* OS/2 PM window handle */
{
    Tcl_HashEntry *hPtr;

    hPtr = Tcl_FindHashEntry(&windowTable, (char *)hwnd);
    if (hPtr) {
	return (TkOS2Drawable *)Tcl_GetHashValue(hPtr);
    }
#ifdef DEBUG
    printf("TkOS2GetDrawableFromHandle %x: NULL\n", hwnd);
#endif
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteWindow --
 *
 *      Remove a window from the window table, and free the resources
 *      associated with the drawable.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Frees the resources associated with a window handle.
 *
 *----------------------------------------------------------------------
 */

static void
DeleteWindow(hwnd)
    HWND hwnd;
{
    TkOS2Drawable *todPtr;
    Tcl_HashEntry *hPtr;

    /*
     * Remove the window from the window table.
     */

    hPtr = Tcl_FindHashEntry(&windowTable, (char *)hwnd);
    if (hPtr) {
        Tcl_DeleteHashEntry(hPtr);
    }

    /*
     * Free the drawable associated with this window, unless the drawable
     * is still in use by a TkWindow.  This only happens in the case of
     * a top level window, since the window gets destroyed when the
     * decorative frame is destroyed.
     */

    todPtr = (TkOS2Drawable *) WinQueryWindowULong(hwnd, QWL_USER);
#ifdef DEBUG
    printf("DeleteWindow: hwnd %x, todPtr %x\n", hwnd, todPtr);
#endif
    if (todPtr) {
#ifdef DEBUG
        printf("    todPtr->window.winPtr %x\n", todPtr->window.winPtr);
#endif
        if (todPtr->window.winPtr == NULL) {
#ifdef DEBUG
            printf("    DeleteWindow ckfree todPtr %x\n", todPtr);
#endif
            ckfree((char *) todPtr);
            todPtr= NULL;
        } else if (!(todPtr->window.winPtr->flags & TK_TOP_LEVEL)) {
#ifdef DEBUG
            printf("     PANIC    flags %x, todPtr->window.handle %x\n",
                   todPtr->window.winPtr->flags, todPtr->window.handle);
#endif
/*
 * We can get here because PM does a depth-first WM_DESTROY tree traversal
 * (children first, then the to-be-destroyed window, while Windows hands the
 * message first to the to-be-destroyed window, then to the children.
 * So don't panic
            panic("Non-toplevel window destroyed before its drawable");
*/
        } else {
#ifdef DEBUG
            printf("    DeleteWindow case 3\n");
#endif
            todPtr->window.handle = NULLHANDLE;
        }
    }
}
