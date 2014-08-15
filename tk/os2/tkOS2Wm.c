/* 
 * tkOS2Wm.c --
 *
 *	This module takes care of the interactions between a Tk-based
 *	application and the window manager.  Among other things, it
 *	implements the "wm" command and passes geometry information
 *	to the window manager.
 *
 * Copyright (c) 1996-1997 Illya Vaes
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */


#include "tkOS2Int.h"

extern Tcl_HashTable windowTable;
                                /* Table of child windows indexed by handle. */

/*
 * This module keeps a list of all top-level windows.
 */

static WmInfo *firstWmPtr = NULL;	/* Points to first top-level window. */
static WmInfo *foregroundWmPtr = NULL;	/* Points to the foreground window. */

/*
 * The variable below is used to enable or disable tracing in this
 * module.  If tracing is enabled, then information is printed on
 * standard output about interesting interactions with the window
 * manager.
 */

static int wmTracing = 0;

/*
 * The following structure is the official type record for geometry
 * management of top-level windows.
 */

static void		TopLevelReqProc (ClientData dummy, Tk_Window tkwin);

static Tk_GeomMgr wmMgrType = {
    "wm",				/* name */
    TopLevelReqProc,			/* requestProc */
    (Tk_GeomLostSlaveProc *) NULL,	/* lostSlaveProc */
};

/*
 * Global system palette.  This value always refers to the currently
 * installed foreground logical palette.
 */

static HPAL systemPalette = NULLHANDLE;

/*
 * Forward declarations for procedures defined in this file:
 */

static void	DeiconifyWindow _ANSI_ARGS_((TkWindow *winPtr));
static void     GetMaxSize _ANSI_ARGS_((WmInfo *wmPtr, int *maxWidthPtr,
                    int *maxHeightPtr));
static void     GetMinSize _ANSI_ARGS_((WmInfo *wmPtr, int *minWidthPtr,
                    int *minHeightPtr));
static void	IconifyWindow _ANSI_ARGS_((TkWindow *winPtr));
static void     InvalidateSubTree _ANSI_ARGS_((TkWindow *winPtr,
                    Colormap colormap));
static int	ParseGeometry _ANSI_ARGS_((Tcl_Interp *interp, char *string,
                    TkWindow *winPtr));
static void     RefreshColormap _ANSI_ARGS_((Colormap colormap));
static void	TopLevelEventProc _ANSI_ARGS_((ClientData clientData,
                    XEvent *eventPtr));
static void	TopLevelReqProc _ANSI_ARGS_((ClientData dummy,
                    Tk_Window tkwin));
static void	UpdateGeometryInfo _ANSI_ARGS_((ClientData clientData));

/*
 *----------------------------------------------------------------------
 *
 * TkOS2WmSetLimits --
 *
 *	Updates the minimum and maximum window size constraints.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Changes the values of the info pointer to reflect the current
 *	minimum and maximum size values.
 *
 *----------------------------------------------------------------------
 */

void
TkOS2WmSetLimits(hwnd, info)
    HWND hwnd;
    TRACKINFO *info;
{
    TkOS2Drawable *todPtr;
    TkWindow *winPtr;
    register WmInfo *wmPtr;
    int maxWidth, maxHeight;
    int minWidth, minHeight;
    int base;

#ifdef DEBUG
    printf("TkOS2WmSetLimits\n");
    printf("    trackinfo: cxBorder %d, cyBorder %d, cxGrid %d, cyGrid %d, \
cxKeyboard %d, cyKeyboard %d,\
rclTrack (%d,%d->%d,%d), rclBoundary (%d,%d->%d,%d),\
ptlMinTrackSize (%d,%d), ptlMaxTrackSize (%d,%d)\n",
           info->cxBorder, info->cyBorder, info->cxGrid, info->cyGrid,
           info->cxKeyboard, info->cyKeyboard, info->rclTrack.xLeft,
           info->rclTrack.yBottom, info->rclTrack.xRight, info->rclTrack.yTop,
           info->rclBoundary.xLeft, info->rclBoundary.yBottom,
           info->rclBoundary.xRight, info->rclBoundary.yTop,
           info->ptlMinTrackSize.x, info->ptlMinTrackSize.y,
           info->ptlMaxTrackSize.x, info->ptlMaxTrackSize.y);
#endif

    /*
     * Get the window from the user data slot.
     */

    todPtr = (TkOS2Drawable *) WinQueryWindowULong(hwnd, QWL_USER);
    winPtr = TkOS2GetWinPtr(todPtr);

    if (winPtr == NULL) {
	return;
    }

    wmPtr = winPtr->wmInfoPtr;
    
    /*
     * Copy latest constraint info.
     */

    wmPtr->defMinWidth = info->ptlMinTrackSize.x;
    wmPtr->defMinHeight = info->ptlMinTrackSize.y;
    wmPtr->defMaxWidth = info->ptlMaxTrackSize.x;
    wmPtr->defMaxHeight = info->ptlMaxTrackSize.y;

    GetMaxSize(wmPtr, &maxWidth, &maxHeight);
    GetMinSize(wmPtr, &minWidth, &minHeight);
    
    if (wmPtr->gridWin != NULL) {
        base = winPtr->reqWidth - (wmPtr->reqGridWidth * wmPtr->widthInc);
        if (base < 0) {
            base = 0;
        }
        base += wmPtr->borderWidth;
        info->ptlMinTrackSize.x = base + (minWidth * wmPtr->widthInc);
        info->ptlMaxTrackSize.x = base + (maxWidth * wmPtr->widthInc);

        base = winPtr->reqHeight - (wmPtr->reqGridHeight * wmPtr->heightInc);
        if (base < 0) {
            base = 0;
        }
        base += wmPtr->borderHeight;
        info->ptlMinTrackSize.y = base + (minHeight * wmPtr->heightInc);
        info->ptlMaxTrackSize.y = base + (maxHeight * wmPtr->heightInc);
    } else {
        info->ptlMaxTrackSize.x = maxWidth + wmPtr->borderWidth;
        info->ptlMaxTrackSize.y = maxHeight + wmPtr->borderHeight;
        info->ptlMinTrackSize.x = minWidth + wmPtr->borderWidth;
        info->ptlMinTrackSize.y = minHeight + wmPtr->borderHeight;
    }

    /*
     * If the window isn't supposed to be resizable, then set the
     * minimum and maximum dimensions to be the same as the current size.
     */

    if (wmPtr->flags & WM_WIDTH_NOT_RESIZABLE) {
        info->ptlMinTrackSize.x = winPtr->changes.width + wmPtr->borderWidth;
        info->ptlMaxTrackSize.x = info->ptlMinTrackSize.x;
    }
    if (wmPtr->flags & WM_HEIGHT_NOT_RESIZABLE) {
        info->ptlMinTrackSize.y = winPtr->changes.height + wmPtr->borderHeight;
        info->ptlMaxTrackSize.y = info->ptlMinTrackSize.y;
    }
#ifdef DEBUG
    printf("    now: cxBorder %d, cyBorder %d, cxGrid %d, cyGrid %d,\
           cxKeyboard %d, cyKeyboard %d,\
           rclTrack (%d,%d->%d,%d), rclBoundary (%d,%d->%d,%d),\
           ptlMinTrackSize (%d,%d), ptlMaxTrackSize (%d,%d)\n",
           info->cxBorder, info->cyBorder, info->cxGrid, info->cyGrid,
           info->cxKeyboard, info->cyKeyboard, info->rclTrack.xLeft,
           info->rclTrack.yBottom, info->rclTrack.xRight, info->rclTrack.yTop,
           info->rclBoundary.xLeft, info->rclBoundary.yBottom,
           info->rclBoundary.xRight, info->rclBoundary.yTop,
           info->ptlMinTrackSize.x, info->ptlMinTrackSize.y,
           info->ptlMaxTrackSize.x, info->ptlMaxTrackSize.y);
#endif
}

/*
 *--------------------------------------------------------------
 *
 * TkWmNewWindow --
 *
 *	This procedure is invoked whenever a new top-level
 *	window is created.  Its job is to initialize the WmInfo
 *	structure for the window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A WmInfo structure gets allocated and initialized.
 *
 *--------------------------------------------------------------
 */

void
TkWmNewWindow(winPtr)
    TkWindow *winPtr;		/* Newly-created top-level window. */
{
    register WmInfo *wmPtr;

    wmPtr = (WmInfo *) ckalloc(sizeof(WmInfo));
#ifdef DEBUG
    printf("TkWmNewWindow, wmPtr %x\n", wmPtr);
#endif
    winPtr->wmInfoPtr = wmPtr;
    wmPtr->winPtr = winPtr;
    wmPtr->reparent = None;
    wmPtr->titleUid = NULL;
    wmPtr->iconName = NULL;
    wmPtr->master = None;
    wmPtr->hints.flags = InputHint | StateHint;
    wmPtr->hints.input = True;
    wmPtr->hints.initial_state = NormalState;
    wmPtr->hints.icon_pixmap = None;
    wmPtr->hints.icon_window = None;
    wmPtr->hints.icon_x = wmPtr->hints.icon_y = 0;
    wmPtr->hints.icon_mask = None;
    wmPtr->hints.window_group = None;
    wmPtr->leaderName = NULL;
    wmPtr->masterWindowName = NULL;
    wmPtr->icon = NULL;
    wmPtr->iconFor = NULL;
    wmPtr->withdrawn = 0;
    wmPtr->sizeHintsFlags = 0;

    /*
     * Default the maximum dimensions to the size of the display.
     */

    wmPtr->defMinWidth = wmPtr->defMinHeight = 0;
    wmPtr->defMaxWidth = DisplayWidth(winPtr->display,
            winPtr->screenNum);
    wmPtr->defMaxHeight = DisplayHeight(winPtr->display,
            winPtr->screenNum);
    wmPtr->minWidth = wmPtr->minHeight = 1;
    wmPtr->maxWidth = wmPtr->maxHeight = 0;
    wmPtr->gridWin = NULL;
    wmPtr->widthInc = wmPtr->heightInc = 1;
    wmPtr->minAspect.x = wmPtr->minAspect.y = 1;
    wmPtr->maxAspect.x = wmPtr->maxAspect.y = 1;
    wmPtr->reqGridWidth = wmPtr->reqGridHeight = -1;
    wmPtr->gravity = NorthWestGravity;
    wmPtr->width = -1;
    wmPtr->height = -1;
    wmPtr->style = WM_TOPLEVEL_STYLE;
    wmPtr->exStyle = EX_TOPLEVEL_STYLE;
    wmPtr->x = winPtr->changes.x;
    wmPtr->y = winPtr->changes.y;

#ifdef DEBUG
    printf("TkWmNewWindow, x %d y %d\n", wmPtr->x, wmPtr->y);
#endif
    wmPtr->borderWidth = -1;
    wmPtr->borderHeight = -1;
    wmPtr->xInParent = 0;
    wmPtr->yInParent = 0;

    wmPtr->cmapList = NULL;
    wmPtr->cmapCount = 0;

    wmPtr->configWidth = -1;
    wmPtr->configHeight = -1;
    wmPtr->protPtr = NULL;
    wmPtr->cmdArgv = NULL;
    wmPtr->clientMachine = NULL;
    wmPtr->flags = WM_NEVER_MAPPED;
    wmPtr->nextPtr = firstWmPtr;
    firstWmPtr = wmPtr;

    /*
     * Tk must monitor structure events for top-level windows, in order
     * to detect size and position changes caused by window managers.
     */

    Tk_CreateEventHandler((Tk_Window) winPtr, StructureNotifyMask,
	    TopLevelEventProc, (ClientData) winPtr);

    /*
     * Arrange for geometry requests to be reflected from the window
     * to the window manager.
     */

    Tk_ManageGeometry((Tk_Window) winPtr, &wmMgrType, (ClientData) 0);
}

/*
 *--------------------------------------------------------------
 *
 * TkWmMapWindow --
 *
 *	This procedure is invoked to map a top-level window.  This
 *	module gets a chance to update all window-manager-related
 *	information in properties before the window manager sees
 *	the map event and checks the properties.  It also gets to
 *	decide whether or not to even map the window after all.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Properties of winPtr may get updated to provide up-to-date
 *	information to the window manager.  The window may also get
 *	mapped, but it may not be if this procedure decides that
 *	isn't appropriate (e.g. because the window is withdrawn).
 *
 *--------------------------------------------------------------
 */

