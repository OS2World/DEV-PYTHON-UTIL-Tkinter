/* 
 * tkOS2Color.c --
 *
 *	Functions to map color names to system color values.
 *
 * Copyright (c) 1996-1997 Illya Vaes
 * Copyright (c) 1995 Sun Microsystems, Inc.
 * Copyright (c) 1994 Software Research Associates, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tkOS2Int.h"
#include "xcolors.h"

/*
 * This variable indicates whether the color table has been initialized.
 */

static int initialized = 0;

/*
 * colorTable is a hash table used to look up X colors by name.
 */

static Tcl_HashTable colorTable;

/*
 * The SystemColorEntries array contains the names and index values for the
 * OS/2 PM indirect system color names.
 */

typedef struct {
    char *name;
    int index;
} SystemColorEntry;

static SystemColorEntry sysColorEntries[] = {
    { "SystemActiveBorder",		SYSCLR_ACTIVEBORDER },
    { "SystemActiveCaption",		SYSCLR_ACTIVETITLE },
    { "SystemAppWorkspace",		SYSCLR_APPWORKSPACE },
    { "SystemBackground",		SYSCLR_BACKGROUND },
    { "SystemButtonFace",		SYSCLR_BUTTONMIDDLE },
    { "SystemButtonHighlight",		SYSCLR_BUTTONLIGHT },
    { "SystemButtonShadow",		SYSCLR_BUTTONDARK },
    { "SystemButtonText",		SYSCLR_MENUTEXT },
    { "SystemCaptionText",		SYSCLR_TITLETEXT },
    { "SystemDisabledText",		SYSCLR_MENUDISABLEDTEXT },
    { "SystemHighlight",		SYSCLR_HILITEBACKGROUND },
    { "SystemHighlightText",		SYSCLR_HILITEFOREGROUND },
    { "SystemInactiveBorder",		SYSCLR_INACTIVEBORDER },
    { "SystemInactiveCaption",		SYSCLR_INACTIVETITLE },
    { "SystemInactiveCaptionText",	SYSCLR_INACTIVETITLETEXTBGND },
    { "SystemMenu",			SYSCLR_MENU },
    { "SystemMenuText",			SYSCLR_MENUTEXT },
    { "SystemScrollbar",		SYSCLR_SCROLLBAR },
    { "SystemWindow",			SYSCLR_WINDOW },
    { "SystemWindowFrame",		SYSCLR_WINDOWFRAME },
    { "SystemWindowText",		SYSCLR_WINDOWTEXT },
    { NULL,				0 }
};

/*
 * The sysColors array is initialized by SetSystemColors().
 */

static XColorEntry sysColors[] = {
    { 0, 0, 0, "SystemActiveBorder" },
    { 0, 0, 0, "SystemActiveCaption" },
    { 0, 0, 0, "SystemAppWorkspace" },
    { 0, 0, 0, "SystemBackground" },
    { 0, 0, 0, "SystemButtonFace" },
    { 0, 0, 0, "SystemButtonHighlight" },
    { 0, 0, 0, "SystemButtonShadow" },
    { 0, 0, 0, "SystemButtonText" },
    { 0, 0, 0, "SystemCaptionText" },
    { 0, 0, 0, "SystemDisabledText" },
    { 0, 0, 0, "SystemHighlight" },
    { 0, 0, 0, "SystemHighlightText" },
    { 0, 0, 0, "SystemInactiveBorder" },
    { 0, 0, 0, "SystemInactiveCaption" },
    { 0, 0, 0, "SystemInactiveCaptionText" },
    { 0, 0, 0, "SystemMenu" },
    { 0, 0, 0, "SystemMenuText" },
    { 0, 0, 0, "SystemScrollbar" },
    { 0, 0, 0, "SystemWindow" },
    { 0, 0, 0, "SystemWindowFrame" },
    { 0, 0, 0, "SystemWindowText" },
    { 0, 0, 0, NULL }
};

/*
 * Forward declarations for functions defined later in this file.
 */
