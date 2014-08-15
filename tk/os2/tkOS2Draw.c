/* 
 * tkOS2Draw.c --
 *
 *	This file contains the Xlib emulation functions pertaining to
 *	actually drawing objects on a window.
 *
 * Copyright (c) 1996-1997 Illya Vaes
 * Copyright (c) 1995 Sun Microsystems, Inc.
 * Copyright (c) 1994 Software Research Associates, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */


#include "tkOS2Int.h"

#define PI 3.14159265358979
#define XAngleToRadians(a) ((double)(a) / 64 * PI / 180)

/*
 * Translation tables between X gc values and OS/2 GPI line attributes.
 */

static int lineStyles[] = {
    LINETYPE_SOLID,	/* LineSolid */
    LINETYPE_SHORTDASH,	/* LineOnOffDash */
    LINETYPE_SHORTDASH	/* LineDoubleDash EXTRA PROCESSING NECESSARY */
};
static int capStyles[] = {
    LINEEND_FLAT,	/* CapNotLast EXTRA PROCESSING NECESSARY */
    LINEEND_FLAT,	/* CapButt */
    LINEEND_ROUND,	/* CapRound */
    LINEEND_SQUARE	/* CapProjecting */
};
static int joinStyles[] = {
    LINEJOIN_MITRE,	/* JoinMiter */
    LINEJOIN_ROUND,	/* JoinRound */
    LINEJOIN_BEVEL	/* JoinBevel */
};

/*
 * Translation table between X gc functions and OS/2 GPI mix attributes.
 */

static int mixModes[] = {
    FM_ZERO,			/* GXclear */
    FM_AND,			/* GXand */
    FM_MASKSRCNOT,		/* GXandReverse */
    FM_OVERPAINT,		/* GXcopy */
    FM_SUBTRACT,		/* GXandInverted */
    FM_LEAVEALONE,		/* GXnoop */
    FM_XOR,			/* GXxor */
    FM_OR,			/* GXor */
    FM_NOTMERGESRC,		/* GXnor */
    FM_NOTXORSRC,		/* GXequiv */
    FM_INVERT,			/* GXinvert */
    FM_MERGESRCNOT,		/* GXorReverse */
    FM_NOTCOPYSRC,		/* GXcopyInverted */
    FM_MERGENOTSRC,		/* GXorInverted */
    FM_NOTMASKSRC,		/* GXnand */
    FM_ONE			/* GXset */
};


/*
 * Translation table between X gc functions and OS/2 GPI BitBlt raster op modes.
 * Some of the operations defined in X don't have names, so we have to construct
 * new opcodes for those functions.  This is arcane and probably not all that
 * useful, but at least it's accurate.
 */

#define NOTSRCAND	(LONG)0x0022 /* dest = (NOT source) AND dest */
#define NOTSRCINVERT	(LONG)0x0099 /* dest = (NOT source) XOR dest */
#define SRCORREVERSE	(LONG)0x00dd /* dest = source OR (NOT dest) */
#define SRCNAND		(LONG)0x0077 /* dest = (NOT source) OR (NOT dest) */

static int bltModes[] = {
    ROP_ZERO,			/* GXclear */
    ROP_SRCAND,			/* GXand */
    ROP_SRCERASE,		/* GXandReverse */
    ROP_SRCCOPY,		/* GXcopy */
    NOTSRCAND,			/* GXandInverted */
    ROP_PATCOPY,		/* GXnoop */
    ROP_SRCINVERT,		/* GXxor */
    ROP_SRCPAINT,		/* GXor */
    ROP_NOTSRCERASE,		/* GXnor */
    NOTSRCINVERT,		/* GXequiv */
    ROP_DSTINVERT,		/* GXinvert */
    SRCORREVERSE,		/* GXorReverse */
    ROP_NOTSRCCOPY,		/* GXcopyInverted */
    ROP_MERGEPAINT,		/* GXorInverted */
    SRCNAND,			/* GXnand */
    ROP_ONE			/* GXset */
};

/*
 * The following raster op uses the source bitmap as a mask for the
 * pattern.  This is used to draw in a foreground color but leave the
 * background color transparent.
 */

#define MASKPAT		0x00e2 /* dest = (src & pat) | (!src & dst) */

/*
 * The following two raster ops are used to copy the foreground and background
 * bits of a source pattern as defined by a stipple used as the pattern.
 */

#define COPYFG		0x00ca /* dest = (pat & src) | (!pat & dst) */
#define COPYBG		0x00ac /* dest = (!pat & src) | (pat & dst) */

/*
 * Macros used later in the file.
 */

#ifndef MIN
#define MIN(a,b)	((a>b) ? b : a)
#endif
#ifndef MAX
#define MAX(a,b)	((a<b) ? b : a)
#endif

/*
 * Forward declarations for procedures defined in this file:
 */

static POINTL *		ConvertPoints (Drawable d, XPoint *points, int npoints,
			    int mode, RECTL *bbox);
static void		DrawOrFillArc (Display *display,
			    Drawable d, GC gc, int x, int y,
			    unsigned int width, unsigned int height,
			    int angle1, int angle2, int fill);
static void		RenderObject (HPS hps, GC gc, Drawable d,
                            XPoint* points, int npoints, int mode,
                            PLINEBUNDLE lineBundle, int func);
static void		TkOS2SetStipple(HPS destPS, HPS bmpPS, HBITMAP stipple,
			    LONG x, LONG y, LONG *oldPatternSet,
			    PPOINTL oldRefPoint);
static void		TkOS2UnsetStipple(HPS destPS, HPS bmpPS, HBITMAP stipple,
			    LONG oldPatternSet, PPOINTL oldRefPoint);

/*
 *----------------------------------------------------------------------
 *
 * TkOS2GetDrawablePS --
 *
 *	Retrieve the Presentation Space from a drawable.
 *
 * Results:
 *	Returns the window PS for windows.  Returns the associated memory PS
 *	for pixmaps.
 *
 * Side effects:
 *	Sets up the palette for the presentation space, and saves the old
 *	presentation space state in the passed in TkOS2PSState structure.
 *
 *----------------------------------------------------------------------
 */

HPS
TkOS2GetDrawablePS(display, d, state)
    Display *display;
    Drawable d;
    TkOS2PSState* state;
{
    HPS hps;
    TkOS2Drawable *todPtr = (TkOS2Drawable *)d;
    Colormap cmap;

    if (todPtr->type != TOD_BITMAP) {
        TkWindow *winPtr = todPtr->window.winPtr;

	hps = WinGetPS(todPtr->window.handle);
#ifdef DEBUG
        printf("TkOS2GetDrawablePS window %x (handle %x, hps %x)\n", todPtr,
               todPtr->window.handle, hps);
#endif
        if (winPtr == NULL) {
	    cmap = DefaultColormap(display, DefaultScreen(display));
        } else {
	    cmap = winPtr->atts.colormap;
        }
        state->palette = TkOS2SelectPalette(hps, todPtr->window.handle, cmap);
    } else {

        hps = todPtr->bitmap.hps;
#ifdef DEBUG
        printf("TkOS2GetDrawablePS bitmap %x (handle %x, hps %x)\n", d,
               todPtr->bitmap.handle, hps);
#endif
        cmap = todPtr->bitmap.colormap;
        state->palette = TkOS2SelectPalette(hps, todPtr->bitmap.parent, cmap);
    }
    return hps;
}

/*
 *----------------------------------------------------------------------
 *
 * TkOS2ReleaseDrawablePS --
 *
 *	Frees the resources associated with a drawable's DC.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Restores the old bitmap handle to the memory DC for pixmaps.
 *
 *----------------------------------------------------------------------
 */