void
TkWmMapWindow(winPtr)
    TkWindow *winPtr;		/* Top-level window that's about to
				 * be mapped. */
{
    register WmInfo *wmPtr = winPtr->wmInfoPtr;
    XEvent event;
    BOOL ret;
    HWND child = TkOS2GetHWND(winPtr->window);

#ifdef DEBUG
    printf("TkWmMapWindow winPtr %x\n", winPtr);
#endif

    if (wmPtr->flags & WM_NEVER_MAPPED) {
	int x, y, width, height;
	TkOS2Drawable *parentPtr;
	HWND frame = NULLHANDLE;
        FRAMECDATA fcdata;
        Tcl_HashEntry *hPtr;
        int new;

	wmPtr->flags &= ~WM_NEVER_MAPPED;

	/*
	 * This is the first time this window has ever been mapped.
	 * Store all the window-manager-related information for the
	 * window.
	 */

	if (wmPtr->titleUid == NULL) {
	    wmPtr->titleUid = winPtr->nameUid;
	}

	/*
         * Pick the decorative frame style.  Override redirect windows get
         * created as undecorated popups.  Transient windows get a modal dialog
         * frame.  Neither override, nor transient windows appear in the
         * tasklist.
	 */
	
	if (winPtr->atts.override_redirect) {
	    wmPtr->style = WM_OVERRIDE_STYLE;
	    wmPtr->exStyle = EX_OVERRIDE_STYLE;
	} else if (wmPtr->master) {
            wmPtr->style = WM_TRANSIENT_STYLE;
            wmPtr->exStyle = EX_TRANSIENT_STYLE;
            frame = TkOS2GetHWND(wmPtr->master);
	} else {
	    wmPtr->style = WM_TOPLEVEL_STYLE;
	    wmPtr->exStyle = EX_TOPLEVEL_STYLE;
	}

        /*
         * If the window is supposed to be visible, we need to create it
         * in that state.  Otherwise we won't get the proper defaulting
         * behavior for the initial position.
         */

        if (wmPtr->hints.initial_state == NormalState) {
            wmPtr->style |= WS_VISIBLE;
        }

        /*
         * Compute the border size and the location of the child window
         * in the reparent window for the current window style.
         * In OS/2, Y coordinates are from below, so y-in-parent is the same
         * as the width of any decorative frame in the Y direction.
         * In a similar fashion, x-in-parent is width of frame in X direction.
         * The borderHeight and borderWidth are the sum of decorations in the
         * Y (2 * frame + titlebar, if applicable) and X (2 * frame) directions.
         */

        if (wmPtr->exStyle & FCF_SIZEBORDER) {
            /* Size border */
            wmPtr->xInParent = xSizeBorder;
            wmPtr->yInParent = ySizeBorder;
            wmPtr->borderWidth = 2 * xSizeBorder;
            wmPtr->borderHeight = 2 * ySizeBorder;
#ifdef DEBUG
            printf("FCF_SIZEBORDER: xIP %d, yIP %d, bW %d, bH %d\n",
                   wmPtr->xInParent, wmPtr->yInParent, wmPtr->borderWidth,
                   wmPtr->borderHeight);
#endif
        } else if (wmPtr->exStyle & FCF_DLGBORDER) {
            /* Dialog border */
            wmPtr->xInParent = xDlgBorder;
            wmPtr->yInParent = yDlgBorder;
            wmPtr->borderWidth = 2 * xDlgBorder;
            wmPtr->borderHeight = 2 * yDlgBorder;
#ifdef DEBUG
            printf("FCF_DLGBORDER: xIP %d, yIP %d, bW %d, bH %d\n",
                   wmPtr->xInParent, wmPtr->yInParent, wmPtr->borderWidth,
                   wmPtr->borderHeight);
#endif
        } else if (wmPtr->exStyle & FCF_BORDER) {
            /* Nominal border */
            wmPtr->xInParent = xBorder;
            wmPtr->yInParent = yBorder;
            wmPtr->borderWidth = 2 * xBorder;
            wmPtr->borderHeight = 2 * yBorder;
#ifdef DEBUG
            printf("FCF_BORDER: xIP %d, yIP %d, bW %d, bH %d\n",
                   wmPtr->xInParent, wmPtr->yInParent, wmPtr->borderWidth,
                   wmPtr->borderHeight);
#endif
        } else {
            /* No border style */
            wmPtr->xInParent = 0;
            wmPtr->yInParent = 0;
            wmPtr->borderWidth = 0;
            wmPtr->borderHeight = 0;
#ifdef DEBUG
            printf("NO border: xIP %d, yIP %d, bW %d, bH %d\n",
                   wmPtr->xInParent, wmPtr->yInParent, wmPtr->borderWidth,
                   wmPtr->borderHeight);
#endif
        }
        if (wmPtr->exStyle & FCF_TITLEBAR) {
            wmPtr->borderHeight += titleBar;
#ifdef DEBUG
            printf("FCF_TITLEBAR: bH now %d\n", wmPtr->borderHeight);
#endif
        }

        /*
         * Compute the geometry of the parent and child windows.
         */

        wmPtr->flags |= WM_CREATE_PENDING|WM_MOVE_PENDING;
        UpdateGeometryInfo((ClientData)winPtr);
        wmPtr->flags &= ~(WM_CREATE_PENDING|WM_MOVE_PENDING);

        width = wmPtr->borderWidth + winPtr->changes.width;
        height = wmPtr->borderHeight + winPtr->changes.height;
#ifdef DEBUG
        printf("+ height %d -> %d\n", winPtr->changes.height, height);
#endif

        /*
         * Set the initial position from the user or program specified
         * location.  If nothing has been specified, then let the system
         * pick a location.
         */

        if (!(wmPtr->sizeHintsFlags & (USPosition | PPosition))) {
            x = CW_USEDEFAULT;
            y = CW_USEDEFAULT;
            wmPtr->style |= FCF_SHELLPOSITION;
#ifdef DEBUG
            printf("Positioning: FCF_SHELLPOSITION\n");
#endif
        } else {
            x = winPtr->changes.x;
            y = winPtr->changes.y;
#ifdef DEBUG
            printf("Positioning: %d,%d\n", x, y);
#endif
        }

        /*
         * Create the containing window.
         */

        parentPtr = (TkOS2Drawable *) ckalloc(sizeof(TkOS2Drawable));
        parentPtr->type = TOD_WM_WINDOW;
        parentPtr->window.winPtr = winPtr;
        wmPtr->reparent = (Window)parentPtr;
#ifdef DEBUG
        printf("Reparent %x (hwnd %x)\n", parentPtr, child);
#endif

        fcdata.cb = sizeof(FRAMECDATA);
        fcdata.flCreateFlags = (ULONG)wmPtr->exStyle;
        fcdata.hmodResources = 0L;
        fcdata.idResources = 0;
	/* Create window with 0 size and pos, so frame will format controls */
#define ID_FRAME 1
	frame  = WinCreateWindow(
		HWND_DESKTOP,		/* Parent */
		WC_FRAME,		/* Class */
		wmPtr->titleUid,	/* Window text */
		wmPtr->style,		/* Style */
		x, yScreen - height - y,			/* Initial X and Y coordinates */
		width,			/* Width */
		height,			/* Height */
		NULLHANDLE,		/* Owner */
		HWND_TOP,		/* Insertbehind (sibling) */
		ID_FRAME,		/* Window ID */
		&fcdata,	/* Ptr to control data */
		NULL);			/* Ptr to presentation parameters */

#ifdef DEBUG
        printf("WinCreateWindow frame %x (%s) wmPtr %x exStyle %s style %s\n",
               frame, wmPtr->titleUid, wmPtr,
               wmPtr->exStyle == EX_TOPLEVEL_STYLE ? "EX_TOPLEVEL_STYLE" :
               (wmPtr->exStyle == EX_OVERRIDE_STYLE ? "EX_OVERRIDE_STYLE" :
               (wmPtr->exStyle == EX_TRANSIENT_STYLE ? "EX_TRANSIENT_STYLE"
                                                     : "unknown")),
               (WM_TOPLEVEL_STYLE == WM_OVERRIDE_STYLE
               || WM_TOPLEVEL_STYLE == WM_TRANSIENT_STYLE
               || WM_OVERRIDE_STYLE == WM_TRANSIENT_STYLE ) ? "ambiguous" :
               (wmPtr->style == WM_TOPLEVEL_STYLE ? "WM_TOPLEVEL_STYLE" :
               (wmPtr->style == WM_OVERRIDE_STYLE ? "WM_OVERRIDE_STYLE" :
               (wmPtr->style == WM_TRANSIENT_STYLE ? "WM_TRANSIENT_STYLE"
                                                   : "unknown"))));
#endif
        /* Force frame window to display the title bar, buttons, ... */
        WinSendMsg(frame, WM_UPDATEFRAME, (MPARAM)0, (MPARAM)0);

        /* Subclass Frame window */
        WinSetWindowULong(frame, QWL_USER, (ULONG)parentPtr);
        oldFrameProc = WinSubclassWindow(frame, TkOS2FrameProc);

        /*
         * Do WM_CREATE processing that wasn't possible because we couldn't
         * subclass the window before its creation...
         */
        /*
         * Add the window and handle to the window table.
         */

        parentPtr->window.handle = frame;
        hPtr = Tcl_CreateHashEntry(&windowTable, (char *)frame, &new);
        if (!new) {
            panic("Duplicate window handle: %p", frame);
        }
        Tcl_SetHashValue(hPtr, parentPtr);

#ifdef DEBUG
        printf("MapWindow(%x), fr%x, %d,%d (PM: %d), %dx%d\n", child, frame,
               x, y, yScreen - (y + height), width, height);
#endif

        /*
         * Now we need to reparent the contained window and set its
         * style appropriately.  Be sure to update the style first so that
         * PM doesn't try to set the focus to the child window.
         * PM windows are always children, no WS_CHILD
         */

	ret= WinSetParent(child, frame, TRUE);
#ifdef DEBUG
        if (ret != TRUE) {
            printf("WinSetParent c %x, p %x ERROR %x\n", child, frame,
                   WinGetLastError(hab));
        } else {
            printf("WinSetParent c %x, p %x OK\n", child, frame);
        }
        ret = WinQueryWindow(child, QW_PARENT);
        if (ret == NULLHANDLE) {
            printf("WinQueryWindow QW_PARENT c %x ERROR %x\n", child,
                   WinGetLastError(hab));
        } else {
            printf("WinQueryWindow QW_PARENT c %x : p %x OK\n", child, ret);
        }
        printf("    winPtr->changes.width %d, winPtr->changes.height %d\n",
               winPtr->changes.width, winPtr->changes.height);
        printf("    wmPtr->xInParent %d, wmPtr->yInParent %d\n",
               wmPtr->xInParent, wmPtr->yInParent);
#endif
        WinSetWindowPos(child, HWND_TOP, wmPtr->xInParent, wmPtr->yInParent,
                  winPtr->changes.width, winPtr->changes.height,
                  SWP_SIZE | SWP_MOVE | SWP_SHOW);

	/*
	 * Generate a reparent event.
	 */

	event.type = ReparentNotify;
	winPtr->display->request++;
	event.xreparent.serial = winPtr->display->request;
	event.xreparent.send_event = False;
#ifdef DEBUG
        printf("    display %x\n", winPtr->display);
#endif
	event.xreparent.display = winPtr->display;
	event.xreparent.event = winPtr->window;
	event.xreparent.window = winPtr->window;
	event.xreparent.parent = wmPtr->reparent;
	event.xreparent.x = wmPtr->xInParent;
	event.xreparent.y = wmPtr->yInParent;
	event.xreparent.override_redirect = winPtr->atts.override_redirect;
	Tk_QueueWindowEvent(&event, TCL_QUEUE_TAIL);
    } else if (wmPtr->hints.initial_state == WithdrawnState) {
	return;
    } else {
	if (wmPtr->flags & WM_UPDATE_PENDING) {
	    Tk_CancelIdleCall(UpdateGeometryInfo, (ClientData) winPtr);
	}
	UpdateGeometryInfo((ClientData) winPtr);

	/* If applicable, make visible in switch-list */
        if (wmPtr->exStyle & FCF_TASKLIST) {
            HSWITCH hSwitch;
            SWCNTRL switchData;

            hSwitch = WinQuerySwitchHandle(TkOS2GetHWND(wmPtr->reparent), 0);
#ifdef DEBUG
            if (hSwitch == NULLHANDLE) {
                printf("WinQuerySwitchHandle ERROR %x\n", WinGetLastError(hab));
            } else {
                printf("WinQuerySwitchHandle OK:%x\n", hSwitch);
            }
#endif
            rc = WinQuerySwitchEntry(hSwitch, &switchData);
#ifdef DEBUG
            if (rc != 0) {
                printf("WinQuerySwitchEntry ERROR %x\n", WinGetLastError(hab));
            } else {
                printf("WinQuerySwitchEntry %x OK\n", hSwitch);
            }
#endif
            /* Set visibility on */
            switchData.uchVisibility = SWL_VISIBLE;
            rc = WinChangeSwitchEntry(hSwitch, &switchData);
#ifdef DEBUG
            if (rc != 0) {
                printf("WinChangeSwitchEntry ERROR %x\n", WinGetLastError(hab));
            } else {
                printf("WinChangeSwitchEntry %x OK\n", hSwitch);
            }
#endif
        }
    }

    /*
     * Map the window in either the iconified or normal state.  Note that
     * we only send a map event if the window is in the normal state.
     */
	
    if (wmPtr->hints.initial_state == IconicState) {
	wmPtr->flags |= WM_SYNC_PENDING;
	rc = WinSetWindowPos(TkOS2GetHWND(wmPtr->reparent), HWND_TOP,
	                     0, 0, 0, 0, SWP_MINIMIZE);
#ifdef DEBUG
        if (rc!=TRUE) {
            printf("MapWindow: WinSetWindowPos ERROR %x\n",
                   WinGetLastError(hab));
        } else {
            printf("MapWindow: WinSetWindowPos OK\n");
        }
#endif
	wmPtr->flags &= ~WM_SYNC_PENDING;

	/*
	 * Send an unmap event if we are iconifying a currently displayed
	 * window.
	 */

	if (!wmPtr->withdrawn) {
	    XUnmapWindow(winPtr->display, winPtr->window);
	}
    } else if (wmPtr->hints.initial_state == WithdrawnState) {
	return;

    } else {
	XMapWindow(winPtr->display, winPtr->window);
	wmPtr->flags |= WM_SYNC_PENDING;
	/* Take care to also *raise* the window so we see it */
	rc = WinSetWindowPos(TkOS2GetHWND(wmPtr->reparent), HWND_TOP,
	                     0, 0, 0, 0,
	                     SWP_RESTORE | SWP_SHOW | SWP_ZORDER |SWP_ACTIVATE);
#ifdef DEBUG
        if (rc!=TRUE) { 
            printf("MapWindow %x: WinSetWindowPos ERROR %x\n",
                   TkOS2GetHWND(wmPtr->reparent), WinGetLastError(hab));
        } else {
            printf("MapWindow %x: WinSetWindowPos OK\n",
                   TkOS2GetHWND(wmPtr->reparent));
        }
#endif
	wmPtr->flags &= ~WM_SYNC_PENDING;
    }
}

/*
 *--------------------------------------------------------------
 *
 * TkWmUnmapWindow --
 *
 *	This procedure is invoked to unmap a top-level window.  The
 *	only thing it does special is unmap the decorative frame before
 *	unmapping the toplevel window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Unmaps the decorative frame and the window.
 *
 *--------------------------------------------------------------
 */

void
TkWmUnmapWindow(winPtr)
    TkWindow *winPtr;		/* Top-level window that's about to
				 * be unmapped. */
{
    register WmInfo *wmPtr = winPtr->wmInfoPtr;

#ifdef DEBUG
    printf("TkWmUnmapWindow, HWND(wmPtr->reparent) %x\n",
           TkOS2GetHWND(wmPtr->reparent));
#endif

    wmPtr->flags |= WM_SYNC_PENDING;
    rc = WinShowWindow(TkOS2GetHWND(wmPtr->reparent), FALSE);
#ifdef DEBUG
    if (rc != TRUE) {
        printf("WinShowWindow ERROR %x\n", WinGetLastError(hab));
    } else {
        printf("WinShowWindow %x OK\n", TkOS2GetHWND(wmPtr->reparent));
    }
#endif

    /* If applicable, remove from task list */
    if (wmPtr->exStyle & FCF_TASKLIST) {
        HSWITCH hSwitch;
        SWCNTRL switchData;
        hSwitch = WinQuerySwitchHandle(TkOS2GetHWND(wmPtr->reparent), 0);
#ifdef DEBUG
        if (hSwitch == NULLHANDLE) {
            printf("WinQuerySwitchHandle ERROR %x\n", WinGetLastError(hab));
        } else {
            printf("WinQuerySwitchHandle OK:%x\n", hSwitch);
        }
#endif
        rc = WinQuerySwitchEntry(hSwitch, &switchData);
#ifdef DEBUG
        if (rc != 0) {
            printf("WinQuerySwitchEntry ERROR %x\n", WinGetLastError(hab));
        } else {
            printf("WinQuerySwitchEntry %x OK\n", hSwitch);
        }
#endif
        /* Set visibility off */
        switchData.uchVisibility = SWL_INVISIBLE;
        rc = WinChangeSwitchEntry(hSwitch, &switchData);
#ifdef DEBUG
        if (rc != 0) {
            printf("WinQuerySwitchEntry ERROR %x\n", WinGetLastError(hab));
        } else {
            printf("WinQuerySwitchEntry %x OK\n", hSwitch);
        }
#endif
    }

    wmPtr->flags &= ~WM_SYNC_PENDING;
    XUnmapWindow(winPtr->display, winPtr->window);
}

