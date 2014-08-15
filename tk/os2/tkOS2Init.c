/* 
 * tkOS2Init.c --
 *
 *	This file contains OS/2-specific interpreter initialization
 *	functions.
 *
 * Copyright (c) 1996-1997 Illya Vaes
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */


#include "tkOS2Int.h"

/*
 * Global variables necessary in modules in the DLL
 */
HAB		hab;			/* Anchor block (instance handle) */
HMQ		hmq;			/* Handle to message queue */
LONG		aDevCaps[CAPS_DEVICE_POLYSET_POINTS];  /* Device Capabilities */
LONG		nextLogicalFont = 1;    /* First free logical font ID */
PFNWP		oldFrameProc = NULL;	/* subclassed frame procedure */
LONG		xScreen;		/* System Value Screen width */
LONG		yScreen;		/* System Value Screen height */
LONG		titleBar;		/* System Value Title Bar */
LONG		xBorder;		/* System Value X nominal border */
LONG		yBorder;		/* System Value Y nominal border */
LONG		xSizeBorder;		/* System Value X Sizing border */
LONG		ySizeBorder;		/* System Value Y Sizing border */
LONG		xDlgBorder;		/* System Value X dialog-frame border */
LONG		yDlgBorder;		/* System Value Y dialog-frame border */
HDC		hScreenDC;		/* Device Context for screen */
HPS		globalPS;		/* Global PS for Fonts (Gpi*Char*) */
HBITMAP		globalBitmap;		/* Bitmap for global PS */
TkOS2Font	logfonts[255];		/* List of logical fonts */
LONG		nextColor;		/* Next free index in color table */
LONG		rc;			/* For checking return values */

/*
 * The following string is the startup script executed in new
 * interpreters.  It looks on disk in several different directories
 * for a script "tk.tcl" that is compatible with this version
 * of Tk.  The tk.tcl script does all of the real work of
 * initialization.
 */
static char *initScript =
"proc init {} {\n\
    global tk_library tk_version tk_patchLevel env\n\
    rename init {}\n\
    set dirs {}\n\
    if [info exists env(TK_LIBRARY)] {\n\
        lappend dirs $env(TK_LIBRARY)\n\
    }\n\
    lappend dirs $tk_library\n\
    lappend dirs [file dirname [info library]]/lib/tk$tk_version\n\
    lappend dirs [file dirname [file dirname [info nameofexecutable]]]/lib/tk$tk_version\n\
    if [string match {*[ab]*} $tk_patchLevel] {\n\
        set lib tk$tk_patchLevel\n\
    } else {\n\
        set lib tk$tk_version\n\
    }\n\
    lappend dirs [file dirname [file dirname [pwd]]]/$lib/library\n\
    lappend dirs [file dirname [pwd]]/library\n\
    foreach i $dirs {\n\
        set tk_library $i\n\
        if ![catch {uplevel #0 source [list $i/tk.tcl]}] {\n\
            return\n\
        }\n\
    }\n\
    set msg \"Can't find a usable tk.tcl in the following directories: \n\"\n\
    append msg \"    $dirs\n\"\n\
    append msg \"This probably means that Tk wasn't installed properly.\n\"\n\
    error $msg\n\
}\n\
init";

/*
 *----------------------------------------------------------------------
 *
 * TkPlatformInit --
 *
 *	Performs OS/2-specific interpreter initialization related to the
 *      tk_library variable.
 *
 * Results:
 *	A standard Tcl completion code (TCL_OK or TCL_ERROR).  Also
 *      leaves information in interp->result.
 *
 * Side effects:
 *	Sets "tk_library" Tcl variable, runs "tk.tcl" script.
 *
 *----------------------------------------------------------------------
 */

