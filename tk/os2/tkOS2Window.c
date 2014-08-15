/* 
 * tkOS2Window.c --
 *
 *	Xlib emulation routines for OS/2 Presentation Manager related to
 *	creating, displaying and destroying windows.
 *
 * Copyright (c) 1996-1997 Illya Vaes
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */


#include "tkOS2Int.h"

/*
 * Forward declarations for procedures defined in this file:
 */

static void             NotifyVisibility _ANSI_ARGS_((XEvent *eventPtr,
                            TkWindow *winPtr));

/*
 *----------------------------------------------------------------------
 *
 * TkMakeWindow --
 *
 *	Creates an OS/2 PM window object based on the current attributes
 *	of the specified TkWindow.
 *
 * Results:
 *	Returns a pointer to a new TkOS2Drawable cast to a Window.
 *
 * Side effects:
 *	Creates a new window.
 *
 *----------------------------------------------------------------------
 */

Window
TkMakeWindow(winPtr, parent)
    TkWindow *winPtr;
    Window parent;
{
    HWND parentWin;
    ULONG yPos;
    TkOS2Drawable *todPtr;
    int style;
    
#ifdef DEBUG
    printf("TkMakeWindow winPtr %x, parent %x; (%d,%d) %dx%d\n", winPtr,
           parent, winPtr->changes.x, winPtr->changes.y,
           winPtr->changes.width, winPtr->changes.height);
#endif

    todPtr = (TkOS2Drawable*) ckalloc(sizeof(TkOS2Drawable));
    if (todPtr == NULL) {
	return (Window)None;
    }
#ifdef DEBUG
    printf("TkMakeWindow: created todPtr %x\n", todPtr);
#endif

    todPtr->type = TOD_WINDOW;
    todPtr->window.winPtr = winPtr;

    if (parent != None) {
	parentWin = TkOS2GetHWND(parent);
	style = WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
        yPos = TkOS2WindowHeight((TkOS2Drawable *)parent) -
               (  winPtr->changes.y + winPtr->changes.height );
    } else {
	parentWin = HWND_DESKTOP;
	style = WS_VISIBLE | WS_CLIPCHILDREN | CS_PARENTCLIP;
        yPos = yScreen - (  winPtr->changes.y + winPtr->changes.height );
    }

    /* Translate Y coordinates to PM */
    /* Use FID_CLIENT in order to get activation right later! */
    todPtr->window.handle = WinCreateWindow(parentWin, TOC_CHILD, "", style,
            winPtr->changes.x, yPos,
            winPtr->changes.width, winPtr->changes.height,
            NULLHANDLE, HWND_TOP, FID_CLIENT, (PVOID)todPtr, NULL);

#ifdef DEBUG
    if (todPtr->window.handle == NULLHANDLE) {
        printf("MakeWindow: WinCreateWindow (parent %x) ERROR %x\n", parentWin,
               WinGetLastError(hab));
    } else {
        printf("MakeWindow: WinCreateWindow: %x (parent %x, flags %x), %d %d %d %d\n",
               todPtr->window.handle, parentWin, todPtr->window.winPtr->flags,
               winPtr->changes.x, yPos, winPtr->changes.width,
               winPtr->changes.height);
    }
#endif

    if (todPtr->window.handle == NULLHANDLE) {
	ckfree((char *) todPtr);
	todPtr = NULL;
    }

    return (Window)todPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * XDestroyWindow --
 *
 *	Destroys the given window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Sends the WM_DESTROY message to the window and then destroys
 *	the resources associated with the window.
 *
 *----------------------------------------------------------------------
 */

void
XDestroyWindow(display, w)
    Display* display;
    Window w;
{
    TkOS2Drawable *todPtr = (TkOS2Drawable *)w;
    TkWindow *winPtr = TkOS2GetWinPtr(w);
    HWND hwnd = TkOS2GetHWND(w);

#ifdef DEBUG
    printf("XDestroyWindow handle %x, winPtr->flags %x\n", hwnd, winPtr->flags);
#endif

    display->request++;

    /*
     * Remove references to the window in the pointer module, and
     * then remove the backpointer from the drawable.
     */

    TkOS2PointerDeadWindow(winPtr);
    todPtr->window.winPtr = NULL;

    /*
     * Don't bother destroying the window if we are going to destroy
     * the parent later.  Also if the window has already been destroyed
     * then we need to free the drawable now.
     */

    if (!hwnd) {
#ifdef DEBUG
        printf("    ckfree todPtr %x\n", todPtr);
#endif
        ckfree((char *)todPtr);
        todPtr= NULL;
    } else if (!(winPtr->flags & TK_PARENT_DESTROYED)) {
        rc = WinDestroyWindow(hwnd);
#ifdef DEBUG
        if (rc != TRUE) {
            printf("WinDestroyWindow hwnd %x ERROR %x\n", hwnd,
                   WinGetLastError(hab));
        } else {
            printf("WinDestroyWindow hwnd %x OK\n", hwnd);
        }
#endif
    }
}

/*
 *----------------------------------------------------------------------
 *
 * XMapWindow --
 *
 *	Cause the given window to become visible.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	Causes the window state to change, and generates a MapNotify
 *	event.
 *
 *----------------------------------------------------------------------
 */

void
XMapWindow(display, w)
    Display* display;
    Window w;
{
    XEvent event;
    TkWindow *parentPtr;
    TkWindow *winPtr = TkOS2GetWinPtr(w);

#ifdef DEBUG
    printf("XMapWindow %x\n", TkOS2GetHWND(w));
#endif

    display->request++;

    WinShowWindow(TkOS2GetHWND(w), TRUE);
    winPtr->flags |= TK_MAPPED;

    event.type = MapNotify;
    event.xmap.serial = display->request;
    event.xmap.send_event = False;
#ifdef DEBUG
    printf("    display %x\n", display);
#endif
    event.xmap.display = display;
    event.xmap.event = winPtr->window;
    event.xmap.window = winPtr->window;
    event.xmap.override_redirect = winPtr->atts.override_redirect;
    Tk_QueueWindowEvent(&event, TCL_QUEUE_TAIL);

    /*
     * Check to see if this window is visible now.  If all of the parent
     * windows up to the first toplevel are mapped, then this window and
     * its mapped children have just become visible.
     */

    if (!(winPtr->flags & TK_TOP_LEVEL)) {
        for (parentPtr = winPtr->parentPtr; ;
                parentPtr = parentPtr->parentPtr) {
            if ((parentPtr == NULL) || !(parentPtr->flags & TK_MAPPED)) {
                return;
            }
            if (parentPtr->flags & TK_TOP_LEVEL) {
                break;
            }
        }
    }

    /*
     * Generate VisibilityNotify events for this window and its mapped
     * children.
     */

    event.type = VisibilityNotify;
    event.xvisibility.state = VisibilityUnobscured;
    NotifyVisibility(&event, winPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * NotifyVisibility --
 *
 *      This function recursively notifies the mapped children of the
 *      specified window of a change in visibility.  Note that we don't
 *      properly report the visibility state, since OS/2 does not
 *      provide that info.  The eventPtr argument must point to an event
 *      that has been completely initialized except for the window slot.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Generates lots of events.
 *
 *----------------------------------------------------------------------
 */

static void
NotifyVisibility(eventPtr, winPtr)
    XEvent *eventPtr;           /* Initialized VisibilityNotify event. */
    TkWindow *winPtr;           /* Window to notify. */
{
#ifdef DEBUG
    printf("NotifyVisibility\n");
#endif
    eventPtr->xvisibility.window = winPtr->window;
    Tk_QueueWindowEvent(eventPtr, TCL_QUEUE_TAIL);
    for (winPtr = winPtr->childList; winPtr != NULL;
            winPtr = winPtr->nextPtr) {
        if (winPtr->flags & TK_MAPPED) {
            NotifyVisibility(eventPtr, winPtr);
        }
    }
}

/*
 *----------------------------------------------------------------------
 *
 * XUnmapWindow --
 *
 *	Cause the given window to become invisible.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	Causes the window state to change, and generates an UnmapNotify
 *	event.
 *
 *----------------------------------------------------------------------
 */

void
XUnmapWindow(display, w)
    Display* display;
    Window w;
{
    XEvent event;
    TkWindow *winPtr = TkOS2GetWinPtr(w);
#ifdef DEBUG
    printf("XUnmapWindow hwnd %x\n", TkOS2GetHWND(w));
#endif

    display->request++;

    WinShowWindow(TkOS2GetHWND(w), FALSE);
    winPtr->flags &= ~TK_MAPPED;

    event.type = UnmapNotify;
    event.xunmap.serial = display->request;
    event.xunmap.send_event = False;
#ifdef DEBUG
    printf("    display %x\n", display);
#endif
    event.xunmap.display = display;
    event.xunmap.event = winPtr->window;
    event.xunmap.window = winPtr->window;
    event.xunmap.from_configure = False;
    Tk_QueueWindowEvent(&event, TCL_QUEUE_TAIL);
}

/*
 *----------------------------------------------------------------------
 *
 * XMoveResizeWindow --
 *
 *	Move and resize a window relative to its parent.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Repositions and resizes the specified window.
 *
 *----------------------------------------------------------------------
 */

void
XMoveResizeWindow(display, w, x, y, width, height)
    Display* display;
    Window w;
    int x;			/* Position relative to parent. */
    int y;
    unsigned int width;
    unsigned int height;
{
    SWP parPos;
    WinQueryWindowPos(WinQueryWindow(TkOS2GetHWND(w), QW_PARENT), &parPos);
    display->request++;
    /* Translate Y coordinates to PM: relative to parent */
    WinSetWindowPos(TkOS2GetHWND(w), HWND_TOP, x,
                    parPos.cy - height - y,
                    width, height, SWP_MOVE | SWP_SIZE);
#ifdef DEBUG
    printf("XMoveResizeWindow hwnd %x, x %d, y %d (x11y: %d), w %d, h %d\n",
           TkOS2GetHWND(w), x, parPos.cy - height - y, y, width, height);
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * XMoveWindow --
 *
 *	Move a window relative to its parent.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Repositions the specified window.
 *
 *----------------------------------------------------------------------
 */

void
XMoveWindow(display, w, x, y)
    Display* display;
    Window w;
    int x;
    int y;
{
    TkWindow *winPtr = TkOS2GetWinPtr(w);
    SWP parPos;
    WinQueryWindowPos(WinQueryWindow(TkOS2GetHWND(w), QW_PARENT), &parPos);

    display->request++;

    /* Translate Y coordinates to PM, relative to parent */
    WinSetWindowPos(TkOS2GetHWND(w), HWND_TOP, x,
                    parPos.cy - winPtr->changes.height - y,
                    winPtr->changes.width, winPtr->changes.height,
                    SWP_MOVE /*| SWP_SIZE*/);
#ifdef DEBUG
    printf("XMoveWindow hwnd %x, x %d, y %d, w %d, h %d\n", TkOS2GetHWND(w),
           x, parPos.cy - winPtr->changes.height - y,
           winPtr->changes.width, winPtr->changes.height);
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * XResizeWindow --
 *
 *	Resize a window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Resizes the specified window.
 *
 *----------------------------------------------------------------------
 */

void
XResizeWindow(display, w, width, height)
    Display* display;
    Window w;
    unsigned int width;
    unsigned int height;
{
    TkWindow *winPtr = TkOS2GetWinPtr(w);
    SWP parPos;
    WinQueryWindowPos(WinQueryWindow(TkOS2GetHWND(w), QW_PARENT), &parPos);

    display->request++;

    /* Translate Y coordinates to PM; relative to parent */
    WinSetWindowPos(TkOS2GetHWND(w), HWND_TOP, winPtr->changes.x,
                    parPos.cy - winPtr->changes.height - winPtr->changes.y,
                    winPtr->changes.width, winPtr->changes.height,
                    /*SWP_MOVE |*/ SWP_SIZE);
#ifdef DEBUG
    printf("XResizeWindow hwnd %x, x %d, y %d, w %d, h %d\n", TkOS2GetHWND(w),
           winPtr->changes.x,
           parPos.cy - winPtr->changes.height - winPtr->changes.y,
           winPtr->changes.width, winPtr->changes.height);
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * XRaiseWindow --
 *
 *	Change the stacking order of a window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Changes the stacking order of the specified window.
 *
 *----------------------------------------------------------------------
 */

void
XRaiseWindow(display, w)
    Display* display;
    Window w;
{
    HWND window = TkOS2GetHWND(w);

#ifdef DEBUG
    printf("XRaiseWindow hwnd %x\n", window);
#endif

    display->request++;
    rc = WinSetWindowPos(window, HWND_TOP, 0, 0, 0, 0, SWP_ZORDER);
#ifdef DEBUG
    if (rc!=TRUE) {
        printf("    WinSetWindowPos ERROR %x\n", WinGetLastError(hab));
    } else {
        printf("    WinSetWindowPos OK\n");
    }
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * XConfigureWindow --
 *
 *	Change the size, position, stacking, or border of the specified
 *	window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Changes the attributes of the specified window.  Note that we
 *	ignore the passed in values and use the values stored in the
 *	TkWindow data structure.
 *
 *----------------------------------------------------------------------
 */

void
XConfigureWindow(display, w, value_mask, values)
    Display* display;
    Window w;
    unsigned int value_mask;
    XWindowChanges* values;
{
    TkWindow *winPtr = TkOS2GetWinPtr(w);
    HWND window = TkOS2GetHWND(w);
    HWND insertAfter;

#ifdef DEBUG
    SWP pos;
    WinQueryWindowPos(window, &pos);
    printf("XConfigureWindow %x, pos (%d,%d;%dx%d)\n", window, pos.x, pos.y,
           pos.cx, pos.cy);
#endif

    display->request++;

    /*
     * Change the shape and/or position of the window.
     */

    if (value_mask & (CWX|CWY|CWWidth|CWHeight)) {
        /* Translate Y coordinates to PM */
        WinSetWindowPos(window, HWND_TOP, winPtr->changes.x,
                        TkOS2WindowHeight((TkOS2Drawable *)w)
                                 - winPtr->changes.height - winPtr->changes.y,
                        winPtr->changes.width, winPtr->changes.height,
                        SWP_MOVE | SWP_SIZE);
#ifdef DEBUG
    printf("    CWX/CWY    hwnd %x, x %d, y %d, w %d, h %d\n", window,
           winPtr->changes.x,
           TkOS2WindowHeight((TkOS2Drawable *)w)
                             - winPtr->changes.height - winPtr->changes.y,
           winPtr->changes.width, winPtr->changes.height);
#endif
    }

    /*
     * Change the stacking order of the window.
     */

    if (value_mask & CWStackMode) {
	if ((value_mask & CWSibling) && (values->sibling != None)) {
	    HWND sibling = TkOS2GetHWND(values->sibling);
#ifdef DEBUG
            printf("    CWStackMode\n");
#endif

	    /*
	     * OS/2 PM doesn't support the Above mode, so we insert the
	     * window just below the sibling and then swap them.
	     */

	    if (values->stack_mode == Above) {
                WinSetWindowPos(window, sibling, 0, 0, 0, 0, SWP_ZORDER);
		insertAfter = window;
		window = sibling;
	    } else {
		insertAfter = sibling;
	    }
	} else {
	    insertAfter = (values->stack_mode == Above) ? HWND_TOP
		: HWND_BOTTOM;
	}
		
	WinSetWindowPos(window, insertAfter, 0, 0, 0, 0, SWP_ZORDER);
    } 
#ifdef DEBUG
    WinQueryWindowPos(window, &pos);
    printf("After XConfigureWindow %x, pos (%d,%d;%dx%d)\n", window,
           pos.x, pos.y, pos.cx, pos.cy);
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * XClearWindow --
 *
 *	Clears the entire window to the current background color.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Erases the current contents of the window.
 *
 *----------------------------------------------------------------------
 */

void
XClearWindow(display, w)
    Display* display;
    Window w;
{
    RECTL rect;
    LONG oldColor, oldPattern;
    HPAL oldPalette, palette;
    TkWindow *winPtr;
    HWND hwnd = TkOS2GetHWND(w);
    HPS hps = WinGetPS(hwnd);

#ifdef DEBUG
    printf("XClearWindow\n");
#endif

    palette = TkOS2GetPalette(display->screens[0].cmap);
    oldPalette = GpiSelectPalette(hps, palette);

    display->request++;

    winPtr = TkOS2GetWinPtr(w);
    oldColor = GpiQueryColor(hps);
    oldPattern = GpiQueryPattern(hps);
    GpiSetPattern(hps, PATSYM_SOLID);
    WinQueryWindowRect(hwnd, &rect);
    WinFillRect(hps, &rect, winPtr->atts.background_pixel);
#ifdef DEBUG
    printf("WinFillRect in XClearWindow\n");
#endif
    GpiSetPattern(hps, oldPattern);
    GpiSelectPalette(hps, oldPalette);
    WinReleasePS(hps);
}

/*
 *----------------------------------------------------------------------
 *
 * XChangeWindowAttributes --
 *
 *      This function is called when the attributes on a window are
 *      updated.  Since Tk maintains all of the window state, the only
 *      relevant value is the cursor.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      May cause the mouse position to be updated.
 *
 *----------------------------------------------------------------------
 */

void
XChangeWindowAttributes(display, w, valueMask, attributes)
    Display* display;
    Window w;
    unsigned long valueMask;
    XSetWindowAttributes* attributes;
{
#ifdef DEBUG
    printf("XChangeWindowAttributes\n");
#endif
    if (valueMask & CWCursor) {
        XDefineCursor(display, w, attributes->cursor);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkOS2WindowHeight --
 *
 *      Determine the height of an OS/2 drawable (of parent for bitmaps).
 *
 * Results:
 *      Height of drawable.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

LONG
TkOS2WindowHeight(todPtr)
    TkOS2Drawable *todPtr;
{
    SWP pos;
    HWND handle;
    HWND parent;
    BOOL rc;

    if (todPtr->type == TOD_BITMAP ) {
        SIZEL sizl;
        /* Bitmap */
        handle = todPtr->bitmap.parent;
        parent = handle;
#ifdef DEBUG
        printf("TkOS2WindowHeight: bitmap %x, parent %x", todPtr->bitmap.handle,
               handle);
#endif
        rc = GpiQueryBitmapDimension(todPtr->bitmap.handle, &sizl);
#ifdef DEBUG
        printf(", bitmapdimension y %d\n", sizl.cy);
#endif
        return sizl.cy;
    } else {
        handle = todPtr->window.handle;
        parent = WinQueryWindow(handle, QW_PARENT);
#ifdef DEBUG
        printf("TkOS2WindowHeight: window %x, parent %x", handle, parent);
#endif
    }
    rc = WinQueryWindowPos(handle, &pos);
    if (rc != TRUE) {
#ifdef DEBUG
        printf("\nWinQueryWindowPos ERROR %x\n", WinGetLastError(hab));
#endif
        return 0;
#ifdef DEBUG
    } else {
        printf(" %d,%d (%dx ==>%d<==)\n", pos.x, pos.y, pos.cx, pos.cy);
#endif
    }
    /* Watch out for frames and/or title bars! */
    if (parent == HWND_DESKTOP) {
#ifdef DEBUG
        printf("    parent == HWND_DESKTOP, cx %d", pos.cx);
#endif
        if (TkOS2GetWinPtr(todPtr)->wmInfoPtr->exStyle & FCF_SIZEBORDER) {
            pos.cy -= 2 * ySizeBorder;
#ifdef DEBUG
            printf(", SIZEBORDER %d", ySizeBorder);
#endif
        } else if (TkOS2GetWinPtr(todPtr)->wmInfoPtr->exStyle & FCF_DLGBORDER) {
            pos.cy -= 2 * yDlgBorder;
#ifdef DEBUG
            printf(", DLGBORDER %d", yDlgBorder);
#endif
        }
        if (TkOS2GetWinPtr(todPtr)->wmInfoPtr->exStyle & FCF_TITLEBAR) {
            pos.cy -= titleBar;
#ifdef DEBUG
            printf(", TITLEBAR %d", titleBar);
#endif
        }
    }
    return pos.cy;
}

/*
 *----------------------------------------------------------------------
 *
 * TkOS2WindowWidth --
 *
 *      Determine the width of an OS/2 drawable (of parent for bitmaps).
 *
 * Results:
 *      Width of drawable.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

LONG
TkOS2WindowWidth(todPtr)
    TkOS2Drawable *todPtr;
{
    SWP pos;
    HWND handle;
    HWND parent;
    BOOL rc;

    if (todPtr->type == TOD_BITMAP ) {
        SIZEL sizl;

        /* Bitmap */
        handle = todPtr->bitmap.parent;
        parent = handle;
#ifdef DEBUG
        printf("TkOS2WindowWidth: bitmap %x, parent %x", todPtr->bitmap.handle,
               handle);
#endif
        rc = GpiQueryBitmapDimension(todPtr->bitmap.handle, &sizl);
#ifdef DEBUG
        printf(", bitmapdimension x %d\n", sizl.cx);
#endif
        return sizl.cx;
    } else {
        handle = todPtr->window.handle;
        parent = WinQueryWindow(handle, QW_PARENT);
#ifdef DEBUG
        printf("TkOS2WindowWidth: window %x, parent %x", handle, parent);
#endif
    }
    rc = WinQueryWindowPos(handle, &pos);
    if (rc != TRUE) return 0;
#ifdef DEBUG
    printf(" %d,%d (%dx%d)\n", pos.x, pos.y, pos.cx, pos.cy);
#endif
    /* Watch out for frames and/or title bars! */
    if (parent == HWND_DESKTOP) {
#ifdef DEBUG
        printf("    parent == HWND_DESKTOP, cx %d", pos.cx);
#endif
        if (TkOS2GetWinPtr(todPtr)->wmInfoPtr->exStyle & FCF_SIZEBORDER) {
            pos.cx -= 2 * xSizeBorder;
#ifdef DEBUG
            printf(", SIZEBORDER %d", xSizeBorder);
#endif
        } else if (TkOS2GetWinPtr(todPtr)->wmInfoPtr->exStyle & FCF_DLGBORDER) {
            pos.cx -= 2 * xDlgBorder;
#ifdef DEBUG
            printf(", DLGBORDER %d", xDlgBorder);
#endif
        } else if (TkOS2GetWinPtr(todPtr)->wmInfoPtr->exStyle & FCF_BORDER) {
                pos.cx -= 2 * xBorder;
#ifdef DEBUG
            printf(", BORDER %d", xBorder);
#endif
        }
#ifdef DEBUG
        printf("; cx now %d\n", pos.cx);
#endif
    }
    return pos.cx;
}
