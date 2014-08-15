/* 
 * tkOS2Clipboard.c --
 *
 *	This file contains functions for managing the clipboard.
 *
 * Copyright (c) 1996-1997 Illya Vaes
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tkOS2Int.h"
#include "tkSelect.h"

/* Needed for signal-catching after we've become clipboard-owner */
#include <signal.h>
void sighandler(int sig);


/*
 *----------------------------------------------------------------------
 *
 * TkSelGetSelection --
 *
 *	Retrieve the specified selection from another process.  For
 *	now, only fetching XA_STRING from CLIPBOARD is supported.
 *	Eventually other types should be allowed.
 * 
 * Results:
 *	The return value is a standard Tcl return value.
 *	If an error occurs (such as no selection exists)
 *	then an error message is left in interp->result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TkSelGetSelection(interp, tkwin, selection, target, proc, clientData)
    Tcl_Interp *interp;		/* Interpreter to use for reporting
				 * errors. */
    Tk_Window tkwin;		/* Window on whose behalf to retrieve
				 * the selection (determines display
				 * from which to retrieve). */
    Atom selection;		/* Selection to retrieve. */
    Atom target;		/* Desired form in which selection
				 * is to be returned. */
    Tk_GetSelProc *proc;	/* Procedure to call to process the
				 * selection, once it has been retrieved. */
    ClientData clientData;	/* Arbitrary value to pass to proc. */
{
    char *data, *buffer, *destPtr;
    int result, length;

    if ((selection == Tk_InternAtom(tkwin, "CLIPBOARD"))
	    && (target == XA_STRING)) {
#ifdef DEBUG
        printf("TkSelGetSelection, CLIPBOARD\n");
#endif
	if (WinOpenClipbrd(hab)) {
#ifdef DEBUG
            printf("WinOpenClipbrd OK\n");
#endif
	    if ((data= (char *)WinQueryClipbrdData(hab, CF_TEXT))) {
		length = strlen(data);
#ifdef DEBUG
                printf("WinQueryClipbrdData OK: %s (%d)\n", data, length);
#endif
		buffer = ckalloc(length+1);
		destPtr = buffer;
		while (*data != '\0') {
		    if (*data != '\r') {
			*destPtr = *data;
			destPtr++;
		    }
		    data++;
		}
		*destPtr = '\0';
		WinCloseClipbrd(hab);
		result = (*proc)(clientData, interp, buffer);
		ckfree(buffer);
		return result;
	    }
#ifdef DEBUG
            printf("WinQueryClipbrdData ERROR or no such format: %x\n", data);
#endif
	    WinCloseClipbrd(hab);
	}
#ifdef DEBUG
          else {
            printf("WinOpenClipbrd ERROR %x\n", WinGetLastError(hab));
        }
#endif
    }
#ifdef DEBUG
      else {
        printf("TkSelGetSelection, != CLIPBOARD, target %x (%s), selection %x (%s)\n",
               target, Tk_GetAtomName(tkwin, target),
               selection, Tk_GetAtomName(tkwin, selection));
    }
#endif

    Tcl_AppendResult(interp, Tk_GetAtomName(tkwin, selection),
	" selection doesn't exist or form \"", Tk_GetAtomName(tkwin, target),
	"\" not defined", (char *) NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * XSetSelectionOwner --
 *
 *	This function claims ownership of the specified selection.
 *	If the selection is CLIPBOARD, then we empty the system
 *	clipboard.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Empties the system clipboard, and claims ownership.
 *
 *----------------------------------------------------------------------
 */

void
XSetSelectionOwner(display, selection, owner, time)
    Display* display;
    Atom selection;
    Window owner;
    Time time;
{
    HWND hwnd = owner ? TkOS2GetHWND(owner) : NULLHANDLE;
    Tk_Window tkwin;

#ifdef DEBUG
    printf("XSetSelectionOwner\n");
#endif

    /*
     * This is a gross hack because the Tk_InternAtom interface is broken.
     * It expects a Tk_Window, even though it only needs a Tk_Display.
     */

    tkwin = (Tk_Window)tkMainWindowList->winPtr;

    if (selection == Tk_InternAtom(tkwin, "CLIPBOARD")) {

#ifdef DEBUG
        printf("    selection CLIPBOARD\n");
#endif
	/*
	 * Only claim and empty the clipboard if we aren't already the
	 * owner of the clipboard.
	 */

	if (WinQueryClipbrdOwner(hab) != hwnd) {
            rc = WinOpenClipbrd(hab);
#ifdef DEBUG
            if (rc==TRUE) printf("WinOpenClipbrd OK\n");
            else printf("WinOpenClipBrd ERROR %x\n", WinGetLastError(hab));
#endif
            rc = WinEmptyClipbrd(hab);
#ifdef DEBUG
            if (rc==TRUE) printf("WinEmptyClipbrd OK\n");
            else printf("WinEmptyClipBrd ERROR %x\n", WinGetLastError(hab));
#endif
            rc = WinSetClipbrdData(hab, NULLHANDLE, CF_TEXT, CFI_POINTER);
#ifdef DEBUG
            if (rc==TRUE) printf("WinSetClipbrdData OK\n");
            else printf("WinSetClipBrdData ERROR %x\n", WinGetLastError(hab));
#endif
            rc = WinCloseClipbrd(hab);
#ifdef DEBUG
            if (rc==TRUE) printf("WinCloseClipbrd OK\n");
            else printf("WinCloseClipBrd ERROR %x\n", WinGetLastError(hab));
#endif
            /*
             * If we've become owner of the clipboard but are terminated by
             * a signal, the whole system will be hanging waiting for it.
             */
            signal(SIGFPE, sighandler);
            signal(SIGTERM, sighandler);
            signal(SIGSEGV, sighandler);
            rc = WinSetClipbrdOwner(hab, hwnd);
#ifdef DEBUG
            if (rc==TRUE) printf("WinSetClipbrdOwner OK\n");
            else printf("WinSetClipBrdOwner ERROR %x\n", WinGetLastError(hab));
#endif
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * sighandler --
 *
 *	This function is invoked upon a terminating signal, so we can
 *	release the clipboard to the system (no owner).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Relinquishises ownership of the clipboard and exits.
 *
 *----------------------------------------------------------------------
 */

void sighandler(int sig)
{
    rc = WinSetClipbrdOwner(hab, NULLHANDLE);
#ifdef DEBUG
    if (rc==TRUE) printf("\nSIGNAL %d: WinSetClipbrdOwner OK\n", sig);
    else printf("\nSIGNAL %d: WinSetClipBrdOwner ERROR %x\n", sig,
                WinGetLastError(hab));
#endif
    exit(1);
}

/*
 *----------------------------------------------------------------------
 *
 * TkOS2ClipboardRender --
 *
 *	This function supplies the contents of the clipboard in
 *	response to a WM_RENDERFMT message.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Sets the contents of the clipboard.
 *
 *----------------------------------------------------------------------
 */

void
TkOS2ClipboardRender(winPtr, format)
    TkWindow *winPtr;
    ULONG format;
{
    TkClipboardTarget *targetPtr;
    TkClipboardBuffer *cbPtr;
    TkDisplay *dispPtr = winPtr->dispPtr;
    PVOID mem;
    char *buffer;
    int length;

    if (format != CF_TEXT) {
#ifdef DEBUG
        printf("TkOS2ClipboardRender != CF_TEXT (%x)\n", format);
#endif
        return;
    }
#ifdef DEBUG
    printf("TkOS2ClipboardRender CF_TEXT\n");
#endif

    for (targetPtr = dispPtr->clipTargetPtr; targetPtr != NULL;
	    targetPtr = targetPtr->nextPtr) {
	if (targetPtr->type == XA_STRING) {
#ifdef DEBUG
            printf("Found target XA_STRING\n");
#endif
	    break;
        }
    }
    length = 0;
    if (targetPtr != NULL) {
	for (cbPtr = targetPtr->firstBufferPtr; cbPtr != NULL;
		cbPtr = cbPtr->nextPtr) {
	    length += cbPtr->length;
	}
    }
    if ( (rc = DosAllocSharedMem(&mem, NULL, length+1,
              OBJ_GIVEABLE | PAG_COMMIT | PAG_READ | PAG_WRITE)) != 0) {
#ifdef DEBUG
        printf("TkOS2ClipboardRender: DosAllocSharedMem ERROR %x\n", rc);
#endif
	return;
    }
#ifdef DEBUG
    printf("TkOS2ClipboardRender: DosAllocSharedMem %s (%d) OK\n", mem, length+1);
#endif
    buffer= (char *)mem;
    if (targetPtr != NULL) {
	for (cbPtr = targetPtr->firstBufferPtr; cbPtr != NULL;
		cbPtr = cbPtr->nextPtr) {
	    strncpy(buffer, cbPtr->buffer, cbPtr->length);
	    buffer += cbPtr->length;
	}
    }
    *buffer = '\0';
    rc = WinSetClipbrdData(hab, (ULONG)mem, CF_TEXT, CFI_POINTER);
#ifdef DEBUG
    if (rc==TRUE) {
        printf("WinSetClipbrdData OK\n", mem);
    } else {
        printf("WinSetClipbrdData ERROR %x\n", WinGetLastError(hab));
    }
#endif
    return;
}

/*
 *----------------------------------------------------------------------
 *
 * TkSelUpdateClipboard --
 *
 *	This function is called to force the clipboard to be updated
 *	after new data is added.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Clears the current contents of the clipboard.
 *
 *----------------------------------------------------------------------
 */

void
TkSelUpdateClipboard(winPtr, targetPtr)
    TkWindow *winPtr;
    TkClipboardTarget *targetPtr;
{
    /*
    HWND hwnd = TkOS2GetHWND(winPtr->window);
    */

#ifdef DEBUG
    printf("TkSelUpdateClipboard\n");
#endif

    rc = WinOpenClipbrd(hab);
#ifdef DEBUG
    if (rc==TRUE) {
        printf("WinOpenClipbrd OK\n");
    } else {
        printf("WinOpenClipBrd ERROR %x\n", WinGetLastError(hab));
    }
#endif
    rc = WinEmptyClipbrd(hab);
#ifdef DEBUG
    if (rc==TRUE) {
        printf("WinEmptyClipbrd OK\n");
    } else {
        printf("WinEmptyClipBrd ERROR %x\n", WinGetLastError(hab));
    }
#endif
    rc = WinSetClipbrdData(hab, NULLHANDLE, CF_TEXT, CFI_POINTER);
#ifdef DEBUG
    if (rc==TRUE) {
        printf("WinSetClipbrdData OK\n");
    } else {
        printf("WinSetClipBrdData ERROR %x\n", WinGetLastError(hab));
    }
#endif
    rc = WinCloseClipbrd(hab);
#ifdef DEBUG
    if (rc==TRUE) {
        printf("WinCloseClipbrd OK\n");
    } else {
        printf("WinCloseClipBrd ERROR %x\n", WinGetLastError(hab));
    }
#endif
}

/*
 *--------------------------------------------------------------
 *
 * TkSelEventProc --
 *
 *	This procedure is invoked whenever a selection-related
 *	event occurs. 
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Lots:  depends on the type of event.
 *
 *--------------------------------------------------------------
 */

void
TkSelEventProc(tkwin, eventPtr)
    Tk_Window tkwin;		/* Window for which event was
				 * targeted. */
    register XEvent *eventPtr;	/* X event:  either SelectionClear,
				 * SelectionRequest, or
				 * SelectionNotify. */
{
#ifdef DEBUG
printf("TkSelEventProc\n");
#endif

    if (eventPtr->type == SelectionClear) {
	TkSelClearSelection(tkwin, eventPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkSelPropProc --
 *
 *	This procedure is invoked when property-change events
 *	occur on windows not known to the toolkit.  This is a stub
 *	function under OS/2 Presentation Manager.
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
TkSelPropProc(eventPtr)
    register XEvent *eventPtr;		/* X PropertyChange event. */
{
}