int
TkPlatformInit(interp)
    Tcl_Interp *interp;
{
    char *libDir;

/*
 * User profile (OS2.INI) support removed since beta.
 * Some of this will come back in version 8.0 to match the Windows port's
 * "registry" command.
 */
/*    Tcl_DStringInit(&ds);
/*
/*    /*
/*     * Look in the environment and User profile for a TK_LIBRARY
/*     * entry.  Store the result in the dynamic string.
/*     */
/*
/*    ptr = Tcl_GetVar2(interp, "env", "TK_LIBRARY", TCL_GLOBAL_ONLY);
/*    if (ptr == NULL) {
/*        /* Not in environment, set Application */
/*        Tcl_DStringInit(&key);
/*	Tcl_DStringAppend(&key, "Tk for OS/2 PM, v.", -1);
/*	Tcl_DStringAppend(&key, TK_VERSION, -1);
/*
/*	/*
/*	 * Open the User profile file (OS2.INI, handle HINI_USERPROFILE)
/*	 */
/*
/*        /* Query for Key TK_LIBRARY */
/*	Tcl_DStringSetLength(&ds, CCHMAXPATH);
/*        result= PrfQueryProfileData(HINI_USERPROFILE, Tcl_DStringValue(&key),
/*                                   "TK_LIBRARY", Tcl_DStringValue(&ds), &size);
/*        Tcl_DStringFree(&key);
/*
/*	if (result == TRUE) {
/*	    Tcl_DStringSetLength(&ds, size);
/*	} else {
/*	    Tcl_DStringSetLength(&ds, 0);
/*	}
/*    } else {
/*	Tcl_DStringSetLength(&ds, 0);
/*	Tcl_DStringAppend(&ds, ptr, -1);
/*    }
/*    Tcl_DStringFree(&ds);
*/

    /*
     * If the path doesn't exist, look for the library relative to the
     * tk.dll library.
     */

    libDir = Tcl_GetVar(interp, "tk_library", TCL_GLOBAL_ONLY);
    if (libDir == NULL) {
        Tcl_SetVar(interp, "tk_library", ".", TCL_GLOBAL_ONLY);
    }

    return Tcl_Eval(interp, initScript);
}

/*
 *----------------------------------------------------------------------
 *
 * TkOS2InitPM --
 *
 *	Performs OS/2 Presentation Manager intialisation.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Fills the global variables hab and hmq.
 *
 *----------------------------------------------------------------------
 */

void
TkOS2InitPM (void)
{
    BOOL rc;
    HDC hScreenDC;
    LONG lStart, lCount;
    DEVOPENSTRUC doStruc= {0L, (PSZ)"DISPLAY", NULL, 0L, 0L, 0L, 0L, 0L, 0L};
    SIZEL sizel = {0,0};
    BITMAPINFOHEADER2 bmpInfo;
#ifdef DEBUG
    LONG *aBitmapFormats;
#endif

    /* Initialize PM */
    hab = WinInitialize (0);

    /* Create message queue, increased size from 10 */
    hmq= WinCreateMsgQueue (hab, 64);

    /* Determine system values */
    xScreen = WinQuerySysValue(HWND_DESKTOP, SV_CXSCREEN);
    yScreen = WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN);
    titleBar = WinQuerySysValue(HWND_DESKTOP, SV_CYTITLEBAR);
    xBorder = WinQuerySysValue(HWND_DESKTOP, SV_CXBORDER);
    yBorder = WinQuerySysValue(HWND_DESKTOP, SV_CYBORDER);
    xSizeBorder = WinQuerySysValue(HWND_DESKTOP, SV_CXSIZEBORDER);
    ySizeBorder = WinQuerySysValue(HWND_DESKTOP, SV_CYSIZEBORDER);
    xDlgBorder = WinQuerySysValue(HWND_DESKTOP, SV_CXDLGFRAME);
    yDlgBorder = WinQuerySysValue(HWND_DESKTOP, SV_CYDLGFRAME);
#ifdef DEBUG
    printf("xScreen %d, yScreen %d, titleBar %d, xBorder %d, yBorder %d,
           xSizeBorder %d, ySizeBorder %d, xDlgBorder %d, yDlgBorder %d\n",
           xScreen, yScreen, titleBar, xBorder, yBorder, xSizeBorder,
           ySizeBorder, xDlgBorder, yDlgBorder);
#endif

    /* Get device characteristics from PM */
    hScreenDC= DevOpenDC(hab, OD_MEMORY, (PSZ)"*", 0, (PDEVOPENDATA)&doStruc,
                         NULLHANDLE);
    lStart= CAPS_FAMILY; lCount= CAPS_DEVICE_POLYSET_POINTS;
    rc= DevQueryCaps (hScreenDC, lStart, lCount, aDevCaps);
    globalPS = GpiCreatePS(hab, hScreenDC, &sizel,
                           PU_PELS | GPIT_MICRO | GPIA_ASSOC);
