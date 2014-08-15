/* 
 * tkOS2Cursor.c --
 *
 *	This file contains OS/2 PM specific cursor related routines.
 *	Note: cursors for the mouse are called "POINTER" in Presentation
 *	Manager, those in text are "CURSOR".
 *
 * Copyright (c) 1996-1997 Illya Vaes
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */


#include "tkOS2Int.h"

/*
 * The following data structure contains the system specific data
 * necessary to control OS/2 PM pointers.
 */

typedef struct {
    TkCursor info;		/* Generic cursor info used by tkCursor.c */
    HPOINTER OS2Pointer;	/* OS/2 PM pointer handle */
    int system;			/* 1 if cursor is a system cursor, else 0. */
} TkOS2Cursor;

/*
 * The table below is used to map from the name of a predefined cursor
 * to its resource identifier.
 */

static struct CursorName {
    char *name;
    LONG id;
} cursorNames[] = {
/*    {"X_cursor",		SPTR_ARROW}, */
    {"arrow",			SPTR_ARROW},
/*    {"based_arrow_down",	SPTR_SIZENS}, */
/*    {"based_arrow_up",		SPTR_SIZENS}, */
/*    {"bottom_left_corner",	SPTR_SIZENESW}, */
/*    {"bottom_right_corner",	SPTR_SIZENWSE}, */
/*    {"bottom_side",		SPTR_SIZENS}, */
/*    {"bottom_tee",		SPTR_SIZENS}, */
/*    {"center_ptr",		SPTR_MOVE}, */
/*    {"clock",			SPTR_WAIT}, */
/*    {"cross",			SPTR_ARROW}, */
/*    {"cross_reverse",		SPTR_ARROW}, */
/*    {"crosshair",		SPTR_TEXT}, */
/*    {"diamond_cross",		SPTR_ARROW}, */
    {"double_arrow",		SPTR_SIZEWE},
    {"fleur",			SPTR_MOVE},
    {"ibeam",			SPTR_TEXT},
    {"left_ptr",		SPTR_ARROW},
    {"left_side",		SPTR_SIZEWE},
    {"left_tee",		SPTR_SIZEWE},
/*    {"mouse",			SPTR_ARROW}, */
    {"no",			SPTR_ILLEGAL},
/*    {"plus",			SPTR_ARROW}, */
/*    {"question_arrow",		SPTR_QUESICON}, */
/*    {"right_ptr",		SPTR_SIZEWE}, */
/*    {"right_side",		SPTR_SIZEWE}, */
/*    {"right_tee",		SPTR_SIZEWE}, */
/*    {"sb_down_arrow",		SPTR_SIZENS}, */
    {"sb_h_double_arrow",	SPTR_SIZEWE},
/*    {"sb_left_arrow",		SPTR_SIZEWE}, */
/*    {"sb_right_arrow",		SPTR_SIZEWE}, */
/*    {"sb_up_arrow",		SPTR_SIZENS}, */
    {"sb_v_double_arrow",	SPTR_SIZENS},
    {"size_nw_se",		SPTR_SIZENWSE},
    {"size_ne_sw",		SPTR_SIZENESW},
    {"size",			SPTR_MOVE},
    {"starting",		SPTR_WAIT},
/*    {"target",			SPTR_SIZE}, */
/*    {"tcross",			SPTR_ARROW}, */
/*    {"top_left_arrow",		SPTR_ARROW}, */
/*    {"top_left_corner",		SPTR_SIZENWSE}, */
/*    {"top_right_corner",	SPTR_SIZENESW}, */
/*    {"top_side",		SPTR_SIZENS}, */
/*    {"top_tee",			SPTR_SIZENS}, */
/*    {"ul_angle",		SPTR_SIZENWSE}, */
    {"uparrow",			SPTR_SIZENS},
/*    {"ur_angle",		SPTR_SIZENESW}, */
    {"watch",			SPTR_WAIT},
    {"wait",			SPTR_WAIT},
    {"xterm",			SPTR_TEXT},
    /* cursors without moderately reasonable equivalents: question mark icon */
/*
    {"boat",			SPTR_QUESICON},
    {"bogosity",		SPTR_QUESICON},
    {"box_spiral",		SPTR_QUESICON},
    {"circle",			SPTR_QUESICON},
    {"coffee_mug",		SPTR_QUESICON},
    {"dot",			SPTR_QUESICON},
    {"dotbox",			SPTR_QUESICON},
    {"draft_large",		SPTR_QUESICON},
    {"draft_small",		SPTR_QUESICON},
    {"draped_box",		SPTR_QUESICON},
    {"exchange",		SPTR_QUESICON},
    {"gobbler",			SPTR_QUESICON},
    {"gumby",			SPTR_QUESICON},
    {"hand1",			SPTR_QUESICON},
    {"hand2",			SPTR_QUESICON},
    {"heart",			SPTR_QUESICON},
    {"icon",			SPTR_QUESICON},
    {"iron_cross",		SPTR_QUESICON},
    {"leftbutton",		SPTR_QUESICON},
    {"ll_angle",		SPTR_QUESICON},
    {"lr_angle",		SPTR_QUESICON},
    {"man",			SPTR_QUESICON},
    {"middlebutton",		SPTR_QUESICON},
    {"pencil",			SPTR_QUESICON},
    {"pirate",			SPTR_QUESICON},
    {"rightbutton",		SPTR_QUESICON},
    {"rtl_logo",		SPTR_QUESICON},
    {"sailboat",		SPTR_QUESICON},
    {"shuttle",			SPTR_QUESICON},
    {"spider",			SPTR_QUESICON},
    {"spraycan",		SPTR_QUESICON},
    {"star",			SPTR_QUESICON},
    {"trek",			SPTR_QUESICON},
    {"umbrella",		SPTR_QUESICON},
*/
    {NULL,			0}
};
/* Include cursors; done by Ilya Zakharevich */
#include "rc/cursors.h"