static int GetColorByName _ANSI_ARGS_((char *name, XColor *color));
static int GetColorByValue _ANSI_ARGS_((char *value, XColor *color));
static void InitColorTable _ANSI_ARGS_((void));
static void SetSystemColors _ANSI_ARGS_((void));



/*
 *----------------------------------------------------------------------
 *
 * SetSystemColors --
 *
 *	Initializes the sysColors array with the current values for
 *	the system colors.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Changes the RGB values stored in the sysColors array.
 *
 *----------------------------------------------------------------------
 */

static void
SetSystemColors()
{
    SystemColorEntry *sPtr;
    XColorEntry *ePtr;
    LONG color;

#ifdef DEBUG
    printf("SetSystemColors\n");
#endif

    for (ePtr = sysColors, sPtr = sysColorEntries;
	 sPtr->name != NULL; ePtr++, sPtr++)
    {
	color = WinQuerySysColor(HWND_DESKTOP, sPtr->index, 0);
	ePtr->red = GetRValue(color);
	ePtr->green = GetGValue(color);
	ePtr->blue = GetBValue(color);
#ifdef DEBUG
        printf("    SystemColor %d: %x (%d,%d,%d)\n", sPtr->index, color,
               ePtr->red, ePtr->green, ePtr->blue);
#endif
    }
}

/*
 *----------------------------------------------------------------------
 *
 * InitColorTable --
 *
 *	Initialize color name database.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Builds a hash table of color names and RGB values.
 *
 *----------------------------------------------------------------------
 */

static void
InitColorTable()
{
    XColorEntry *colorPtr;
    Tcl_HashEntry *hPtr;
    int dummy;
    char localname[32];

#ifdef DEBUG
    printf("InitColorTable\n");
#endif
    Tcl_InitHashTable(&colorTable, TCL_STRING_KEYS);

    /*
     * Add X colors to table.
     */

    for (colorPtr = xColors; colorPtr->name != NULL; colorPtr++) {
        /* We need a *modifiable* copy of the string from xColors! */
        strncpy(localname, colorPtr->name, 32);
        hPtr = Tcl_CreateHashEntry(&colorTable, strlwr(localname), &dummy);
        Tcl_SetHashValue(hPtr, colorPtr);
    }
    
    /*
     * Add OS/2 PM indirect system colors to table.
     */

    SetSystemColors();
    for (colorPtr = sysColors; colorPtr->name != NULL; colorPtr++) {
        /* We need a *modifiable* copy of the string from sysColors! */
        strncpy(localname, colorPtr->name, 32);
        hPtr = Tcl_CreateHashEntry(&colorTable, strlwr(localname), &dummy);
        Tcl_SetHashValue(hPtr, colorPtr);
    }

    initialized = 1;
}

/*
 *----------------------------------------------------------------------
 *
 * GetColorByName --
 *
 *	Looks for a color in the color table by name, then finds the
 *	closest available color in the palette and converts it to an
 *	XColor structure.
 *
 * Results:
 *	If it finds a match, the color is returned in the color
 *	parameter and the return value is 1.  Otherwise the return
 *	value is 0.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
GetColorByName(name, color)
    char *name;			/* An X color name, e.g. "red" */
    XColor *color;		/* The closest available color. */
{
    Tcl_HashEntry *hPtr;
    XColorEntry *colorPtr;
    char localname[32];

#ifdef DEBUG
    printf("GetColorByName %s\n", name);
#endif

    if (!initialized) {
	InitColorTable();
    }

    /* We need a *modifiable* copy of the string name! */
    strncpy(localname, name, 32);
    hPtr = Tcl_FindHashEntry(&colorTable, (char *) strlwr(localname));

    if (hPtr == NULL) {
	return 0;
    }

    colorPtr = (XColorEntry *) Tcl_GetHashValue(hPtr);
    color->pixel = RGB(colorPtr->red, colorPtr->green, colorPtr->blue);
    color->red = colorPtr->red << 8;
    color->green = colorPtr->green << 8;
    color->blue = colorPtr->blue << 8;
#ifdef DEBUG
    printf("RGB %x %x %x (PM %d %d %d)\n", color->red, color->green,
           color->blue, colorPtr->red, colorPtr->green, colorPtr->blue);
#endif
    color->pad = 0;

    return 1;
}      