#ifdef DEBUG
    printf("globalPS %x\n", globalPS);
    printf("%d bitmap formats: ", aDevCaps[CAPS_BITMAP_FORMATS]);
    if ((aBitmapFormats =
         (PLONG)ckalloc(2*aDevCaps[CAPS_BITMAP_FORMATS]*sizeof(LONG)))!=NULL &&
        GpiQueryDeviceBitmapFormats(globalPS, 2*aDevCaps[CAPS_BITMAP_FORMATS],
                                    aBitmapFormats)) {
        for (lCount=0; lCount < 2*aDevCaps[CAPS_BITMAP_FORMATS]; lCount++) {
            printf("(%d,", aBitmapFormats[lCount]);
            lCount++;
            printf("%d) ", aBitmapFormats[lCount]);
        }
        ckfree((char *)aBitmapFormats);
    }
    printf("\n");
    printf("    CAPS_GRAPHICS_CHAR_WIDTH %d, CAPS_GRAPHICS_CHAR_HEIGHT %d\n",
           aDevCaps[CAPS_GRAPHICS_CHAR_WIDTH],
           aDevCaps[CAPS_GRAPHICS_CHAR_HEIGHT]);
    printf("    CAPS_HORIZONTAL_FONT_RES %d, CAPS_VERTICAL_FONT_RES %d\n",
           aDevCaps[CAPS_HORIZONTAL_FONT_RES],
           aDevCaps[CAPS_VERTICAL_FONT_RES]);
    printf("    CAPS_ADDITIONAL_GRAPHICS & CAPS_COSMETIC_WIDELINE_SUPPORT %d\n",
           aDevCaps[CAPS_ADDITIONAL_GRAPHICS] & CAPS_COSMETIC_WIDELINE_SUPPORT);
    printf("    CAPS_LINEWIDTH_THICK %d\n", aDevCaps[CAPS_LINEWIDTH_THICK]);
    printf("    CAPS_HORIZONTAL_RESOLUTION %d, CAPS_VERTICAL_RESOLUTION %d\n",
           aDevCaps[CAPS_HORIZONTAL_RESOLUTION],
           aDevCaps[CAPS_VERTICAL_RESOLUTION]);
    printf("    (hor) 1cm = %d pixels, 1in = %d pixels\n",
           aDevCaps[CAPS_HORIZONTAL_RESOLUTION] / 100,
           aDevCaps[CAPS_HORIZONTAL_RESOLUTION] / 39);
#endif

    if (globalPS == GPI_ERROR) {
#ifdef DEBUG
        printf("globalPS ERROR %x\n", WinGetLastError(hab));
#endif
        return;
    }
    GpiSetCharMode(globalPS, CM_MODE2);
    bmpInfo.cbFix = 16L;
    bmpInfo.cx = xScreen;
    bmpInfo.cy = yScreen;
    bmpInfo.cPlanes = 1;
    bmpInfo.cBitCount = aDevCaps[CAPS_COLOR_BITCOUNT];
    globalBitmap = GpiCreateBitmap(globalPS, &bmpInfo, 0L, NULL, NULL);
#ifdef DEBUG
    if (globalBitmap!=GPI_ERROR) {
        printf("GpiCreateBitmap globalBitmap OK (%x)\n", globalBitmap);
    } else {
        printf("GpiCreateBitmap globalBitmap GPI_ERROR, error %x\n",
               WinGetLastError(hab));
    }
#endif
    rc = GpiSetBitmap(globalPS, globalBitmap);
#ifdef DEBUG
    if (rc!=GPI_ALTERROR) {
        printf("GpiSetBitmap globalBitmap OK\n");
    } else {
        printf("GpiSetBitmap globalBitmap GPI_ALTERROR, error %x\n",
               WinGetLastError(hab));
    }
#endif
    /* Determine color table if no palette support but color table support */
    if (!(aDevCaps[CAPS_ADDITIONAL_GRAPHICS] & CAPS_PALETTE_MANAGER) &&
        aDevCaps[CAPS_COLOR_TABLE_SUPPORT]) {
        LONG aClrData[4];

        nextColor = 16;	/* Assume VGA color table */
        rc = GpiQueryColorData(globalPS, 4, aClrData);
#ifdef DEBUG
        if (rc!=TRUE) {
            printf("GpiQueryColorData ERROR %x\n", WinGetLastError(hab));
        } else {
            printf("GpiQueryColorData: format %x, loind %d, hiind %d, options %x\n",
                    aClrData[QCD_LCT_FORMAT], aClrData[QCD_LCT_LOINDEX],
                    aClrData[QCD_LCT_HIINDEX], aClrData[QCD_LCT_OPTIONS]); 
        }
#endif
        nextColor = aClrData[QCD_LCT_HIINDEX] + 1;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkOS2ExitPM --
 *
 *	Performs OS/2 Presentation Manager sign-off routines.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Resets global variables hab and hmq.
 *
 *----------------------------------------------------------------------
 */

void
TkOS2ExitPM (void)
{
    GpiSetBitmap(globalPS, NULLHANDLE);
    GpiDestroyPS(globalPS);
    DevCloseDC(hScreenDC);
    WinDestroyMsgQueue(hmq);
    WinTerminate(hab);
    hmq= hab= 0;
}
