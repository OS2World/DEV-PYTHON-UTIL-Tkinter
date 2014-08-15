/* 
 * tkOS2Region.c --
 *
 *	Tk Region emulation code.
 *
 * Copyright (c) 1996-1997 Illya Vaes
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */


#include "tkOS2Int.h"



/*
 *----------------------------------------------------------------------
 *
 * TkCreateRegion --
 *
 *	Construct an empty region.
 *
 * Results:
 *	Returns a new region handle.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TkRegion
TkCreateRegion()
{
    HRGN region;
    HPS hps;

    hps= WinGetPS(HWND_DESKTOP);
    region = GpiCreateRegion(hps, 0, NULL);
    WinReleasePS(hps);
#ifdef DEBUG
    printf("TkCreateRegion region %x, hps %x\n", region, hps);
#endif
    return (TkRegion) region;
}

/*
 *----------------------------------------------------------------------
 *
 * TkDestroyRegion --
 *
 *	Destroy the specified region.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Frees the storage associated with the specified region.
 *
 *----------------------------------------------------------------------
 */

void
TkDestroyRegion(r)
    TkRegion r;
{
    HPS hps= WinGetPS(HWND_DESKTOP);
#ifdef DEBUG
    printf("TkDestroyRegion\n");
#endif
    GpiDestroyRegion(hps, (HRGN) r);
    WinReleasePS(hps);
}

/*
 *----------------------------------------------------------------------
 *
 * TkClipBox --
 *
 *	Computes the bounding box of a region.
 *
 * Results:
 *	Sets rect_return to the bounding box of the region.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TkClipBox(r, rect_return)
    TkRegion r;
    XRectangle* rect_return;
{
    RECTL rect;
    HPS hps;
    LONG ret;

    hps = WinGetPS(HWND_DESKTOP);
    ret = GpiQueryRegionBox(hps, (HRGN)r, &rect);
    WinReleasePS(hps);
    if (ret == RGN_ERROR) {
#ifdef DEBUG
    printf("TkClipBox: GpiQueryRegionBox returns RGN_ERROR\n");
#endif
        return;
    }
#ifdef DEBUG
    printf("TkClipBox: GpiQueryRegionBox %x returns %x\n", r, ret);
#endif
    rect_return->x = rect.xLeft;
    rect_return->width = rect.xRight - rect.xLeft;
    /* Return the Y coordinate that X expects (top) */
    rect_return->y = yScreen - rect.yTop;
    /* PM coordinates are just reversed, translate */
    rect_return->height = rect.yTop - rect.yBottom;
#ifdef DEBUG
    printf("          x %d y %d w %d h %d\n", rect_return->x, rect_return->y,
           rect_return->width, rect_return->height);
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * TkIntersectRegion --
 *
 *	Compute the intersection of two regions.
 *
 * Results:
 *	Returns the result in the dr_return region.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TkIntersectRegion(sra, srb, dr_return)
    TkRegion sra;
    TkRegion srb;
    TkRegion dr_return;
{
    HPS hps= WinGetPS(HWND_DESKTOP);
#ifdef DEBUG
    printf("TkIntersectRegion\n");
#endif
    GpiCombineRegion(hps, (HRGN) dr_return, (HRGN) sra, (HRGN) srb, CRGN_AND);
    WinReleasePS(hps);
}

/*
 *----------------------------------------------------------------------
 *
 * TkUnionRectWithRegion --
 *
 *	Create the union of a source region and a rectangle.
 *
 * Results:
 *	Returns the result in the dr_return region.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TkUnionRectWithRegion(rectangle, src_region, dest_region_return)
    XRectangle* rectangle;
    TkRegion src_region;
    TkRegion dest_region_return;
{
    HRGN rectRgn;
    HPS hps;
    RECTL rect;

#ifdef DEBUG
    printf("TkUnionRectWithRegion Xrect: x %x, y %x, w %x, h %x\n",
           rectangle->x, rectangle->y, rectangle->width, rectangle->height);
#endif
    hps= WinGetPS(HWND_DESKTOP);
    rect.xLeft = rectangle->x;
    rect.xRight = rectangle->x + rectangle->width;
    /* Translate coordinates to PM */
    rect.yTop = yScreen - rectangle->y;
    rect.yBottom = rect.yTop - rectangle->height;
    rectRgn = GpiCreateRegion(hps, 1, &rect);
    GpiCombineRegion(hps, (HRGN) dest_region_return, (HRGN) src_region, rectRgn,
                     CRGN_OR);
    GpiDestroyRegion(hps, rectRgn);
    WinReleasePS(hps);
}

/*
 *----------------------------------------------------------------------
 *
 * TkRectInRegion --
 *
 *	Test whether a given rectangle overlaps with a region.
 *
 * Results:
 *	Returns RectanglePart, RectangleIn or RectangleOut.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TkRectInRegion(r, x, y, width, height)
    TkRegion r;
    int x;
    int y;
    unsigned int width;
    unsigned int height;
{
    RECTL rect;
    LONG in;
    HPS hps;

#ifdef DEBUG
    printf("TkRectInRegion\n");
#endif

    /* Translate coordinates to PM */
    rect.yTop = yScreen - y;
    rect.xLeft = x;
    rect.yBottom = rect.yTop - height;
    rect.xRight = x+width;
    hps= WinGetPS(HWND_DESKTOP);
    in = GpiRectInRegion(hps, (HRGN)r, &rect);
    WinReleasePS(hps);
    if (in == RRGN_INSIDE) {
        /* all in the region */
        return RectangleIn;
    } else if (in == RRGN_PARTIAL) {
        /* partly in the region */
        return RectanglePart;
    } else {
        /* not in region or error */
        return RectangleOut;
    }
}