/*
 *----------------------------------------------------------------------
 *
 * GetColorByValue --
 *
 *	Parses an X RGB color string and finds the closest available
 *	color in the palette and converts it to an XColor structure.
 *	The returned color will have RGB values in the range 0 to 255.
 *
 * Results:
 *	If it finds a match, the color is returned in the color
 *	parameter and the return value is 1.  Otherwise the return
 *	value is 0.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
GetColorByValue(value, color)
    char *value;		/* a string of the form "#RGB", "#RRGGBB", */
				/* "#RRRGGGBBB", or "#RRRRGGGGBBBB" */
    XColor *color;		/* The closest available color. */
{
    char fmt[16];
    int i;

    i = strlen(value+1);
    if (i % 3) {
	return 0;
    }
    i /= 3;
    if (i == 0) {
	return 0;
    }
    sprintf(fmt, "%%%dx%%%dx%%%dx", i, i, i);
    sscanf(value+1, fmt, &color->red, &color->green, &color->blue);
    /*
     * Scale the parse values into 8 bits.
     */
#ifdef DEBUG
    printf("GetColorByValue: %x %x %d\n", color->red, color->green, color->blue);
#endif
    if (i == 1) {
	color->red <<= 4;
	color->green <<= 4;
	color->blue <<= 4;
    } else if (i != 2) {
	color->red >>= (4*(i-2));
	color->green >>= (4*(i-2));
	color->blue >>= (4*(i-2));
    }	
    color->pad = 0;
    color->pixel = RGB(color->red, color->green, color->blue); 
    color->red = color->red << 8;
    color->green = color->green << 8;
    color->blue = color->blue << 8;

#ifdef DEBUG
    printf("GetColorByValue: now %x %x %d\n", color->red, color->green, color->blue);
#endif

    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * XParseColor --
 *
 *	Decodes an X color specification.
 *
 * Results:
 *	Sets exact_def_return to the parsed color.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
XParseColor(display, colormap, spec, exact_def_return)
    Display* display;
    Colormap colormap;
    _Xconst char* spec;
    XColor* exact_def_return;
{
    /*
     * Note that we are violating the const-ness of spec.  This is
     * probably OK in most cases.  But this is a bug in general.
     */

#ifdef DEBUG
    printf("XParseColor %s\n", spec);
#endif

    if (spec[0] == '#') {
	return GetColorByValue((char *)spec, exact_def_return);
    } else {
	return GetColorByName((char *)spec, exact_def_return);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * XAllocColor --
 *
 *	Find the closest available color to the specified XColor.
 *
 * Results:
 *	Updates the color argument and returns 1 on success.  Otherwise
 *	returns 0.
 *
 * Side effects:
 *	Allocates a new color in the palette.
 *
 *----------------------------------------------------------------------
 */

int
XAllocColor(display, colormap, color)
    Display* display;
    Colormap colormap;
    XColor* color;
{
    TkOS2Colormap *cmap = (TkOS2Colormap *) colormap;
    RGB entry;
    HPAL oldPal;
    ULONG *palInfo;
    LONG i, found = -1;

    /* We lose significance when converting to PM, 256 values per color */
    /*
    entry.bRed = (color->red) / 256;
    entry.bGreen = (color->green) / 256;
    entry.bBlue = (color->blue) / 256;
    */
    entry.bRed = (color->red) >> 8;
    entry.bGreen = (color->green) >> 8;
    entry.bBlue = (color->blue) >> 8;

#ifdef DEBUG
    printf("XAllocColor %d %d %d (PM: %d %d %d), cmap %x\n", color->red,
           color->green, color->blue, entry.bRed, entry.bGreen, entry.bBlue,
           cmap);
#endif

    if (aDevCaps[CAPS_ADDITIONAL_GRAPHICS] & CAPS_PALETTE_MANAGER) {
	/*
	 * Palette support
	 */
	ULONG newPixel;
        HPS hps;
	int new, refCount;
	Tcl_HashEntry *entryPtr;

        hps = WinGetScreenPS(HWND_DESKTOP);

#ifdef DEBUG
        printf("    Palette Manager\n");
#endif

	/*
	 * Find the nearest existing palette entry.
	 */
	
	newPixel = RGB(entry.bRed, entry.bGreen, entry.bBlue);
	oldPal= GpiSelectPalette(hps, cmap->palette);
	if (oldPal == PAL_ERROR) {
#ifdef DEBUG
            printf("GpiSelectPalette PAL_ERROR: %x\n", WinGetLastError(hab));
#endif
            WinReleasePS(hps);
            return 0;
	}
	palInfo= (ULONG *) ckalloc(sizeof(ULONG) * (cmap->size+1));

	if (GpiQueryPaletteInfo(cmap->palette, hps, 0L, 0L, cmap->size,
	                        palInfo) == PAL_ERROR) {
#ifdef DEBUG
        printf("GpiQueryPaletteInfo PAL_ERROR: %x\n", WinGetLastError(hab));
#endif
	    GpiSelectPalette(hps, oldPal);
            WinReleasePS(hps);
	    ckfree((char *) palInfo);
            return 0;
	}

	/*
	 * If this is not a duplicate, allocate a new entry.
	 */
	
        for (i=0; i<cmap->size; i++) {
            if (palInfo[i] == newPixel) {
                found = i;
            }
        }
#ifdef DEBUG
        printf("GpiQueryPaletteInfo found %d\n", found);
#endif

	if (found == -1) {

	    /*
	     * Fails if the palette is full.
	     */
	    if (cmap->size == aDevCaps[CAPS_COLOR_INDEX]) {
#ifdef DEBUG
            printf("palette is full\n");
#endif
	        GpiSelectPalette(hps, oldPal);
                WinReleasePS(hps);
	        ckfree((char *) palInfo);
		return 0;
	    }
	
	    cmap->size++;
#ifdef DEBUG
            printf("adding palInfo[%d]: %d (%d, %d, %d)\n", cmap->size-1,
                   RGB(entry.bRed, entry.bGreen, entry.bBlue),
                   entry.bRed, entry.bGreen, entry.bBlue);
#endif
	    palInfo[cmap->size-1]= newPixel;
	    GpiSetPaletteEntries(cmap->palette, LCOLF_CONSECRGB, 0L, cmap->size,
	                         palInfo);
	}

	ckfree((char *) palInfo);
        /*
         * Assign the _index_ in the palette as the pixel, for later use in
         * GpiSetColor et al. ()
         */
        color->pixel = cmap->size-1;
	entryPtr = Tcl_CreateHashEntry(&cmap->refCounts,
		(char *)color->pixel, &new);
	if (new) {
#ifdef DEBUG
            printf("Created new HashEntry: %d\n", color->pixel);
#endif
	    refCount = 1;
	} else {
	    refCount = ((int) Tcl_GetHashValue(entryPtr)) + 1;
#ifdef DEBUG
            printf("Created HashEntry %d: %d\n", refCount, color->pixel);
#endif
	}
	Tcl_SetHashValue(entryPtr, (ClientData)refCount);

	WinReleasePS(hps);

    } else {
       LONG index, iColor;

#ifdef DEBUG
       printf("    no Palette Manager, nr. colors %d (%s)\n",
              aDevCaps[CAPS_COLORS],
              aDevCaps[CAPS_COLOR_TABLE_SUPPORT] ? "loadable color table" :
              "no loadable color table");
#endif
	
	/*
	 * Determine what color will actually be used on non-colormap systems.
	 */

	color->pixel = GpiQueryNearestColor(globalPS, 0L,
		RGB(entry.bRed, entry.bGreen, entry.bBlue));
	color->red = (GetRValue(color->pixel) << 8);
	color->green = (GetGValue(color->pixel) << 8);
	color->blue = (GetBValue(color->pixel) << 8);
#ifdef DEBUG
        if (color->pixel==GPI_ALTERROR) {
            printf("GpiQueryNearestColor ERROR %x\n", WinGetLastError(hab));
        } else {
            printf("    Using nearest color %x (%d,%d,%d) for %x (%d,%d,%d)\n",
                   color->pixel, GetRValue(color->pixel),
                   GetGValue(color->pixel), GetBValue(color->pixel),
                   RGB(entry.bRed, entry.bGreen, entry.bBlue),
                   color->red, color->green, color->blue);
        }
#endif
if (display->screens->root_visual->class != TrueColor){
	/* See if this color is already in the color table */
        index = GpiQueryColorIndex(globalPS, 0L, color->pixel);
#ifdef DEBUG
        if (index==GPI_ALTERROR) {
            printf("GpiQueryColorIndex ERROR %x\n", WinGetLastError(hab));
        } else {
            printf("            Colorindex %d for %x\n", index, color->pixel);
        }
#endif
	rc = GpiQueryLogColorTable(globalPS, 0L, index, 1, &iColor);
#ifdef DEBUG
        if (iColor==QLCT_ERROR) {
            printf("GpiQueryLogColorTable ERROR %x\n", WinGetLastError(hab));
        } else {
            if (iColor==QLCT_RGB) {
                printf("GpiQueryLogColorTable in RGB mode\n");
            } else {
                printf("GpiQueryLogColorTable %d: %x \n", index, iColor);
            }
        }
#endif
	/*
	 * If the color isn't in the table yet and loadable color table support,
	 * add this color to the table, else just use what's available.
	 */
	if (iColor != color->pixel && aDevCaps[CAPS_COLOR_TABLE_SUPPORT]
	    && (nextColor <= aDevCaps[CAPS_COLOR_INDEX])) {
	    rc = GpiCreateLogColorTable(globalPS, 0L, LCOLF_RGB, nextColor,
	                                1, &color->pixel);
            if (rc==TRUE) {
#ifdef DEBUG
                printf("    GpiCreateLogColorTable %x at %d OK\n", color->pixel,
                       nextColor);
#endif
                color->pixel = nextColor;
                nextColor++;
            } else {
                color->pixel = index;
#ifdef DEBUG
                printf("    GpiCreateLogColorTable %x at %d ERROR %x\n",
                       color->pixel, nextColor, WinGetLastError(hab));
#endif
            }
        } else {
            color->pixel = index;
        }
} /* TrueColor */
    }
#ifdef DEBUG
    printf("color->pixel now %x\n", color->pixel);
#endif

    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * XAllocNamedColor --
 *
 *	Find the closest color of the given name.
 *
 * Results:
 *	Returns 1 on success with the resulting color in
 *	exact_def_return.  Returns 0 on failure.
 *
 * Side effects:
 *	Allocates a new color in the palette.
 *
 *----------------------------------------------------------------------
 */

int
XAllocNamedColor(display, colormap, color_name, screen_def_return,
	exact_def_return)
    Display* display;
    Colormap colormap;
    _Xconst char* color_name;
    XColor* screen_def_return;
    XColor* exact_def_return;
{
    int rval = GetColorByName((char *)color_name, exact_def_return);
#ifdef DEBUG
    printf("XAllocNamedColor\n");
#endif

    if (rval) {
	*screen_def_return = *exact_def_return;
	return XAllocColor(display, colormap, exact_def_return);
    } 
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * XFreeColors --
 *
 *	Deallocate a block of colors.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Removes entries for the current palette and compacts the
 *	remaining set.
 *
 *----------------------------------------------------------------------
 */

void
XFreeColors(display, colormap, pixels, npixels, planes)
    Display* display;
    Colormap colormap;
    unsigned long* pixels;
    int npixels;
    unsigned long planes;
{
    TkOS2Colormap *cmap = (TkOS2Colormap *) colormap;
    ULONG cref;
    ULONG refCount;
    int i, old, new;
    ULONG *entries;
    Tcl_HashEntry *entryPtr;

#ifdef DEBUG
    printf("XFreeColors\n");
#endif

    /*
     * We don't have to do anything for non-palette devices.
     */
    
    if (aDevCaps[CAPS_ADDITIONAL_GRAPHICS] & CAPS_PALETTE_MANAGER) {

	/*
	 * This is really slow for large values of npixels.
	 */
	for (i = 0; i < npixels; i++) {
	    entryPtr = Tcl_FindHashEntry(&cmap->refCounts,
		    (char *) pixels[i]);
	    if (!entryPtr) {
		panic("Tried to free a color that isn't allocated.");
	    }
	    refCount = (int) Tcl_GetHashValue(entryPtr) - 1;
	    if (refCount == 0) {
		cref = pixels[i] & 0x00ffffff;
                entries = (ULONG *) ckalloc(sizeof(ULONG) * cmap->size);
                /* hps value ignored for specific values of palette */
                if (GpiQueryPaletteInfo(cmap->palette, NULLHANDLE, 0L, 0L,
                                        cmap->size, entries) == PAL_ERROR) {
                   ckfree((char *)entries);
                   return;
	        }
		/* Copy all entries except the one to delete */
		for (old= new= 0; old<cmap->size; old++) {
		    if (entries[old] != cref) {
		        entries[new] = entries[old];
		        new++;
		    }
		}
		cmap->size--;
	        GpiSetPaletteEntries(cmap->palette, LCOLF_CONSECRGB, 0,
	                             cmap->size, entries);
		ckfree((char *) entries);
		Tcl_DeleteHashEntry(entryPtr);
	    }
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * XCreateColormap --
 *
 *	Allocate a new colormap.
 *
 * Results:
 *	Returns a newly allocated colormap.
 *
 * Side effects:
 *	Allocates an empty palette and color list.
 *
 *----------------------------------------------------------------------
 */

Colormap
XCreateColormap(display, w, visual, alloc)
    Display* display;
    Window w;
    Visual* visual;
    int alloc;
{
    TkOS2Colormap *cmap = (TkOS2Colormap *) ckalloc(sizeof(TkOS2Colormap));

#ifdef DEBUG
    printf("XCreateColormap (%d colors),
           visual id %d class %d, bits %d, map entries %d, masks R%d G%d B%d\n",
           aDevCaps[CAPS_COLOR_INDEX]+1, visual->visualid, visual->class,
           visual->bits_per_rgb, visual->map_entries, visual->red_mask,
           visual->green_mask, visual->blue_mask);
    printf("    CAPS_COLORS %d\n", aDevCaps[CAPS_COLORS]);
#endif

    /*
     * Create a palette when we have palette management. Otherwise store the
     * presentation space handle of the window, since color tables are PS-
     * specific.
     */
    if (aDevCaps[CAPS_ADDITIONAL_GRAPHICS] & CAPS_PALETTE_MANAGER) {
        ULONG logPalette[1];

        logPalette[0] = 0;
        cmap->palette = GpiCreatePalette(hab, 0L, LCOLF_CONSECRGB, 1L,
                                         logPalette);
#ifdef DEBUG
        if (cmap->palette == GPI_ERROR) {
            printf("    GpiCreatePalette GPI_ERROR %x\n", WinGetLastError(hab));
            ckfree((char *)cmap);
        } else {
            printf("    GpiCreatePalette: %x\n", cmap->palette);
        }
#endif
    } else {
        cmap->palette = (HPAL)NULLHANDLE;
    }
    cmap->size = 0;
    cmap->stale = 0;
    Tcl_InitHashTable(&cmap->refCounts, TCL_ONE_WORD_KEYS);
    return (Colormap)cmap;
}

/*
 *----------------------------------------------------------------------
 *
 * XFreeColormap --
 *
 *	Frees the resources associated with the given colormap.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Deletes the palette associated with the colormap.  Note that
 *	the palette must not be selected into a device context when
 *	this occurs.
 *
 *----------------------------------------------------------------------
 */

void
XFreeColormap(display, colormap)
    Display* display;
    Colormap colormap;
{
    TkOS2Colormap *cmap = (TkOS2Colormap *) colormap;

#ifdef DEBUG
    printf("XFreeColormap\n");
#endif

    if (aDevCaps[CAPS_ADDITIONAL_GRAPHICS] & CAPS_PALETTE_MANAGER) {
        /* Palette management */
        if (!GpiDeletePalette(cmap->palette)) {
            /* Try to free memory anyway */
            ckfree((char *) cmap);
	    panic("Unable to free colormap, palette is still selected.");
        }
    }
    Tcl_DeleteHashTable(&cmap->refCounts);
    ckfree((char *) cmap);
}

/*
 *----------------------------------------------------------------------
 *
 * TkOS2SelectPalette --
 *
 *	This function sets up the specified device context with a
 *	given palette.  If the palette is stale, it realizes it in
 *	the background unless the palette is the current global
 *	palette.
 *
 * Results:
 *	Returns the previous palette selected into the device context.
 *
 * Side effects:
 *	May change the system palette.
 *
 *----------------------------------------------------------------------
 */

HPAL
TkOS2SelectPalette(hps, hwnd, colormap)
    HPS hps;
    HWND hwnd;
    Colormap colormap;
{
    TkOS2Colormap *cmap = (TkOS2Colormap *) colormap;
    HPAL oldPalette;
    ULONG mapped, changed;

#ifdef DEBUG
    printf("TkOS2SelectPalette (%x), nextColor %d\n", cmap->palette, nextColor);
#endif

    if (aDevCaps[CAPS_ADDITIONAL_GRAPHICS] & CAPS_PALETTE_MANAGER) {
        oldPalette = GpiSelectPalette(hps, cmap->palette);
#ifdef DEBUG
        if (oldPalette == PAL_ERROR) {
            printf("GpiSelectPalette PAL_ERROR: %x\n", WinGetLastError(hab));
        } else {
            printf("GpiSelectPalette: %x\n", oldPalette);
        }
#endif
        mapped = WinRealizePalette(hwnd, hps, &changed);
#ifdef DEBUG
        if (mapped == PAL_ERROR) {
            printf("WinRealizePalette PAL_ERROR: %x\n", WinGetLastError(hab));
        } else {
            printf("WinRealizePalette: %x\n", mapped);
        }
#endif
        return oldPalette;
    } else {
        PULONG alArray;
        /* Retrieve the "global" color table and create it in this PS */
#ifdef DEBUG
        printf("    ckalloc\'ing %d (%dx%d)\n",
               (unsigned)(sizeof(LONG) * (nextColor)), sizeof(LONG), nextColor);
#endif
        alArray = (PLONG) ckalloc ((unsigned)(sizeof(LONG) * (nextColor)));
        rc = GpiQueryLogColorTable(globalPS, 0L, 0L, nextColor, alArray);
#ifdef DEBUG
        if (rc==QLCT_ERROR) {
            printf("    GpiQueryLogColorTable (0-%d) ERROR %x\n", nextColor-1,
                   WinGetLastError(hab));
        } else {
            if (rc==QLCT_RGB) {
                printf("    GpiQueryLogColorTable in RGB mode\n");
            } else {
                printf("    GpiQueryLogColorTable (0-%d) OK\n", nextColor-1);
            }
        }
#endif
        if (rc > 0) {
            rc = GpiCreateLogColorTable(hps, 0L, LCOLF_RGB, 0,
                                        nextColor, alArray);
#ifdef DEBUG
            if (rc!=TRUE) {
                printf("    GpiCreateLogColorTable ERROR %x\n",
                       WinGetLastError(hab));
            } else {
                printf("    GpiCreateLogColorTable OK\n");
            }
#endif
        }
        ckfree((char *)alArray);
        return (HPAL)0;
    }
}