/*
 *--------------------------------------------------------------
 *
 * TkWmDeadWindow --
 *
 *	This procedure is invoked when a top-level window is
 *	about to be deleted.  It cleans up the wm-related data
 *	structures for the window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The WmInfo structure for winPtr gets freed up.
 *
 *--------------------------------------------------------------
 */

void
TkWmDeadWindow(winPtr)
    TkWindow *winPtr;		/* Top-level window that's being deleted. */
{
    register WmInfo *wmPtr = winPtr->wmInfoPtr;
    WmInfo *wmPtr2;

    if (wmPtr == NULL) {
#ifdef DEBUG
        printf("TkWmDeadWindow, null wmPtr\n");
#endif
	return;
    }
#ifdef DEBUG
    printf("TkWmDeadWindow\n");
#endif

    /*
     * Clean up event related window info.
     */

    if (firstWmPtr == wmPtr) {
	firstWmPtr = wmPtr->nextPtr;
    } else {
	register WmInfo *prevPtr;

	for (prevPtr = firstWmPtr; ; prevPtr = prevPtr->nextPtr) {
	    if (prevPtr == NULL) {
		panic("couldn't unlink window in TkWmDeadWindow");
	    }
	    if (prevPtr->nextPtr == wmPtr) {
		prevPtr->nextPtr = wmPtr->nextPtr;
		break;
	    }
	}
    }
    if (wmPtr->hints.flags & IconPixmapHint) {
	Tk_FreeBitmap(winPtr->display, wmPtr->hints.icon_pixmap);
    }
    if (wmPtr->hints.flags & IconMaskHint) {
	Tk_FreeBitmap(winPtr->display, wmPtr->hints.icon_mask);
    }
    if (wmPtr->icon != NULL) {
	wmPtr2 = ((TkWindow *) wmPtr->icon)->wmInfoPtr;
	wmPtr2->iconFor = NULL;
	wmPtr2->withdrawn = 1;
    }
    if (wmPtr->iconFor != NULL) {
	wmPtr2 = ((TkWindow *) wmPtr->iconFor)->wmInfoPtr;
	wmPtr2->icon = NULL;
	wmPtr2->hints.flags &= ~IconWindowHint;
    }
    while (wmPtr->protPtr != NULL) {
	ProtocolHandler *protPtr;

	protPtr = wmPtr->protPtr;
	wmPtr->protPtr = protPtr->nextPtr;
	Tcl_EventuallyFree((ClientData) protPtr, TCL_DYNAMIC);
    }
    if (wmPtr->cmdArgv != NULL) {
	ckfree((char *) wmPtr->cmdArgv);
    }
    if (wmPtr->clientMachine != NULL) {
	ckfree((char *) wmPtr->clientMachine);
    }
    if (wmPtr->flags & WM_UPDATE_PENDING) {
	Tcl_CancelIdleCall(UpdateGeometryInfo, (ClientData) winPtr);
    }

    /*
     * Destroy the decorative frame window.  Note that the back pointer
     * to the child window must be cleared before calling WinDestroyWindow
     * to avoid generating events on a window that is already dead.
     * Note that we don't free the reparent here because it will be
     * freed when the WM_DESTROY message is processed.
     */

#ifdef DEBUG
    printf("TkWmDeadWindow reparent %x\n", wmPtr->reparent);
#endif
    if (wmPtr->reparent != None) {
	((TkOS2Drawable *) wmPtr->reparent)->window.winPtr = NULL;
#ifdef DEBUG
        printf("WM WinDestroyWindow %x\n", TkOS2GetHWND(wmPtr->reparent));
#endif
	WinDestroyWindow(TkOS2GetHWND(wmPtr->reparent));
    }
    ckfree((char *) wmPtr);
#ifdef DEBUG
    printf("TkWmDeadWindow freed wmPtr %x\n", wmPtr);
#endif
    winPtr->wmInfoPtr = NULL;
}

/*
 *--------------------------------------------------------------
 *
 * TkWmSetClass --
 *
 *	This procedure is invoked whenever a top-level window's
 *	class is changed.  If the window has been mapped then this
 *	procedure updates the window manager property for the
 *	class.  If the window hasn't been mapped, the update is
 *	deferred until just before the first mapping.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A window property may get updated.
 *
 *--------------------------------------------------------------
 */