/*
 * The default cursor is used whenever no other cursor has been specified.
 */

#define TK_DEFAULT_CURSOR	SPTR_ARROW


/*
 *----------------------------------------------------------------------
 *
 * TkGetCursorByName --
 *
 *	Retrieve a system cursor by name.  
 *
 * Results:
 *	Returns a new cursor, or NULL on errors.  
 *
 * Side effects:
 *	Allocates a new cursor.
 *
 *----------------------------------------------------------------------
 */

TkCursor *
TkGetCursorByName(interp, tkwin, string)
    Tcl_Interp *interp;		/* Interpreter to use for error reporting. */
    Tk_Window tkwin;		/* Window in which cursor will be used. */
    Tk_Uid string;		/* Description of cursor.  See manual entry
				 * for details on legal syntax. */
{
    struct CursorName *namePtr;
    TkOS2Cursor *cursorPtr;

#ifdef DEBUG
    printf("TkGetCursorByName %s\n", string);
#endif

    /*
     * Check for the cursor in the system cursor set.
     */

    for (namePtr = cursorNames; namePtr->name != NULL; namePtr++) {
	if (strcmp(namePtr->name, string) == 0) {
	    break;
	}
    }

    cursorPtr = (TkOS2Cursor *) ckalloc(sizeof(TkOS2Cursor));
    cursorPtr->info.cursor = (Tk_Cursor) cursorPtr;
    if (namePtr->name != NULL) {
	/* Found a system cursor, make a reference (not a copy) */
	cursorPtr->OS2Pointer = WinQuerySysPointer(HWND_DESKTOP, namePtr->id,
	                                           FALSE);
	cursorPtr->system = 1;
    } else {
	myCursor *curPtr = cursors;

	/* Added X-derived cursors by Ilya Zakharevich */
	cursorPtr->system = 0;
	while (curPtr->name) {
	    if (strcmp(curPtr->name, string) == 0) {
	        break;
	    }
	    curPtr++;
	}
	if (curPtr->name &&
	    (cursorPtr->OS2Pointer = WinLoadPointer(HWND_DESKTOP,
	                                            TkOS2GetTkModule(),
	                                            curPtr->id))
            != NULLHANDLE) {
           cursorPtr->system = 0;
        } else {
	    /* Try a system cursor, make a reference (not a copy) */
	    cursorPtr->OS2Pointer = WinQuerySysPointer(HWND_DESKTOP,
	                                               SPTR_QUESICON, FALSE);
	    cursorPtr->system = 1;
        }
    }
    if (cursorPtr->OS2Pointer == NULLHANDLE) {
	ckfree((char *)cursorPtr);
	Tcl_AppendResult(interp, "bad cursor spec \"", string, "\"",
		(char *) NULL);
	return NULL;
    } else {
	return (TkCursor *) cursorPtr;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkCreateCursorFromData --
 *
 *	Creates a cursor from the source and mask bits.
 *
 * Results:
 *	Returns a new cursor, or NULL on errors.
 *
 * Side effects:
 *	Allocates a new cursor.
 *
 *----------------------------------------------------------------------
 */

TkCursor *
TkCreateCursorFromData(tkwin, source, mask, width, height, xHot, yHot,
	fgColor, bgColor)
    Tk_Window tkwin;		/* Window in which cursor will be used. */
    char *source;		/* Bitmap data for cursor shape. */
    char *mask;			/* Bitmap data for cursor mask. */
    int width, height;		/* Dimensions of cursor. */
    int xHot, yHot;		/* Location of hot-spot in cursor. */
    XColor fgColor;		/* Foreground color for cursor. */
    XColor bgColor;		/* Background color for cursor. */
{
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TkFreeCursor --
 *
 *	This procedure is called to release a cursor allocated by
 *	TkGetCursorByName.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The cursor data structure is deallocated.
 *
 *----------------------------------------------------------------------
 */

void
TkFreeCursor(cursorPtr)
    TkCursor *cursorPtr;
{
    TkOS2Cursor *OS2PointerPtr = (TkOS2Cursor *) cursorPtr;
    ckfree((char *) OS2PointerPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TkOS2UpdateCursor --
 *
 *	Set the PM global cursor to the cursor associated with the given
 *      Tk window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Changes the mouse cursor.
 *
 *----------------------------------------------------------------------
 */

void
TkOS2UpdateCursor(winPtr)
    TkWindow *winPtr;
{
    HPOINTER cursor = NULLHANDLE;

#ifdef DEBUG
    printf("TkOS2UpdateCursor winPtr %x\n", winPtr);
#endif

    /*
     * A window inherits its cursor from its parent if it doesn't
     * have one of its own.  Top level windows inherit the default
     * cursor.
     */

    while (winPtr != NULL) {
#ifdef DEBUG
        printf("    winPtr->atts %x\n", winPtr->atts);
        printf("    winPtr->atts.cursor %x\n", winPtr->atts.cursor);
#endif
	if (winPtr->atts.cursor != None) {
	    cursor = ((TkOS2Cursor *) (winPtr->atts.cursor))->OS2Pointer;
	    break;
	} else if (winPtr->flags & TK_TOP_LEVEL) {
	    cursor = WinQuerySysPointer(HWND_DESKTOP, TK_DEFAULT_CURSOR, FALSE);
	    break;
	}
	winPtr = winPtr->parentPtr;
#ifdef DEBUG
        printf("    winPtr %x\n", winPtr);
#endif
    }
#ifdef DEBUG
    printf("    cursor %x\n", cursor);
#endif
    if (cursor != NULLHANDLE) {
	WinSetPointer(HWND_DESKTOP, cursor);
    }
}