void
TkOS2ReleaseDrawablePS(d, hps, state)
    Drawable d;
    HPS hps;
    TkOS2PSState *state;
{
    ULONG changed;
    HPAL oldPal;
    TkOS2Drawable *todPtr = (TkOS2Drawable *)d;

    oldPal = GpiSelectPalette(hps, state->palette);
#ifdef DEBUG
    if (oldPal == PAL_ERROR) {
        printf("GpiSelectPalette TkOS2ReleaseDrawablePS PAL_ERROR: %x\n",
               WinGetLastError(hab));
    } else {
        printf("GpiSelectPalette TkOS2ReleaseDrawablePS: %x\n", oldPal);
    }
#endif
    if (todPtr->type != TOD_BITMAP) {
#ifdef DEBUG
        printf("TkOS2ReleaseDrawablePS window %x\n", d);
#endif
        WinRealizePalette(TkOS2GetHWND(d), hps, &changed);
        WinReleasePS(hps);
    } else {
#ifdef DEBUG
        printf("TkOS2ReleaseDrawablePS bitmap %x released %x\n", d,
               state->bitmap);
#endif
        WinRealizePalette(todPtr->bitmap.parent, hps, &changed);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ConvertPoints --
 *
 *	Convert an array of X points to an array of OS/2 GPI points.
 *
 * Results:
 *	Returns the converted array of POINTLs.
 *
 * Side effects:
 *	Allocates a block of memory that should not be freed.
 *
 *----------------------------------------------------------------------
 */

static POINTL *
ConvertPoints(d, points, npoints, mode, bbox)
    Drawable d;
    XPoint *points;
    int npoints;
    int mode;			/* CoordModeOrigin or CoordModePrevious. */
    RECTL *bbox;			/* Bounding box of points. */
{
    static POINTL *os2Points = NULL; /* Array of points that is reused. */
    static int nOS2Points = -1;	    /* Current size of point array. */
    LONG windowHeight;
    int i;

#ifdef DEBUG
    printf("ConvertPoints %s\n", mode == CoordModeOrigin ? "CoordModeOrigin":
           "CoordModePrevious");
#endif

    windowHeight = TkOS2WindowHeight((TkOS2Drawable *)d);

    /*
     * To avoid paying the cost of a malloc on every drawing routine,
     * we reuse the last array if it is large enough.
     */

    if (npoints > nOS2Points) {
	if (os2Points != NULL) {
#ifdef DEBUG
            printf("    ckfree os2Points %x\n", os2Points);
#endif
	    ckfree((char *) os2Points);
	}
	os2Points = (POINTL *) ckalloc(sizeof(POINTL) * npoints);
#ifdef DEBUG
        printf("    ckalloc os2Points %x\n", os2Points);
#endif
	if (os2Points == NULL) {
	    nOS2Points = -1;
	    return NULL;
	}
	nOS2Points = npoints;
    }

    /* Convert to PM Coordinates */
    bbox->xLeft = bbox->xRight = points[0].x;
    bbox->yTop = bbox->yBottom = windowHeight - points[0].y;
    
    if (mode == CoordModeOrigin) {
	for (i = 0; i < npoints; i++) {
	    os2Points[i].x = points[i].x;
	    /* convert to PM */
	    os2Points[i].y = windowHeight - points[i].y;
	    bbox->xLeft = MIN(bbox->xLeft, os2Points[i].x);
	    /* Since GpiBitBlt excludes top & right, add one */
	    bbox->xRight = MAX(bbox->xRight, os2Points[i].x + 1);
	    /* y: min and max switched for PM */
	    bbox->yTop = MAX(bbox->yTop, os2Points[i].y + 1);
	    bbox->yBottom = MIN(bbox->yBottom, os2Points[i].y);
#ifdef DEBUG
            printf("   point %d: x %d, y %d; bbox xL %d, xR %d, yT %d, yB %d\n",
                   i, points[i].x, points[i].y, bbox->xLeft, bbox->xRight,
                   bbox->yTop, bbox->yBottom);
#endif
	}
    } else {
	os2Points[0].x = points[0].x;
	os2Points[0].y = windowHeight - points[0].y;
	for (i = 1; i < npoints; i++) {
	    os2Points[i].x = os2Points[i-1].x + points[i].x;
	    /* convert to PM */
	    os2Points[i].y = os2Points[i-1].y -
	                     (windowHeight - points[i].y);
	    bbox->xLeft = MIN(bbox->xLeft, os2Points[i].x);
	    /* Since GpiBitBlt excludes top & right, add one */
	    bbox->xRight = MAX(bbox->xRight, os2Points[i].x + 1);
	    /* y: min and max switched for PM */
	    bbox->yTop = MAX(bbox->yTop, os2Points[i].y + 1);
	    bbox->yBottom = MIN(bbox->yBottom, os2Points[i].y);
#ifdef DEBUG
            printf("    point %d: x %d, y %d; bbox xL %d, xR %d, yT %d, yB %d\n",
                   points[i].x, points[i].y, bbox->xLeft, bbox->xRight,
                   bbox->yTop, bbox->yBottom);
            printf("    os2point: x %d, y %d\n", os2Points[i].x, os2Points[i].y);
#endif
	}
    }
    return os2Points;
}

/*
 *----------------------------------------------------------------------
 *
 * XCopyArea --
 *
 *	Copies data from one drawable to another using block transfer
 *	routines.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Data is moved from a window or bitmap to a second window or
 *	bitmap.
 *
 *----------------------------------------------------------------------
 */

void
XCopyArea(display, src, dest, gc, src_x, src_y, width, height, dest_x, dest_y)
    Display* display;
    Drawable src;
    Drawable dest;
    GC gc;
    int src_x, src_y;
    unsigned int width, height;
    int dest_x, dest_y;
{
    HPS srcPS, destPS;
    TkOS2PSState srcState, destState;
    POINTL aPoints[3]; /* Lower-left, upper-right, lower-left source */
    BOOL rc;
    LONG windowHeight;

#ifdef DEBUG
    printf("XCopyArea (%d,%d) -> (%d,%d), %dx%d, gc->func %x, fg %x, bg %x\n",
           src_x, src_y, dest_x, dest_y, width, height, gc->function,
           gc->foreground, gc->background);
#endif
    /* Translate the Y coordinates to PM coordinates */
    /* Determine height of window */
    windowHeight = TkOS2WindowHeight((TkOS2Drawable *)dest);
    aPoints[0].x = dest_x;
    aPoints[0].y = windowHeight - dest_y - height;
    aPoints[1].x = dest_x + width;
    aPoints[1].y = windowHeight - dest_y;
    aPoints[2].x = src_x;
    if (src != dest) {
        windowHeight = TkOS2WindowHeight((TkOS2Drawable *)src);
    }
    aPoints[2].y = windowHeight - src_y - height;
    srcPS = TkOS2GetDrawablePS(display, src, &srcState);
#ifdef DEBUG
    printf("    PM: (%d,%d)-(%d,%d) <- (%d,%d)\n", aPoints[0].x, aPoints[0].y,
           aPoints[1].x, aPoints[1].y, aPoints[2].x, aPoints[2].y);
#endif

    if (src != dest) {
	destPS = TkOS2GetDrawablePS(display, dest, &destState);
    } else {
	destPS = srcPS;
    }
#ifdef DEBUG
    rc = GpiRectVisible(destPS, (PRECTL)&aPoints[0]);
    if (rc==RVIS_PARTIAL || rc==RVIS_VISIBLE) {
        printf("GpiRectVisible (%d,%d) (%d,%d) (partially) visible\n",
               aPoints[0].x, aPoints[0].y, aPoints[1].x, aPoints[1].y);
    } else {
        if (rc==RVIS_INVISIBLE) {
            printf("GpiRectVisible (%d,%d) (%d,%d) invisible\n",
                   aPoints[0].x, aPoints[0].y, aPoints[1].x, aPoints[1].y);
        } else {
            printf("GpiRectVisible (%d,%d) (%d,%d) ERROR, error %x\n",
                   aPoints[0].x, aPoints[0].y, aPoints[1].x, aPoints[1].y,
                   WinGetLastError(hab));
        }
    }
#endif
    rc = GpiBitBlt(destPS, srcPS, 3, aPoints, bltModes[gc->function],
                   BBO_IGNORE);
#ifdef DEBUG
    printf("    srcPS %x, type %s, destPS %x, type %s\n", srcPS,
           ((TkOS2Drawable *)src)->type == TOD_BITMAP ? "bitmap" : "window",
           destPS,
           ((TkOS2Drawable *)dest)->type == TOD_BITMAP ? "bitmap" : "window");
    printf("    GpiBitBlt %x -> %x, 3, (%d,%d)(%d,%d)(%d,%d), %x returns %d\n",
           srcPS, destPS, aPoints[0].x, aPoints[0].y,
           aPoints[1].x, aPoints[1].y, aPoints[2].x, aPoints[2].y,
           bltModes[gc->function], rc);
#endif

    if (src != dest) {
	TkOS2ReleaseDrawablePS(dest, destPS, &destState);
    }
    TkOS2ReleaseDrawablePS(src, srcPS, &srcState);
}

/*
 *----------------------------------------------------------------------
 *
 * XCopyPlane --
 *
 *	Copies a bitmap from a source drawable to a destination
 *	drawable.  The plane argument specifies which bit plane of
 *	the source contains the bitmap.  Note that this implementation
 *	ignores the gc->function.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Changes the destination drawable.
 *
 *----------------------------------------------------------------------
 */

void
XCopyPlane(display, src, dest, gc, src_x, src_y, width, height, dest_x,
	dest_y, plane)
    Display* display;
    Drawable src;
    Drawable dest;
    GC gc;
    int src_x, src_y;
    unsigned int width, height;
    int dest_x, dest_y;
    unsigned long plane;
{
    HPS srcPS, destPS;
    TkOS2PSState srcState, destState;
    LONG oldPattern;
    LONG oldMix, oldBackMix;
    LONG oldColor, oldBackColor;
    LONG srcWindowHeight, destWindowHeight;
    POINTL aPoints[3]; /* Lower-left, upper-right, lower-left source */
    LONG rc;

#ifdef DEBUG
    printf("XCopyPlane (%d,%d) -> (%d,%d), w %d, h %d; fg %x, bg %x, gc->func %x\n",
           src_x, src_y, dest_x, dest_y, width, height, gc->foreground,
           gc->background, gc->function);
#endif

    /* Translate the Y coordinates to PM coordinates */
    destWindowHeight = TkOS2WindowHeight((TkOS2Drawable *)dest);
    if (src != dest) {
        srcWindowHeight = TkOS2WindowHeight((TkOS2Drawable *)src);
    } else {
        srcWindowHeight = destWindowHeight;
    }
#ifdef DEBUG
    printf("srcWindowHeight %d, destWindowHeight %d\n", srcWindowHeight,
           destWindowHeight);
#endif
    aPoints[0].x = dest_x;
    aPoints[0].y = destWindowHeight - dest_y - height;
    aPoints[1].x = dest_x + width;
    aPoints[1].y = destWindowHeight - dest_y;
    aPoints[2].x = src_x;
    aPoints[2].y = src_y;
    display->request++;

    if (plane != 1) {
	panic("Unexpected plane specified for XCopyPlane");
    }

    srcPS = TkOS2GetDrawablePS(display, src, &srcState);

    if (src != dest) {
	destPS = TkOS2GetDrawablePS(display, dest, &destState);
    } else {
	destPS = srcPS;
    }
#ifdef DEBUG
    printf("    srcPS %x, type %s, destPS %x, type %s, clip_mask %x\n", srcPS,
           ((TkOS2Drawable *)src)->type == TOD_BITMAP ? "bitmap" : "window",
           destPS,
           ((TkOS2Drawable *)dest)->type == TOD_BITMAP ? "bitmap" : "window",
           gc->clip_mask);
    printf("    (%d,%d) (%d,%d) (%d,%d)\n", aPoints[0].x, aPoints[0].y,
           aPoints[1].x, aPoints[1].y, aPoints[2].x, aPoints[2].y);
#endif

    if (gc->clip_mask == src) {
#ifdef DEBUG
        printf("XCopyPlane case1\n");
#endif

	/*
	 * Case 1: transparent bitmaps are handled by setting the
	 * destination to the foreground color whenever the source
	 * pixel is set.
	 */

	oldColor = GpiQueryColor(destPS);
        oldBackColor = GpiQueryBackColor(destPS);
	oldPattern = GpiQueryPattern(destPS);
	GpiSetColor(destPS, gc->foreground);
        rc= GpiSetBackColor(destPS, gc->background);
	GpiSetPattern(destPS, PATSYM_SOLID);
        rc = GpiBitBlt(destPS, srcPS, 3, aPoints, MASKPAT, BBO_IGNORE);
#ifdef DEBUG
        printf("    GpiBitBlt (clip_mask src) %x, %x returns %d\n", destPS,
               srcPS, rc);
#endif
	GpiSetPattern(destPS, oldPattern);
        GpiSetBackColor(destPS, oldBackColor);
	GpiSetColor(destPS, oldColor);

    } else if (gc->clip_mask == None) {
#ifdef DEBUG
        printf("XCopyPlane case2\n");
#endif

	/*
	 * Case 2: opaque bitmaps.
	 * Conversion from a monochrome bitmap to a color bitmap/device:
	 *     source 1 -> image foreground color
	 *     source 0 -> image background color
	 *
	 */

        oldColor = GpiQueryColor(destPS);
        oldBackColor = GpiQueryBackColor(destPS);
        oldMix = GpiQueryMix(destPS);
        oldBackMix = GpiQueryBackMix(destPS);
#ifdef DEBUG
        printf("    oldColor %d, oldBackColor %d, oldMix %d, oldBackMix %d\n",
               oldColor, oldBackColor, oldMix, oldBackMix);
        printf("    oldColor srcPS %d, oldBackColor srcPS %d\n",
               GpiQueryColor(srcPS), GpiQueryBackColor(srcPS));
#endif

        /*
        rc= GpiSetColor(destPS, gc->foreground);
        */
        rc= GpiSetColor(destPS, gc->background);
#ifdef DEBUG
        if (rc==TRUE) printf("    GpiSetColor %x OK\n", gc->foreground);
        else printf("    GpiSetColor %x ERROR: %x\n", gc->foreground,
                    WinGetLastError(hab));
#endif
        /*
        rc= GpiSetBackColor(destPS, gc->background);
        */
        rc= GpiSetBackColor(destPS, gc->foreground);
#ifdef DEBUG
        if (rc==TRUE) printf("    GpiSetBackColor %x OK\n", gc->background);
        else printf("    GpiSetBackColor %x ERROR: %x\n", gc->background,
                    WinGetLastError(hab));
#endif

        rc = GpiBitBlt(destPS, srcPS, 3, aPoints, ROP_SRCCOPY, BBO_IGNORE);
#ifdef DEBUG
        printf("    GpiBitBlt (clip_mask None) %x -> %x returns %x\n", srcPS,
               destPS, rc);
fflush(stdout);
#endif
        rc= GpiSetColor(destPS, oldColor);
        rc= GpiSetBackColor(destPS, oldBackColor);
        rc= GpiSetMix(destPS, oldMix);
        rc= GpiSetBackMix(destPS, oldBackMix);
    } else {

	/*
	 * Case 3: two arbitrary bitmaps.  Copy the source rectangle
	 * into a color pixmap.  Use the result as a brush when
	 * copying the clip mask into the destination.	 
	 */

	HPS memPS, maskPS;
	BITMAPINFOHEADER2 bmpInfo;
	HBITMAP bitmap, oldBitmap;
	TkOS2PSState maskState;

#ifdef DEBUG
        printf("XCopyPlane case3\n");
#endif

	oldColor = GpiQueryColor(destPS);
	oldPattern = GpiQueryPattern(destPS);

	maskPS = TkOS2GetDrawablePS(display, gc->clip_mask, &maskState);
	memPS = WinGetScreenPS(HWND_DESKTOP);
	bmpInfo.cbFix = sizeof(BITMAPINFOHEADER2);
	bmpInfo.cx = width;
	bmpInfo.cy = height;
	bmpInfo.cPlanes = 1;
	bmpInfo.cBitCount = 1;
	bitmap = GpiCreateBitmap(memPS, &bmpInfo, 0L, NULL, NULL);
#ifdef DEBUG
        printf("    GpiCreateBitmap (%d,%d) returned %x\n", width, height,
               bitmap);
#endif
	oldBitmap = GpiSetBitmap(memPS, bitmap);
#ifdef DEBUG
        printf("    GpiSetBitmap %x returned %x\n", bitmap, oldBitmap);
#endif

	/*
	 * Set foreground bits.  We create a new bitmap containing
	 * (source AND mask), then use it to set the foreground color
	 * into the destination.
	 */

        /* Translate the Y coordinates to PM coordinates */
        aPoints[0].x = 0; /* dest_x = 0 */
        aPoints[0].y = destWindowHeight - height; /* dest_y = 0 */
        aPoints[1].x = width;
        aPoints[1].y = destWindowHeight;
        aPoints[2].x = src_x;
        aPoints[2].y = srcWindowHeight - src_y - height;
        rc = GpiBitBlt(memPS, srcPS, 3, aPoints, ROP_SRCCOPY, BBO_IGNORE);
#ifdef DEBUG
        printf("    GpiBitBlt nr1 %x, %x returns %d\n", destPS, srcPS, rc);
#endif
        /* Translate the Y coordinates to PM coordinates */
        aPoints[0].x = 0; /* dest_x = 0 */
        aPoints[0].y = destWindowHeight - height; /* dest_y = 0 */
        aPoints[1].x = dest_x + width;
        aPoints[1].y = destWindowHeight;
        aPoints[2].x = dest_x - gc->clip_x_origin;
        aPoints[2].y = srcWindowHeight - dest_y + gc->clip_y_origin - height;
        rc = GpiBitBlt(memPS, maskPS, 3, aPoints, ROP_SRCAND, BBO_IGNORE);
#ifdef DEBUG
        printf("    GpiBitBlt nr2 %x, %x returns %d\n", destPS, srcPS, rc);
#endif
        /* Translate the Y coordinates to PM coordinates */
        aPoints[0].x = dest_x;
        aPoints[0].y = destWindowHeight - dest_y - height;
        aPoints[1].x = dest_x + width;
        aPoints[1].y = destWindowHeight - dest_y;
        aPoints[2].x = 0; /* src_x = 0 */
        aPoints[2].y = srcWindowHeight - height; /* src_y = 0 */
	GpiSetColor(destPS, gc->foreground);
	GpiSetPattern(destPS, PATSYM_SOLID);
        rc = GpiBitBlt(destPS, memPS, 3, aPoints, MASKPAT, BBO_IGNORE);
#ifdef DEBUG
        printf("    GpiBitBlt nr3 %x, %x returns %d\n", destPS, srcPS, rc);
#endif

	/*
	 * Set background bits.  Same as foreground, except we use
	 * ((NOT source) AND mask) and the background brush.
	 */

        /* Translate the Y coordinates to PM coordinates */
        aPoints[0].x = 0; /* dest_x = 0 */
        aPoints[0].y = destWindowHeight - height; /* dest_y = 0 */
        aPoints[1].x = width;
        aPoints[1].y = destWindowHeight;
        aPoints[2].x = src_x;
        aPoints[2].y = srcWindowHeight - src_y - height;
        rc = GpiBitBlt(memPS, srcPS, 3, aPoints, ROP_NOTSRCCOPY, BBO_IGNORE);
#ifdef DEBUG
        printf("    GpiBitBlt nr4 %x, %x returns %d\n", destPS, srcPS, rc);
#endif
        /* Translate the Y coordinates to PM coordinates */
        aPoints[0].x = 0; /* dest_x = 0 */
        aPoints[0].y = destWindowHeight - height; /* dest_y = 0 */
        aPoints[1].x = dest_x + width;
        aPoints[1].y = destWindowHeight;
        aPoints[2].x = dest_x - gc->clip_x_origin;
        aPoints[2].y = destWindowHeight - dest_y + gc->clip_y_origin - height;
        rc = GpiBitBlt(memPS, maskPS, 3, aPoints, ROP_SRCAND, BBO_IGNORE);
#ifdef DEBUG
        printf("    GpiBitBlt nr5 %x, %x returns %d\n", destPS, srcPS, rc);
#endif
	GpiSetColor(destPS, gc->background);
	GpiSetPattern(destPS, PATSYM_SOLID);
        /* Translate the Y coordinates to PM coordinates */
        aPoints[0].x = dest_x;
        aPoints[0].y = destWindowHeight - dest_y - height;
        aPoints[1].x = dest_x + width;
        aPoints[1].y = destWindowHeight - dest_y;
        aPoints[2].x = 0; /* src_x = 0 */
        aPoints[2].y = srcWindowHeight - height; /* src_y = 0 */
        rc = GpiBitBlt(destPS, memPS, 3, aPoints, MASKPAT, BBO_IGNORE);
#ifdef DEBUG
        printf("    GpiBitBlt nr6 %x, %x returns %d\n", destPS, srcPS, rc);
#endif

	TkOS2ReleaseDrawablePS(gc->clip_mask, maskPS, &maskState);
	GpiSetPattern(destPS, oldPattern);
	GpiSetColor(destPS, oldColor);
	GpiSetBitmap(memPS, oldBitmap);
	GpiDeleteBitmap(bitmap);
	WinReleasePS(memPS);
    }
    if (src != dest) {
	TkOS2ReleaseDrawablePS(dest, destPS, &destState);
    }
    TkOS2ReleaseDrawablePS(src, srcPS, &srcState);
}

/*
 *----------------------------------------------------------------------
 *
 * TkPutImage --
 *
 *	Copies a subimage from an in-memory image to a rectangle of
 *	of the specified drawable.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Draws the image on the specified drawable.
 *
 *----------------------------------------------------------------------
 */

void
TkPutImage(colors, ncolors, display, d, gc, image, src_x, src_y, dest_x,
	dest_y, width, height)
    unsigned long *colors;		/* Array of pixel values used by this
					 * image.  May be NULL. */
    int ncolors;			/* Number of colors used, or 0. */
    Display* display;
    Drawable d;				/* Destination drawable. */
    GC gc;
    XImage* image;			/* Source image. */
    int src_x, src_y;			/* Offset of subimage. */      
    int dest_x, dest_y;			/* Position of subimage origin in
					 * drawable.  */
    unsigned int width, height;		/* Dimensions of subimage. */
{
    HPS hps;
    LONG rc;
    TkOS2PSState state;
    BITMAPINFOHEADER2 bmpInfo;
    BITMAPINFO2 *infoPtr;
    char *data;
    LONG windowHeight;

    /* Translate the Y coordinates to PM coordinates */
    windowHeight = TkOS2WindowHeight((TkOS2Drawable *)d);

    display->request++;

    hps = TkOS2GetDrawablePS(display, d, &state);

#ifdef DEBUG
    printf("TkPutImage d %x hps %x (%d,%d) => (%d,%d) %dx%d imgh %d mix %d\n",
           d, hps, src_x, src_y, dest_x, dest_y, width, height, image->height,
           gc->function);
    printf("    nrColors %d, gc->foreground %d, gc->background %d\n", ncolors,
           gc->foreground, gc->background);
#endif

    rc = GpiSetMix(hps, mixModes[gc->function]);
#ifdef DEBUG
    if (rc == FALSE) {
        printf("    GpiSetMix ERROR %x\n", WinGetLastError(hab));
    } else {
        printf("    GpiSetMix %x OK\n", mixModes[gc->function]);
    }
    printf("    hps color %d, hps back color %d\n", GpiQueryColor(hps),
           GpiQueryBackColor(hps));
#endif

    if (image->bits_per_pixel == 1) {
#ifdef DEBUG
        printf("image->bits_per_pixel == 1\n");
#endif

        /* Bitmap must be reversed in OS/2 wrt. the Y direction */
        /* This is done best in a modified version of TkAlignImageData */
	data = TkAlignImageData(image, sizeof(ULONG), MSBFirst);
	bmpInfo.cbFix = 16L;
	bmpInfo.cx = image->width;
	bmpInfo.cy = image->height;
	bmpInfo.cPlanes = 1;
	bmpInfo.cBitCount = 1;
        rc = GpiSetBitmapBits(hps, windowHeight - dest_y - height, height,
                              (PBYTE)data, (BITMAPINFO2*) &bmpInfo);
#ifdef DEBUG
        if (rc == GPI_ALTERROR) {
            SIZEL dim;
            printf("    GpiSetBitmapBits returned GPI_ALTERROR %x\n",
                   WinGetLastError(hab));
            rc=GpiQueryBitmapDimension(((TkOS2Drawable *)d)->bitmap.handle,
                                       &dim);
            if (rc == FALSE) {
                printf("    GpiQueryBitmapDimension ERROR %x\n",
                       WinGetLastError(hab));
            } else {
                printf("    GpiQueryBitmapDimension: %dx%d\n", dim.cx, dim.cy);
            }
        } else {
            printf("   GpiSetBitmapBits (%d) set %d scanlines (hps %x, h %x)\n", 
                   windowHeight - dest_y - height, rc, hps,
                   ((TkOS2Drawable *)d)->bitmap.handle);
        }
#endif

	ckfree(data);
    } else {
	int usePalette;

#ifdef DEBUG
	LONG defBitmapFormat[2];
        printf("image->bits_per_pixel %d\n", image->bits_per_pixel);
#endif

	/*
	 * Do not use a palette for TrueColor images.
	 */
	
	usePalette = (image->bits_per_pixel < 24);

	if (usePalette) {
#ifdef DEBUG
        printf("using palette (not TrueColor)\n");
#endif
	    infoPtr = (BITMAPINFO2*) ckalloc(sizeof(BITMAPINFO2)
		    + sizeof(RGB2)*ncolors);
	} else {
#ifdef DEBUG
        printf("not using palette (TrueColor)\n");
#endif
	    infoPtr = (BITMAPINFO2*) ckalloc(sizeof(BITMAPINFO2));
	}
	if (infoPtr == NULL) return;

        /* Bitmap must be reversed in OS/2 wrt. the Y direction */
	data = TkOS2ReverseImageLines(image, height);
	
	infoPtr->cbFix = sizeof(BITMAPINFOHEADER2);
	infoPtr->cx = width;
        infoPtr->cy = height;

#ifdef DEBUG
	rc = GpiQueryDeviceBitmapFormats(hps, 2, defBitmapFormat);
        if (rc != TRUE) {
            printf("    GpiQueryDeviceBitmapFormats ERROR %x -> mono\n",
                   WinGetLastError(hab));
	    infoPtr->cPlanes = 1;
	    infoPtr->cBitCount = 1;
        } else {
            printf("    GpiQueryDeviceBitmapFormats OK planes %d, bits %d\n",
                   defBitmapFormat[0], defBitmapFormat[1]);
	    infoPtr->cPlanes = defBitmapFormat[0];
	    infoPtr->cBitCount = defBitmapFormat[1];
        }
#endif

	infoPtr->cPlanes = 1;
	infoPtr->cBitCount = image->bits_per_pixel;
	infoPtr->ulCompression = BCA_UNCOMP;
	infoPtr->cbImage = 0;
	infoPtr->cxResolution = aDevCaps[CAPS_HORIZONTAL_RESOLUTION];
	infoPtr->cyResolution = aDevCaps[CAPS_VERTICAL_RESOLUTION];
	infoPtr->cclrUsed = 0;
	infoPtr->cclrImportant = 0;
	infoPtr->usUnits = BRU_METRIC;
	infoPtr->usReserved = 0;
	infoPtr->usRecording = BRA_BOTTOMUP;
	infoPtr->usRendering = BRH_NOTHALFTONED;
	infoPtr->cSize1 = 0L;
	infoPtr->cSize2 = 0L;
	infoPtr->ulColorEncoding = BCE_RGB;
	infoPtr->ulIdentifier = 0L;

	if (usePalette) {
	    BOOL palManager = FALSE;

	    if (aDevCaps[CAPS_ADDITIONAL_GRAPHICS] & CAPS_PALETTE_MANAGER) {
	        palManager = TRUE;
#ifdef DEBUG
	        printf("    Palette manager\n");
#endif
	    }
            if (palManager) {
                rc = GpiQueryPaletteInfo(GpiQueryPalette(hps), hps, 0, 0,
                                         ncolors, (PLONG) &infoPtr->argbColor);
#ifdef DEBUG
                if (rc != TRUE) {
                    printf("    GpiQueryPaletteInfo ERROR %x\n",
                           WinGetLastError(hab));
                }
#endif
            } else {
                rc = GpiQueryLogColorTable(hps, 0, 0, ncolors,
                                           (PLONG) &infoPtr->argbColor);
#ifdef DEBUG
                if (rc == QLCT_ERROR) {
                    printf("    GpiQueryLogColorTable ERROR %x\n",
                           WinGetLastError(hab));
                } else {
                    if (rc == QLCT_RGB) {
                        printf("    table in RGB mode, no element returned");
                    }
                }
#endif
	    }
	} else {
	    infoPtr->cclrUsed = 0;
	}

        rc = GpiSetBitmapBits(hps, windowHeight - dest_y - height, height,
                              (PBYTE)data, infoPtr);
#ifdef DEBUG
        printf("windowHeight %d, dest_y %d, height %d, image->height %d\n",
               windowHeight, dest_y, height, image->height);
        if (rc == GPI_ALTERROR) {
            printf("GpiSetBitmapBits returned GPI_ALTERROR %x\n",
                   WinGetLastError(hab));
        } else {
            printf("    GpiSetBitmapBits (%d) set %d scanlines (hps %x, h %x)\n", 
                   windowHeight - dest_y - height, rc, hps,
                   ((TkOS2Drawable *)d)->bitmap.handle);
        }
#endif

	ckfree((char *)infoPtr);
	ckfree(data);
    }
    TkOS2ReleaseDrawablePS(d, hps, &state);
}

/*
 *----------------------------------------------------------------------
 *
 * XDrawString --
 *
 *	Draw a single string in the current font.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Renders the specified string in the drawable.
 *
 *----------------------------------------------------------------------
 */

void
XDrawString(display, d, gc, x, y, string, length)
    Display* display;
    Drawable d;
    GC gc;
    int x;
    int y;
    _Xconst char* string;
    int length;
{
    HPS hps;
    SIZEF oldCharBox;
    LONG oldFont = 0L;
    LONG oldHorAlign, oldVerAlign;
    LONG oldBackMix;
    LONG oldColor, oldBackColor = 0L;
    POINTL oldRefPoint;
    LONG oldPattern;
    HBITMAP oldBitmap;
    POINTL noShear= {0, 1};
    TkOS2PSState state;
    POINTL aPoints[3]; /* Lower-left, upper-right, lower-left source */
    CHARBUNDLE cBundle;
    AREABUNDLE aBundle;
    LONG windowHeight, l;
    char *str;
    POINTL refPoint;
    BOOL rc;

    display->request++;

    if (d == None) {
	return;
    }

#ifdef DEBUG
    printf("XDrawString %s\"%s\" (%d) on type %s (%x), mixmode %d\n",
          ( (gc->fill_style == FillStippled
             || gc->fill_style == FillOpaqueStippled)
           && gc->stipple != None) ? "stippled " : "", string, length,
          ((TkOS2Drawable *)d)->type == TOD_BITMAP ? "bitmap" : "window", d,
          mixModes[gc->function]);
#endif
    hps = TkOS2GetDrawablePS(display, d, &state);
    GpiSetMix(hps, mixModes[gc->function]);

    /* If this is an outline font, set the char box */
    if (logfonts[(LONG)gc->font].outline) {
#ifdef DEBUG
        SIZEF charBox;
#endif
        rc = GpiQueryCharBox(hps, &oldCharBox);
#ifdef DEBUG
        if (rc!=TRUE) {
            printf("GpiQueryCharBox ERROR %x\n", WinGetLastError(hab));
        } else {
            printf("GpiQueryCharBox OK: cx %d (%d,%d), cy %d (%d,%d)\n",
                   oldCharBox.cx, FIXEDINT(oldCharBox.cx),
                   FIXEDFRAC(oldCharBox.cx), oldCharBox.cy,
                   FIXEDINT(oldCharBox.cy), FIXEDFRAC(oldCharBox.cy));
        }
#endif
        rc = TkOS2ScaleFont(hps, logfonts[(LONG)gc->font].deciPoints, 0);
#ifdef DEBUG
        if (rc!=TRUE) {
            printf("TkOS2ScaleFont %d ERROR %x\n",
                   logfonts[(LONG)gc->font].deciPoints, WinGetLastError(hab));
        } else {
            printf("TkOS2ScaleFont %d OK\n",
                   logfonts[(LONG)gc->font].deciPoints);
        }
        rc = GpiQueryCharBox(hps, &charBox);
        if (rc!=TRUE) {
            printf("GpiQueryCharBox ERROR %x\n");
        } else {
            printf("GpiQueryCharBox OK: now cx %d (%d,%d), cy %d (%d,%d)\n",
                   charBox.cx, FIXEDINT(charBox.cx), FIXEDFRAC(charBox.cx),
                   charBox.cy, FIXEDINT(charBox.cy), FIXEDFRAC(charBox.cy));
        }
#endif
    }

    /* Translate the Y coordinates to PM coordinates */
    windowHeight = TkOS2WindowHeight((TkOS2Drawable *)d);
#ifdef DEBUG
    printf("    x %d, y %d (PM: %d)\n", x, y, windowHeight - y);
#endif
    y = windowHeight - y;

    if ((gc->fill_style == FillStippled || gc->fill_style == FillOpaqueStippled)
        && gc->stipple != None) {

	TkOS2Drawable *todPtr = (TkOS2Drawable *)gc->stipple;
	HDC dcMem;
	HPS psMem;
        DEVOPENSTRUC dop = {0L, (PSZ)"DISPLAY", NULL, 0L, 0L, 0L, 0L, 0L, 0L};
        SIZEL sizl = {0,0}; /* use same page size as device */
	HBITMAP bitmap;
        BITMAPINFOHEADER2 bmpInfo;
        RECTL rect;
        POINTL textBox[TXTBOX_COUNT];

#ifdef DEBUG
       printf("XDrawString stippled \"%s\" (%x) at %d,%d; fg %d, bg %d, bmp %x\n",
              string, gc->stipple, x, y, gc->foreground, gc->background,
              todPtr->bitmap.handle);
#endif

	if (todPtr->type != TOD_BITMAP) {
	    panic("unexpected drawable type in stipple");
	}

	/*
	 * Select stipple pattern into destination PS.
	 * gc->ts_x_origin and y_origin are relative to origin of the
	 * destination drawable, while PatternRefPoint is in world coords.
	 */

	dcMem = DevOpenDC(hab, OD_MEMORY, (PSZ)"*", 5L, (PDEVOPENDATA)&dop,
	                  NULLHANDLE);
	if (dcMem == DEV_ERROR) {
#ifdef DEBUG
            printf("DevOpenDC ERROR %x in XDrawString\n", WinGetLastError(hab));
#endif
	    return;
	}
#ifdef DEBUG
        printf("DevOpenDC in XDrawString returns %x\n", dcMem);
#endif
        psMem = GpiCreatePS(hab, dcMem, &sizl,
                            PU_PELS | GPIT_NORMAL | GPIA_ASSOC);
        if (psMem == GPI_ERROR) {
#ifdef DEBUG
            printf("GpiCreatePS ERROR %x in XDrawString\n", WinGetLastError(hab));
#endif
            DevCloseDC(dcMem);
            return;
        }
#ifdef DEBUG
        printf("GpiCreatePS in XDrawString returns %x\n", psMem);
#endif
	/*
	 * Compute the bounding box and create a compatible bitmap.
	 */

	rc = WinQueryWindowRect(((TkOS2Drawable *)d)->bitmap.parent, &rect);
#ifdef DEBUG
        if (rc != TRUE) {
            printf("WinQueryWindowRect ERROR %x\n", WinGetLastError(hab));
        } else {
            printf("WinQueryWindowRect OK %d,%d->%d,%d\n", rect.xLeft,
                   rect.yBottom, rect.xRight, rect.yTop);
        }
#endif
	bmpInfo.cbFix = 16L;
	/*
	bmpInfo.cx = rect.xRight - rect.xLeft;
	bmpInfo.cy = rect.yTop - rect.yBottom;
	*/
	bmpInfo.cx = xScreen;
	bmpInfo.cy = yScreen;
	bmpInfo.cPlanes = 1;
	bmpInfo.cBitCount = display->screens[display->default_screen].root_depth;
	bitmap = GpiCreateBitmap(psMem, &bmpInfo, 0L, NULL, NULL);
#ifdef DEBUG
        if (bitmap == GPI_ERROR) {
            printf("GpiCreateBitmap (%d,%d) GPI_ERROR %x\n", bmpInfo.cx,
                   bmpInfo.cy, WinGetLastError(hab));
        } else {
            printf("GpiCreateBitmap (%d,%d) returned %x\n", bmpInfo.cx,
                   bmpInfo.cy, bitmap);
        }
#endif
        oldBitmap = GpiSetBitmap(psMem, bitmap);
#ifdef DEBUG
        if (bitmap == HBM_ERROR) {
            printf("GpiSetBitmap (%x) HBM_ERROR %x\n", bitmap,
                   WinGetLastError(hab));
        } else {
            printf("GpiSetBitmap %x returned %x\n", bitmap, oldBitmap);
        }
#endif

	refPoint.x = gc->ts_x_origin;
	refPoint.y = windowHeight - gc->ts_y_origin;

#ifdef DEBUG
        printf("gc->ts_x_origin=%d (->%d), gc->ts_y_origin=%d (->%d)\n",
               gc->ts_x_origin, refPoint.x, gc->ts_y_origin, refPoint.y);
#endif
        /* The bitmap mustn't be selected in the HPS */
        TkOS2SetStipple(hps, todPtr->bitmap.hps, todPtr->bitmap.handle,
                        refPoint.x, refPoint.y, &oldPattern, &oldRefPoint);

        GpiQueryTextAlignment(psMem, &oldHorAlign, &oldVerAlign);
        GpiSetTextAlignment(psMem, TA_LEFT, TA_BASE);

        GpiQueryAttrs(psMem, PRIM_CHAR, LBB_COLOR, (PBUNDLE)&cBundle);
        oldColor = cBundle.lColor;
        cBundle.lColor = gc->foreground;
        rc = GpiSetAttrs(psMem, PRIM_CHAR, LBB_COLOR, 0L, (PBUNDLE)&cBundle);
#ifdef DEBUG
        if (rc!=TRUE) {
            printf("GpiSetAttrs textColor %d ERROR %x\n", cBundle.lColor,
                   WinGetLastError(hab));
        } else {
            printf("GpiSetAttrs textColor %d OK\n", cBundle.lColor);
        }
#endif
        aBundle.lColor = gc->foreground;
        rc = GpiSetAttrs(hps, PRIM_AREA, LBB_COLOR, 0L, (PBUNDLE)&aBundle);
#ifdef DEBUG
        if (rc!=TRUE) {
            printf("GpiSetAttrs areaColor %d ERROR %x\n", aBundle.lColor,
                   WinGetLastError(hab));
        } else {
            printf("GpiSetAttrs areaColor %d OK\n", aBundle.lColor);
        }
#endif

	oldBackMix = GpiQueryBackMix(psMem);
	rc = GpiSetBackMix(psMem, BM_LEAVEALONE);
#ifdef DEBUG
        if (rc!=TRUE) {
            printf("GpiSetBackMix ERROR %x\n", WinGetLastError(hab));
        } else {
            printf("GpiSetBackMix OK\n");
        }
#endif
        oldBackColor = GpiQueryBackColor(psMem);
        GpiSetBackColor(psMem, CLR_FALSE);
#ifdef DEBUG
        if (rc!=TRUE) {
            printf("GpiSetBackColor CLR_FALSE ERROR %x\n", WinGetLastError(hab));
        } else {
            printf("GpiSetBackColor CLR_FALSE OK\n");
        }
#endif

	if (gc->font != None) {
            rc = GpiCreateLogFont(psMem, NULL, (LONG)gc->font,
                                  &(logfonts[(LONG)gc->font].fattrs));
#ifdef DEBUG
            if (rc!=GPI_ERROR) {
                printf("GpiCreateLogFont (%x, id %d) OK\n", psMem, gc->font);
            } else {
                printf("GpiCreateLogFont (%x, id %d) ERROR, error %x\n", psMem,
                       gc->font, WinGetLastError(hab));
            }
#endif
	    oldFont = GpiQueryCharSet(psMem);
#ifdef DEBUG
            if (rc==LCID_ERROR) {
                printf("GpiQueryCharSet ERROR %x\n", WinGetLastError(hab));
            } else {
                printf("GpiQueryCharSet OK\n");
            }
#endif
	    rc = GpiSetCharSet(psMem, (LONG)gc->font);
#ifdef DEBUG
            if (rc!=TRUE) {
                printf("GpiSetCharSet (%x, id %d, face [%s]) ERROR, error %x\n",
                       psMem, gc->font,
                       logfonts[(LONG)gc->font].fattrs.szFacename,
                       WinGetLastError(hab));
            } else {
                printf("GpiSetCharSet (%x, id %d, face [%s]) OK\n", psMem,
                       gc->font, logfonts[(LONG)gc->font].fattrs.szFacename);
            }
#endif
	    /* Set slant if necessary */
	    if (logfonts[(LONG)gc->font].setShear) {
	        rc = GpiSetCharShear(psMem, &(logfonts[(LONG)gc->font].shear));
#ifdef DEBUG
                if (rc!=TRUE) {
                    printf("GpiSetCharShear font %d ERROR %x\n", gc->font,
                           WinGetLastError(hab));
                } else {
                    printf("GpiSetCharShear font %d OK\n", gc->font);
                }
#endif
	    }
            /* If this is an outline font, set the char box */
            if (logfonts[(LONG)gc->font].outline) {
#ifdef DEBUG
                SIZEF charBox;
#endif
                rc = TkOS2ScaleFont(psMem, logfonts[(LONG)gc->font].deciPoints,
                                    0);
#ifdef DEBUG
                if (rc!=TRUE) {
                    printf("TkOS2ScaleFont %d ERROR %x\n",
                           logfonts[(LONG)gc->font].deciPoints,
                           WinGetLastError(hab));
                } else {
                    printf("TkOS2ScaleFont %d OK\n",
                           logfonts[(LONG)gc->font].deciPoints);
                }
                rc = GpiQueryCharBox(psMem, &charBox);
                if (rc!=TRUE) {
                    printf("GpiQueryCharBox ERROR %x\n");
                } else {
                    printf("GpiQueryCharBox OK: now cx %d (%d,%d), cy %d (%d,%d)\n", charBox.cx,
                           FIXEDINT(charBox.cx), FIXEDFRAC(charBox.cx),
                           charBox.cy, FIXEDINT(charBox.cy),
                           FIXEDFRAC(charBox.cy));
	        }
#endif
            }
	}

        refPoint.x = x;
        refPoint.y = y;
        rc = GpiSetCurrentPosition(psMem, &refPoint);
#ifdef DEBUG
        if (rc!=TRUE) {
            printf("GpiSetCurrentPosition %d,%d ERROR %x\n", refPoint.x, refPoint.y,
                   WinGetLastError(hab));
        } else {
            printf("GpiSetCurrentPosition %d,%d OK\n", refPoint.x, refPoint.y);
        }
#endif
	rc = GpiQueryTextBox(psMem, length, (PCH)string, TXTBOX_COUNT, textBox);
#ifdef DEBUG
        if (rc != TRUE) {
            printf("GpiQueryTextBox \"%s\" (%d) ERROR %x\n", string, length,
                   WinGetLastError(hab));
        } else {
            printf("GpiQueryTextBox \"%s\" (%d) OK: (%d,%d)(%d,%d)(%d,%d)(%d,%d), refPoint (%d,%d)\n",
                   string, length,
                   textBox[TXTBOX_TOPLEFT].x, textBox[TXTBOX_TOPLEFT].y,
                   textBox[TXTBOX_BOTTOMLEFT].x, textBox[TXTBOX_BOTTOMLEFT].y,
                   textBox[TXTBOX_TOPRIGHT].x, textBox[TXTBOX_TOPRIGHT].y,
                   textBox[TXTBOX_BOTTOMRIGHT].x, textBox[TXTBOX_BOTTOMRIGHT].y,
                   refPoint.x, refPoint.y);
        }
#endif

        aPoints[0].x = refPoint.x + textBox[TXTBOX_BOTTOMLEFT].x;
        aPoints[0].y = refPoint.y + textBox[TXTBOX_BOTTOMLEFT].y;
        aPoints[1].x = refPoint.x + textBox[TXTBOX_TOPRIGHT].x;
	aPoints[1].y = refPoint.y + textBox[TXTBOX_TOPRIGHT].y;
        aPoints[2].x = refPoint.x + textBox[TXTBOX_BOTTOMLEFT].x;
	aPoints[2].y = refPoint.y + textBox[TXTBOX_BOTTOMLEFT].y;
#ifdef DEBUG
        printf("aPoints: %d,%d %d,%d <- %d,%d\n", aPoints[0].x, aPoints[0].y,
               aPoints[1].x, aPoints[1].y, aPoints[2].x, aPoints[2].y);
#endif

	/*
	 * The following code is tricky because fonts are rendered in multiple
	 * colors.  First we draw onto a black background and copy the white
	 * bits.  Then we draw onto a white background and copy the black bits.
	 * Both the foreground and background bits of the font are ANDed with
	 * the stipple pattern as they are copied.
	 */

        rc = GpiBitBlt(psMem, (HPS)0, 2, aPoints, ROP_ZERO, BBO_IGNORE);
#ifdef DEBUG
        if (rc!=TRUE) {
            printf("GpiBitBlt ZERO %d,%d ERROR %x\n", aPoints[0].x, aPoints[0].y,
                   WinGetLastError(hab));
        } else {
            printf("GpiBitBlt ZERO %d,%d OK\n", aPoints[0].x, aPoints[0].y);
        }
#endif

        /* only 512 bytes allowed in string */
        rc = GpiSetCurrentPosition(psMem, &refPoint);
#ifdef DEBUG
        if (rc!=TRUE) {
            printf("GpiSetCurrentPosition %d,%d ERROR %x\n", refPoint.x, refPoint.y,
                   WinGetLastError(hab));
        } else {
            printf("GpiSetCurrentPosition %d,%d OK\n", refPoint.x, refPoint.y);
        }
#endif
        l = length;
        str = (char *)string;
        while (length>512) {
            rc = GpiCharString(psMem, l, (PCH)str);
#ifdef DEBUG
        if (rc==GPI_ERROR) {
            printf("GpiCharString ERROR %x\n", WinGetLastError(hab));
        } else {
            printf("GpiCharString OK\n");
        }
#endif
            l -= 512;
            str += 512;
        }
        rc = GpiCharString(psMem, l, (PCH)str);
#ifdef DEBUG
        if (rc==GPI_ERROR) {
            printf("GpiCharString ERROR %x\n", WinGetLastError(hab));
        } else {
            printf("GpiCharString OK\n");
        }
#endif

        rc = GpiBitBlt(hps, psMem, 3, aPoints, (LONG)0x00ea, BBO_IGNORE);
#ifdef DEBUG
        if (rc==GPI_ERROR) {
            printf("GpiBitBlt ERROR %x\n", WinGetLastError(hab));
        } else {
            printf("GpiBitBlt OK\n");
        }
#endif

        rc = GpiBitBlt(psMem, (HPS)0, 2, aPoints, ROP_ONE, BBO_IGNORE);
#ifdef DEBUG
        if (rc!=TRUE) {
            printf("GpiBitBlt ONE %d,%d ERROR %x\n", aPoints[0].x, aPoints[0].y,
                   WinGetLastError(hab));
        } else {
            printf("GpiBitBlt ONE %d,%d OK\n", aPoints[0].x, aPoints[0].y);
        }
#endif

        /* only 512 bytes allowed in string */
        rc = GpiSetCurrentPosition(psMem, &refPoint);
#ifdef DEBUG
        if (rc!=TRUE) {
            printf("GpiSetCurrentPosition %d, %d ERROR %x\n", refPoint.x, refPoint.y,
                   WinGetLastError(hab));
        } else {
            printf("GpiSetCurrentPosition %d,%d OK\n", refPoint.x, refPoint.y);
        }
#endif
        l = length;
        str = (char *)string;
        while (length>512) {
            rc = GpiCharString(psMem, 512, (PCH)str);
#ifdef DEBUG
        if (rc==GPI_ERROR) {
            printf("GpiCharString ERROR %x\n", WinGetLastError(hab));
        } else {
            printf("GpiCharString OK\n");
        }
#endif
            l -= 512;
            str += 512;
        }
        rc = GpiCharString(psMem, l, (PCH)str);
#ifdef DEBUG
        if (rc==GPI_ERROR) {
            printf("GpiCharString ERROR %x\n", WinGetLastError(hab));
        } else {
            printf("GpiCharString OK\n");
        }
#endif

        rc = GpiBitBlt(hps, psMem, 3, aPoints, (LONG)0x00ba, BBO_IGNORE);
#ifdef DEBUG
        if (rc==GPI_ERROR) {
            printf("GpiBitBlt ERROR %x\n", WinGetLastError(hab));
        } else {
            printf("GpiBitBlt OK\n");
        }
#endif
	/*
	 * Destroy the temporary bitmap and restore the device context.
	 */

	GpiSetBitmap(psMem, oldBitmap);
	GpiDeleteBitmap(bitmap);
	GpiDestroyPS(psMem);
        DevCloseDC(dcMem);

	if (gc->font != None) {
	    /* Set slant if necessary */
	    if (logfonts[(LONG)gc->font].setShear) {
	        rc = GpiSetCharShear(hps, &noShear);
	    }
	    rc = GpiSetCharSet(hps, oldFont);
	}
	rc = GpiSetBackMix(hps, oldBackMix);
	cBundle.lColor = oldColor;
	rc = GpiSetAttrs(hps, PRIM_CHAR, LBB_COLOR, 0L, (PBUNDLE)&cBundle);
        /* The bitmap must be reselected in the HPS */
        TkOS2UnsetStipple(hps, todPtr->bitmap.hps, todPtr->bitmap.handle,
                          oldPattern, &oldRefPoint);

    } else {

	GpiQueryTextAlignment(hps, &oldHorAlign, &oldVerAlign);
	GpiSetTextAlignment(hps, TA_LEFT, TA_BASE);

	GpiQueryAttrs(hps, PRIM_CHAR, LBB_COLOR, (PBUNDLE)&cBundle);
	oldColor = cBundle.lColor;
	cBundle.lColor = gc->foreground;
	rc = GpiSetAttrs(hps, PRIM_CHAR, LBB_COLOR, 0L, (PBUNDLE)&cBundle);
#ifdef DEBUG
        if (rc!=TRUE) {
            printf("GpiSetAttrs color %x ERROR %x\n", gc->foreground,
                   WinGetLastError(hab));
        } else {
            printf("GpiSetAttrs color %x OK\n", gc->foreground);
        }
#endif

	oldBackMix = GpiQueryBackMix(hps);
	/* We get a crash in PMMERGE.DLL on anything other than BM_LEAVEALONE */
	GpiSetBackMix(hps, BM_LEAVEALONE);

	if (gc->font != None) {
            rc = GpiCreateLogFont(hps, NULL, (LONG)gc->font,
                                  &(logfonts[(LONG)gc->font].fattrs));
#ifdef DEBUG
            if (rc!=GPI_ERROR) {
                printf("GpiCreateLogFont (%x, id %d) OK\n", hps, gc->font);
            } else {
                printf("GpiCreateLogFont (%x, id %d) ERROR, error %x\n", hps,
                       gc->font, WinGetLastError(hab));
            }
#endif
	    oldFont = GpiQueryCharSet(hps);
	    rc = GpiSetCharSet(hps, (LONG)gc->font);
#ifdef DEBUG
            if (rc==TRUE) {
                printf("GpiSetCharSet (%x, id %d, %s) OK\n", hps, gc->font,
                       logfonts[(LONG)gc->font].fattrs.szFacename);
            } else {
                printf("GpiSetCharSet (%x, id %d, %s) ERROR, error %x\n", hps,
                       gc->font, logfonts[(LONG)gc->font].fattrs.szFacename,
                       WinGetLastError(hab));
            }
#endif
	    /* Set slant if necessary */
	    if (logfonts[(LONG)gc->font].setShear) {
	        GpiSetCharShear(hps, &(logfonts[(LONG)gc->font].shear));
#ifdef DEBUG
                if (rc!=TRUE) {
                    printf("GpiSetCharShear font %d ERROR %x\n",
                           logfonts[(LONG)gc->font],
                           WinGetLastError(hab));
                } else {
                    printf("GpiSetCharShear font %d OK\n",
                           logfonts[(LONG)gc->font]);
                }
#endif
	    }
            /* If this is an outline font, set the char box */
            if (logfonts[(LONG)gc->font].outline) {
#ifdef DEBUG
                SIZEF charBox;
#endif
                rc = TkOS2ScaleFont(hps, logfonts[(LONG)gc->font].deciPoints, 0);
#ifdef DEBUG
                if (rc!=TRUE) {
                    printf("TkOS2ScaleFont %d ERROR %x\n",
                           logfonts[(LONG)gc->font].deciPoints,
                           WinGetLastError(hab));
                } else {
                    printf("TkOS2ScaleFont %d OK\n",
                           logfonts[(LONG)gc->font].deciPoints);
                }
                rc = GpiQueryCharBox(hps, &charBox);
                if (rc!=TRUE) {
                    printf("GpiQueryCharBox ERROR %x\n");
                } else {
                    printf("GpiQueryCharBox OK: now cx %d (%d,%d), cy %d (%d,%d)\n", charBox.cx,
                           FIXEDINT(charBox.cx), FIXEDFRAC(charBox.cx),
                           charBox.cy, FIXEDINT(charBox.cy),
                           FIXEDFRAC(charBox.cy));
                }
#endif
            }
	}

	refPoint.x = x;
	refPoint.y = y;
        /* only 512 bytes allowed in string */
        rc = GpiSetCurrentPosition(hps, &refPoint);
#ifdef DEBUG
        if (rc==TRUE) {
            printf("GpiSetCurrentPosition %d,%d OK\n", refPoint.x, refPoint.y);
        } else {
            printf("GpiSetCurrentPosition %d,%d ERROR %x\n", refPoint.x,
                   refPoint.y, WinGetLastError(hab));
        }
#endif
        l = length;
        str = (char *)string;
        while (l>512) {
            rc = GpiCharString(hps, 512, (PCH)str);
#ifdef DEBUG
        if (rc==GPI_OK) {
            printf("GpiCharString returns GPI_OK\n");
        } else {
            printf("GpiCharString returns %d, ERROR %x\n", rc,
                   WinGetLastError(hab));
        }
#endif
            l -= 512;
            str += 512;
        }
        rc = GpiCharString(hps, l, (PCH)str);
#ifdef DEBUG
        if (rc==GPI_OK) {
            printf("GpiCharString returns GPI_OK\n");
        } else {
            printf("GpiCharString returns %d, ERROR %x\n", rc,
                   WinGetLastError(hab));
        }
#endif

        rc = GpiSetCharSet(hps, LCID_DEFAULT);
#ifdef DEBUG
        if (rc==TRUE) {
            printf("GpiSetCharSet (%x, default) OK\n", hps);
        } else {
            printf("GpiSetCharSet (%x, default) ERROR, error %x\n", hps,
                   WinGetLastError(hab));
        }
#endif
        rc = GpiDeleteSetId(hps, (LONG)gc->font);
#ifdef DEBUG
        if (rc==TRUE) {
            printf("GpiDeleteSetId (%x, id %d) OK\n", hps, gc->font);
        } else {
            printf("GpiDeleteSetId (%x, id %d) ERROR, error %x\n", hps,
                   gc->font, WinGetLastError(hab));
        }
#endif

	if (gc->font != None) {
	    /* Set slant if necessary */
	    if (logfonts[(LONG)gc->font].setShear) {
	        GpiSetCharShear(hps, &noShear);
	    }
	    GpiSetCharSet(hps, oldFont);
	}
	GpiSetBackMix(hps, oldBackMix);
	GpiSetBackColor(hps, oldBackColor);
	cBundle.lColor = oldColor;
	GpiSetAttrs(hps, PRIM_CHAR, LBB_COLOR, 0L, (PBUNDLE)&cBundle);
	GpiSetTextAlignment(hps, oldHorAlign, oldVerAlign);
    }

    TkOS2ReleaseDrawablePS(d, hps, &state);
}

/*
 *----------------------------------------------------------------------
 *
 * XFillRectangles --
 *
 *	Fill multiple rectangular areas in the given drawable.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Draws onto the specified drawable.
 *
 *----------------------------------------------------------------------
 */

void
XFillRectangles(display, d, gc, rectangles, nrectangles)
    Display* display;
    Drawable d;
    GC gc;
    XRectangle* rectangles;
    int nrectangles;
{
    HPS hps;
    int i;
    RECTL rect;
    TkOS2PSState state;
    LONG windowHeight;
    POINTL refPoint;
    LONG oldPattern, oldBitmap;
    TkOS2Drawable *todPtr = (TkOS2Drawable *)d;
    LONG rc;

    if (d == None) {
	return;
    }
#ifdef DEBUG
    if (todPtr->type == TOD_BITMAP) {
         printf("XFillRectangles bitmap %x, h %x, cmap %x, depth %d, mix %x\n",
                todPtr, todPtr->bitmap.handle, todPtr->bitmap.colormap,
                todPtr->bitmap.depth, gc->function);
    } else {
         printf("XFillRectangles todPtr %x, winPtr %x, h %x, mix %x\n", todPtr,
                todPtr->window.winPtr, todPtr->window.handle, gc->function);
    }
#endif

    windowHeight = TkOS2WindowHeight(todPtr);

    hps = TkOS2GetDrawablePS(display, d, &state);
    GpiSetMix(hps, mixModes[gc->function]);

    if ((gc->fill_style == FillStippled
	    || gc->fill_style == FillOpaqueStippled)
	    && gc->stipple != None) {
	HBITMAP bitmap;
        BITMAPINFOHEADER2 bmpInfo;
        LONG rc;
        DEVOPENSTRUC dop = {0L, (PSZ)"DISPLAY", NULL, 0L, 0L, 0L, 0L, 0L, 0L};
        SIZEL sizl = {0,0}; /* use same page size as device */
        HDC dcMem;
	HPS psMem;
        POINTL aPoints[3]; /* Lower-left, upper-right, lower-left source */
        POINTL oldRefPoint;

#ifdef DEBUG
        printf("                stippled\n");
#endif
	todPtr = (TkOS2Drawable *)gc->stipple;

	if (todPtr->type != TOD_BITMAP) {
	    panic("unexpected drawable type in stipple");
	}

	/*
	 * Select stipple pattern into destination dc.
	 */

	refPoint.x = gc->ts_x_origin;
	/* Translate Xlib y to PM y */
	refPoint.y = windowHeight - gc->ts_y_origin;

        /* The bitmap mustn't be selected in the HPS */
        TkOS2SetStipple(hps, todPtr->bitmap.hps, todPtr->bitmap.handle,
                        refPoint.x, refPoint.y, &oldPattern, &oldRefPoint);

	dcMem = DevOpenDC(hab, OD_MEMORY, (PSZ)"*", 5L, (PDEVOPENDATA)&dop,
	                  NULLHANDLE);
	if (dcMem == DEV_ERROR) {
#ifdef DEBUG
            printf("DevOpenDC ERROR in XFillRectangles\n");
#endif
	    return;
	}
#ifdef DEBUG
        printf("DevOpenDC in XFillRectangles returns %x\n", dcMem);
#endif
        psMem = GpiCreatePS(hab, dcMem, &sizl,
                            PU_PELS | GPIT_NORMAL | GPIA_ASSOC);
        if (psMem == GPI_ERROR) {
#ifdef DEBUG
            printf("GpiCreatePS ERROR in XFillRectangles: %x\n",
                   WinGetLastError(hab));
#endif
            DevCloseDC(dcMem);
            return;
        }
#ifdef DEBUG
        printf("GpiCreatePS in XFillRectangles returns %x\n", psMem);
#endif

	/*
	 * For each rectangle, create a drawing surface which is the size of
	 * the rectangle and fill it with the background color.  Then merge the
	 * result with the stipple pattern.
	 */

	for (i = 0; i < nrectangles; i++) {
	    bmpInfo.cbFix = 16L;
	    bmpInfo.cx = rectangles[i].width + 1;
	    bmpInfo.cy = rectangles[i].height + 1;
	    bmpInfo.cPlanes = 1;
	    bmpInfo.cBitCount = 1;
	    bitmap = GpiCreateBitmap(psMem, &bmpInfo, 0L, NULL, NULL);
#ifdef DEBUG
            if (bitmap == GPI_ERROR) {
                printf("GpiCreateBitmap (%d,%d) GPI_ERROR %x\n",
                       bmpInfo.cx, bmpInfo.cy, WinGetLastError(hab));
            } else {
                printf("GpiCreateBitmap (%d,%d) returned %x\n", bmpInfo.cx, bmpInfo.cy, bitmap);
            }
#endif
	    oldBitmap = GpiSetBitmap(psMem, bitmap);
#ifdef DEBUG
            if (bitmap == HBM_ERROR) {
                printf("GpiSetBitmap (%x) HBM_ERROR %x\n", bitmap,
                       WinGetLastError(hab));
            } else {
                printf("GpiSetBitmap %x returned %x\n", bitmap, oldBitmap);
            }
#endif

            /* Translate the Y coordinates to PM coordinates */

	    rect.xLeft = 0;
	    rect.xRight = rectangles[i].width + 1;
	    rect.yBottom = 0;
	    rect.yTop = rectangles[i].height + 1;

	    oldPattern = GpiQueryPattern(psMem);
	    GpiSetPattern(psMem, PATSYM_SOLID);
	    rc = WinFillRect(psMem, &rect, gc->foreground);
#ifdef DEBUG
            if (rc != TRUE) {
                printf("WinFillRect3 (%d, %d)->(%d,%d) fg %x ERROR %x\n",
                       rect.xLeft, rect.yBottom, rect.xRight, rect.yTop,
                       gc->foreground, WinGetLastError(hab));
            } else {
                printf("WinFillRect3 (%d, %d)->(%d,%d) fg %x OK\n",
                       rect.xLeft, rect.yBottom, rect.xRight, rect.yTop,
                       gc->foreground);
            }
#endif
            /* Translate the Y coordinates to PM coordinates */
            aPoints[0].x = rectangles[i].x;
            aPoints[0].y = windowHeight - rectangles[i].y -
                           rectangles[i].height;
            aPoints[1].x = rectangles[i].x + rectangles[i].width + 1;
            aPoints[1].y = windowHeight - rectangles[i].y + 1;
	    aPoints[2].x = 0;
	    aPoints[2].y = 0;
            rc = GpiBitBlt(hps, psMem, 3, aPoints, COPYFG, BBO_IGNORE);
#ifdef DEBUG
            if (rc!=TRUE) {
                printf("    GpiBitBlt FG %x (%d,%d)(%d,%d) <- (%d,%d) ERROR %x\n",
                       gc->foreground, aPoints[0].x, aPoints[0].y,
                       aPoints[1].x, aPoints[1].y,
                       aPoints[2].x, aPoints[2].y, WinGetLastError(hab));
            } else {
                printf("    GpiBitBlt FG %x (%d,%d)(%d,%d) <- (%d,%d) OK\n",
                       gc->foreground, aPoints[0].x, aPoints[0].y,
                       aPoints[1].x, aPoints[1].y,
                       aPoints[2].x, aPoints[2].y);
            }
#endif
	    if (gc->fill_style == FillOpaqueStippled) {
	        rc = WinFillRect(psMem, &rect, gc->background);
#ifdef DEBUG
                if (rc != TRUE) {
                    printf("WinFillRect4 (%d, %d)->(%d,%d) bg %x ERROR %x\n",
                           rect.xLeft, rect.yBottom, rect.xRight, rect.yTop,
                           gc->background, WinGetLastError(hab));
                } else {
                    printf("WinFillRect4 (%d, %d)->(%d,%d) bg %x OK\n",
                           rect.xLeft, rect.yBottom, rect.xRight, rect.yTop,
                           gc->background);
                }
#endif
                rc = GpiBitBlt(hps, psMem, 3, aPoints, COPYBG, BBO_IGNORE);
#ifdef DEBUG
                if (rc!=TRUE) {
                    printf("    GpiBitBlt BG %x (%d,%d)(%d,%d) <- (%d,%d) ERROR %x\n",
                           gc->background, aPoints[0].x, aPoints[0].y,
                           aPoints[1].x, aPoints[1].y,
                           aPoints[2].x, aPoints[2].y, WinGetLastError(hab));
                } else {
                    printf("    GpiBitBlt BG %x (%d,%d)(%d,%d) <- (%d,%d) OK\n",
                           gc->background, aPoints[0].x, aPoints[0].y,
                           aPoints[1].x, aPoints[1].y,
                           aPoints[2].x, aPoints[2].y);
	        }
#endif
	    }
	    GpiSetPattern(psMem, oldPattern);
	    GpiDeleteBitmap(bitmap);
	}
	GpiDestroyPS(psMem);
        DevCloseDC(dcMem);
        /* The bitmap must be reselected in the HPS */
        TkOS2UnsetStipple(hps, todPtr->bitmap.hps, todPtr->bitmap.handle,
                          oldPattern, &oldRefPoint);
    } else {

        for (i = 0; i < nrectangles; i++) {
            rect.xLeft = rectangles[i].x;
            rect.xRight = rect.xLeft + rectangles[i].width;
            rect.yBottom = windowHeight - rectangles[i].y -
                           rectangles[i].height;
            rect.yTop = windowHeight - rectangles[i].y;
#ifdef DEBUG
            printf("rectangle (%d,%d) %dx%d; %s %x\n", rectangles[i].x,
                   rectangles[i].y, rectangles[i].width, rectangles[i].height,
                   todPtr->type == TOD_BITMAP ? "bitmap" : "window",
                   todPtr->type == TOD_BITMAP ? todPtr->bitmap.handle
                                              : todPtr->window.handle);
#endif
            rc = WinFillRect(hps, &rect, gc->foreground);
#ifdef DEBUG
            if (rc==TRUE) {
                printf("    WinFillRect5(hps %x, fg %x, (%d,%d)(%d,%d)) OK\n",
                       hps, gc->foreground, rect.xLeft, rect.yBottom,
                       rect.xRight, rect.yTop);
            } else {
                printf("    WinFillRect5(hps %x, fg %x, (%d,%d)(%d,%d)) ERROR %x\n",
                       hps, gc->foreground, rect.xLeft, rect.yBottom,
                       rect.xRight, rect.yTop, WinGetLastError(hab));
            }
#endif
            GpiSetPattern(hps, oldPattern);
        }
    }

    TkOS2ReleaseDrawablePS(d, hps, &state);
}

/*
 *----------------------------------------------------------------------
 *
 * RenderObject --
 *
 *	This function draws a shape using a list of points, a
 *	stipple pattern, and the specified drawing function.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
RenderObject(hps, gc, d, points, npoints, mode, pLineBundle, func)
    HPS hps;
    GC gc;
    Drawable d;
    XPoint* points;
    int npoints;
    int mode;
    PLINEBUNDLE pLineBundle;
    int func;
{
    RECTL rect;
    LINEBUNDLE oldLineBundle;
    LONG oldPattern;
    LONG oldColor;
    POINTL oldRefPoint;
    POINTL *os2Points;
    POINTL refPoint;
    POINTL aPoints[3]; /* Lower-left, upper-right, lower-left source */
    LONG windowHeight;
    POLYGON polygon;

#ifdef DEBUG
    printf("RenderObject, GC: line_width %d, line_style %x, cap_style %x,
           join_style %x, fill_style %x, fill_rule %x, arc_mode %x\n",
           gc->line_width, gc->line_style, gc->cap_style, gc->join_style,
           gc->fill_style, gc->fill_rule, gc->arc_mode);
#endif
    pLineBundle->lColor = gc->foreground;
    pLineBundle->lGeomWidth = gc->line_width;
    if ( func == TOP_POLYGONS) {
        pLineBundle->usType = LINETYPE_INVISIBLE;
    } else {
        pLineBundle->usType = lineStyles[gc->line_style];
    }
    pLineBundle->usEnd = capStyles[gc->cap_style];
    pLineBundle->usJoin = joinStyles[gc->join_style];

    /* os2Points/rect get *PM* coordinates handed to it by ConvertPoints */
    os2Points = ConvertPoints(d, points, npoints, mode, &rect);

    windowHeight = TkOS2WindowHeight((TkOS2Drawable *)d);

    if ((gc->fill_style == FillStippled
	    || gc->fill_style == FillOpaqueStippled)
	    && gc->stipple != None) {

	TkOS2Drawable *todPtr = (TkOS2Drawable *)gc->stipple;
        DEVOPENSTRUC dop = {0L, (PSZ)"DISPLAY", NULL, 0L, 0L, 0L, 0L, 0L, 0L};
        SIZEL sizl = {0,0}; /* use same page size as device */
        HDC dcMem;
	HPS psMem;
	LONG width, height;
	int i;
	HBITMAP bitmap, oldBitmap;
        BITMAPINFOHEADER2 bmpInfo;

#ifdef DEBUG
        printf("RenderObject stippled (%x)\n", todPtr->bitmap.handle);
#endif
	
	if (todPtr->type != TOD_BITMAP) {
	    panic("unexpected drawable type in stipple");
	}

    
	width = rect.xRight - rect.xLeft;
	/* PM coordinates are just reverse: top - bottom */
	height = rect.yTop - rect.yBottom;

	/*
	 * Select stipple pattern into destination hps.
	 */
	
	refPoint.x = gc->ts_x_origin;
	/* Translate Xlib y to PM y */
	refPoint.y = windowHeight - gc->ts_y_origin;

        TkOS2SetStipple(hps, todPtr->bitmap.hps, todPtr->bitmap.handle,
                        refPoint.x, refPoint.y, &oldPattern, &oldRefPoint);

	/*
	 * Create temporary drawing surface containing a copy of the
	 * destination equal in size to the bounding box of the object.
	 */
	
	dcMem = DevOpenDC(hab, OD_MEMORY, (PSZ)"*", 5L, (PDEVOPENDATA)&dop,
	                  NULLHANDLE);
	if (dcMem == DEV_ERROR) {
#ifdef DEBUG
            printf("DevOpenDC ERROR in RenderObject\n");
#endif
	    return;
	}
#ifdef DEBUG
        printf("DevOpenDC in RenderObject returns %x\n", dcMem);
#endif
        psMem = GpiCreatePS(hab, dcMem, &sizl,
                            PU_PELS | GPIT_NORMAL | GPIA_ASSOC);
        if (psMem == GPI_ERROR) {
#ifdef DEBUG
            printf("GpiCreatePS ERROR in RenderObject: %x\n",
                   WinGetLastError(hab));
#endif
            DevCloseDC(dcMem);
            return;
        }
#ifdef DEBUG
        printf("GpiCreatePS in RenderObject returns %x\n", psMem);
#endif

	TkOS2SelectPalette(psMem, HWND_DESKTOP, todPtr->bitmap.colormap);

    /*
     * X filling includes top and left sides, excludes bottom and right sides.
     * PM filling (WinFillRect) and BitBlt-ing (GpiBitBlt) includes bottom and
     * left sides, excludes top and right sides.
     * NB! X fills a box exactly as wide and high as width and height specify,
     * while PM cuts one pixel off the right and top.
     * => decrement y (X Window System) by one / increment y (PM) by one AND
     *    increment height by one, and increment width by one.
     */

	bmpInfo.cbFix = sizeof(BITMAPINFOHEADER2);
	bmpInfo.cx = width + 1;
	bmpInfo.cy = height + 1;
	bitmap = GpiCreateBitmap(psMem, &bmpInfo, 0L, NULL, NULL);
#ifdef DEBUG
        if (rc==GPI_ERROR) {
            printf("    GpiCreateBitmap %dx%d ERROR %x\n", width, height,
                   WinGetLastError(hab));
        } else {
            printf("    GpiCreateBitmap %dx%d OK: %x\n", width, height, bitmap);
        }
#endif
	oldBitmap = GpiSetBitmap(psMem, bitmap);
#ifdef DEBUG
        if (rc==HBM_ERROR) {
            printf("    GpiSetBitmap %x ERROR %x\n", bitmap,
                   WinGetLastError(hab));
        } else {
            printf("    GpiSetBitmap %x OK: %x\n", bitmap, oldBitmap);
        }
#endif
	rc = GpiSetAttrs(psMem, PRIM_LINE, LBB_COLOR | LBB_GEOM_WIDTH
	                 | LBB_TYPE | LBB_END | LBB_JOIN, 0L, pLineBundle);
#ifdef DEBUG
        if (rc != TRUE) {
            printf("GpiSetAttrs color %x, width %d, type %x ERROR %x\n",
                   pLineBundle->lColor, pLineBundle->lGeomWidth,
                   pLineBundle->usType, WinGetLastError(hab));
        } else {
            printf("GpiSetAttrs color %x, width %d, type %x OK\n",
                   pLineBundle->lColor, pLineBundle->lGeomWidth,
                   pLineBundle->usType);
        }
#endif
        /* Translate the Y coordinates to PM coordinates */
        aPoints[0].x = 0;	/* dest_x 0 */
        aPoints[0].y = 0;	/* dest_y 0 */
        aPoints[1].x = width + 1;	/* dest_x + width */
        aPoints[1].y = height + 1;	/* dest_y + height */
        aPoints[2].x = rect.xLeft;
        aPoints[2].y = rect.yBottom;

        GpiBitBlt(psMem, hps, 3, aPoints, ROP_SRCCOPY, BBO_IGNORE);
#ifdef DEBUG
        if (rc!=TRUE) {
            printf("    GpiBitBlt %x->%x ERROR %x\n", hps, psMem,
        WinGetLastError(hab));
        } else {
            printf("    GpiBitBlt %x->%x OK, aPoints (%d,%d)(%d,%d) (%d,%d)\n",
                   hps, psMem, aPoints[0].x, aPoints[0].y, aPoints[1].x,
                   aPoints[1].y, aPoints[2].x, aPoints[2].y);
        }
#endif

	/*
	 * Translate the object to 0,0 for rendering in the temporary drawing
	 * surface. 
	 */

	for (i = 0; i < npoints; i++) {
	    os2Points[i].x -= rect.xLeft;
	    os2Points[i].y -= rect.yBottom;
#ifdef DEBUG
            printf("os2Points[%d].x %d, os2Points[%d].y %d\n", i,
                   os2Points[i].x, i, os2Points[i].y);
#endif
	}

	/*
	 * Draw the object in the foreground color and copy it to the
	 * destination wherever the pattern is set.
	 */

	rc = GpiSetColor(psMem, gc->foreground);
#ifdef DEBUG
        if (rc != TRUE) {
            printf("GpiSetColor %x ERROR %x\n", gc->foreground,
                   WinGetLastError(hab));
        } else {
            printf("GpiSetColor %x OK\n", gc->foreground);
        }
#endif
	rc = GpiSetPattern(psMem, PATSYM_SOLID);
	if (func == TOP_POLYGONS) {
	    rc = GpiSetCurrentPosition(psMem, os2Points);
#ifdef DEBUG
            if (rc != TRUE) {
                printf("GpiSetCurrentPosition %d,%d ERROR %x\n",
                       os2Points[0].x, os2Points[0].y, WinGetLastError(hab));
            } else {
                printf("GpiSetCurrentPosition %d,%d OK\n",
                       os2Points[0].x, os2Points[0].y);
            }
#endif
            polygon.ulPoints = npoints-1;
            polygon.aPointl = os2Points+1;
	    rc = GpiPolygons(psMem, 1, &polygon, POLYGON_BOUNDARY |
	                     (gc->fill_rule == EvenOddRule) ? POLYGON_ALTERNATE
	                                                    : POLYGON_WINDING,
	                     POLYGON_INCL);
#ifdef DEBUG
            if (rc == GPI_ERROR) {
                printf("GpiPolygons ERROR %x\n", WinGetLastError(hab));
            } else {
                printf("GpiPolygons OK\n");
            }
#endif
	} else { /* TOP_POLYLINE */
	    rc = GpiSetCurrentPosition(psMem, os2Points);
#ifdef DEBUG
            if (rc != TRUE) {
                printf("GpiSetCurrentPosition %d,%d ERROR %x\n",
                       os2Points[0].x, os2Points[0].y, WinGetLastError(hab));
            } else {
                printf("GpiSetCurrentPosition %d,%d OK\n",
                       os2Points[0].x, os2Points[0].y);
            }
#endif
            rc = GpiBeginPath(psMem, 1);
#ifdef DEBUG
            if (rc != TRUE) {
                printf("GpiBeginPath ERROR %x\n", WinGetLastError(hab));
            } else {
                printf("GpiBeginPath OK\n");
            }
#endif
            rc = GpiPolyLine(psMem, npoints-1, os2Points+1);
#ifdef DEBUG
                if (rc == GPI_ERROR) {
                    printf("GpiPolyLine ERROR %x\n", WinGetLastError(hab));
                } else {
                    printf("GpiPolyLine OK\n");
                }
#endif
            rc = GpiEndPath(psMem);
#ifdef DEBUG
            if (rc != TRUE) {
                printf("GpiEndPath ERROR %x\n", WinGetLastError(hab));
            } else {
                printf("GpiEndPath OK\n");
            }
#endif
            rc = GpiStrokePath(psMem, 1, 0);
#ifdef DEBUG
            if (rc == GPI_OK) {
                printf("GpiStrokePath OK\n");
            } else {
                printf("GpiStrokePath ERROR %x\n", WinGetLastError(hab));
            }
#endif
	}
        aPoints[0].x = rect.xLeft;	/* dest_x */
        aPoints[0].y = rect.yBottom;
        aPoints[1].x = rect.xRight;	/* dest_x + width */
        aPoints[1].y = rect.yTop;	/* dest_y */
        aPoints[2].x = 0;	/* src_x 0 */
        aPoints[2].y = 0;	/* Src_y */
        rc = GpiBitBlt(hps, psMem, 3, aPoints, COPYFG, BBO_IGNORE);
#ifdef DEBUG
        if (rc!=TRUE) {
            printf("GpiBitBlt FG %d,%d-%d,%d ERROR %x\n",
                   aPoints[0].x, aPoints[0].y, aPoints[1].x, aPoints[1].y,
                   WinGetLastError(hab));
        } else {
            printf("GpiBitBlt FG %d,%d-%d,%d OK\n",
                   aPoints[0].x, aPoints[0].y, aPoints[1].x, aPoints[1].y);
        }
#endif

	/*
	 * If we are rendering an opaque stipple, then draw the polygon in the
	 * background color and copy it to the destination wherever the pattern
	 * is clear.
	 */

	if (gc->fill_style == FillOpaqueStippled) {
	    GpiSetColor(psMem, gc->background);
	    if (func == TOP_POLYGONS) {
                polygon.ulPoints = npoints;
                polygon.aPointl = os2Points;
	        rc = GpiPolygons(psMem, 1, &polygon,
	                      (gc->fill_rule == EvenOddRule) ? POLYGON_ALTERNATE
	                                                     : POLYGON_WINDING,
	                      0);
#ifdef DEBUG
                if (rc == GPI_ERROR) {
                    printf("GpiPolygons ERROR %x\n", WinGetLastError(hab));
                } else {
                    printf("GpiPolygons OK\n");
                }
#endif
	        } else { /* TOP_POLYLINE */
                rc = GpiBeginPath(psMem, 1);
#ifdef DEBUG
                if (rc != TRUE) {
                    printf("GpiBeginPath ERROR %x\n", WinGetLastError(hab));
                } else {
                    printf("GpiBeginPath OK\n");
                }
#endif
                rc = GpiPolyLine(psMem, npoints, os2Points);
#ifdef DEBUG
                if (rc == GPI_ERROR) {
                    printf("GpiPolyLine ERROR %x\n", WinGetLastError(hab));
                } else {
                    printf("GpiPolyLine OK\n");
                }
#endif
                rc = GpiEndPath(psMem);
#ifdef DEBUG
                if (rc != TRUE) {
                    printf("GpiEndPath ERROR %x\n", WinGetLastError(hab));
                } else {
                    printf("GpiEndPath OK\n");
                }
#endif
                rc = GpiStrokePath(psMem, 1, 0);
#ifdef DEBUG
                if (rc == GPI_OK) {
                    printf("GpiStrokePath OK\n");
                } else {
                    printf("GpiStrokePath ERROR %x\n", WinGetLastError(hab));
                }
#endif
	    }
            rc = GpiBitBlt(hps, psMem, 3, aPoints, COPYBG, BBO_IGNORE);
#ifdef DEBUG
            if (rc!=TRUE) {
                printf("GpiBitBlt BG %d,%d-%d,%d ERROR %x\n",
                       aPoints[0].x, aPoints[0].y, aPoints[1].x, aPoints[1].y,
                       WinGetLastError(hab));
            } else {
                printf("GpiBitBlt BG %d,%d-%d,%d OK\n",
                       aPoints[0].x, aPoints[0].y, aPoints[1].x, aPoints[1].y);
            }
#endif
	}
	/* end of using 254 */
        TkOS2UnsetStipple(hps, todPtr->bitmap.hps, todPtr->bitmap.handle,
                          oldPattern, &oldRefPoint);
	GpiDestroyPS(psMem);
        DevCloseDC(dcMem);
    } else {

	GpiQueryAttrs(hps, PRIM_LINE, LBB_COLOR | LBB_GEOM_WIDTH | LBB_TYPE,
	              &oldLineBundle);
	rc = GpiSetAttrs(hps, PRIM_LINE, LBB_COLOR | LBB_GEOM_WIDTH | LBB_TYPE
	                 | LBB_END | LBB_JOIN, 0L, pLineBundle);
#ifdef DEBUG
        if (rc != TRUE) {
            printf("GpiSetAttrs color %x, width %d, type %x ERROR %x\n",
                   pLineBundle->lColor, pLineBundle->lGeomWidth,
                   pLineBundle->usType, WinGetLastError(hab));
        } else {
            printf("GpiSetAttrs color %x, width %d, type %x OK\n",
                   pLineBundle->lColor, pLineBundle->lGeomWidth,
                   pLineBundle->usType);
        }
#endif
        oldColor = GpiQueryColor(hps);
        oldPattern = GpiQueryPattern(hps);
	rc = GpiSetColor(hps, gc->foreground);
#ifdef DEBUG
        if (rc != TRUE) {
            printf("GpiSetColor %x ERROR %x\n", gc->foreground,
                   WinGetLastError(hab));
        } else {
            printf("GpiSetColor %x OK\n", gc->foreground);
        }
#endif
	rc = GpiSetPattern(hps, PATSYM_SOLID);
	rc = GpiSetMix(hps, mixModes[gc->function]);

        if (func == TOP_POLYGONS) {
	    rc = GpiSetCurrentPosition(hps, os2Points);
#ifdef DEBUG
            if (rc != TRUE) {
                printf("GpiSetCurrentPosition %d,%d ERROR %x\n",
                       os2Points[0].x, os2Points[0].y, WinGetLastError(hab));
            } else {
                printf("GpiSetCurrentPosition %d,%d OK\n",
                       os2Points[0].x, os2Points[0].y);
            }
#endif
            polygon.ulPoints = npoints-1;
            polygon.aPointl = os2Points+1;
            rc = GpiPolygons(hps, 1, &polygon, POLYGON_BOUNDARY |
                             (gc->fill_rule == EvenOddRule) ? POLYGON_ALTERNATE
                                                            : POLYGON_WINDING,
	                     POLYGON_INCL);
#ifdef DEBUG
            if (rc == GPI_ERROR) {
                printf("GpiPolygons ERROR %x\n", WinGetLastError(hab));
            } else {
                printf("GpiPolygons OK\n");
            }
#endif
        } else { /* TOP_POLYLINE */
	    rc = GpiSetCurrentPosition(hps, os2Points);
#ifdef DEBUG
            if (rc != TRUE) {
                printf("GpiSetCurrentPosition %d,%d ERROR %x\n",
                       os2Points[0].x, os2Points[0].y, WinGetLastError(hab));
            } else {
                printf("GpiSetCurrentPosition %d,%d OK\n",
                       os2Points[0].x, os2Points[0].y);
            }
#endif
            rc = GpiBeginPath(hps, 1);
#ifdef DEBUG
            if (rc != TRUE) {
                printf("GpiBeginPath ERROR %x\n", WinGetLastError(hab));
            } else {
                printf("GpiBeginPath OK\n");
            }
#endif
            rc = GpiPolyLine(hps, npoints-1, os2Points+1);
#ifdef DEBUG
            if (rc == GPI_ERROR) {
                printf("GpiPolyLine ERROR %x\n", WinGetLastError(hab));
            } else {
                printf("GpiPolyLine OK\n");
            }
#endif

            rc = GpiEndPath(hps);
#ifdef DEBUG
            if (rc != TRUE) {
                printf("GpiEndPath ERROR %x\n", WinGetLastError(hab));
            } else {
                printf("GpiEndPath OK\n");
            }
#endif
            rc = GpiStrokePath(hps, 1, 0);
#ifdef DEBUG
            if (rc == GPI_OK) {
                printf("GpiStrokePath OK\n");
            } else {
                printf("GpiStrokePath ERROR %x\n", WinGetLastError(hab));
            }
#endif
        }

	GpiSetColor(hps, oldColor);
	GpiSetPattern(hps, oldPattern);
	GpiSetAttrs(hps, PRIM_LINE, LBB_COLOR | LBB_GEOM_WIDTH | LBB_TYPE, 0L,
	            &oldLineBundle);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * XDrawLines --
 *
 *	Draw connected lines.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Renders a series of connected lines.
 *
 *----------------------------------------------------------------------
 */

void
XDrawLines(display, d, gc, points, npoints, mode)
    Display* display;
    Drawable d;
    GC gc;
    XPoint* points;
    int npoints;
    int mode;
{
    LINEBUNDLE lineBundle;
    TkOS2PSState state;
    HPS hps;

#ifdef DEBUG
    printf("XDrawLines fg %x, bg %x, width %d\n", gc->foreground,
           gc->background, gc->line_width);
#endif
    
    if (d == None) {
	return;
    }

    hps = TkOS2GetDrawablePS(display, d, &state);

    RenderObject(hps, gc, d, points, npoints, mode, &lineBundle, TOP_POLYLINE);
    
    TkOS2ReleaseDrawablePS(d, hps, &state);
}

/*
 *----------------------------------------------------------------------
 *
 * XFillPolygon --
 *
 *	Draws a filled polygon.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Draws a filled polygon on the specified drawable.
 *
 *----------------------------------------------------------------------
 */

void
XFillPolygon(display, d, gc, points, npoints, shape, mode)
    Display* display;
    Drawable d;
    GC gc;
    XPoint* points;
    int npoints;
    int shape;
    int mode;
{
    LINEBUNDLE lineBundle;
    TkOS2PSState state;
    HPS hps;

#ifdef DEBUG
    printf("XFillPolygon\n");
#endif

    if (d == None) {
	return;
    }

    hps = TkOS2GetDrawablePS(display, d, &state);

    RenderObject(hps, gc, d, points, npoints, mode, &lineBundle, TOP_POLYGONS);

    TkOS2ReleaseDrawablePS(d, hps, &state);
}

/*
 *----------------------------------------------------------------------
 *
 * XDrawRectangle --
 *
 *	Draws a rectangle.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Draws a rectangle on the specified drawable.
 *
 *----------------------------------------------------------------------
 */

void
XDrawRectangle(display, d, gc, x, y, width, height)
    Display* display;
    Drawable d;
    GC gc;
    int x;
    int y;
    unsigned int width;
    unsigned int height;
{
    LINEBUNDLE lineBundle, oldLineBundle;
    TkOS2PSState state;
    LONG oldPattern;
    HPS hps;
    POINTL oldCurrent, changePoint;
    LONG windowHeight;

#ifdef DEBUG
    printf("XDrawRectangle\n");
#endif

    if (d == None) {
	return;
    }

    windowHeight = TkOS2WindowHeight((TkOS2Drawable *)d);

    hps = TkOS2GetDrawablePS(display, d, &state);

    GpiQueryAttrs(hps, PRIM_LINE, LBB_COLOR | LBB_GEOM_WIDTH | LBB_TYPE,
                  &oldLineBundle);
    lineBundle.lColor = gc->foreground;
    lineBundle.lGeomWidth = gc->line_width;
    lineBundle.usType = LINETYPE_SOLID;
    GpiSetAttrs(hps, PRIM_LINE, LBB_COLOR | LBB_GEOM_WIDTH | LBB_TYPE, 0L,
                &lineBundle);
    oldPattern = GpiQueryPattern(hps);
    GpiSetPattern(hps, PATSYM_NOSHADE);
    GpiSetMix(hps, mixModes[gc->function]);

    GpiQueryCurrentPosition(hps, &oldCurrent);
    changePoint.x = x;
    /* Translate the Y coordinates to PM coordinates */
    changePoint.y = windowHeight - y;
    GpiSetCurrentPosition(hps, &changePoint);
    /*
     * Now put other point of box in changePoint.
     * NB! X draws a box 1 pixel wider and higher than width and height
     * specify.
     */
    changePoint.x += width + 1;
    changePoint.y -= (height + 1);	/* PM coordinates are reverse */
    GpiBox(hps, DRO_OUTLINE, &changePoint, 0L, 0L);
    GpiSetCurrentPosition(hps, &oldCurrent);

    GpiSetAttrs(hps, PRIM_LINE, LBB_COLOR | LBB_GEOM_WIDTH | LBB_TYPE, 0L,
                &oldLineBundle);
    GpiSetPattern(hps, oldPattern);
    TkOS2ReleaseDrawablePS(d, hps, &state);
}

/*
 *----------------------------------------------------------------------
 *
 * XDrawArc --
 *
 *	Draw an arc.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Draws an arc on the specified drawable.
 *
 *----------------------------------------------------------------------
 */

void
XDrawArc(display, d, gc, x, y, width, height, angle1, angle2)
    Display* display;
    Drawable d;
    GC gc;
    int x;
    int y;
    unsigned int width;
    unsigned int height;
    int angle1;
    int angle2;
{
#ifdef DEBUG
    printf("XDrawArc d %x (%d,%d) %dx%d, a1 %d a2 %d\n", d, x, y, width, height,
           angle1, angle2);
#endif
    display->request++;

    DrawOrFillArc(display, d, gc, x, y, width, height, angle1, angle2, 0);
}

/*
 *----------------------------------------------------------------------
 *
 * XFillArc --
 *
 *	Draw a filled arc.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Draws a filled arc on the specified drawable.
 *
 *----------------------------------------------------------------------
 */

void
XFillArc(display, d, gc, x, y, width, height, angle1, angle2)
    Display* display;
    Drawable d;
    GC gc;
    int x;
    int y;
    unsigned int width;
    unsigned int height;
    int angle1;
    int angle2;
{
#ifdef DEBUG
    printf("XFillArc d %x (%d,%d) %dx%d, a1 %d a2 %d\n", d, x, y, width, height,
           angle1, angle2);
#endif
    display->request++;

    DrawOrFillArc(display, d, gc, x, y, width, height, angle1, angle2, 1);
}

/*
 *----------------------------------------------------------------------
 *
 * DrawOrFillArc --
 *
 *	This procedure handles the rendering of drawn or filled
 *	arcs and chords.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Renders the requested arc.
 *
 *----------------------------------------------------------------------
 */

static void
DrawOrFillArc(display, d, gc, x, y, width, height, angle1, angle2, fill)
    Display *display;
    Drawable d;
    GC gc;
    int x, y;			/* left top */
    unsigned int width, height;
    int angle1;			/* angle1: three-o'clock (deg*64) */
    int angle2;			/* angle2: relative (deg*64) */
    int fill;			/* ==0 draw, !=0 fill */
{
    HPS hps;
    LONG oldColor, oldMix, oldPattern;
    LINEBUNDLE lineBundle, oldLineBundle;
    AREABUNDLE aBundle;
    int sign;
    POINTL center, curPt;
    TkOS2PSState state;
    LONG windowHeight;
    ARCPARAMS arcParams, oldArcParams;
    double a1sin, a1cos;
    TkOS2Drawable *todPtr = (TkOS2Drawable *)d;
    POINTL refPoint;

    if (d == None) {
	return;
    }
    a1sin = sin(XAngleToRadians(angle1));
    a1cos = cos(XAngleToRadians(angle1));
    windowHeight = TkOS2WindowHeight(todPtr);

#ifdef DEBUG
    printf("DrawOrFillArc %d,%d (PM %d), %dx%d, c %x, a1 %d (%f), a2 %d, %s, %s, %s\n",
           x, y, windowHeight - y, width, height, gc->foreground, angle1,
           XAngleToRadians(angle1), angle2, fill ? "fill" : "nofill",
           gc->arc_mode == ArcChord ? "Chord" : "Arc/Pie",
           ((gc->fill_style == FillStippled || gc->fill_style == FillOpaqueStippled)
             && gc->stipple != None) ? "stippled " : "not stippled");
#endif

    /* Translate the Y coordinates to PM coordinates */
    y = windowHeight - y;
    /* Translate angles back to positive degrees */
    angle1 = abs(angle1 / 64);
    if (angle2 < 0) {
        sign = -1;
        /*
         * Not only the sweep but also the starting angle gets computed
         * counter-clockwise when Arc Param Q is negative (p*q actually).
         */
        angle1 = 360 - angle1;
    }
    else {
        sign = 1;
    }
    angle2 = abs(angle2 / 64);

    hps = TkOS2GetDrawablePS(display, d, &state);

    oldColor = GpiQueryColor(hps);
    oldPattern = GpiQueryPattern(hps);
    oldMix = GpiQueryMix(hps);
    GpiSetColor(hps, gc->foreground);
    GpiSetPattern(hps, PATSYM_SOLID);
    GpiSetMix(hps, mixModes[gc->function]);

    /*
     * Now draw a filled or open figure.
     */

    GpiQueryAttrs(hps, PRIM_LINE, LBB_COLOR | LBB_GEOM_WIDTH | LBB_TYPE,
                  &oldLineBundle);
    if ((gc->fill_style == FillStippled || gc->fill_style == FillOpaqueStippled)
        && gc->stipple != None) {
        HBITMAP bitmap, oldBitmap;
        BITMAPINFOHEADER2 bmpInfo;
        LONG rc;
        DEVOPENSTRUC dop = {0L, (PSZ)"DISPLAY", NULL, 0L, 0L, 0L, 0L, 0L, 0L};
        SIZEL sizl = {0,0}; /* use same page size as device */
        HDC dcMem;
        HPS psMem;
        POINTL aPoints[3]; /* Lower-left, upper-right, lower-left source */
        POINTL oldRefPoint;

        todPtr = (TkOS2Drawable *)gc->stipple;

        if (todPtr->type != TOD_BITMAP) {
            panic("unexpected drawable type in stipple");
        }

        /*
         * Select stipple pattern into destination dc.
         */
        /* Translate Xlib y to PM y */
        refPoint.x = gc->ts_x_origin;
        refPoint.y = windowHeight - gc->ts_y_origin;

        dcMem = DevOpenDC(hab, OD_MEMORY, (PSZ)"*", 5L, (PDEVOPENDATA)&dop,
                          NULLHANDLE);
        if (dcMem == DEV_ERROR) {
#ifdef DEBUG
            printf("DevOpenDC ERROR in DrawOrFillArc\n");
#endif
            return;
        }
#ifdef DEBUG
        printf("DevOpenDC in DrawOrFillArc returns %x\n", dcMem);
#endif
        psMem = GpiCreatePS(hab, dcMem, &sizl,
                            PU_PELS | GPIT_NORMAL | GPIA_ASSOC);
        if (psMem == GPI_ERROR) {
#ifdef DEBUG
            printf("GpiCreatePS ERROR in DrawOrFillArc: %x\n",
                   WinGetLastError(hab));
#endif
            DevCloseDC(dcMem);
            return;
        }
#ifdef DEBUG
        printf("GpiCreatePS in DrawOrFillArc returns %x\n", psMem);
#endif

        rc = GpiQueryArcParams(psMem, &oldArcParams);
        arcParams.lP = width / 2;
        arcParams.lQ = sign * (height / 2);
        arcParams.lR = 0;
        arcParams.lS = 0;
        rc = GpiSetArcParams(psMem, &arcParams);
#ifdef DEBUG
        if (rc != TRUE) {
            printf("GpiSetArcParams p %d q %d (%d) ERROR %x\n", arcParams.lP,
                   arcParams.lQ, arcParams.lP * arcParams.lQ,
                   WinGetLastError(hab));
        } else {
            printf("GpiSetArcParams p %d q %d (%d) OK\n", arcParams.lP,
                   arcParams.lQ, arcParams.lP * arcParams.lQ);
        }
#endif

	/*
	 * Draw the object in the foreground color and copy it to the
	 * destination wherever the pattern is set.
	 */

	rc = GpiSetColor(psMem, gc->foreground);
#ifdef DEBUG
        if (rc != TRUE) {
            printf("GpiSetColor %x ERROR %x\n", gc->foreground,
                   WinGetLastError(hab));
        } else {
            printf("GpiSetColor %x OK\n", gc->foreground);
        }
#endif

    /*
     * X filling includes top and left sides, excludes bottom and right sides.
     * PM filling (WinFillRect) and BitBlt-ing (GpiBitBlt) includes bottom and
     * left sides, excludes top and right sides.
     * NB! X fills a box exactly as wide and high as width and height specify,
     * while PM cuts one pixel off the right and top.
     * => decrement y (X Window System) by one / increment y (PM) by one AND
     *    increment height by one, and increment width by one.
     */

        bmpInfo.cbFix = 16L;
	/* Bitmap must be able to contain a thicker line! */
        bmpInfo.cx = width + gc->line_width + 1;
        bmpInfo.cy = height + gc->line_width + 1;
        bmpInfo.cPlanes = 1;
        bmpInfo.cBitCount = display->screens[display->default_screen].root_depth;
        bitmap = GpiCreateBitmap(psMem, &bmpInfo, 0L, NULL, NULL);
#ifdef DEBUG
        if (bitmap == GPI_ERROR) {
            printf("GpiCreateBitmap (%d,%d) GPI_ERROR %x\n", bmpInfo.cx,
                   bmpInfo.cy, WinGetLastError(hab));
        } else {
            printf("GpiCreateBitmap (%d,%d) returned %x\n", bmpInfo.cx,
                   bmpInfo.cy, bitmap);
        }
#endif
        oldBitmap = GpiSetBitmap(psMem, bitmap);
#ifdef DEBUG
        if (bitmap == HBM_ERROR) {
            printf("GpiSetBitmap (%x) HBM_ERROR %x\n", bitmap,
                   WinGetLastError(hab));
        } else {
            printf("GpiSetBitmap %x returned %x\n", bitmap, oldBitmap);
        }
#endif

        TkOS2SelectPalette(psMem, HWND_DESKTOP, todPtr->bitmap.colormap);

        /* Line width! */
        aPoints[0].x = 0;
        aPoints[0].y = 0;
        aPoints[1].x = width + gc->line_width + 1;
        aPoints[1].y = height + gc->line_width + 1;
        aPoints[2].x = x - (gc->line_width/2);
        aPoints[2].y = y - height + 1 - (gc->line_width/2);

        rc = GpiBitBlt(psMem, hps, 3, aPoints, ROP_SRCCOPY, BBO_IGNORE);
#ifdef DEBUG
        if (rc!=TRUE) {
            printf("    GpiBitBlt %x->%x ERROR %x\n", hps, psMem,
                   WinGetLastError(hab));
        } else {
            printf("    GpiBitBlt %x->%x OK, aPoints (%d,%d)(%d,%d) (%d,%d)\n", hps,
                   psMem, aPoints[0].x, aPoints[0].y, aPoints[1].x,
                   aPoints[1].y, aPoints[2].x, aPoints[2].y);
        }
#endif

        /* The bitmap mustn't be selected in the HPS */
        TkOS2SetStipple(hps, todPtr->bitmap.hps, todPtr->bitmap.handle,
                        refPoint.x, refPoint.y, &oldPattern, &oldRefPoint);
        /* Drawing */
        /* Center of arc is at x+(0.5*width),y-(0.5*height) */
        /* Translate to 0,0 for rendering in psMem */
        center.x = (0.5 * width) + (gc->line_width/2);
        center.y = (0.5 * height) + (gc->line_width/2);
        lineBundle.lColor = gc->foreground;
        lineBundle.lGeomWidth = gc->line_width;
        lineBundle.usType = LINETYPE_SOLID;
        rc = GpiSetAttrs(psMem, PRIM_LINE,
                         LBB_COLOR | LBB_GEOM_WIDTH | LBB_TYPE,
                         0L, &lineBundle);
#ifdef DEBUG
        if (rc != TRUE) {
            printf("GpiSetAttrs ERROR %x\n", WinGetLastError(hab));
        } else {
            printf("GpiSetAttrs OK\n");
        }
#endif
        aBundle.lColor = gc->foreground;
        rc = GpiSetAttrs(psMem, PRIM_AREA, LBB_COLOR, 0L, (PBUNDLE)&aBundle);
#ifdef DEBUG
        if (rc!=TRUE) {
            printf("GpiSetAttrs areaColor %d ERROR %x\n", aBundle.lColor,
                   WinGetLastError(hab));
        } else {
            printf("GpiSetAttrs areaColor %d OK\n", aBundle.lColor);
        }
#endif
        if (!fill) {
            curPt.x = center.x + (int) (0.5 * width * a1cos);
            curPt.y = center.y + (int) (0.5 * height * a1sin);
            rc = GpiSetCurrentPosition(psMem, &curPt);
#ifdef DEBUG
            if (rc != TRUE) {
                printf("GpiSetCurrentPosition %d,%d -> %d,%d ERROR %x\n",
                       center.x, center.y, curPt.x, curPt.y,
                       WinGetLastError(hab));
            } else {
                printf("GpiSetCurrentPosition %d,%d -> %d,%d OK\n",
                       center.x, center.y, curPt.x, curPt.y);
            }
#endif
            rc = GpiBeginPath(psMem, 1);
#ifdef DEBUG
            if (rc != TRUE) {
                printf("GpiBeginPath ERROR %x\n", WinGetLastError(hab));
            } else {
                printf("GpiBeginPath OK\n");
            }
#endif
            rc= GpiPartialArc(psMem, &center, MAKEFIXED(1, 0),
                              MAKEFIXED(angle1, 0), MAKEFIXED(angle2, 0));
#ifdef DEBUG
            if (rc == GPI_ERROR) {
                printf("GpiPartialArc a1 %d, a2 %d ERROR %x\n", angle1, angle2,
                       WinGetLastError(hab));
            } else {
                printf("GpiPartialArc a1 %d, a2 %d OK\n", angle1, angle2);
            }
#endif
            rc = GpiEndPath(psMem);
#ifdef DEBUG
            if (rc != TRUE) {
                printf("GpiEndPath ERROR %x\n", WinGetLastError(hab));
            } else {
                printf("GpiEndPath OK\n");
            }
#endif
            rc = GpiStrokePath(psMem, 1, 0);
#ifdef DEBUG
            if (rc == GPI_OK) {
                printf("GpiStrokePath OK\n");
            } else {
                printf("GpiStrokePath ERROR %x\n", WinGetLastError(hab));
            }
#endif
        } else {
            curPt.x = center.x + (int) (0.5 * width * a1cos);
            curPt.y = center.y + (int) (0.5 * height * a1sin);
            rc = GpiSetCurrentPosition(psMem, &curPt);
            if (gc->arc_mode == ArcChord) {
                /* Chord */
                /*
                 * See GPI reference: first do GpiPartialArc with invisible,
                 * line then again with visible line, in an Area for filling.
                 */
                rc = GpiSetLineType(psMem, LINETYPE_INVISIBLE);
#ifdef DEBUG
                if (rc != TRUE) {
                    printf("GpiSetLineType ERROR %x\n", WinGetLastError(hab));
                } else {
                    printf("GpiSetLineType OK\n");
                }
#endif
                rc = GpiPartialArc(psMem, &center, MAKEFIXED(1, 0),
                                   MAKEFIXED(angle1, 0), MAKEFIXED(angle2, 0));
#ifdef DEBUG
                if (rc == GPI_ERROR) {
                    printf("GpiPartialArc a1 %d, a2 %d ERROR %x\n", angle1,
                           angle2, WinGetLastError(hab));
                } else {
                    printf("GpiPartialArc a1 %d, a2 %d OK\n", angle1, angle2);
                }
#endif
                rc = GpiSetLineType(psMem, LINETYPE_SOLID);
#ifdef DEBUG
                if (rc != TRUE) {
                    printf("GpiSetLineType ERROR %x\n", WinGetLastError(hab));
                } else {
                    printf("GpiSetLineType OK\n");
                }
#endif
                rc = GpiBeginArea(psMem, BA_NOBOUNDARY|BA_ALTERNATE);
#ifdef DEBUG
                if (rc != TRUE) {
                    printf("GpiBeginArea ERROR %x\n", WinGetLastError(hab));
                } else {
                    printf("GpiBeginArea OK\n");
                }
#endif
                rc = GpiPartialArc(psMem, &center, MAKEFIXED(1, 0),
                                   MAKEFIXED(angle1, 0), MAKEFIXED(angle2, 0));
#ifdef DEBUG
                if (rc == GPI_ERROR) {
                    printf("GpiPartialArc a1 %d, a2 %d ERROR %x\n", angle1,
                           angle2, WinGetLastError(hab));
                } else {
                    printf("GpiPartialArc a1 %d, a2 %d OK\n", angle1, angle2);
                }
#endif
                rc = GpiEndArea(psMem);
#ifdef DEBUG
                if (rc == GPI_OK) {
                    printf("GpiEndArea OK\n");
                } else {
                    printf("GpiEndArea ERROR %x\n", WinGetLastError(hab));
                }
#endif
            } else if ( gc->arc_mode == ArcPieSlice ) {
                /* Pie */
                rc = GpiSetCurrentPosition(psMem, &center);
                rc = GpiBeginArea(psMem, BA_NOBOUNDARY|BA_ALTERNATE);
#ifdef DEBUG
                if (rc != TRUE) {
                    printf("GpiBeginArea ERROR %x\n", WinGetLastError(hab));
                } else {
                    printf("GpiBeginArea OK\n");
                }
#endif
                rc = GpiPartialArc(psMem, &center, MAKEFIXED(1, 0),
                                   MAKEFIXED(angle1, 0), MAKEFIXED(angle2, 0));
#ifdef DEBUG
                if (rc == GPI_ERROR) {
                    printf("GpiPartialArc a1 %d, a2 %d ERROR %x\n", angle1,
                           angle2, WinGetLastError(hab));
                } else {
                    printf("GpiPartialArc a1 %d, a2 %d OK\n", angle1, angle2);
                }
#endif
                rc = GpiLine(psMem, &center);
#ifdef DEBUG
                if (rc == GPI_OK) {
                    printf("GpiLine OK\n");
                } else {
                    printf("GpiLine ERROR %x\n", WinGetLastError(hab));
                }
#endif
                rc = GpiEndArea(psMem);
#ifdef DEBUG
                if (rc == GPI_OK) {
                    printf("GpiEndArea OK\n");
                } else {
                    printf("GpiEndArea ERROR %x\n", WinGetLastError(hab));
                }
#endif
            }
        }
        /* Translate the Y coordinates to PM coordinates */
        aPoints[0].x = x - (gc->line_width/2);
        aPoints[0].y = y - height + 1 - (gc->line_width/2);
        aPoints[1].x = x + width + 1 + (gc->line_width/2);
        aPoints[1].y = y + 2 + (gc->line_width/2);
        aPoints[2].x = 0;
        aPoints[2].y = 0;
        rc = GpiBitBlt(hps, psMem, 3, aPoints, COPYFG, BBO_IGNORE);
#ifdef DEBUG
        if (rc!=TRUE) {
            printf("    GpiBitBlt FG %x (%d,%d)(%d,%d) <- (%d,%d) ERROR %x\n",
                   gc->foreground, aPoints[0].x, aPoints[0].y,
                   aPoints[1].x, aPoints[1].y,
                   aPoints[2].x, aPoints[2].y, WinGetLastError(hab));
        } else {
            printf("    GpiBitBlt FG %x (%d,%d)(%d,%d) <- (%d,%d) OK\n",
                   gc->foreground, aPoints[0].x, aPoints[0].y,
                   aPoints[1].x, aPoints[1].y,
                   aPoints[2].x, aPoints[2].y);
        }
#endif
        GpiSetAttrs(psMem, PRIM_LINE, LBB_COLOR | LBB_GEOM_WIDTH | LBB_TYPE, 0L,
                    &oldLineBundle);
        /*
         * Destroy the temporary bitmap and restore the device context.
         */

        GpiSetBitmap(psMem, oldBitmap);
        GpiDeleteBitmap(bitmap);
        GpiDestroyPS(psMem);
        DevCloseDC(dcMem);
        /* The bitmap must be reselected in the HPS */
        TkOS2UnsetStipple(hps, todPtr->bitmap.hps, todPtr->bitmap.handle,
                          oldPattern, &oldRefPoint);
    } else {

        /* Not stippled */

        rc = GpiQueryArcParams(hps, &oldArcParams);
        arcParams.lP = width / 2;
        arcParams.lQ = sign * (height / 2);
        arcParams.lR = 0;
        arcParams.lS = 0;
        rc = GpiSetArcParams(hps, &arcParams);
#ifdef DEBUG
        if (rc != TRUE) {
            printf("GpiSetArcParams p %d q %d (%d) ERROR %x\n", arcParams.lP,
                   arcParams.lQ, arcParams.lP * arcParams.lQ,
                   WinGetLastError(hab));
        } else {
            printf("GpiSetArcParams p %d q %d (%d) OK\n", arcParams.lP,
                   arcParams.lQ, arcParams.lP * arcParams.lQ);
        }
#endif

        /* Center of arc is at x+(0.5*width),y-(0.5*height) */
        center.x = x + (0.5 * width);
        center.y = y - (0.5 * height);	/* PM y coordinate reversed */
        lineBundle.lColor = gc->foreground;
        lineBundle.lGeomWidth = gc->line_width;
        lineBundle.usType = LINETYPE_SOLID;
        rc = GpiSetAttrs(hps, PRIM_LINE, LBB_COLOR | LBB_GEOM_WIDTH | LBB_TYPE,
                         0L, &lineBundle);
#ifdef DEBUG
        if (rc != TRUE) {
            printf("GpiSetAttrs ERROR %x\n", WinGetLastError(hab));
        } else {
            printf("GpiSetAttrs OK\n");
        }
#endif
        if (!fill) {
	    /* direction of arc is determined by arc parameters, while angles
	     * are always positive
	     * p*q > r*s -> direction counterclockwise
	     * p*q < r*s -> direction clockwise
	     * p*q = r*s -> straight line
	     * When comparing the Remarks for function GpiSetArcParams in the
	     * GPI Guide and Reference with the Xlib Programming Manual
	     * (Fig.6-1), * the 3 o'clock point of the unit arc is defined by
	     * (p,s) and the 12 * o'clock point by (r,q), when measuring from
	     * (0,0) -> (cx+p, cy+s) and * (cx+r, cy+q) from center of arc at
	     * (cx, cy). => p = 0.5 width, q = (sign*)0.5 height, r=s=0
	     * GpiPartialArc draws a line from the current point to the start
	     * of the partial arc, so we have to set the current point to it
	     * first.
	     * this is (cx+0.5*width*cos(angle1), cy+0.5*height*sin(angle1))
	     */
	    curPt.x = center.x + (int) (0.5 * width * a1cos);
	    curPt.y = center.y + (int) (0.5 * height * a1sin);
	    rc = GpiSetCurrentPosition(hps, &curPt);
#ifdef DEBUG
            if (rc != TRUE) {
                printf("GpiSetCurrentPosition %d,%d -> %d,%d ERROR %x\n",
                       center.x, center.y, curPt.x, curPt.y,
                       WinGetLastError(hab));
            } else {
                printf("GpiSetCurrentPosition %d,%d -> %d,%d OK\n",
                       center.x, center.y, curPt.x, curPt.y);
            }
#endif
            rc = GpiBeginPath(hps, 1);
#ifdef DEBUG
            if (rc != TRUE) {
                printf("GpiBeginPath ERROR %x\n", WinGetLastError(hab));
            } else {
                printf("GpiBeginPath OK\n");
            }
#endif
	    rc= GpiPartialArc(hps, &center, MAKEFIXED(1, 0),
	                      MAKEFIXED(angle1, 0), MAKEFIXED(angle2, 0));
#ifdef DEBUG
            if (rc == GPI_ERROR) {
                printf("GpiPartialArc a1 %d, a2 %d ERROR %x\n", angle1, angle2,
                       WinGetLastError(hab));
            } else {
                printf("GpiPartialArc a1 %d, a2 %d OK\n", angle1, angle2);
            }
#endif
            rc = GpiEndPath(hps);
#ifdef DEBUG
            if (rc != TRUE) {
                printf("GpiEndPath ERROR %x\n", WinGetLastError(hab));
            } else {
                printf("GpiEndPath OK\n");
            }
#endif
            rc = GpiStrokePath(hps, 1, 0);
#ifdef DEBUG
            if (rc == GPI_OK) {
                printf("GpiStrokePath OK\n");
            } else {
                printf("GpiStrokePath ERROR %x\n", WinGetLastError(hab));
            }
#endif
        } else {
            curPt.x = center.x + (int) (0.5 * width * a1cos);
            curPt.y = center.y + (int) (0.5 * height * a1sin);
            rc = GpiSetCurrentPosition(hps, &curPt);
	    if (gc->arc_mode == ArcChord) {
                /* Chord */
                /*
                 * See GPI reference: first do GpiPartialArc with invisible
                 * line, then again with visible line, in an Area for filling.
                 */
	        GpiSetLineType(hps, LINETYPE_INVISIBLE);
	        GpiPartialArc(hps, &center, MAKEFIXED(1, 0),
	                      MAKEFIXED(angle1, 0), MAKEFIXED(angle2, 0));
	        GpiSetLineType(hps, LINETYPE_SOLID);
	        rc = GpiBeginArea(hps, BA_NOBOUNDARY|BA_ALTERNATE);
#ifdef DEBUG
                    if (rc != TRUE) {
                    printf("GpiBeginArea ERROR %x\n", WinGetLastError(hab));
                } else {
                    printf("GpiBeginArea OK\n");
                }
#endif
	        rc = GpiPartialArc(hps, &center, MAKEFIXED(1, 0),
	                           MAKEFIXED(angle1, 0), MAKEFIXED(angle2, 0));
#ifdef DEBUG
                if (rc == GPI_ERROR) {
                    printf("GpiPartialArc a1 %d, a2 %d ERROR %x\n", angle1,
                           angle2, WinGetLastError(hab));
                } else {
                    printf("GpiPartialArc a1 %d, a2 %d OK\n", angle1, angle2);
                }
#endif
	        rc = GpiEndArea(hps);
#ifdef DEBUG
                if (rc == GPI_OK) {
                    printf("GpiEndArea OK\n");
                } else {
                    printf("GpiEndArea ERROR %x\n", WinGetLastError(hab));
                }
#endif
	    } else if ( gc->arc_mode == ArcPieSlice ) {
                /* Pie */
	        GpiSetCurrentPosition(hps, &center);
	        GpiBeginArea(hps, BA_NOBOUNDARY|BA_ALTERNATE);
                rc = GpiPartialArc(hps, &center, MAKEFIXED(1, 0),
                                   MAKEFIXED(angle1, 0), MAKEFIXED(angle2, 0));
#ifdef DEBUG
                if (rc == GPI_ERROR) {
                    printf("GpiPartialArc a1 %d, a2 %d ERROR %x\n", angle1,
                           angle2, WinGetLastError(hab));
                } else {
                    printf("GpiPartialArc a1 %d, a2 %d OK\n", angle1, angle2);
                }
#endif
                GpiLine(hps, &center);
	        GpiEndArea(hps);
	    }
        }
        GpiSetAttrs(hps, PRIM_LINE, LBB_COLOR | LBB_GEOM_WIDTH | LBB_TYPE, 0L,
                    &oldLineBundle);
    } /* not Stippled */
    GpiSetPattern(hps, oldPattern);
    GpiSetColor(hps, oldColor);
    GpiSetMix(hps, oldMix);
    rc = GpiSetArcParams(hps, &oldArcParams);
    TkOS2ReleaseDrawablePS(d, hps, &state);
}

/*
 *----------------------------------------------------------------------
 *
 * TkScrollWindow --
 *
 *	Scroll a rectangle of the specified window and accumulate
 *	a damage region.
 *
 * Results:
 *	Returns 0 if the scroll genereated no additional damage.
 *	Otherwise, sets the region that needs to be repainted after
 *	scrolling and returns 1.
 *
 * Side effects:
 *	Scrolls the bits in the window.
 *
 *----------------------------------------------------------------------
 */

int
TkScrollWindow(tkwin, gc, x, y, width, height, dx, dy, damageRgn)
    Tk_Window tkwin;		/* The window to be scrolled. */
    GC gc;			/* GC for window to be scrolled. */
    int x, y, width, height;	/* Position rectangle to be scrolled. */
    int dx, dy;			/* Distance rectangle should be moved. */
    TkRegion damageRgn;		/* Region to accumulate damage in. */
{
    HWND hwnd = TkOS2GetHWND(Tk_WindowId(tkwin));
    RECTL scrollRect;
    LONG lReturn;
    LONG windowHeight;

#ifdef DEBUG
    printf("TkScrollWindow\n");
#endif

    windowHeight = TkOS2WindowHeight((TkOS2Drawable *)Tk_WindowId(tkwin));

    /* Translate the Y coordinates to PM coordinates */
    y = windowHeight - y;
    dy = -dy;
    scrollRect.xLeft = x;
    scrollRect.yTop = y;
    scrollRect.xRight = x + width;
    scrollRect.yBottom = y - height;	/* PM coordinate reversed */
    /* Hide cursor, just in case */
    WinShowCursor(hwnd, FALSE);
    lReturn = WinScrollWindow(hwnd, dx, dy, &scrollRect, NULL, (HRGN) damageRgn,
                              NULL, 0);
    /* Show cursor again */
    WinShowCursor(hwnd, TRUE);
    return ( lReturn == RGN_NULL ? 0 : 1);
}

/*
 *----------------------------------------------------------------------
 *
 * TkOS2SetStipple --
 *
 *	Set the pattern set of a HPS to a "stipple" (bitmap).
 *
 * Results:
 *	Returns the old pattern set and reference point.
 *
 * Side effects:
 *	Unsets the bitmap in/from "its" HPS, appoints a bitmap ID to it,
 *	sets that ID as the pattern set, with its reference point as given.
 *
 *----------------------------------------------------------------------
 */

void
TkOS2SetStipple(destPS, bmpPS, stipple, x, y, oldPatternSet, oldRefPoint)
    HPS destPS;		/* The HPS to receive the stipple. */
    HPS bmpPS;		/* The HPS of the stipple-bitmap. */
    HBITMAP stipple;	/* Stipple-bitmap. */
    LONG x, y;			/* Reference point for the stipple. */
    LONG *oldPatternSet;	/* Pattern set that was in effect in the HPS. */
    PPOINTL oldRefPoint;	/* Reference point that was in effect. */
{
    POINTL refPoint;

#ifdef DEBUG
    printf("TkOS2SetStipple destPS %x, bmpPS %x, stipple %x, (%d,%d)\n", destPS,
           bmpPS, stipple, x, y);
#endif
    refPoint.x = x;
    refPoint.y = y;
    rc = GpiQueryPatternRefPoint(destPS, oldRefPoint);
#ifdef DEBUG
    if (rc!=TRUE) {
        printf("    GpiQueryPatternRefPoint ERROR %x\n", WinGetLastError(hab));
    } else  {
        printf("    GpiQueryPatternRefPoint OK: %d,%d\n", oldRefPoint->x,
               oldRefPoint->y);
    }
#endif
    rc = GpiSetPatternRefPoint(destPS, &refPoint);
#ifdef DEBUG
    if (rc!=TRUE) {
        printf("    GpiSetPatternRefPoint ERROR %x\n", WinGetLastError(hab));
    } else {
        printf("    GpiSetPatternRefPoint %d,%d OK\n", refPoint.x, refPoint.y);
    }
#endif
    *oldPatternSet = GpiQueryPatternSet(destPS);
#ifdef DEBUG
    if (rc==LCID_ERROR) {
        printf("    GpiQueryPatternSet ERROR %x\n", WinGetLastError(hab));
    } else {
        printf("    GpiQueryPatternSet %x\n", oldPatternSet);
    }
#endif
    GpiSetBitmap(bmpPS, NULLHANDLE);
    rc = GpiSetBitmapId(destPS, stipple, 254L);
#ifdef DEBUG
    if (rc!=TRUE) {
        printf("    GpiSetBitmapId %x ERROR %x\n", stipple,
               WinGetLastError(hab));
    } else {
        printf("    GpiSetBitmapId %x OK\n", stipple);
    }
#endif
    rc = GpiSetPatternSet(destPS, 254L);
#ifdef DEBUG
    if (rc!=TRUE) {
        printf("    GpiSetPatternSet ERROR %x\n", WinGetLastError(hab));
    } else {
        printf("    GpiSetPatternSet OK\n");
    }
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * TkOS2UnsetStipple --
 *
 *	Unset the "stipple" (bitmap) from a HPS.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Resets the pattern set and refpoint of the hps to their original
 *	(given) values and reassociates the bitmap with its "own" HPS.
 *
 *----------------------------------------------------------------------
 */

void
TkOS2UnsetStipple(destPS, bmpPS, stipple, oldPatternSet, oldRefPoint)
    HPS destPS;		/* The HPS to give up the stipple. */
    HPS bmpPS;		/* The HPS of the stipple-bitmap. */
    HBITMAP stipple;	/* Stipple-bitmap. */
    LONG oldPatternSet;		/* Pattern set to be put back in effect. */
    PPOINTL oldRefPoint;	/* Reference point to put back in effect. */
{
#ifdef DEBUG
    printf("TkOS2UnsetStipple destPS %x, bmpPS %x, stipple %x, oldRP %d,%d\n",
           destPS, bmpPS, stipple, oldRefPoint->x, oldRefPoint->y);
#endif
    rc = GpiSetPatternSet(destPS, oldPatternSet);
    rc = GpiSetPatternRefPoint(destPS, oldRefPoint);

    rc = GpiDeleteSetId(destPS, 254L);
    /* end of using 254 */
    /* The bitmap must be reselected in the HPS */
    GpiSetBitmap(bmpPS, stipple);
}