void
TkWmSetClass(winPtr)
    TkWindow *winPtr;		/* Newly-created top-level window. */
{
    return;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_WmCmd --
 *
 *	This procedure is invoked to process the "wm" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tk_WmCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Tk_Window tkwin = (Tk_Window) clientData;
    TkWindow *winPtr;
    register WmInfo *wmPtr;
    int c;
    size_t length;

#ifdef DEBUG
    printf("Tk_WmCmd\n");
#endif

    if (argc < 2) {
	wrongNumArgs:
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " option window ?arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    c = argv[1][0];
    length = strlen(argv[1]);
    if ((c == 't') && (strncmp(argv[1], "tracing", length) == 0)
	    && (length >= 3)) {
	if ((argc != 2) && (argc != 3)) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " tracing ?boolean?\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 2) {
	    interp->result = (wmTracing) ? "on" : "off";
	    return TCL_OK;
	}
	return Tcl_GetBoolean(interp, argv[2], &wmTracing);
    }

    if (argc < 3) {
	goto wrongNumArgs;
    }
#ifdef DEBUG
    printf("   %s %s %s\n", argv[0], argv[1], argv[2]);
#endif
    winPtr = (TkWindow *) Tk_NameToWindow(interp, argv[2], tkwin);
    if (winPtr == NULL) {
	return TCL_ERROR;
    }
    if (!(winPtr->flags & TK_TOP_LEVEL)) {
	Tcl_AppendResult(interp, "window \"", winPtr->pathName,
		"\" isn't a top-level window", (char *) NULL);
	return TCL_ERROR;
    }
    wmPtr = winPtr->wmInfoPtr;
    if ((c == 'a') && (strncmp(argv[1], "aspect", length) == 0)) {
	int numer1, denom1, numer2, denom2;

	if ((argc != 3) && (argc != 7)) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " aspect window ?minNumer minDenom ",
		    "maxNumer maxDenom?\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    if (wmPtr->sizeHintsFlags & PAspect) {
		sprintf(interp->result, "%d %d %d %d", wmPtr->minAspect.x,
			wmPtr->minAspect.y, wmPtr->maxAspect.x,
			wmPtr->maxAspect.y);
	    }
	    return TCL_OK;
	}
	if (*argv[3] == '\0') {
	    wmPtr->sizeHintsFlags &= ~PAspect;
	} else {
	    if ((Tcl_GetInt(interp, argv[3], &numer1) != TCL_OK)
		    || (Tcl_GetInt(interp, argv[4], &denom1) != TCL_OK)
		    || (Tcl_GetInt(interp, argv[5], &numer2) != TCL_OK)
		    || (Tcl_GetInt(interp, argv[6], &denom2) != TCL_OK)) {
		return TCL_ERROR;
	    }
	    if ((numer1 <= 0) || (denom1 <= 0) || (numer2 <= 0) ||
		    (denom2 <= 0)) {
		interp->result = "aspect number can't be <= 0";
		return TCL_ERROR;
	    }
	    wmPtr->minAspect.x = numer1;
	    wmPtr->minAspect.y = denom1;
	    wmPtr->maxAspect.x = numer2;
	    wmPtr->maxAspect.y = denom2;
	    wmPtr->sizeHintsFlags |= PAspect;
	}
	goto updateGeom;
    } else if ((c == 'c') && (strncmp(argv[1], "client", length) == 0)
	    && (length >= 2)) {
	if ((argc != 3) && (argc != 4)) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " client window ?name?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    if (wmPtr->clientMachine != NULL) {
		interp->result = wmPtr->clientMachine;
	    }
	    return TCL_OK;
	}
	if (argv[3][0] == 0) {
	    if (wmPtr->clientMachine != NULL) {
		ckfree((char *) wmPtr->clientMachine);
		wmPtr->clientMachine = NULL;
		if (!(wmPtr->flags & WM_NEVER_MAPPED)) {
		    XDeleteProperty(winPtr->display, winPtr->window,
			    Tk_InternAtom((Tk_Window) winPtr,
			    "WM_CLIENT_MACHINE"));
		}
	    }
	    return TCL_OK;
	}
	if (wmPtr->clientMachine != NULL) {
	    ckfree((char *) wmPtr->clientMachine);
	}
	wmPtr->clientMachine = (char *)
		ckalloc((unsigned) (strlen(argv[3]) + 1));
	strcpy(wmPtr->clientMachine, argv[3]);
	if (!(wmPtr->flags & WM_NEVER_MAPPED)) {
	    XTextProperty textProp;
	    if (XStringListToTextProperty(&wmPtr->clientMachine, 1, &textProp)
		    != 0) {
		XSetWMClientMachine(winPtr->display, winPtr->window,
			&textProp);
		XFree((char *) textProp.value);
	    }
	}
    } else if ((c == 'c') && (strncmp(argv[1], "colormapwindows", length) == 0)
	    && (length >= 3)) {
	TkWindow **cmapList;
	TkWindow *winPtr2;
	int i, windowArgc, gotToplevel = 0;
	char **windowArgv;

	if ((argc != 3) && (argc != 4)) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " colormapwindows window ?windowList?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    Tk_MakeWindowExist((Tk_Window) winPtr);
	    for (i = 0; i < wmPtr->cmapCount; i++) {
		if ((i == (wmPtr->cmapCount-1))
			&& (wmPtr->flags & WM_ADDED_TOPLEVEL_COLORMAP)) {
		    break;
		}
		Tcl_AppendElement(interp, wmPtr->cmapList[i]->pathName);
	    }
	    return TCL_OK;
	}
	if (Tcl_SplitList(interp, argv[3], &windowArgc, &windowArgv)
		!= TCL_OK) {
	    return TCL_ERROR;
	}
	cmapList = (TkWindow **) ckalloc((unsigned)
		((windowArgc+1)*sizeof(TkWindow*)));
	for (i = 0; i < windowArgc; i++) {
	    winPtr2 = (TkWindow *) Tk_NameToWindow(interp, windowArgv[i],
		    tkwin);
	    if (winPtr2 == NULL) {
		ckfree((char *) cmapList);
		ckfree((char *) windowArgv);
		return TCL_ERROR;
	    }
	    if (winPtr2 == winPtr) {
		gotToplevel = 1;
	    }
	    if (winPtr2->window == None) {
		Tk_MakeWindowExist((Tk_Window) winPtr2);
	    }
	    cmapList[i] = winPtr2;
	}
	if (!gotToplevel) {
	    wmPtr->flags |= WM_ADDED_TOPLEVEL_COLORMAP;
	    cmapList[windowArgc] = winPtr;
	    windowArgc++;
	} else {
	    wmPtr->flags &= ~WM_ADDED_TOPLEVEL_COLORMAP;
	}
	wmPtr->flags |= WM_COLORMAPS_EXPLICIT;
	if (wmPtr->cmapList != NULL) {
	    ckfree((char *)wmPtr->cmapList);
	}
	wmPtr->cmapList = cmapList;
	wmPtr->cmapCount = windowArgc;
	ckfree((char *) windowArgv);

	/*
	 * Now we need to force the updated colormaps to be installed.
	 */

        if (wmPtr == foregroundWmPtr) {
            TkOS2WmInstallColormaps(TkOS2GetHWND(wmPtr->reparent),
              /* WM_QUERYNEWPALETTE -> WM_REALIZEPALETTE + focus notification */
                    WM_REALIZEPALETTE, 1);
        } else {
            if ((TkOS2Drawable *)wmPtr->reparent != (TkOS2Drawable *)NULL) {
                TkOS2WmInstallColormaps(TkOS2GetHWND(wmPtr->reparent),
               /* WM_PALETTECHANGED -> WM_REALIZEPALETTE + focus notification */
                                        WM_REALIZEPALETTE, 0);
            }
        }
	return TCL_OK;
    } else if ((c == 'c') && (strncmp(argv[1], "command", length) == 0)
	    && (length >= 3)) {
	int cmdArgc;
	char **cmdArgv;

	if ((argc != 3) && (argc != 4)) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " command window ?value?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    if (wmPtr->cmdArgv != NULL) {
		interp->result = Tcl_Merge(wmPtr->cmdArgc, wmPtr->cmdArgv);
		interp->freeProc = TCL_DYNAMIC;
	    }
	    return TCL_OK;
	}
	if (argv[3][0] == 0) {
	    if (wmPtr->cmdArgv != NULL) {
		ckfree((char *) wmPtr->cmdArgv);
		wmPtr->cmdArgv = NULL;
		if (!(wmPtr->flags & WM_NEVER_MAPPED)) {
		    XDeleteProperty(winPtr->display, winPtr->window,
			    Tk_InternAtom((Tk_Window) winPtr, "WM_COMMAND"));
		}
	    }
	    return TCL_OK;
	}
	if (Tcl_SplitList(interp, argv[3], &cmdArgc, &cmdArgv) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (wmPtr->cmdArgv != NULL) {
	    ckfree((char *) wmPtr->cmdArgv);
	}
	wmPtr->cmdArgc = cmdArgc;
	wmPtr->cmdArgv = cmdArgv;
	if (!(wmPtr->flags & WM_NEVER_MAPPED)) {
	    XSetCommand(winPtr->display, winPtr->window, cmdArgv, cmdArgc);
	}
    } else if ((c == 'd') && (strncmp(argv[1], "deiconify", length) == 0)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " deiconify window\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (wmPtr->iconFor != NULL) {
	    Tcl_AppendResult(interp, "can't deiconify ", argv[2],
		    ": it is an icon for ", winPtr->pathName, (char *) NULL);
	    return TCL_ERROR;
	}
	DeiconifyWindow(winPtr);
    } else if ((c == 'f') && (strncmp(argv[1], "focusmodel", length) == 0)
	    && (length >= 2)) {
	if ((argc != 3) && (argc != 4)) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " focusmodel window ?active|passive?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    interp->result = wmPtr->hints.input ? "passive" : "active";
	    return TCL_OK;
	}
	c = argv[3][0];
	length = strlen(argv[3]);
	if ((c == 'a') && (strncmp(argv[3], "active", length) == 0)) {
	    wmPtr->hints.input = False;
	} else if ((c == 'p') && (strncmp(argv[3], "passive", length) == 0)) {
	    wmPtr->hints.input = True;
	} else {
	    Tcl_AppendResult(interp, "bad argument \"", argv[3],
		    "\": must be active or passive", (char *) NULL);
	    return TCL_ERROR;
	}
    } else if ((c == 'f') && (strncmp(argv[1], "frame", length) == 0)
	    && (length >= 2)) {
	Window window;

	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " frame window\"", (char *) NULL);
	    return TCL_ERROR;
	}
	window = wmPtr->reparent;
	if (window == None) {
	    window = Tk_WindowId((Tk_Window) winPtr);
	}
	sprintf(interp->result, "0x%x", (unsigned int) window);
    } else if ((c == 'g') && (strncmp(argv[1], "geometry", length) == 0)
	    && (length >= 2)) {
	char xSign, ySign;
	int width, height;

	if ((argc != 3) && (argc != 4)) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " geometry window ?newGeometry?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    xSign = (wmPtr->flags & WM_NEGATIVE_X) ? '-' : '+';
	    ySign = (wmPtr->flags & WM_NEGATIVE_Y) ? '-' : '+';
	    if (wmPtr->gridWin != NULL) {
		width = wmPtr->reqGridWidth + (winPtr->changes.width
			- winPtr->reqWidth)/wmPtr->widthInc;
		height = wmPtr->reqGridHeight + (winPtr->changes.height
			- winPtr->reqHeight)/wmPtr->heightInc;
	    } else {
		width = winPtr->changes.width;
		height = winPtr->changes.height;
	    }
	    sprintf(interp->result, "%dx%d%c%d%c%d", width, height,
		    xSign, wmPtr->x, ySign, wmPtr->y);
	    return TCL_OK;
	}
	if (*argv[3] == '\0') {
	    wmPtr->width = -1;
	    wmPtr->height = -1;
	    goto updateGeom;
	}
	return ParseGeometry(interp, argv[3], winPtr);
    } else if ((c == 'g') && (strncmp(argv[1], "grid", length) == 0)
	    && (length >= 3)) {
	int reqWidth, reqHeight, widthInc, heightInc;

	if ((argc != 3) && (argc != 7)) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " grid window ?baseWidth baseHeight ",
		    "widthInc heightInc?\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    if (wmPtr->sizeHintsFlags & PBaseSize) {
		sprintf(interp->result, "%d %d %d %d", wmPtr->reqGridWidth,
			wmPtr->reqGridHeight, wmPtr->widthInc,
			wmPtr->heightInc);
	    }
	    return TCL_OK;
	}
	if (*argv[3] == '\0') {
	    /*
	     * Turn off gridding and reset the width and height
	     * to make sense as ungridded numbers.
	     */

	    wmPtr->sizeHintsFlags &= ~(PBaseSize|PResizeInc);
	    if (wmPtr->width != -1) {
		wmPtr->width = winPtr->reqWidth + (wmPtr->width
			- wmPtr->reqGridWidth)*wmPtr->widthInc;
		wmPtr->height = winPtr->reqHeight + (wmPtr->height
			- wmPtr->reqGridHeight)*wmPtr->heightInc;
	    }
	    wmPtr->widthInc = 1;
	    wmPtr->heightInc = 1;
	} else {
	    if ((Tcl_GetInt(interp, argv[3], &reqWidth) != TCL_OK)
		    || (Tcl_GetInt(interp, argv[4], &reqHeight) != TCL_OK)
		    || (Tcl_GetInt(interp, argv[5], &widthInc) != TCL_OK)
		    || (Tcl_GetInt(interp, argv[6], &heightInc) != TCL_OK)) {
		return TCL_ERROR;
	    }
	    if (reqWidth < 0) {
		interp->result = "baseWidth can't be < 0";
		return TCL_ERROR;
	    }
	    if (reqHeight < 0) {
		interp->result = "baseHeight can't be < 0";
		return TCL_ERROR;
	    }
	    if (widthInc < 0) {
		interp->result = "widthInc can't be < 0";
		return TCL_ERROR;
	    }
	    if (heightInc < 0) {
		interp->result = "heightInc can't be < 0";
		return TCL_ERROR;
	    }
	    Tk_SetGrid((Tk_Window) winPtr, reqWidth, reqHeight, widthInc,
		    heightInc);
	}
	goto updateGeom;
    } else if ((c == 'g') && (strncmp(argv[1], "group", length) == 0)
	    && (length >= 3)) {
	Tk_Window tkwin2;

	if ((argc != 3) && (argc != 4)) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " group window ?pathName?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    if (wmPtr->hints.flags & WindowGroupHint) {
		interp->result = wmPtr->leaderName;
	    }
	    return TCL_OK;
	}
	if (*argv[3] == '\0') {
	    wmPtr->hints.flags &= ~WindowGroupHint;
	    wmPtr->leaderName = NULL;
	} else {
	    tkwin2 = Tk_NameToWindow(interp, argv[3], tkwin);
	    if (tkwin2 == NULL) {
		return TCL_ERROR;
	    }
	    Tk_MakeWindowExist(tkwin2);
	    wmPtr->hints.window_group = Tk_WindowId(tkwin2);
	    wmPtr->hints.flags |= WindowGroupHint;
	    wmPtr->leaderName = Tk_PathName(tkwin2);
	}
    } else if ((c == 'i') && (strncmp(argv[1], "iconbitmap", length) == 0)
	    && (length >= 5)) {
	Pixmap pixmap;

	if ((argc != 3) && (argc != 4)) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " iconbitmap window ?bitmap?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    if (wmPtr->hints.flags & IconPixmapHint) {
		interp->result = Tk_NameOfBitmap(winPtr->display,
			wmPtr->hints.icon_pixmap);
	    }
	    return TCL_OK;
	}
	if (*argv[3] == '\0') {
	    if (wmPtr->hints.icon_pixmap != None) {
		Tk_FreeBitmap(winPtr->display, wmPtr->hints.icon_pixmap);
	    }
	    wmPtr->hints.flags &= ~IconPixmapHint;
	} else {
	    pixmap = Tk_GetBitmap(interp, (Tk_Window) winPtr,
		    Tk_GetUid(argv[3]));
	    if (pixmap == None) {
		return TCL_ERROR;
	    }
	    wmPtr->hints.icon_pixmap = pixmap;
	    wmPtr->hints.flags |= IconPixmapHint;
	}
    } else if ((c == 'i') && (strncmp(argv[1], "iconify", length) == 0)
	    && (length >= 5)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " iconify window\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (Tk_Attributes((Tk_Window) winPtr)->override_redirect) {
	    Tcl_AppendResult(interp, "can't iconify \"", winPtr->pathName,
		    "\": override-redirect flag is set", (char *) NULL);
	    return TCL_ERROR;
	}
	if (wmPtr->master != None) {
	    Tcl_AppendResult(interp, "can't iconify \"", winPtr->pathName,
		    "\": it is a transient", (char *) NULL);
	    return TCL_ERROR;
	}
	if (wmPtr->iconFor != NULL) {
	    Tcl_AppendResult(interp, "can't iconify ", argv[2],
		    ": it is an icon for ", winPtr->pathName, (char *) NULL);
	    return TCL_ERROR;
	}
	IconifyWindow(winPtr);
    } else if ((c == 'i') && (strncmp(argv[1], "iconmask", length) == 0)
	    && (length >= 5)) {
	Pixmap pixmap;

	if ((argc != 3) && (argc != 4)) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " iconmask window ?bitmap?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    if (wmPtr->hints.flags & IconMaskHint) {
		interp->result = Tk_NameOfBitmap(winPtr->display,
			wmPtr->hints.icon_mask);
	    }
	    return TCL_OK;
	}
	if (*argv[3] == '\0') {
	    if (wmPtr->hints.icon_mask != None) {
		Tk_FreeBitmap(winPtr->display, wmPtr->hints.icon_mask);
	    }
	    wmPtr->hints.flags &= ~IconMaskHint;
	} else {
	    pixmap = Tk_GetBitmap(interp, tkwin, Tk_GetUid(argv[3]));
	    if (pixmap == None) {
		return TCL_ERROR;
	    }
	    wmPtr->hints.icon_mask = pixmap;
	    wmPtr->hints.flags |= IconMaskHint;
	}
    } else if ((c == 'i') && (strncmp(argv[1], "iconname", length) == 0)
	    && (length >= 5)) {
	if (argc > 4) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " iconname window ?newName?\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    interp->result = (wmPtr->iconName != NULL) ? wmPtr->iconName : "";
	    return TCL_OK;
	} else {
	    wmPtr->iconName = Tk_GetUid(argv[3]);
	    if (!(wmPtr->flags & WM_NEVER_MAPPED)) {
		XSetIconName(winPtr->display, winPtr->window, wmPtr->iconName);
	    }
	}
    } else if ((c == 'i') && (strncmp(argv[1], "iconposition", length) == 0)
	    && (length >= 5)) {
	int x, y;

	if ((argc != 3) && (argc != 5)) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " iconposition window ?x y?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    if (wmPtr->hints.flags & IconPositionHint) {
		sprintf(interp->result, "%d %d", wmPtr->hints.icon_x,
			wmPtr->hints.icon_y);
	    }
	    return TCL_OK;
	}
	if (*argv[3] == '\0') {
	    wmPtr->hints.flags &= ~IconPositionHint;
	} else {
	    if ((Tcl_GetInt(interp, argv[3], &x) != TCL_OK)
		    || (Tcl_GetInt(interp, argv[4], &y) != TCL_OK)){
		return TCL_ERROR;
	    }
	    wmPtr->hints.icon_x = x;
	    wmPtr->hints.icon_y = y;
	    wmPtr->hints.flags |= IconPositionHint;
	}
    } else if ((c == 'i') && (strncmp(argv[1], "iconwindow", length) == 0)
	    && (length >= 5)) {
	Tk_Window tkwin2;
	WmInfo *wmPtr2;
        XSetWindowAttributes atts;

	if ((argc != 3) && (argc != 4)) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " iconwindow window ?pathName?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    if (wmPtr->icon != NULL) {
		interp->result = Tk_PathName(wmPtr->icon);
	    }
	    return TCL_OK;
	}
	if (*argv[3] == '\0') {
	    wmPtr->hints.flags &= ~IconWindowHint;
	    if (wmPtr->icon != NULL) {
                /*
                 * Let the window use button events again, then remove
                 * it as icon window.
                 */

                atts.event_mask = Tk_Attributes(wmPtr->icon)->event_mask
                        | ButtonPressMask;
                Tk_ChangeWindowAttributes(wmPtr->icon, CWEventMask, &atts);
		wmPtr2 = ((TkWindow *) wmPtr->icon)->wmInfoPtr;
		wmPtr2->iconFor = NULL;
		wmPtr2->withdrawn = 1;
		wmPtr2->hints.initial_state = WithdrawnState;
	    }
	    wmPtr->icon = NULL;
	} else {
	    tkwin2 = Tk_NameToWindow(interp, argv[3], tkwin);
	    if (tkwin2 == NULL) {
		return TCL_ERROR;
	    }
	    if (!Tk_IsTopLevel(tkwin2)) {
		Tcl_AppendResult(interp, "can't use ", argv[3],
			" as icon window: not at top level", (char *) NULL);
		return TCL_ERROR;
	    }
	    wmPtr2 = ((TkWindow *) tkwin2)->wmInfoPtr;
	    if (wmPtr2->iconFor != NULL) {
		Tcl_AppendResult(interp, argv[3], " is already an icon for ",
			Tk_PathName(wmPtr2->iconFor), (char *) NULL);
		return TCL_ERROR;
	    }
	    if (wmPtr->icon != NULL) {
		WmInfo *wmPtr3 = ((TkWindow *) wmPtr->icon)->wmInfoPtr;
		wmPtr3->iconFor = NULL;
		wmPtr3->withdrawn = 1;

                /*
                 * Let the window use button events again.
                 */

                atts.event_mask = Tk_Attributes(wmPtr->icon)->event_mask
                        | ButtonPressMask;
                Tk_ChangeWindowAttributes(wmPtr->icon, CWEventMask, &atts);
	    }

            /*
             * Disable button events in the icon window:  some window
             * managers (like olvwm) want to get the events themselves,
             * but X only allows one application at a time to receive
             * button events for a window.
             */

            atts.event_mask = Tk_Attributes(tkwin2)->event_mask
                    & ~ButtonPressMask;
            Tk_ChangeWindowAttributes(tkwin2, CWEventMask, &atts);
	    Tk_MakeWindowExist(tkwin2);
	    wmPtr->hints.icon_window = Tk_WindowId(tkwin2);
	    wmPtr->hints.flags |= IconWindowHint;
	    wmPtr->icon = tkwin2;
	    wmPtr2->iconFor = (Tk_Window) winPtr;
	    if (!wmPtr2->withdrawn && !(wmPtr2->flags & WM_NEVER_MAPPED)) {
		wmPtr2->withdrawn = 0;
		if (XWithdrawWindow(Tk_Display(tkwin2), Tk_WindowId(tkwin2),
			Tk_ScreenNumber(tkwin2)) == 0) {
		    interp->result =
			    "couldn't send withdraw message to window manager";
#ifdef DEBUG
                    printf("calling XWithdrawWindow(%x, %x, %d)\n",
                           Tk_Display(tkwin2), Tk_WindowId(tkwin2),
                           Tk_ScreenNumber(tkwin2));
#endif
		    return TCL_ERROR;
		}
	    }
	}
    } else if ((c == 'm') && (strncmp(argv[1], "maxsize", length) == 0)
	    && (length >= 2)) {
	int width, height;
	if ((argc != 3) && (argc != 5)) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " maxsize window ?width height?\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
            GetMaxSize(wmPtr, &width, &height);
	    sprintf(interp->result, "%d %d", width, height);
	    return TCL_OK;
	}
	if ((Tcl_GetInt(interp, argv[3], &width) != TCL_OK)
		|| (Tcl_GetInt(interp, argv[4], &height) != TCL_OK)) {
	    return TCL_ERROR;
	}
	wmPtr->maxWidth = width;
	wmPtr->maxHeight = height;
	goto updateGeom;
    } else if ((c == 'm') && (strncmp(argv[1], "minsize", length) == 0)
	    && (length >= 2)) {
	int width, height;
	if ((argc != 3) && (argc != 5)) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " minsize window ?width height?\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
            GetMinSize(wmPtr, &width, &height);
            sprintf(interp->result, "%d %d", width, height);
	    return TCL_OK;
	}
	if ((Tcl_GetInt(interp, argv[3], &width) != TCL_OK)
		|| (Tcl_GetInt(interp, argv[4], &height) != TCL_OK)) {
	    return TCL_ERROR;
	}
	wmPtr->minWidth = width;
	wmPtr->minHeight = height;
	goto updateGeom;
    } else if ((c == 'o')
	    && (strncmp(argv[1], "overrideredirect", length) == 0)) {
	int boolean;
	XSetWindowAttributes atts;

	if ((argc != 3) && (argc != 4)) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " overrideredirect window ?boolean?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    if (Tk_Attributes((Tk_Window) winPtr)->override_redirect) {
		interp->result = "1";
	    } else {
		interp->result = "0";
	    }
	    return TCL_OK;
	}
	if (Tcl_GetBoolean(interp, argv[3], &boolean) != TCL_OK) {
	    return TCL_ERROR;
	}
	atts.override_redirect = (boolean) ? True : False;
	Tk_ChangeWindowAttributes((Tk_Window) winPtr, CWOverrideRedirect,
		&atts);
    } else if ((c == 'p') && (strncmp(argv[1], "positionfrom", length) == 0)
	    && (length >= 2)) {
	if ((argc != 3) && (argc != 4)) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " positionfrom window ?user/program?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    if (wmPtr->sizeHintsFlags & USPosition) {
		interp->result = "user";
	    } else if (wmPtr->sizeHintsFlags & PPosition) {
		interp->result = "program";
	    }
	    return TCL_OK;
	}
	if (*argv[3] == '\0') {
	    wmPtr->sizeHintsFlags &= ~(USPosition|PPosition);
	} else {
	    c = argv[3][0];
	    length = strlen(argv[3]);
	    if ((c == 'u') && (strncmp(argv[3], "user", length) == 0)) {
		wmPtr->sizeHintsFlags &= ~PPosition;
		wmPtr->sizeHintsFlags |= USPosition;
	    } else if ((c == 'p') && (strncmp(argv[3], "program", length) == 0)) {
		wmPtr->sizeHintsFlags &= ~USPosition;
		wmPtr->sizeHintsFlags |= PPosition;
	    } else {
		Tcl_AppendResult(interp, "bad argument \"", argv[3],
			"\": must be program or user", (char *) NULL);
		return TCL_ERROR;
	    }
	}
	goto updateGeom;
    } else if ((c == 'p') && (strncmp(argv[1], "protocol", length) == 0)
	    && (length >= 2)) {
	register ProtocolHandler *protPtr, *prevPtr;
	Atom protocol;
	int cmdLength;

	if ((argc < 3) || (argc > 5)) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " protocol window ?name? ?command?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    /*
	     * Return a list of all defined protocols for the window.
	     */
	    for (protPtr = wmPtr->protPtr; protPtr != NULL;
		    protPtr = protPtr->nextPtr) {
		Tcl_AppendElement(interp,
			Tk_GetAtomName((Tk_Window) winPtr, protPtr->protocol));
	    }
	    return TCL_OK;
	}
	protocol = Tk_InternAtom((Tk_Window) winPtr, argv[3]);
	if (argc == 4) {
	    /*
	     * Return the command to handle a given protocol.
	     */
	    for (protPtr = wmPtr->protPtr; protPtr != NULL;
		    protPtr = protPtr->nextPtr) {
		if (protPtr->protocol == protocol) {
		    interp->result = protPtr->command;
		    return TCL_OK;
		}
	    }
	    return TCL_OK;
	}

	/*
	 * Delete any current protocol handler, then create a new
	 * one with the specified command, unless the command is
	 * empty.
	 */

	for (protPtr = wmPtr->protPtr, prevPtr = NULL; protPtr != NULL;
		prevPtr = protPtr, protPtr = protPtr->nextPtr) {
	    if (protPtr->protocol == protocol) {
		if (prevPtr == NULL) {
		    wmPtr->protPtr = protPtr->nextPtr;
		} else {
		    prevPtr->nextPtr = protPtr->nextPtr;
		}
		Tcl_EventuallyFree((ClientData) protPtr, TCL_DYNAMIC);
		break;
	    }
	}
	cmdLength = strlen(argv[4]);
	if (cmdLength > 0) {
	    protPtr = (ProtocolHandler *) ckalloc(HANDLER_SIZE(cmdLength));
	    protPtr->protocol = protocol;
	    protPtr->nextPtr = wmPtr->protPtr;
	    wmPtr->protPtr = protPtr;
	    protPtr->interp = interp;
	    strcpy(protPtr->command, argv[4]);
	}
    } else if ((c == 'r') && (strncmp(argv[1], "resizable", length) == 0)) {
        int width, height;

        if ((argc != 3) && (argc != 5)) {
            Tcl_AppendResult(interp, "wrong # arguments: must be \"",
                    argv[0], " resizable window ?width height?\"",
                    (char *) NULL);
            return TCL_ERROR;
        }
        if (argc == 3) {
            sprintf(interp->result, "%d %d",
                    (wmPtr->flags  & WM_WIDTH_NOT_RESIZABLE) ? 0 : 1,
                    (wmPtr->flags  & WM_HEIGHT_NOT_RESIZABLE) ? 0 : 1);
            return TCL_OK;
        }
        if ((Tcl_GetBoolean(interp, argv[3], &width) != TCL_OK)
                || (Tcl_GetBoolean(interp, argv[4], &height) != TCL_OK)) {
            return TCL_ERROR;
        }
        if (width) {
            wmPtr->flags &= ~WM_WIDTH_NOT_RESIZABLE;
        } else {
            wmPtr->flags |= WM_WIDTH_NOT_RESIZABLE;
        }
        if (height) {
            wmPtr->flags &= ~WM_HEIGHT_NOT_RESIZABLE;
        } else {
            wmPtr->flags |= WM_HEIGHT_NOT_RESIZABLE;
        }
        wmPtr->flags |= WM_UPDATE_SIZE_HINTS;
        goto updateGeom;
    } else if ((c == 's') && (strncmp(argv[1], "sizefrom", length) == 0)
	    && (length >= 2)) {
	if ((argc != 3) && (argc != 4)) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " sizefrom window ?user|program?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    if (wmPtr->sizeHintsFlags & USSize) {
		interp->result = "user";
	    } else if (wmPtr->sizeHintsFlags & PSize) {
		interp->result = "program";
	    }
	    return TCL_OK;
	}
	if (*argv[3] == '\0') {
	    wmPtr->sizeHintsFlags &= ~(USSize|PSize);
	} else {
	    c = argv[3][0];
	    length = strlen(argv[3]);
	    if ((c == 'u') && (strncmp(argv[3], "user", length) == 0)) {
		wmPtr->sizeHintsFlags &= ~PSize;
		wmPtr->sizeHintsFlags |= USSize;
	    } else if ((c == 'p')
		    && (strncmp(argv[3], "program", length) == 0)) {
		wmPtr->sizeHintsFlags &= ~USSize;
		wmPtr->sizeHintsFlags |= PSize;
	    } else {
		Tcl_AppendResult(interp, "bad argument \"", argv[3],
			"\": must be program or user", (char *) NULL);
		return TCL_ERROR;
	    }
	}
	goto updateGeom;
    } else if ((c == 's') && (strncmp(argv[1], "state", length) == 0)
	    && (length >= 2)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " state window\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (wmPtr->iconFor != NULL) {
	    interp->result = "icon";
	} else if (wmPtr->withdrawn) {
	    interp->result = "withdrawn";
	} else if (Tk_IsMapped((Tk_Window) winPtr)
		|| ((wmPtr->flags & WM_NEVER_MAPPED)
		&& (wmPtr->hints.initial_state == NormalState))) {
	    interp->result = "normal";
	} else {
	    interp->result = "iconic";
	}
    } else if ((c == 't') && (strncmp(argv[1], "title", length) == 0)
	    && (length >= 2)) {
	if (argc > 4) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " title window ?newTitle?\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    interp->result = (wmPtr->titleUid != NULL) ? wmPtr->titleUid
		    : winPtr->nameUid;
	    return TCL_OK;
	} else {
	    wmPtr->titleUid = Tk_GetUid(argv[3]);
	    if ((!(wmPtr->flags & WM_NEVER_MAPPED)) &&
	            wmPtr->reparent != None) {
                HSWITCH hSwitch;
                SWCNTRL switchData;

                hSwitch = WinQuerySwitchHandle(TkOS2GetHWND(wmPtr->reparent), 0);
#ifdef DEBUG
                if (hSwitch == NULLHANDLE) {
                    printf("WinQuerySwitchHandle ERROR %x\n", WinGetLastError(hab));
                } else {
                    printf("WinQuerySwitchHandle OK:%x\n", hSwitch);
                }
#endif
                rc = WinQuerySwitchEntry(hSwitch, &switchData);
#ifdef DEBUG
                if (rc != 0) {
                    printf("WinQuerySwitchEntry ERROR %x\n", WinGetLastError(hab));
                } else {
                    printf("WinQuerySwitchEntry %x OK\n", hSwitch);
                }
#endif
                /* Set visibility on */
                strncpy(switchData.szSwtitle, wmPtr->titleUid, MAXNAMEL+4);
                rc = WinChangeSwitchEntry(hSwitch, &switchData);
#ifdef DEBUG
                if (rc != 0) {
                    printf("WinChangeSwitchEntry ERROR %x\n", WinGetLastError(hab));
                } else {
                    printf("WinChangeSwitchEntry %x OK\n", hSwitch);
                }
#endif
		/*
		*/
		WinSetWindowText(TkOS2GetHWND(wmPtr->reparent), wmPtr->titleUid);
	    }
	}
    } else if ((c == 't') && (strncmp(argv[1], "transient", length) == 0)
	    && (length >= 3)) {
	Tk_Window master;

	if ((argc != 3) && (argc != 4)) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " transient window ?master?\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    if (wmPtr->master != None) {
		interp->result = wmPtr->masterWindowName;
	    }
	    return TCL_OK;
	}
	if (argv[3][0] == '\0') {
	    wmPtr->master = None;
	    wmPtr->masterWindowName = NULL;
	} else {
	    master = Tk_NameToWindow(interp, argv[3], tkwin);
	    if (master == NULL) {
		return TCL_ERROR;
	    }
	    Tk_MakeWindowExist(master);
	    wmPtr->master = Tk_WindowId(master);
	    wmPtr->masterWindowName = Tk_PathName(master);
	}
	if (!(wmPtr->flags & WM_NEVER_MAPPED)) {
	    XSetTransientForHint(winPtr->display, winPtr->window,
		    wmPtr->master);
	}
    } else if ((c == 'w') && (strncmp(argv[1], "withdraw", length) == 0)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " withdraw window\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (wmPtr->iconFor != NULL) {
	    Tcl_AppendResult(interp, "can't withdraw ", argv[2],
		    ": it is an icon for ", Tk_PathName(wmPtr->iconFor),
		    (char *) NULL);
	    return TCL_ERROR;
	}
	wmPtr->hints.initial_state = WithdrawnState;
	wmPtr->withdrawn = 1;
	if (wmPtr->flags & WM_NEVER_MAPPED) {
	    return TCL_OK;
	}

	TkWmUnmapWindow(winPtr);
    } else {
	Tcl_AppendResult(interp, "unknown or ambiguous option \"", argv[1],
		"\": must be aspect, client, command, deiconify, ",
		"focusmodel, frame, geometry, grid, group, iconbitmap, ",
		"iconify, iconmask, iconname, iconposition, ",
		"iconwindow, maxsize, minsize, overrideredirect, ",
		"positionfrom, protocol, resizable, sizefrom, state, title, ",
		"tracing, transient, or withdraw",
		(char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;

    updateGeom:
    if (!(wmPtr->flags & (WM_UPDATE_PENDING|WM_NEVER_MAPPED))) {
	Tcl_DoWhenIdle(UpdateGeometryInfo, (ClientData) winPtr);
	wmPtr->flags |= WM_UPDATE_PENDING;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_SetGrid --
 *
 *	This procedure is invoked by a widget when it wishes to set a grid
 *	coordinate system that controls the size of a top-level window.
 *	It provides a C interface equivalent to the "wm grid" command and
 *	is usually asscoiated with the -setgrid option.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Grid-related information will be passed to the window manager, so
 *	that the top-level window associated with tkwin will resize on
 *	even grid units.  If some other window already controls gridding
 *	for the top-level window then this procedure call has no effect.
 *
 *----------------------------------------------------------------------
 */

void
Tk_SetGrid(tkwin, reqWidth, reqHeight, widthInc, heightInc)
    Tk_Window tkwin;		/* Token for window.  New window mgr info
				 * will be posted for the top-level window
				 * associated with this window. */
    int reqWidth;		/* Width (in grid units) corresponding to
				 * the requested geometry for tkwin. */
    int reqHeight;		/* Height (in grid units) corresponding to
				 * the requested geometry for tkwin. */
    int widthInc, heightInc;	/* Pixel increments corresponding to a
				 * change of one grid unit. */
{
    TkWindow *winPtr = (TkWindow *) tkwin;
    register WmInfo *wmPtr;

#ifdef DEBUG
    printf("Tk_SetGrid\n");
#endif

    /*
     * Find the top-level window for tkwin, plus the window manager
     * information.
     */

    while (!(winPtr->flags & TK_TOP_LEVEL)) {
	winPtr = winPtr->parentPtr;
    }
    wmPtr = winPtr->wmInfoPtr;

    if ((wmPtr->gridWin != NULL) && (wmPtr->gridWin != tkwin)) {
	return;
    }

    if ((wmPtr->reqGridWidth == reqWidth)
	    && (wmPtr->reqGridHeight == reqHeight)
	    && (wmPtr->widthInc == widthInc)
	    && (wmPtr->heightInc == heightInc)
	    && ((wmPtr->sizeHintsFlags & (PBaseSize|PResizeInc))
		    == (PBaseSize|PResizeInc))) {
	return;
    }

    /*
     * If gridding was previously off, then forget about any window
     * size requests made by the user or via "wm geometry":  these are
     * in pixel units and there's no easy way to translate them to
     * grid units since the new requested size of the top-level window in
     * pixels may not yet have been registered yet (it may filter up
     * the hierarchy in DoWhenIdle handlers).  However, if the window
     * has never been mapped yet then just leave the window size alone:
     * assume that it is intended to be in grid units but just happened
     * to have been specified before this procedure was called.
     */

    if ((wmPtr->gridWin == NULL) && !(wmPtr->flags & WM_NEVER_MAPPED)) {
	wmPtr->width = -1;
	wmPtr->height = -1;
    }

    /* 
     * Set the new gridding information, and start the process of passing
     * all of this information to the window manager.
     */

    wmPtr->gridWin = tkwin;
    wmPtr->reqGridWidth = reqWidth;
    wmPtr->reqGridHeight = reqHeight;
    wmPtr->widthInc = widthInc;
    wmPtr->heightInc = heightInc;
    wmPtr->sizeHintsFlags |= PBaseSize|PResizeInc;
    if (!(wmPtr->flags & (WM_UPDATE_PENDING|WM_NEVER_MAPPED))) {
	Tcl_DoWhenIdle(UpdateGeometryInfo, (ClientData) winPtr);
	wmPtr->flags |= WM_UPDATE_PENDING;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_UnsetGrid --
 *
 *	This procedure cancels the effect of a previous call
 *	to Tk_SetGrid.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If tkwin currently controls gridding for its top-level window,
 *	gridding is cancelled for that top-level window;  if some other
 *	window controls gridding then this procedure has no effect.
 *
 *----------------------------------------------------------------------
 */

void
Tk_UnsetGrid(tkwin)
    Tk_Window tkwin;		/* Token for window that is currently
				 * controlling gridding. */
{
    TkWindow *winPtr = (TkWindow *) tkwin;
    register WmInfo *wmPtr;

#ifdef DEBUG
    printf("Tk_UnsetGrid\n");
#endif

    /*
     * Find the top-level window for tkwin, plus the window manager
     * information.
     */

    while (!(winPtr->flags & TK_TOP_LEVEL)) {
	winPtr = winPtr->parentPtr;
    }
    wmPtr = winPtr->wmInfoPtr;
    if (tkwin != wmPtr->gridWin) {
	return;
    }

    wmPtr->gridWin = NULL;
    wmPtr->sizeHintsFlags &= ~(PBaseSize|PResizeInc);
    if (wmPtr->width != -1) {
	wmPtr->width = winPtr->reqWidth + (wmPtr->width
		- wmPtr->reqGridWidth)*wmPtr->widthInc;
	wmPtr->height = winPtr->reqHeight + (wmPtr->height
		- wmPtr->reqGridHeight)*wmPtr->heightInc;
    }
    wmPtr->widthInc = 1;
    wmPtr->heightInc = 1;

    if (!(wmPtr->flags & (WM_UPDATE_PENDING|WM_NEVER_MAPPED))) {
	Tcl_DoWhenIdle(UpdateGeometryInfo, (ClientData) winPtr);
	wmPtr->flags |= WM_UPDATE_PENDING;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TopLevelEventProc --
 *
 *	This procedure is invoked when a top-level (or other externally-
 *	managed window) is restructured in any way.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Tk's internal data structures for the window get modified to
 *	reflect the structural change.
 *
 *----------------------------------------------------------------------
 */

static void
TopLevelEventProc(clientData, eventPtr)
    ClientData clientData;		/* Window for which event occurred. */
    XEvent *eventPtr;			/* Event that just happened. */
{
    register TkWindow *winPtr = (TkWindow *) clientData;

#ifdef DEBUG
    printf("TopLevelEventProc, winPtr %x, eventPtr %x\n", winPtr, eventPtr);
#endif

    if (eventPtr->type == DestroyNotify) {
	Tk_ErrorHandler handler;

	if (!(winPtr->flags & TK_ALREADY_DEAD)) {
	    /*
	     * A top-level window was deleted externally (e.g., by the window
	     * manager).  This is probably not a good thing, but cleanup as
	     * best we can.  The error handler is needed because
	     * Tk_DestroyWindow will try to destroy the window, but of course
	     * it's already gone.
	     */
    
#ifdef DEBUG
            printf("    TK_ALREADY_DEAD\n");
#endif
	    handler = Tk_CreateErrorHandler(winPtr->display, -1, -1, -1,
		    (Tk_ErrorProc *) NULL, (ClientData) NULL);
	    Tk_DestroyWindow((Tk_Window) winPtr);
	    Tk_DeleteErrorHandler(handler);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TopLevelReqProc --
 *
 *	This procedure is invoked by the geometry manager whenever
 *	the requested size for a top-level window is changed.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Arrange for the window to be resized to satisfy the request
 *	(this happens as a when-idle action).
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static void
TopLevelReqProc(dummy, tkwin)
    ClientData dummy;			/* Not used. */
    Tk_Window tkwin;			/* Information about window. */
{
    TkWindow *winPtr = (TkWindow *) tkwin;
    WmInfo *wmPtr;

#ifdef DEBUG
    printf("TopLevelReqProc\n");
#endif

    wmPtr = winPtr->wmInfoPtr;
    if (!(wmPtr->flags & (WM_UPDATE_PENDING|WM_NEVER_MAPPED))) {
	Tcl_DoWhenIdle(UpdateGeometryInfo, (ClientData) winPtr);
	wmPtr->flags |= WM_UPDATE_PENDING;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateGeometryInfo --
 *
 *	This procedure is invoked when a top-level window is first
 *	mapped, and also as a when-idle procedure, to bring the
 *	geometry and/or position of a top-level window back into
 *	line with what has been requested by the user and/or widgets.
 *	This procedure doesn't return until the window manager has
 *	responded to the geometry change.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The window's size and location may change, unless the WM prevents
 *	that from happening.
 *
 *----------------------------------------------------------------------
 */

static void
UpdateGeometryInfo(clientData)
    ClientData clientData;		/* Pointer to the window's record. */
{
    register TkWindow *winPtr = (TkWindow *) clientData;
    register WmInfo *wmPtr = winPtr->wmInfoPtr;
    int x, y, width, height;

#ifdef DEBUG
    printf("UpdateGeometryInfo\n");
#endif

    wmPtr->flags &= ~WM_UPDATE_PENDING;

    /*
     * Compute the new size for the top-level window.  See the
     * user documentation for details on this, but the size
     * requested depends on (a) the size requested internally
     * by the window's widgets, (b) the size requested by the
     * user in a "wm geometry" command or via wm-based interactive
     * resizing (if any), and (c) whether or not the window is
     * gridded.  Don't permit sizes <= 0 because this upsets
     * the X server.
     */

    if (wmPtr->width == -1) {
	width = winPtr->reqWidth;
    } else if (wmPtr->gridWin != NULL) {
	width = winPtr->reqWidth
		+ (wmPtr->width - wmPtr->reqGridWidth)*wmPtr->widthInc;
    } else {
	width = wmPtr->width;
    }
    if (width <= 0) {
	width = 1;
    }
    if (wmPtr->height == -1) {
        height = winPtr->reqHeight;
#ifdef DEBUG
        printf("UpdateGeometryInfo height = winPtr->reqHeight: %d\n", height);
#endif
    } else if (wmPtr->gridWin != NULL) {
        height = winPtr->reqHeight
                + (wmPtr->height - wmPtr->reqGridHeight)*wmPtr->heightInc;
#ifdef DEBUG
        printf("UpdateGeometryInfo height = winPtr->reqHeight + ...: %d (%d + (%d - %d)* %d\n",
               height, winPtr->reqHeight, wmPtr->height, wmPtr->reqGridHeight,
               wmPtr->heightInc);
#endif
    } else {
        height = wmPtr->height;
#ifdef DEBUG
        printf("UpdateGeometryInfo height = wmPtr->height: %d\n", height);
#endif
    }
    if (height <= 0) {
	height = 1;
#ifdef DEBUG
        printf("UpdateGeometryInfo height = 1\n");
#endif
    }

    /*
     * Compute the new position for the upper-left pixel of the window's
     * decorative frame.  This is tricky, because we need to include the
     * border widths supplied by a reparented parent in this calculation,
     * but can't use the parent's current overall size since that may
     * change as a result of this code.
     */

    if (wmPtr->flags & WM_NEGATIVE_X) {
	x = DisplayWidth(winPtr->display, winPtr->screenNum) - wmPtr->x
		- (width + wmPtr->borderWidth);
    } else {
	x =  wmPtr->x;
#ifdef DEBUG
        printf("UpdateGeometryInfo x = wmPtr->x: %d\n", x);
#endif
    }
    if (wmPtr->flags & WM_NEGATIVE_Y) {
	y = DisplayHeight(winPtr->display, winPtr->screenNum) - wmPtr->y
		- (height + wmPtr->borderHeight);
    } else {
	y =  wmPtr->y;
#ifdef DEBUG
        printf("UpdateGeometryInfo y = wmPtr->y: %d\n", y);
#endif
    }

    /*
     * Reconfigure the window if it isn't already configured correctly.  Base
     * the size check on what we *asked for* last time, not what we got.
     */

    if ((wmPtr->flags & WM_MOVE_PENDING)
	    || (width != wmPtr->configWidth)
	    || (height != wmPtr->configHeight)) {
	wmPtr->configWidth = width;
	wmPtr->configHeight = height;

       /*
        * Don't bother moving the window if we are in the process of
        * creating it.  Just update the geometry info based on what
        * we asked for.
        */

        if (!(wmPtr->flags & WM_CREATE_PENDING)) {

            wmPtr->flags |= WM_SYNC_PENDING;
            /* Make sure we get the right size/position */
#ifdef DEBUG
            printf("   WinSetWindowPos(%x HWND_TOP %d, %d, %d, %d SIZE|MOVE)\n",
                   TkOS2GetHWND(wmPtr->reparent), x,
                   yScreen - (y + height + wmPtr->borderHeight),
                   width + wmPtr->borderWidth, height + wmPtr->borderHeight);
#endif
	    WinSetWindowPos(TkOS2GetHWND(wmPtr->reparent), HWND_TOP,
                            x, yScreen - (y + height + wmPtr->borderHeight),
                            width + wmPtr->borderWidth,
                            height + wmPtr->borderHeight, SWP_SIZE | SWP_MOVE);
            wmPtr->flags &= ~WM_SYNC_PENDING;
        } else {
            winPtr->changes.x = x;
            winPtr->changes.y = y;
            winPtr->changes.width = width;
            winPtr->changes.height = height;
#ifdef DEBUG
            printf("UpdateGeometryInfo changes (%d,%d) %dx%d\n", 
                   winPtr->changes.x, winPtr->changes.y,
                   winPtr->changes.width, winPtr->changes.height);
#endif
        }
    } else {
	return;
    }
}

/*
 *--------------------------------------------------------------
 *
 * ParseGeometry --
 *
 *	This procedure parses a geometry string and updates
 *	information used to control the geometry of a top-level
 *	window.
 *
 * Results:
 *	A standard Tcl return value, plus an error message in
 *	interp->result if an error occurs.
 *
 * Side effects:
 *	The size and/or location of winPtr may change.
 *
 *--------------------------------------------------------------
 */

static int
ParseGeometry(interp, string, winPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    char *string;		/* String containing new geometry.  Has the
				 * standard form "=wxh+x+y". */
    TkWindow *winPtr;		/* Pointer to top-level window whose
				 * geometry is to be changed. */
{
    register WmInfo *wmPtr = winPtr->wmInfoPtr;
    int x, y, width, height, flags;
    char *end;
    register char *p = string;

#ifdef DEBUG
    printf("ParseGeometry\n");
#endif

    /*
     * The leading "=" is optional.
     */

    if (*p == '=') {
	p++;
    }

    /*
     * Parse the width and height, if they are present.  Don't
     * actually update any of the fields of wmPtr until we've
     * successfully parsed the entire geometry string.
     */

    width = wmPtr->width;
    height = wmPtr->height;
    x = wmPtr->x;
    y = wmPtr->y;
    flags = wmPtr->flags;
    if (isdigit(UCHAR(*p))) {
	width = strtoul(p, &end, 10);
	p = end;
	if (*p != 'x') {
	    goto error;
	}
	p++;
	if (!isdigit(UCHAR(*p))) {
	    goto error;
	}
	height = strtoul(p, &end, 10);
	p = end;
    }

    /*
     * Parse the X and Y coordinates, if they are present.
     */

    if (*p != '\0') {
	flags &= ~(WM_NEGATIVE_X | WM_NEGATIVE_Y);
	if (*p == '-') {
	    flags |= WM_NEGATIVE_X;
	} else if (*p != '+') {
	    goto error;
	}
	x = strtol(p+1, &end, 10);
	p = end;
	if (*p == '-') {
	    flags |= WM_NEGATIVE_Y;
	} else if (*p != '+') {
	    goto error;
	}
	y = strtol(p+1, &end, 10);
	if (*end != '\0') {
	    goto error;
	}

	/*
	 * Assume that the geometry information came from the user,
	 * unless an explicit source has been specified.  Otherwise
	 * most window managers assume that the size hints were
	 * program-specified and they ignore them.
	 */

	if ((wmPtr->sizeHintsFlags & (USPosition|PPosition)) == 0) {
	    wmPtr->sizeHintsFlags |= USPosition;
	}
    }

    /*
     * Everything was parsed OK.  Update the fields of *wmPtr and
     * arrange for the appropriate information to be percolated out
     * to the window manager at the next idle moment.
     */

    wmPtr->width = width;
    wmPtr->height = height;
    if ((x != wmPtr->x) || (y != wmPtr->y)
	    || ((flags & (WM_NEGATIVE_X|WM_NEGATIVE_Y))
	    != (wmPtr->flags & (WM_NEGATIVE_X|WM_NEGATIVE_Y)))) {
	wmPtr->x = x;
	wmPtr->y = y;
	flags |= WM_MOVE_PENDING;
    }
    wmPtr->flags = flags;

    if (!(wmPtr->flags & (WM_UPDATE_PENDING|WM_NEVER_MAPPED))) {
	Tcl_DoWhenIdle(UpdateGeometryInfo, (ClientData) winPtr);
	wmPtr->flags |= WM_UPDATE_PENDING;
    }
    return TCL_OK;

    error:
    Tcl_AppendResult(interp, "bad geometry specifier \"",
	    string, "\"", (char *) NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_GetRootCoords --
 *
 *	Given a token for a window, this procedure traces through the
 *	window's lineage to find the (virtual) root-window coordinates
 *	corresponding to point (0,0) in the window.
 *
 * Results:
 *	The locations pointed to by xPtr and yPtr are filled in with
 *	the root coordinates of the (0,0) point in tkwin.  If a virtual
 *	root window is in effect for the window, then the coordinates
 *	in the virtual root are returned.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
Tk_GetRootCoords(tkwin, xPtr, yPtr)
    Tk_Window tkwin;		/* Token for window. */
    int *xPtr;			/* Where to store x-displacement of (0,0). */
    int *yPtr;			/* Where to store y-displacement of (0,0). */
{
    int x, y;
    register TkWindow *winPtr = (TkWindow *) tkwin;

#ifdef DEBUG
    printf("Tk_GetRootCoords\n");
#endif

    /*
     * Search back through this window's parents all the way to a
     * top-level window, combining the offsets of each window within
     * its parent.
     */

    x = y = 0;
    while (1) {
	x += winPtr->changes.x + winPtr->changes.border_width;
	y += winPtr->changes.y + winPtr->changes.border_width;
	if (winPtr->flags & TK_TOP_LEVEL) {
	    x += winPtr->wmInfoPtr->xInParent;
	    y += winPtr->wmInfoPtr->yInParent;
	    break;
	}
	winPtr = winPtr->parentPtr;
    }
    *xPtr = x;
    *yPtr = y;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_CoordsToWindow --
 *
 *	Given the (virtual) root coordinates of a point, this procedure
 *	returns the token for the top-most window covering that point,
 *	if there exists such a window in this application.
 *
 * Results:
 *	The return result is either a token for the window corresponding
 *	to rootX and rootY, or else NULL to indicate that there is no such
 *	window.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tk_Window
Tk_CoordsToWindow(rootX, rootY, tkwin)
    int rootX, rootY;		/* Coordinates of point in root window.  If
				 * a virtual-root window manager is in use,
				 * these coordinates refer to the virtual
				 * root, not the real root. */
    Tk_Window tkwin;		/* Token for any window in application;
				 * used to identify the display. */
{
    POINTL pos;
    HWND hwnd;
    TkOS2Drawable *todPtr;
    TkWindow *winPtr;

    pos.x = rootX;
    /* Translate to PM coordinates */
    pos.y = yScreen - rootY;
    hwnd = WinWindowFromPoint(HWND_DESKTOP, &pos, TRUE);
#ifdef DEBUG
    printf("Tk_CoordsToWindow (%d,%d): %x\n", pos.x, pos.y, hwnd);
#endif
    if (hwnd == HWND_DESKTOP) return NULL;

    todPtr = TkOS2GetDrawableFromHandle(hwnd);
    if (todPtr && (todPtr->type == TOD_WINDOW)) {
	winPtr = TkOS2GetWinPtr(todPtr);
	if (winPtr->mainPtr == ((TkWindow *) tkwin)->mainPtr) {
	    return (Tk_Window) winPtr;
	}
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_GetVRootGeometry --
 *
 *	This procedure returns information about the virtual root
 *	window corresponding to a particular Tk window.
 *
 * Results:
 *	The values at xPtr, yPtr, widthPtr, and heightPtr are set
 *	with the offset and dimensions of the root window corresponding
 *	to tkwin.  If tkwin is being managed by a virtual root window
 *	manager these values correspond to the virtual root window being
 *	used for tkwin;  otherwise the offsets will be 0 and the
 *	dimensions will be those of the screen.
 *
 * Side effects:
 *	Vroot window information is refreshed if it is out of date.
 *
 *----------------------------------------------------------------------
 */

void
Tk_GetVRootGeometry(tkwin, xPtr, yPtr, widthPtr, heightPtr)
    Tk_Window tkwin;		/* Window whose virtual root is to be
				 * queried. */
    int *xPtr, *yPtr;		/* Store x and y offsets of virtual root
				 * here. */
    int *widthPtr, *heightPtr;	/* Store dimensions of virtual root here. */
{
    TkWindow *winPtr = (TkWindow *) tkwin;

#ifdef DEBUG
    printf("Tk_GetVRootGeometry\n");
#endif

    *xPtr = 0;
    *yPtr = 0;
    *widthPtr = DisplayWidth(winPtr->display, winPtr->screenNum);
    *heightPtr = DisplayHeight(winPtr->display, winPtr->screenNum);
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_MoveToplevelWindow --
 *
 *	This procedure is called instead of Tk_MoveWindow to adjust
 *	the x-y location of a top-level window.  It delays the actual
 *	move to a later time and keeps window-manager information
 *	up-to-date with the move
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The window is eventually moved so that its upper-left corner
 *	(actually, the upper-left corner of the window's decorative
 *	frame, if there is one) is at (x,y).
 *
 *----------------------------------------------------------------------
 */

void
Tk_MoveToplevelWindow(tkwin, x, y)
    Tk_Window tkwin;		/* Window to move. */
    int x, y;			/* New location for window (within
				 * parent). */
{
    TkWindow *winPtr = (TkWindow *) tkwin;
    register WmInfo *wmPtr = winPtr->wmInfoPtr;

#ifdef DEBUG
    printf("Tk_MoveToplevelWindow\n");
#endif

    if (!(winPtr->flags & TK_TOP_LEVEL)) {
	panic("Tk_MoveToplevelWindow called with non-toplevel window");
    }
    wmPtr->x = x;
    wmPtr->y = y;
    wmPtr->flags |= WM_MOVE_PENDING;
    wmPtr->flags &= ~(WM_NEGATIVE_X|WM_NEGATIVE_Y);
    if ((wmPtr->sizeHintsFlags & (USPosition|PPosition)) == 0) {
	wmPtr->sizeHintsFlags |= USPosition;
    }

    /*
     * If the window has already been mapped, must bring its geometry
     * up-to-date immediately, otherwise an event might arrive from the
     * server that would overwrite wmPtr->x and wmPtr->y and lose the
     * new position.
     */

    if (!(wmPtr->flags & WM_NEVER_MAPPED)) {
	if (wmPtr->flags & WM_UPDATE_PENDING) {
	    Tk_CancelIdleCall(UpdateGeometryInfo, (ClientData) winPtr);
	}
	UpdateGeometryInfo((ClientData) winPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkWmProtocolEventProc --
 *
 *	This procedure is called by the Tk_HandleEvent whenever a
 *	ClientMessage event arrives whose type is "WM_PROTOCOLS".
 *	This procedure handles the message from the window manager
 *	in an appropriate fashion.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Depends on what sort of handler, if any, was set up for the
 *	protocol.
 *
 *----------------------------------------------------------------------
 */

void
TkWmProtocolEventProc(winPtr, eventPtr)
    TkWindow *winPtr;		/* Window to which the event was sent. */
    XEvent *eventPtr;		/* X event. */
{
    WmInfo *wmPtr;
    register ProtocolHandler *protPtr;
    Atom protocol;
    int result;
    Tcl_Interp *interp;

#ifdef DEBUG
    printf("TkWmProtocolEventProc\n");
#endif

    wmPtr = winPtr->wmInfoPtr;
    if (wmPtr == NULL) {
	return;
    }
    protocol = (Atom) eventPtr->xclient.data.l[0];
    for (protPtr = wmPtr->protPtr; protPtr != NULL;
	    protPtr = protPtr->nextPtr) {
	if (protocol == protPtr->protocol) {
	    Tcl_Preserve((ClientData) protPtr);
            interp = protPtr->interp;
            Tcl_Preserve((ClientData) interp);
            result = Tcl_GlobalEval(interp, protPtr->command);
	    if (result != TCL_OK) {
                Tcl_AddErrorInfo(interp, "\n    (command for \"");
                Tcl_AddErrorInfo(interp,
                        Tk_GetAtomName((Tk_Window) winPtr, protocol));
                Tcl_AddErrorInfo(interp, "\" window manager protocol)");
                Tcl_BackgroundError(interp);
	    }
	    Tcl_Release((ClientData) interp);
	    Tcl_Release((ClientData) protPtr);
	    return;
	}
    }

    /*
     * No handler was present for this protocol.  If this is a
     * WM_DELETE_WINDOW message then just destroy the window.
     */

    if (protocol == Tk_InternAtom((Tk_Window) winPtr, "WM_DELETE_WINDOW")) {
	Tk_DestroyWindow((Tk_Window) winPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkWmRestackToplevel --
 *
 *	This procedure restacks a top-level window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	WinPtr gets restacked  as specified by aboveBelow and otherPtr.
 *	This procedure doesn't return until the restack has taken
 *	effect and the ConfigureNotify event for it has been received.
 *
 *----------------------------------------------------------------------
 */

void
TkWmRestackToplevel(winPtr, aboveBelow, otherPtr)
    TkWindow *winPtr;		/* Window to restack. */
    int aboveBelow;		/* Gives relative position for restacking;
				 * must be Above or Below. */
    TkWindow *otherPtr;		/* Window relative to which to restack;
				 * if NULL, then winPtr gets restacked
				 * above or below *all* siblings. */
{
    XWindowChanges changes;
    unsigned int mask;
    Window window;

#ifdef DEBUG
    printf("TkWmRestackToplevel\n");
#endif

    changes.stack_mode = aboveBelow;
    mask = CWStackMode;
    if (winPtr->window == None) {
	Tk_MakeWindowExist((Tk_Window) winPtr);
    }
    if (winPtr->wmInfoPtr->flags & WM_NEVER_MAPPED) {
        /*
         * Can't set stacking order properly until the window is on the
         * screen (mapping it may give it a reparent window), so make sure
         * it's on the screen.
         */

        TkWmMapWindow(winPtr);
    }
    window = (winPtr->wmInfoPtr->reparent != None)
	    ? winPtr->wmInfoPtr->reparent : winPtr->window;
    if (otherPtr != NULL) {
	if (otherPtr->window == None) {
	    Tk_MakeWindowExist((Tk_Window) otherPtr);
	}
        if (otherPtr->wmInfoPtr->flags & WM_NEVER_MAPPED) {
            TkWmMapWindow(otherPtr);
        }
	changes.sibling = (otherPtr->wmInfoPtr->reparent != None)
		? otherPtr->wmInfoPtr->reparent : otherPtr->window;
	mask = CWStackMode|CWSibling;
    }

    /*
     * Reconfigure the window.  Since this is Presentation Manager, the
     * reconfiguration will happen immediately.
     */

    XConfigureWindow(winPtr->display, window, mask, &changes);
}

/*
 *----------------------------------------------------------------------
 *
 * TkWmAddToColormapWindows --
 *
 *	This procedure is called to add a given window to the
 *	WM_COLORMAP_WINDOWS property for its top-level, if it
 *	isn't already there.  It is invoked by the Tk code that
 *	creates a new colormap, in order to make sure that colormap
 *	information is propagated to the window manager by default.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	WinPtr's window gets added to the WM_COLORMAP_WINDOWS
 *	property of its nearest top-level ancestor, unless the
 *	colormaps have been set explicitly with the
 *	"wm colormapwindows" command.
 *
 *----------------------------------------------------------------------
 */

void
TkWmAddToColormapWindows(winPtr)
    TkWindow *winPtr;		/* Window with a non-default colormap.
				 * Should not be a top-level window. */
{
    TkWindow *topPtr;
    TkWindow **oldPtr, **newPtr;
    int count, i;

#ifdef DEBUG
    printf("TkWmAddToColormapWindows\n");
#endif

    if (winPtr->window == None) {
	return;
    }

    for (topPtr = winPtr->parentPtr; ; topPtr = topPtr->parentPtr) {
	if (topPtr == NULL) {
	    /*
	     * Window is being deleted.  Skip the whole operation.
	     */

	    return;
	}
	if (topPtr->flags & TK_TOP_LEVEL) {
	    break;
	}
    }
    if (topPtr->wmInfoPtr->flags & WM_COLORMAPS_EXPLICIT) {
	return;
    }

    /*
     * Make sure that the window isn't already in the list.
     */

    count = topPtr->wmInfoPtr->cmapCount;
    oldPtr = topPtr->wmInfoPtr->cmapList;

    for (i = 0; i < count; i++) {
	if (oldPtr[i] == winPtr) {
	    return;
	}
    }

    /*
     * Make a new bigger array and use it to reset the property.
     * Automatically add the toplevel itself as the last element
     * of the list.
     */

    newPtr = (TkWindow **) ckalloc((unsigned) ((count+2)*sizeof(TkWindow*)));
    if (count > 0) {
	memcpy(newPtr, oldPtr, count * sizeof(TkWindow*));
    }
    newPtr[count] = winPtr;
    newPtr[count+1] = topPtr;
    if (oldPtr != NULL) {
	ckfree((char *) oldPtr);
    }

    topPtr->wmInfoPtr->cmapList = newPtr;
    topPtr->wmInfoPtr->cmapCount = count+2;

    /*
     * Now we need to force the updated colormaps to be installed.
     */

    if (topPtr->wmInfoPtr == foregroundWmPtr) {
        TkOS2WmInstallColormaps(TkOS2GetHWND(topPtr->wmInfoPtr->reparent),
              /* WM_QUERYNEWPALETTE -> WM_REALIZEPALETTE + focus notification */
                                WM_REALIZEPALETTE, 1);
    } else {
        TkOS2WmInstallColormaps(TkOS2GetHWND(topPtr->wmInfoPtr->reparent),
               /* WM_PALETTECHANGED -> WM_REALIZEPALETTE + focus notification */
                                WM_REALIZEPALETTE, 0);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkWmRemoveFromColormapWindows --
 *
 *      This procedure is called to remove a given window from the
 *      WM_COLORMAP_WINDOWS property for its top-level.  It is invoked
 *      when windows are deleted.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      WinPtr's window gets removed from the WM_COLORMAP_WINDOWS
 *      property of its nearest top-level ancestor, unless the
 *      top-level itself is being deleted too.
 *
 *----------------------------------------------------------------------
 */

void
TkWmRemoveFromColormapWindows(winPtr)
    TkWindow *winPtr;           /* Window that may be present in
                                 * WM_COLORMAP_WINDOWS property for its
                                 * top-level.  Should not be a top-level
                                 * window. */
{
    TkWindow *topPtr;
    TkWindow **oldPtr;
    int count, i, j;

#ifdef DEBUG
    printf("TkWmRemoveFromColormapWindows\n");
#endif

    for (topPtr = winPtr->parentPtr; ; topPtr = topPtr->parentPtr) {
        if (topPtr == NULL) {
            /*
             * Ancestors have been deleted, so skip the whole operation.
             * Seems like this can't ever happen?
             */

            return;
        }
        if (topPtr->flags & TK_TOP_LEVEL) {
            break;
        }
    }
    if (topPtr->flags & TK_ALREADY_DEAD) {
        /*
         * Top-level is being deleted, so there's no need to cleanup
         * the WM_COLORMAP_WINDOWS property.
         */

        return;
    }

    /*
     * Find the window and slide the following ones down to cover
     * it up.
     */

    count = topPtr->wmInfoPtr->cmapCount;
    oldPtr = topPtr->wmInfoPtr->cmapList;
    for (i = 0; i < count; i++) {
        if (oldPtr[i] == winPtr) {
            for (j = i ; j < count-1; j++) {
                oldPtr[j] = oldPtr[j+1];
            }
            topPtr->wmInfoPtr->cmapCount = count-1;
            break;
        }
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkOS2WmConfigure --
 *
 *	Generate a ConfigureNotify event based on the current position
 *	information.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Queues a new event.
 *
 *----------------------------------------------------------------------
 */

void
TkOS2WmConfigure(winPtr, pos)
    TkWindow *winPtr;
    SWP *pos; /* OS/2 PM y-coordinate */
{
    XEvent event;
    WmInfo *wmPtr;
    int width, height;
    SWP swp;
    ULONG x11y;
    ULONG rc;

    if (winPtr == NULL) {
	return;
    }

    wmPtr = winPtr->wmInfoPtr;
#ifdef DEBUG
    printf("TkOS2WmConfigure: POS fl %x, (%d,%d) %dx%d\n", pos->fl, pos->x,
           pos->y, pos->cx, pos->cy);
#endif

    /* PM Coordinates are reversed, translate wrt. screen height */
    x11y = yScreen - pos->cy - pos->y;

    /*
     * If the window was just iconified, then we don't need to update
     * the geometry, just iconify the window.
     */

    /* Is the window already iconified ? */
    rc = WinQueryWindowPos(TkOS2GetHWND(wmPtr->reparent), &swp);
    if (rc == FALSE) {
#ifdef DEBUG
        printf("ERROR in WinQueryWindowPos: %x\n", WinGetLastError(hab));
#endif
        return;
    }
    if (swp.fl & SWP_MINIMIZE) {
#ifdef DEBUG
        printf("TkOS2WmConfigure minimized, pos: (%d,%d) %dx%d (x11y: %d)\n",
               pos->x, pos->y, pos->cx, pos->cy, x11y);
#endif
        /* window now minimized */
	if (wmPtr->hints.initial_state == NormalState) {
            /* synchronize Tk with it */
	    IconifyWindow(winPtr);
	}
        return;
    } else if (wmPtr->hints.initial_state == IconicState ||
               (wmPtr->hints.initial_state == WithdrawnState && pos->fl & SWP_RESTORE)) {
	    DeiconifyWindow(winPtr);
    }
#ifdef DEBUG
    printf("TkOS2WmConfigure %x, pos: (%d,%d) %dx%d (x11y: %d), fl %x\n",
           TkOS2GetHWND(wmPtr->reparent), pos->x, pos->y, pos->cx, pos->cy,
           x11y, pos->fl);
#endif

    width = pos->cx - wmPtr->borderWidth;
    height = pos->cy - wmPtr->borderHeight;

    /*
     * Update size information from the event.  There are a couple of
     * tricky points here:
     *
     * 1. If the user changed the size externally then set wmPtr->width
     *    and wmPtr->height just as if a "wm geometry" command had been
     *    invoked with the same information.
     * 2. However, if the size is changing in response to a request
     *    coming from us (WM_SYNC_PENDING is set), then don't set wmPtr->width
     *    or wmPtr->height (otherwise the window will stop tracking geometry
     *    manager requests).
     */

   if (!(wmPtr->flags & WM_SYNC_PENDING)) {
       if ((width != winPtr->changes.width)
               || (height != winPtr->changes.height)) {
           if ((wmPtr->width == -1) && (width == winPtr->reqWidth)) {
               /*
                * Don't set external width, since the user didn't change it
                * from what the widgets asked for.
                */
           } else {
               if (wmPtr->gridWin != NULL) {
                   wmPtr->width = wmPtr->reqGridWidth
                       + (width - winPtr->reqWidth)/wmPtr->widthInc;
                   if (wmPtr->width < 0) {
                       wmPtr->width = 0;
                   }
               } else {
                   wmPtr->width = width;
               }
           }
           if ((wmPtr->height == -1) && (height == winPtr->reqHeight)) {
               /*
                * Don't set external height, since the user didn't change it
                * from what the widgets asked for.
                 */
            } else {
                if (wmPtr->gridWin != NULL) {
                    wmPtr->height = wmPtr->reqGridHeight
                        + (height - winPtr->reqHeight)/wmPtr->heightInc;
                    if (wmPtr->height < 0) {
                        wmPtr->height = 0;
                    }
                } else {
                    wmPtr->height = height;
                }
            }
            wmPtr->configWidth = width;
            wmPtr->configHeight = height;
        }
        wmPtr->x = pos->x;
        wmPtr->y = x11y;
        wmPtr->flags &= ~(WM_NEGATIVE_X | WM_NEGATIVE_Y);
    }

    /*
     * Update the shape of the contained window.
     */

    winPtr->changes.x = pos->x;
    if (wmPtr->exStyle & FCF_TITLEBAR) {
        x11y += titleBar;
#ifdef DEBUG
        printf("TkOS2WmConfigure: adding FCF_TITLEBAR (%d)\n", titleBar);
#endif
    }
    winPtr->changes.y = x11y;
    winPtr->changes.width = width;
    winPtr->changes.height = height;
#ifdef DEBUG
    printf("TkOS2WmConfigure %x, width %d, height %d, wmPtr->width %d, wmPtr->height %d\n",
           TkOS2GetHWND(wmPtr->reparent), width, height,
           wmPtr->width, wmPtr->height);
#endif
    WinSetWindowPos(TkOS2GetHWND(winPtr->window), HWND_TOP, wmPtr->xInParent,
                    wmPtr->yInParent, width, height, SWP_MOVE | SWP_SIZE);

    /*
     * Generate a ConfigureNotify event.
     */

    event.type = ConfigureNotify;
    event.xconfigure.serial = winPtr->display->request;
    event.xconfigure.send_event = False;
    event.xconfigure.display = winPtr->display;
    event.xconfigure.event = winPtr->window;
    event.xconfigure.window = winPtr->window;
    event.xconfigure.border_width = winPtr->changes.border_width;
    event.xconfigure.override_redirect = winPtr->atts.override_redirect;
    event.xconfigure.x = pos->x;
    event.xconfigure.y = x11y;
    event.xconfigure.width = width;
    event.xconfigure.height = height;
#ifdef DEBUG
    printf("    event: x %d, y %d, w %d, h %d\n", event.xconfigure.x,
           event.xconfigure.y, event.xconfigure.width, event.xconfigure.height);
#endif
    if (winPtr->changes.stack_mode == Above) {
        event.xconfigure.above = winPtr->changes.sibling;
    } else {
        event.xconfigure.above = None;
    }
    Tk_QueueWindowEvent(&event, TCL_QUEUE_TAIL);
}

/*
 *----------------------------------------------------------------------
 *
 * IconifyWindow --
 *
 *	Put a toplevel window into the iconified state.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Iconifies the window, possibly mapping it for the first time.
 *
 *----------------------------------------------------------------------
 */

void
IconifyWindow(winPtr)
    TkWindow *winPtr;		/* Window to be iconified. */
{
    WmInfo *wmPtr = winPtr->wmInfoPtr;

#ifdef DEBUG
    printf("IconifyWindow hwnd %x\n", wmPtr->flags & WM_NEVER_MAPPED
           ? -1 : TkOS2GetHWND(wmPtr->reparent));
#endif
    wmPtr->hints.initial_state = IconicState;
    if (!(wmPtr->flags & WM_NEVER_MAPPED)) {
	if (wmPtr->withdrawn) {
	    Tk_MapWindow((Tk_Window) winPtr);
	    wmPtr->withdrawn = 0;
	} else {
	    wmPtr->flags |= WM_SYNC_PENDING;
	    WinSetWindowPos(TkOS2GetHWND(wmPtr->reparent), HWND_DESKTOP,
	    		    0, 0, 0, 0, SWP_MINIMIZE);
	    wmPtr->flags &= ~WM_SYNC_PENDING;
	    XUnmapWindow(winPtr->display, winPtr->window);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * DeiconifyWindow --
 *
 *	Put a toplevel window into the deiconified state.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Deiconifies the window, possibly mapping it for the first time.
 *
 *----------------------------------------------------------------------
 */

void
DeiconifyWindow(winPtr)
    TkWindow *winPtr;		/* Window to be deiconified. */
{
    WmInfo *wmPtr = winPtr->wmInfoPtr;

#ifdef DEBUG
    printf("DeiconifyWindow hwnd %x\n", wmPtr->flags & WM_NEVER_MAPPED
           ? -1 : TkOS2GetHWND(wmPtr->reparent));
#endif
    wmPtr->hints.initial_state = NormalState;
    wmPtr->withdrawn = 0;
    if (!(wmPtr->flags & WM_NEVER_MAPPED)) {
	Tk_MapWindow((Tk_Window) winPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkOS2WmInstallColormaps --
 *
 *	Installs the colormaps associated with the toplevel which is
 *	currently active.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	May change the system palette and generate damage.
 *
 *----------------------------------------------------------------------
 */

int
TkOS2WmInstallColormaps(hwnd, message, isForemost)
    HWND hwnd;			/* Toplevel wrapper window whose colormaps
				 * should be installed. */
    int message;		/* Either WM_REALIZEPALETTE or
				 * WM_SETFOCUS */
    int isForemost;		/* 1 if window is foremost, else 0 */
{
    int i;
    HPS hps;
    HPAL oldPalette;
    ULONG colorsChanged;
    TkOS2Drawable *todPtr =
        (TkOS2Drawable *) WinQueryWindowULong(hwnd, QWL_USER);
    TkWindow *winPtr = TkOS2GetWinPtr(todPtr);
    WmInfo *wmPtr;

#ifdef DEBUG
    printf("TkOS2WmInstallColormap\n");
#endif
	    
    if (winPtr == NULL) {
	return 0;
    }

    wmPtr = winPtr->wmInfoPtr;
    hps = WinGetPS(hwnd);

    /*
     * The application should call WinRealizePalette if it has a palette,
     * or pass on to the default window procedure if it doesn't.
     * If the return value from WinRealizePalette is greater than 0, the
     * application should invalidate its window to cause a repaint using
     * the newly-realized palette.
     */

    /*
     * Install all of the palettes.
     */

    if (wmPtr->cmapCount > 0) {
        winPtr = wmPtr->cmapList[0];
    }
    i = 1;

    oldPalette = GpiSelectPalette(hps, TkOS2GetPalette(winPtr->atts.colormap));
    if ( WinRealizePalette(hwnd, hps, &colorsChanged) > 0 ) {
        RefreshColormap(winPtr->atts.colormap);
    }
    for (; i < wmPtr->cmapCount; i++) {
        HPS winPS;
        winPtr = wmPtr->cmapList[i];
        winPS = WinGetPS(TkOS2GetHWND(winPtr->window));
        GpiSelectPalette(winPS, TkOS2GetPalette(winPtr->atts.colormap));
        if ( WinRealizePalette(TkOS2GetHWND(winPtr->window), winPS, &colorsChanged) > 0 ) {
        }
        WinReleasePS(winPS);
    }

    WinReleasePS(hps);
    return TRUE;
}

/*
 *----------------------------------------------------------------------
 *
 * RefreshColormap --
 *
 *      This function is called to force all of the windows that use
 *      a given colormap to redraw themselves.  The quickest way to
 *      do this is to iterate over the toplevels, looking in the
 *      cmapList for matches.  This will quickly eliminate subtrees
 *      that don't use a given colormap.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Causes damage events to be generated.
 *
 *----------------------------------------------------------------------
 */

static void
RefreshColormap(colormap)
    Colormap colormap;
{
    WmInfo *wmPtr;
    int i;

#ifdef DEBUG
    printf("RefreshColormap\n");
#endif

    for (wmPtr = firstWmPtr; wmPtr != NULL; wmPtr = wmPtr->nextPtr) {
        if (wmPtr->cmapCount > 0) {
            for (i = 0; i < wmPtr->cmapCount; i++) {
                if ((wmPtr->cmapList[i]->atts.colormap == colormap)
                        && Tk_IsMapped(wmPtr->cmapList[i])) {
                    InvalidateSubTree(wmPtr->cmapList[i], colormap);
                }
            }
        } else if ((wmPtr->winPtr->atts.colormap == colormap)
                && Tk_IsMapped(wmPtr->winPtr)) {
            InvalidateSubTree(wmPtr->winPtr, colormap);
        }
    }
}

/*
 *----------------------------------------------------------------------
 *
 * InvalidateSubTree --
 *
 *      This function recursively generates damage for a window and
 *      all of its mapped children that belong to the same toplevel and
 *      are using the specified colormap.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Generates damage for the specified subtree.
 *
 *----------------------------------------------------------------------
 */

static void
InvalidateSubTree(winPtr, colormap)
    TkWindow *winPtr;
    Colormap colormap;
{

    TkWindow *childPtr;

    /*
     * Generate damage for the current window if it is using the
     * specified colormap.
     */

#ifdef DEBUG
    printf("InvalidateSubTree win %x, cmap %x\n", winPtr, colormap);
#endif

    if (winPtr->atts.colormap == colormap) {
        WinInvalidateRect(TkOS2GetHWND(winPtr->window), NULL, FALSE);
    }
 
     for (childPtr = winPtr->childList; childPtr != NULL;
             childPtr = childPtr->nextPtr) {
         /*
          * We can stop the descent when we hit an unmapped or
          * toplevel window.
          */
 
         if (!Tk_IsTopLevel(childPtr) && Tk_IsMapped(childPtr)) {
#ifdef DEBUG
             printf("    recurse from %x to %x\n", winPtr, childPtr);
#endif
             InvalidateSubTree(childPtr, colormap);
         }
     }
}

/*
 *----------------------------------------------------------------------
 *
 * TkOS2GetSystemPalette --
 *
 *	Retrieves the currently installed foreground palette.
 *
 * Results:
 *	Returns the global foreground palette, if there is one.
 *	Otherwise, returns NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

HPAL
TkOS2GetSystemPalette()
{
    return systemPalette;
}

/*
 *----------------------------------------------------------------------
 *
 * GetMinSize --
 *
 *      This procedure computes the current minWidth and minHeight
 *      values for a window, taking into account the possibility
 *      that they may be defaulted.
 *
 * Results:
 *      The values at *minWidthPtr and *minHeightPtr are filled
 *      in with the minimum allowable dimensions of wmPtr's window,
 *      in grid units.  If the requested minimum is smaller than the
 *      system required minimum, then this procedure computes the
 *      smallest size that will satisfy both the system and the
 *      grid constraints.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
GetMinSize(wmPtr, minWidthPtr, minHeightPtr)
    WmInfo *wmPtr;              /* Window manager information for the
                                 * window. */
    int *minWidthPtr;           /* Where to store the current minimum
                                 * width of the window. */
    int *minHeightPtr;          /* Where to store the current minimum
                                 * height of the window. */
{
    int tmp, base;
    TkWindow *winPtr = wmPtr->winPtr;

#ifdef DEBUG
    printf("GetMinSize\n");
#endif

    /*
     * Compute the minimum width by taking the default client size
     * and rounding it up to the nearest grid unit.  Return the greater
     * of the default minimum and the specified minimum.
     */

    tmp = wmPtr->defMinWidth - wmPtr->borderWidth;
    if (tmp < 0) {
        tmp = 0;
    }
    if (wmPtr->gridWin != NULL) {
        base = winPtr->reqWidth - (wmPtr->reqGridWidth * wmPtr->widthInc);
        if (base < 0) {
            base = 0;
        }
        tmp = ((tmp - base) + wmPtr->widthInc - 1)/wmPtr->widthInc;
    }
    if (tmp < wmPtr->minWidth) {
        tmp = wmPtr->minWidth;
    }
    *minWidthPtr = tmp;

    /*
     * Compute the minimum height in a similar fashion.
     */

    tmp = wmPtr->defMinHeight - wmPtr->borderHeight;
    if (tmp < 0) {
        tmp = 0;
    }
    if (wmPtr->gridWin != NULL) {
        base = winPtr->reqHeight - (wmPtr->reqGridHeight * wmPtr->heightInc);
        if (base < 0) {
            base = 0;
        }
        tmp = ((tmp - base) + wmPtr->heightInc - 1)/wmPtr->heightInc;
    }
    if (tmp < wmPtr->minHeight) {
        tmp = wmPtr->minHeight;
    }
    *minHeightPtr = tmp;
}

/*
 *----------------------------------------------------------------------
 *
 * GetMaxSize --
 *
 *      This procedure computes the current maxWidth and maxHeight
 *      values for a window, taking into account the possibility
 *      that they may be defaulted.
 *
 * Results:
 *      The values at *maxWidthPtr and *maxHeightPtr are filled
 *      in with the maximum allowable dimensions of wmPtr's window,
 *      in grid units.  If no maximum has been specified for the
 *      window, then this procedure computes the largest sizes that
 *      will fit on the screen.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
GetMaxSize(wmPtr, maxWidthPtr, maxHeightPtr)
    WmInfo *wmPtr;              /* Window manager information for the
                                 * window. */
    int *maxWidthPtr;           /* Where to store the current maximum
                                 * width of the window. */
    int *maxHeightPtr;          /* Where to store the current maximum
                                 * height of the window. */
{
    int tmp;

#ifdef DEBUG
    printf("GetMaxSize\n");
#endif

    if (wmPtr->maxWidth > 0) {
        *maxWidthPtr = wmPtr->maxWidth;
    } else {
        /*
         * Must compute a default width.  Fill up the display, leaving a
         * bit of extra space for the window manager's borders.
         */

        tmp = wmPtr->defMaxWidth - wmPtr->borderWidth;
        if (wmPtr->gridWin != NULL) {
            /*
             * Gridding is turned on;  convert from pixels to grid units.
             */

            tmp = wmPtr->reqGridWidth
                    + (tmp - wmPtr->winPtr->reqWidth)/wmPtr->widthInc;
        }
        *maxWidthPtr = tmp;
    }
    if (wmPtr->maxHeight > 0) {
        *maxHeightPtr = wmPtr->maxHeight;
    } else {
        tmp = wmPtr->defMaxHeight - wmPtr->borderHeight;
        if (wmPtr->gridWin != NULL) {
            tmp = wmPtr->reqGridHeight
                    + (tmp - wmPtr->winPtr->reqHeight)/wmPtr->heightInc;
        }
        *maxHeightPtr = tmp;
    }
}
