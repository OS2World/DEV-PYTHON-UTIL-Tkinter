/* 
 * tkOS2Font.c --
 *
 *	This file contains the Xlib emulation routines relating to
 *	creating and manipulating fonts.
 *
 * Copyright (c) 1996-1997 Illya Vaes
 * Copyright (c) 1995 Sun Microsystems, Inc.
 * Copyright (c) 1994 Software Research Associates, Inc. 
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */


#include "tkOS2Int.h"

/*
 * Forward declarations for functions used in this file.
 */
static int		NameToFont (_Xconst char *name, TkOS2Font *logfont);
static int		XNameToFont (_Xconst char *name, TkOS2Font *logfont);

/*
 * Code pages used in this file, 1004 is Windows compatible, 65400 must be
 * used if the font contains special glyphs, ie. Symbol.
 */

#define CP_LATIN1 850L
#define CP_1004   1004L
#define CP_65400  65400L

#ifdef IGNOREPMRES
    /*
     * Shrink 120 to 96 to facilitate 'better' sizing for those displays
     * which report a resolution of 120dpi but have actual resolution close
     * to 96dpi (VGA upto ?800x600?).
     * This is obviously dependent on both resolution and screen size,
     * as higher resolutions usually use 120dpi fonts, regardless of any
     * screen size.
     */
    #define PIXTOPOINT(pixels) ( \
        (aDevCaps[CAPS_VERTICAL_FONT_RES] == 120) \
        ? (((pixels) * 72) / 96) \
        : (((pixels) * 72) / aDevCaps[CAPS_VERTICAL_FONT_RES]) \
    )
    #define POINTTOPIX(points) ( \
        (aDevCaps[CAPS_VERTICAL_FONT_RES] == 120) \
        ? (((points) * 96) / 72) \
        : (((points) * aDevCaps[CAPS_VERTICAL_FONT_RES]) / 72) \
    )
    #define PTOP(p) ( \
        (aDevCaps[CAPS_VERTICAL_FONT_RES] == 120) \
        ? (((p) * 96) / 120 ) \
        : (p) \
    )
#else
    #define PIXTOPOINT(pixels) \
        (((pixels) * 72) / aDevCaps[CAPS_VERTICAL_FONT_RES])
    #define POINTTOPIX(points) \
        (((points) * aDevCaps[CAPS_VERTICAL_FONT_RES]) / 72)
    #define PTOP(p)  (p)
#endif


/*
 *----------------------------------------------------------------------
 *
 * NameToFont --
 *
 *	Converts into a logical font description:
 *	   - a three part font name of the form:
 *		"Family point_size style_list"
 *	     Style_list contains a list of one or more attributes:
 *		normal, bold, italic, underline, strikeout
 *	     Point size in decipoints.
 *
 * Results:
 *	Returns false if the font name was syntactically invalid,
 *	else true.  Sets the fields of the passed in TkOS2Font.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
NameToFont(name, logfont)
    _Xconst char *name;
    TkOS2Font *logfont;
{
    int argc, argc2;
    char **argv, **argv2;
    int nameLen, i, pointSize = 0;
    Tcl_Interp *dummy = Tcl_CreateInterp();

#ifdef DEBUG
    printf("NameToFont %s\n", name);
#endif

    if (Tcl_SplitList(dummy, (char *) name, &argc, &argv) != TCL_OK) {
#ifdef DEBUG
    printf("    Tcl_SplitList failed\n");
#endif
	goto nomatch;
    }
    if (argc != 3) {
#ifdef DEBUG
        printf("    not 3 args\n");
        for (i=0;i<argc;i++) { printf("    arg[%d]=[%s]\n", i, argv[i]);}
#endif
	ckfree((char *) argv);
	goto nomatch;
    }

    /*
     * Determine the font family name.
     */

    nameLen = strlen(argv[0]);
    if (nameLen > FACESIZE) {
	nameLen = FACESIZE;
    }
    strncpy(logfont->fattrs.szFacename, argv[0], nameLen);
#ifdef DEBUG
    printf("    copied [%s] -> [%s] (%d)\n", argv[0],
           logfont->fattrs.szFacename, nameLen);
#endif

    /*
     * Check the character set.
     */

    logfont->fattrs.usCodePage = 0;
    if (stricmp(logfont->fattrs.szFacename, "Symbol") == 0) {
	logfont->fattrs.usCodePage = CP_65400;
    } else if (stricmp(logfont->fattrs.szFacename, "Symbol Set") == 0) {
	logfont->fattrs.usCodePage = CP_65400;
    } else if (stricmp(logfont->fattrs.szFacename, "WingDings") == 0) {
	logfont->fattrs.usCodePage = CP_65400;
    } else if (stricmp(logfont->fattrs.szFacename, "ZapfDingbats") == 0) {
	logfont->fattrs.usCodePage = CP_65400;
    } else if (stricmp(logfont->fattrs.szFacename, "StarBats") == 0) {
	logfont->fattrs.usCodePage = CP_65400;
    }
	
    /*
     * Determine the font size.
     */

    if (Tcl_GetInt(dummy, argv[1], &pointSize) != TCL_OK) {
	ckfree((char *) argv);
	goto nomatch;
    }
    logfont->fattrs.lMaxBaselineExt = POINTTOPIX(pointSize);
    logfont->deciPoints = pointSize;
#ifdef DEBUG
    printf("    pointSize %d, lMBE %d, deciPoints %d\n", pointSize,
           logfont->fattrs.lMaxBaselineExt, logfont->deciPoints);
#endif

    /*
     * Apply any style modifiers.
     */
	
    if (Tcl_SplitList(dummy, (char *) argv[2], &argc2, &argv2) != TCL_OK) {
	ckfree((char*) argv);
	goto nomatch;
    }
    for (i = 0; i < argc2; i++) {
	if (stricmp(argv2[i], "bold") == 0) {
	    logfont->fattrs.fsSelection |= FATTR_SEL_BOLD;
	} else if (stricmp(argv2[i], "heavy") == 0) {
	    logfont->fattrs.fsSelection |= FATTR_SEL_BOLD;
	} else if (stricmp(argv2[i], "semibold") == 0) {
	    logfont->fattrs.fsSelection |= FATTR_SEL_BOLD;
	} else if (stricmp(argv2[i], "extrabold") == 0) {
	    logfont->fattrs.fsSelection |= FATTR_SEL_BOLD;
	} else if (stricmp(argv2[i], "italic") == 0) {
	    logfont->fattrs.fsSelection |= FATTR_SEL_ITALIC;
	} else if (stricmp(argv2[i], "oblique") == 0) {
	    logfont->fattrs.fsSelection |= FATTR_SEL_ITALIC;
	} else if (stricmp(argv2[i], "underline") == 0) {
	    logfont->fattrs.fsSelection |= FATTR_SEL_UNDERSCORE;
	} else if (stricmp(argv2[i], "strikeout") == 0) {
	    logfont->fattrs.fsSelection |= FATTR_SEL_STRIKEOUT;
	} else {
	    /* ignore for now */
	}
    }

    ckfree((char *) argv);
    ckfree((char *) argv2);
    return True;

    nomatch:
#ifdef DEBUG
    printf("NameToFont: nomatch\n");
#endif
    Tcl_DeleteInterp(dummy);
    return False;
}

/*
 *----------------------------------------------------------------------
 *
 * XNameToFont --
 *
 *	This function constructs a logical font description from an
 *	X font name.  This code only handles font names with all 13
 *	parts, although a part can be '*'.
 *
 * Results:
 *	Returns false if the font name was syntactically invalid,
 *	else true.  Sets the fields of the passed in TkOS2Font.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
XNameToFont(name, logfont)
    _Xconst char *name;
    TkOS2Font *logfont;
{
    const char *head, *tail;
    const char *field[13];
    int flen[13];
    int i, len, togo = 0;

#ifdef DEBUG
    printf("XNameToFont %s\n", name);
#endif

    /*
     * Valid font name patterns must have a leading '-' or '*'.
     */

    head = tail = name;
    if (*tail == '-') {
	head++; tail++;
    } else if (*tail != '*') {
#ifdef DEBUG
    printf("    no tail\n");
#endif
	return FALSE;
    }

    /*
     * Identify field boundaries.  Stores a pointer to the beginning
     * of each field in field[i], and the length of the field in flen[i].
     * Fields are separated by dashes.  Each '*' becomes a field by itself.
     */

    i = 0;
    while (*tail != '\0' && i < 12) {
	if (*tail == '-') {
	    flen[i] = tail - head;
	    field[i] = head;
	    tail++;
	    head = tail;
	    i++;
	} else if (*tail == '*') {
	    len = tail - head;
	    if (len > 0) {
		flen[i] = tail - head;
		field[i] = head;
	    } else {
		flen[i] = 1;
		field[i] = head;
		tail++;
		if (*tail == '-') {
		    tail++;
		}
	    }
	    head = tail;
	    i++;
	} else {
	    tail++;
	}
    }

    /*
     * We handle the last field as a special case, since it may contain
     * an embedded hyphen.
     */

    flen[i] = strlen(head);
    field[i] = head;

    /*
     * Bail if we don't have at least a font and size.
     */

    if (i < 8) {
#ifdef DEBUG
        printf("    not enough fields (just %d)\n", i);
#endif
	return FALSE;
    } 

togo=i;
    /*
     * Now fill in the logical font description from the fields we have
     * identified.
     */

    /*
     * Field 1: Foundry.  Skip.
     */
    if (--togo <= 0) return TRUE;

    /*
     * Field 2: Font Family.
     */

    i = 1;
    if (!(flen[i] == 0 ||
	  (flen[i] == 1 && (field[i][0] == '*' || field[i][0] == '?'))))
    {
	len = (flen[i] < FACESIZE) ? flen[i] : FACESIZE - 1;
	strncpy(logfont->fattrs.szFacename, field[i], len);
#ifdef DEBUG
        printf("    copied [%s] -> [%s] (%d)\n", field[i],
               logfont->fattrs.szFacename, len);
#endif

	/*
	 * Need to handle Symbol fonts specially.
	 */

	if (stricmp(logfont->fattrs.szFacename, "Symbol") == 0) {
#ifdef DEBUG
            printf("    Symbol font-> codepage CP_65400\n");
#endif
	    logfont->fattrs.usCodePage = CP_65400;
        } else if (stricmp(logfont->fattrs.szFacename, "Symbol Set") == 0) {
#ifdef DEBUG
            printf("    Symbol Set font-> codepage CP_65400\n");
#endif
	    logfont->fattrs.usCodePage = CP_65400;
        } else if (stricmp(logfont->fattrs.szFacename, "WingDings") == 0) {
#ifdef DEBUG
            printf("    WingDings font-> codepage CP_65400\n");
#endif
	    logfont->fattrs.usCodePage = CP_65400;
        } else if (stricmp(logfont->fattrs.szFacename, "ZapfDingbats") == 0) {
#ifdef DEBUG
            printf("    ZapfDingbats font-> codepage CP_65400\n");
#endif
	    logfont->fattrs.usCodePage = CP_65400;
        } else if (stricmp(logfont->fattrs.szFacename, "StarBats") == 0) {
#ifdef DEBUG
            printf("    StarBats font-> codepage CP_65400\n");
#endif
	    logfont->fattrs.usCodePage = CP_65400;
	}
    }
    if (--togo <= 0) return TRUE;

    /*
     * Field 3: Weight.  Default is medium.
     */

    i = 2;
    if ((flen[i] > 0) && (strnicmp(field[i], "bold", flen[i]) == 0)) {
#ifdef DEBUG
        printf("    bold -> FATTR_SEL_BOLD\n");
#endif
	logfont->fattrs.fsSelection |= FATTR_SEL_BOLD;
    } else {
	logfont->fattrs.fsSelection &= ~FATTR_SEL_BOLD;
    }
    if (--togo <= 0) return TRUE;
	    
    /*
     * Field 4: Slant.  Default is Roman.
     */
    
    i = 3;
    if (!(flen[i] == 0 ||
	  (flen[i] == 1 && (field[i][0] == '*' || field[i][0] == '?'))))
    {
	if (strnicmp(field[i], "r", flen[i]) == 0) {
	    /* Roman. */
	    logfont->fattrs.fsSelection &= ~FATTR_SEL_ITALIC;
	} else if (strnicmp(field[i], "i", flen[i]) == 0) {
	    /* Italic */
#ifdef DEBUG
            printf("    italic -> FATTR_SEL_ITALIC\n");
#endif
	    logfont->fattrs.fsSelection |= FATTR_SEL_ITALIC;
	} else if (strnicmp(field[i], "o", flen[i]) == 0) {
#ifdef DEBUG
            printf("    oblique -> FATTR_SEL_ITALIC + slant 15deg\n");
#endif
	    /* Oblique, set to 15 degree slant forward */
	    logfont->shear.x = 2588;	/* 10000*cos(75) */
	    logfont->shear.y = 9659;	/* 10000*sin(75) */
	    logfont->setShear = TRUE;
	    logfont->fattrs.fsFontUse |= FATTR_FONTUSE_TRANSFORMABLE;
            logfont->fattrs.fsSelection |= FATTR_SEL_ITALIC;
	} else if (strnicmp(field[i], "ri", flen[i]) == 0) {
#ifdef DEBUG
            printf("    reverse italic -> FATTR_SEL_ITALIC + slant -30deg\n");
#endif
	    /* Reverse Italic, set to 30 degree slant backward */
	    logfont->shear.x = -5000;	/* 10000*cos(120) */
	    logfont->shear.y = 8660;	/* 10000*sin(120) */
	    logfont->setShear = TRUE;
	    logfont->fattrs.fsFontUse |= FATTR_FONTUSE_TRANSFORMABLE;
            logfont->fattrs.fsSelection |= FATTR_SEL_ITALIC;
	} else if (strnicmp(field[i], "ro", flen[i]) == 0) {
#ifdef DEBUG
            printf("    reverse oblique -> FATTR_SEL_ITALIC + slant -15deg\n");
#endif
	    /* Reverse Oblique, set to 15 degree slant backward */
	    logfont->shear.x = -2588;	/* 10000*cos(105) */
	    logfont->shear.y = 9659;	/* 10000*sin(105) */
	    logfont->setShear = TRUE;
	    logfont->fattrs.fsFontUse |= FATTR_FONTUSE_TRANSFORMABLE;
            logfont->fattrs.fsSelection |= FATTR_SEL_ITALIC;
	} else if (strnicmp(field[i], "ot", flen[i]) == 0) {
	    /* Other */
	} else {
#ifdef DEBUG
            printf("    Unknown slant\n");
#endif
	    return FALSE;
	}
    }
    if (--togo <= 0) return TRUE;

    /*
     * Field 5 & 6: Set Width & Blank.  Skip.
     */
    if (--togo <= 0) return TRUE;

    /*
     * Field 7: Pixels.  Use this as the points if no points set.
     */
    if (--togo <= 0) return TRUE;

    i = 6;
    if (!(flen[i] == 0 ||
	  (flen[i] == 1 && (field[i][0] == '*' || field[i][0] == '?'))))
    {
        logfont->fattrs.lMaxBaselineExt = atoi(field[i]);
        /* Points is  (pixels * 72) / FONT_RES ; Note: decipixels! */
        logfont->deciPoints = PIXTOPOINT(logfont->fattrs.lMaxBaselineExt * 10);

#ifdef DEBUG
        printf("    pixels %s, Vert.Font Res. %d, decipixels %d\n", field[i],
               aDevCaps[CAPS_VERTICAL_FONT_RES], logfont->deciPoints);
        printf("    lMaxBaselineExt (pixel size) %d\n",
               logfont->fattrs.lMaxBaselineExt);
#endif
    }
    if (--togo <= 0) return TRUE;

    /*
     * Field 8: Points in tenths of a point.
     */

    i = 7;
    if (!(flen[i] == 0 ||
	  (flen[i] == 1 && (field[i][0] == '*' || field[i][0] == '?'))))
    {
        logfont->deciPoints = atoi(field[i]);
        logfont->fattrs.lMaxBaselineExt = POINTTOPIX(logfont->deciPoints) / 10;
#ifdef DEBUG
        printf("    lMaxBaselineExt (point size) %d\n",
               logfont->fattrs.lMaxBaselineExt);
#endif
    }
    if (--togo <= 0) return TRUE;

    /*
     * Field 9: Horizontal Resolution in DPI.  Skip.
     * Field 10: Vertical Resolution in DPI.  Skip.
     */
    if (--togo <= 0) return TRUE;

    /*
     * Field 11: Spacing.
     * we can't request this via PM's TkOS2Font, so skip.
     */
    if (--togo <= 0) return TRUE;

    /*
     * Field 12: Average Width.
     * Width should be 0 for outline fonts.
     */

    i = 11;
    if (!(flen[i] == 0 ||
         (flen[i] == 1 && (field[i][0] == '*' || field[i][0] == '?'))))
    {
        logfont->fattrs.lAveCharWidth = (atoi(field[i]) / 10);
#ifdef DEBUG
        printf("    lAveCharWidth (average width) %d\n",
               logfont->fattrs.lAveCharWidth);
#endif
    }
    if (--togo <= 0) return TRUE;

    /*
     * Field 13: Character Set.  Skip.
     */
    if (--togo <= 0) return TRUE;

    return TRUE;
}

/*
 *----------------------------------------------------------------------
 *
 * XLoadFont --
 *
 *	Get the font handle for the specified font.
 *	Also handles:
 *	   - a "Presentation Parameter"-style specification
 *		"pointsize.fontname[.attr][.attr][.attr][.attr][.attr]"
 *	     Point size in points.
 *	     Each attr one of bold, italic, underline, strikeout, outline.
 *
 * Results:
 *	Returns the font handle.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Font
XLoadFont(display, name)
    Display* display;
    _Xconst char* name;
{
    LONG lFontID= nextLogicalFont;
    LONG match = 0;
    PFONTMETRICS os2fonts;
    LONG reqFonts, remFonts;
    BOOL found = FALSE;
    /* Use nominal size unless given Presentation Parameter style spec. */
    /*
    BOOL useIntended = TRUE;
    */
    BOOL useIntended = FALSE;
    LONG outline = -1;
    LONG font = 0;
    int i, error = 30000, best = -1;

#ifdef DEBUG
    SIZEF charBox;
    printf("XLoadFont %s\n", name);
#endif
    if (lFontID > MAX_LID) {
        /* We can't simultaneously  use more than MAX_LID fonts */
        return (Font) 0;
    }

#ifdef DEBUG
    /* Determine total number of fonts */
    reqFonts = 0L;
    remFonts = GpiQueryFonts(globalPS, QF_PUBLIC, NULL, &reqFonts,
                             (LONG) sizeof(FONTMETRICS), NULL);
    printf("    nr.of fonts: %d\n", remFonts);
    /* Allocate space for the fonts */
    os2fonts = (PFONTMETRICS) ckalloc(remFonts * sizeof(FONTMETRICS));
    if (os2fonts == NULL) {
        return (Font) 0;
    }
    /* Retrieve the fonts */
    reqFonts = remFonts;
    remFonts = GpiQueryFonts(globalPS, QF_PUBLIC, NULL, &reqFonts,
                             (LONG) sizeof(FONTMETRICS), os2fonts);
    printf("    got %d (%d remaining)\n", reqFonts, remFonts);
    for (i=0; i<reqFonts; i++) {
        printf("m%d, Em %d (nom %ddpt, lMBE %d), xR %d, yR %d, %s, %s, face[%s]%s, fam[%s]%s\n",
              os2fonts[i].lMatch, os2fonts[i].lEmHeight, os2fonts[i].sNominalPointSize,
              os2fonts[i].lMaxBaselineExt, os2fonts[i].sXDeviceRes,
              os2fonts[i].sYDeviceRes,
              (os2fonts[i].fsType & FM_TYPE_FIXED) ? "fix" : "prop",
              (os2fonts[i].fsDefn & FM_DEFN_OUTLINE) ? "outl" : "bmp",
              os2fonts[i].szFacename,
              (os2fonts[i].fsType & FM_TYPE_FACETRUNC) ? " (trunc()" : "",
              os2fonts[i].szFamilyname,
              (os2fonts[i].fsType & FM_TYPE_FAMTRUNC) ? " (trunc()" : "");
    }
    ckfree((char *)os2fonts);
#endif

    /* Set defaults in logfont */
    logfonts[lFontID].fattrs.usRecordLength = (USHORT)sizeof(FATTRS);
    logfonts[lFontID].fattrs.fsSelection = (USHORT)0;
    logfonts[lFontID].fattrs.lMatch = 0L;
    memset(logfonts[lFontID].fattrs.szFacename, '\0', FACESIZE);
    logfonts[lFontID].fattrs.idRegistry = 0;	/* Unknown */
    logfonts[lFontID].fattrs.usCodePage = 0;	/* Use present codepage */
    logfonts[lFontID].fattrs.lMaxBaselineExt = 0L;	/* 0 for vector fonts */
    logfonts[lFontID].fattrs.lAveCharWidth = 0L;	/* 0 for vector fonts */
    logfonts[lFontID].fattrs.fsType = 0;
    logfonts[lFontID].fattrs.fsFontUse = 0;
    logfonts[lFontID].shear.x = 0;
    logfonts[lFontID].shear.y = 1;	/* Upright characters by default */
    /* Not necessary to set shear by default */
    logfonts[lFontID].setShear = FALSE;
    logfonts[lFontID].outline = FALSE;
    logfonts[lFontID].deciPoints = 120;
    logfonts[lFontID].fattrs.lMaxBaselineExt = 12;

    if (! (((name[0] == '-') || (name[0] == '*')) &&
	     XNameToFont(name, &logfonts[lFontID]))) {
        if (!NameToFont(name, &logfonts[lFontID])) {
            /* Our own (non-X) fonts, recognize Windows fontnames for
             * compatibility of scripts.
             */
    
            if ((stricmp(name, "system") == 0) ||	/* OS/2 and Windows font */
                (stricmp(name, "device") == 0)) {	/* Windows font */
                strncpy(logfonts[lFontID].fattrs.szFacename, "System VIO", 10);
            } else if ((strnicmp(name, "systemmonospaced", 16) == 0) ||
                       (strnicmp(name, "ansifixed", 9) == 0) ||	/* Windows font */
                       (strnicmp(name, "systemfixed", 11) == 0) ||  /* Windows font */
                       (strnicmp(name, "oemfixed", 8) == 0)) {	/* Windows font */
                strncpy(logfonts[lFontID].fattrs.szFacename, "System Monospaced", 17);
            } else if ((strnicmp(name, "systemproportional", 18) == 0) ||
                       (strnicmp(name, "ansi", 4) == 0)) {	/* Windows font */
                strncpy(logfonts[lFontID].fattrs.szFacename, "System Proportional", 19);
            } else {
                /*
                 * The following code suggested by Ilya Zakharevich.
                 * Its use is to allow font selection "in OS/2-style", like
                 * "10.Courier".
                 * Ilya's way of supplying attributes of the font is against
                 * the documented "pointSize.Fontname[.attr ...]" though,
                 * because it gives attributes between the pointsize and the
                 * name of the font.
                 * I take the "official" stance and also supply the rest of the
                 * font Presentation Parameters: underline, strikeout, outline.
                 */
                int l, off = 0;

#ifdef DEBUG
                printf("    trying Presentation Parameters-notation font\n");
#endif
                if (sscanf(name, "%d.%n", &l, &off) && off > 0) {
                    int fields;
#ifdef DEBUG
                printf("    d %d, n %d\n", l, off);
#endif
                    logfonts[lFontID].fattrs.lMaxBaselineExt = POINTTOPIX(l);
                    logfonts[lFontID].deciPoints = l * 10;
                    name += off;
                    useIntended = TRUE;
                    /* Get the fontname out */
                    fields = sscanf(name, "%[^.]%n",
                                   (char *)&logfonts[lFontID].fattrs.szFacename,
                                   &off);
#ifdef DEBUG
                    printf("    sscanf returns %d, off %d\n", fields, off);
#endif
                    if (fields==1 && strlen(name)==off) {
                        /* Fontname is last part */
                        l = strlen(name);
                        if (l > FACESIZE - 1) {
                            l = FACESIZE - 1;
                        }
                        strncpy(logfonts[lFontID].fattrs.szFacename, name, l);
#ifdef DEBUG
                        printf("    font [%s] last part\n", name);
#endif
                    } else {
#ifdef DEBUG
                        printf("    decomposing [%s]\n", name);
#endif
                        /* There are attributes after the fontname */
                        name += off;
                        while (TRUE) {
                            if (strnicmp(name, ".bold", 5) == 0) {
                                logfonts[lFontID].fattrs.fsSelection
                                    |= FATTR_SEL_BOLD;
#ifdef DEBUG
                                printf("    .bold -> FATTR_SEL_BOLD\n");
#endif
                                name += 5;
                            } else if (strnicmp(name, ".italic", 7) == 0) {
                                logfonts[lFontID].fattrs.fsSelection
                                    |= FATTR_SEL_ITALIC;
#ifdef DEBUG
                                printf("    .italic -> FATTR_SEL_ITALIC\n");
#endif
                                name += 7;
                            } else if (strnicmp(name, ".underline", 10) == 0) {
                                logfonts[lFontID].fattrs.fsSelection
                                    |= FATTR_SEL_UNDERSCORE;
#ifdef DEBUG
                                printf("    .underline -> FATTR_SEL_UNDERSCORE\n");
#endif
                                name += 10;
                            } else if (strnicmp(name, ".strikeout", 10) == 0) {
                                logfonts[lFontID].fattrs.fsSelection
                                    |= FATTR_SEL_STRIKEOUT;
#ifdef DEBUG
                                printf("    .strikeout -> FATTR_SEL_STRIKEOUT\n");
#endif
                                name += 10;
                            } else if (strnicmp(name, ".outline", 8) == 0) {
                                logfonts[lFontID].fattrs.fsSelection
                                    |= FATTR_SEL_OUTLINE;
#ifdef DEBUG
                                printf("    .outline -> FATTR_SEL_OUTLINE\n");
#endif
                                name += 8;
                            } else if (*name == '.') {
                                name++;
                                break;
                            } else {
                                break;
                            }
                        }
                    }
                } else {
                    l = strlen(name);
                    if (l > FACESIZE - 1) {
                        l = FACESIZE - 1;
                    }
                    strncpy(logfonts[lFontID].fattrs.szFacename, name, l);
                }
            }
#ifdef DEBUG
            printf("XLoadFont trying font [%s]\n",
                   logfonts[lFontID].fattrs.szFacename);
#endif
        }
    }
    /* Name has now been filled in with a correct or sane value */
    /* Determine number of fonts */
    reqFonts = 0L;
    remFonts = GpiQueryFonts(globalPS, QF_PUBLIC,
                             logfonts[lFontID].fattrs.szFacename, &reqFonts,
                             (LONG) sizeof(FONTMETRICS), NULL);
#ifdef DEBUG
    printf("    nr.of fonts: %d\n", remFonts);
#endif
    reqFonts = remFonts;
    if (reqFonts) {
        /* Allocate space for the fonts */
        os2fonts = (PFONTMETRICS) ckalloc(remFonts * sizeof(FONTMETRICS));
        if (os2fonts == NULL) {
            return (Font) 0;
        }
        /* Get the fonts that apply */
        remFonts = GpiQueryFonts(globalPS, QF_PUBLIC,
                                 logfonts[lFontID].fattrs.szFacename, &reqFonts,
                                 (LONG) sizeof(FONTMETRICS), os2fonts);
#ifdef DEBUG
        if (remFonts == GPI_ALTERROR) {
            printf("    GpiQueryFonts %s ERROR %x\n", logfonts[lFontID].fattrs.szFacename,
            WinGetLastError(hab));
        } else {
            printf("    nr.of fonts [%s]: %d (%d remaining)\n",
                   logfonts[lFontID].fattrs.szFacename, reqFonts, remFonts);
        }
#endif
    } else {
        os2fonts = NULL;
    }
    /*
     * Determine the one that has the right size, preferring a bitmap font over
     * a scalable (outline) one if it exists.
     */
    for (i=0; i<reqFonts && !found; i++) {
        /*
         * Note: scalable fonts appear to always return lEmHeight 16, so first
         * check for outline, then "point size" to not match on size 16.
         */
#ifdef DEBUG
        printf("    trying %s font %s (%ddp, lMaxBaselineExt %d), match %d\n",
               (os2fonts[i].fsDefn & FM_DEFN_OUTLINE) ? "outline" : "fixed",
               os2fonts[i].szFacename, os2fonts[i].sNominalPointSize,
               os2fonts[i].lMaxBaselineExt, os2fonts[i].lMatch);
#endif
        if (os2fonts[i].fsDefn & FM_DEFN_OUTLINE) {
            /* Remember we found an outline font */
            outline = i;
#ifdef DEBUG
            printf("    found outline font %s, match %d\n",
                   os2fonts[i].szFacename, os2fonts[i].lMatch);
#endif
        } else {
            /* Bitmap font, check size, type, resolution */
            int cerror = 0, err1;

            /*
             * Note: FONTMETRICS.fsSelection can contain FM_SEL_ISO9241_TESTED,
             * FATTRS.fsSelection cannot.
             */
#ifdef DEBUG
        printf("m%d, Em %d (nom %ddpt, lMBE %d), xR %d, yR %d, %s, %s, face[%s]%s, fam[%s]%s\n",
              os2fonts[i].lMatch, os2fonts[i].lEmHeight, os2fonts[i].sNominalPointSize,
              os2fonts[i].lMaxBaselineExt, os2fonts[i].sXDeviceRes,
              os2fonts[i].sYDeviceRes,
              (os2fonts[i].fsType & FM_TYPE_FIXED) ? "fix" : "prop",
              (os2fonts[i].fsDefn & FM_DEFN_OUTLINE) ? "outl" : "bmp",
              os2fonts[i].szFacename,
              (os2fonts[i].fsType & FM_TYPE_FACETRUNC) ? " (trunc()" : "",
              os2fonts[i].szFamilyname,
              (os2fonts[i].fsType & FM_TYPE_FAMTRUNC) ? " (trunc()" : "");
#endif
/*
            if (
                ((useIntended ? os2fonts[i].sNominalPointSize :
                                (os2fonts[i].lMaxBaselineExt * 10)) ==
                       logfonts[lFontID].fattrs.lMaxBaselineExt * 10)
*/
            /* If we need a transformable font, we *need* an outline */
/*
                &&
        (!(logfonts[lFontID].fattrs.fsFontUse & FATTR_FONTUSE_TRANSFORMABLE))
                &&
                (os2fonts[i].sXDeviceRes == aDevCaps[CAPS_HORIZONTAL_FONT_RES])
                &&
                (os2fonts[i].sYDeviceRes == aDevCaps[CAPS_VERTICAL_FONT_RES])
                ) {
                found = TRUE;
                match = os2fonts[i].lMatch;
                font = i;
*/
#ifdef DEBUG
            printf("useInt %d, os2f.sNom %d, os2f.lMBE %d, logf.lMBE %d\n",
              useIntended, os2fonts[i].sNominalPointSize,
              os2fonts[i].lMaxBaselineExt * 10,
              logfonts[lFontID].fattrs.lMaxBaselineExt * 10);
#endif
/*
            err1 = ( useIntended ? os2fonts[i].sNominalPointSize
                                 : (os2fonts[i].lMaxBaselineExt * 10))
                     - logfonts[lFontID].fattrs.lMaxBaselineExt * 10;
*/
            err1 = ( (os2fonts[i].lEmHeight * 10) 
                     - logfonts[lFontID].fattrs.lMaxBaselineExt * 10 );
            if (err1 < 0) {
                err1 = -err1;
            }
            cerror = err1;
            if (logfonts[lFontID].fattrs.lAveCharWidth) {
                err1 = logfonts[lFontID].fattrs.lAveCharWidth
                       - os2fonts[i].lAveCharWidth;
                if (err1 < 0) {
                    err1 = -err1;
                }
                cerror += err1 * 3;	/* 10/3 times cheaper */
            }
            if (os2fonts[i].sXDeviceRes != aDevCaps[CAPS_HORIZONTAL_FONT_RES] ||
                os2fonts[i].sYDeviceRes != aDevCaps[CAPS_VERTICAL_FONT_RES]) {
                cerror += 1;
            }
            if (cerror < error) {
                error = cerror;
                best = i;
            }
            if (cerror == 0) {
                found = TRUE;
                font = best;
                match = os2fonts[best].lMatch;
            }
#ifdef DEBUG
            if (found) printf("    found bitmap font %s, match %d (size %d)\n",
                   os2fonts[i].szFacename, os2fonts[i].lMatch,
                   os2fonts[i].sNominalPointSize);
/*
            } else { if (os2fonts[i].sNominalPointSize !=
                    logfonts[lFontID].fattrs.lMaxBaselineExt * 10) {
            if (os2fonts[i].sNominalPointSize !=
                logfonts[lFontID].fattrs.lMaxBaselineExt * 10) {
                printf("    height %d doesn't match required %d\n",
                       os2fonts[i].sNominalPointSize,
                       logfonts[lFontID].fattrs.lMaxBaselineExt * 10);
*/
            if (os2fonts[i].lEmHeight !=
                logfonts[lFontID].fattrs.lMaxBaselineExt) {
                printf("    height %d doesn't match required %d\n",
                       os2fonts[i].lEmHeight,
                       logfonts[lFontID].fattrs.lMaxBaselineExt);
            } else if (os2fonts[i].sXDeviceRes !=
                aDevCaps[CAPS_HORIZONTAL_FONT_RES]) {
                printf("    hor. device res %d doesn't match required %d\n",
                       os2fonts[i].sXDeviceRes,
                       aDevCaps[CAPS_HORIZONTAL_FONT_RES]);
            } else if (os2fonts[i].sYDeviceRes !=
                aDevCaps[CAPS_VERTICAL_FONT_RES]) {
                printf("    vert. device res %d doesn't match required %d\n",
                       os2fonts[i].sYDeviceRes,
                       aDevCaps[CAPS_VERTICAL_FONT_RES]);
            } else if ( logfonts[lFontID].fattrs.fsFontUse
                        & FATTR_FONTUSE_TRANSFORMABLE) {
                printf("    transformations require outline font\n");
            }
#endif
        }
    }
    /* If an exact bitmap for a different resolution found, take it */
    if (!found && error <= 1) {
        match = os2fonts[best].lMatch;
        font = best;
        found = TRUE;
    }
    /* If no bitmap but an outline found, take it */
    if (!found && outline != -1) {
        match = os2fonts[outline].lMatch;
        font = outline;
        found = TRUE;
        logfonts[lFontID].outline = TRUE;
#ifdef DEBUG
        printf("    using outline font %s, match %d\n",
               os2fonts[font].szFacename, os2fonts[font].lMatch);
#endif
    }
    /* If no exact bitmap but an approximate found, take it */
    if (!found && best != -1) {
        match = os2fonts[best].lMatch;
        font = best;
        found = TRUE;
    }
    if (!found) {
	/* Select default font by making facename empty */
#ifdef DEBUG
        printf("    GpiCreateLogFont ERROR %x\n", WinGetLastError(hab));
        printf("XLoadFont trying default font\n");
#endif
        memset(logfonts[lFontID].fattrs.szFacename, '\0', FACESIZE);
        match= GpiCreateLogFont(globalPS, NULL, lFontID,
                                &(logfonts[lFontID].fattrs));
        if (match == GPI_ERROR) {
            if (os2fonts) {
                ckfree((char *)os2fonts);
            }
            return (Font) 0;
        } else if (match == FONT_DEFAULT) {
            FONTMETRICS fm;
            rc= GpiQueryFontMetrics(globalPS, sizeof(FONTMETRICS), &fm);
            if (!rc) {
                return (Font) 0;
            }
            logfonts[lFontID].fattrs.lMatch = 0;
            strcpy(logfonts[lFontID].fattrs.szFacename, fm.szFacename);
            logfonts[lFontID].fattrs.idRegistry = fm.idRegistry;
            logfonts[lFontID].fattrs.usCodePage = fm.usCodePage;
            logfonts[lFontID].fattrs.lMaxBaselineExt = fm.lMaxBaselineExt;
            logfonts[lFontID].fattrs.lAveCharWidth = fm.lAveCharWidth;
            logfonts[lFontID].fattrs.fsType = 0;
            logfonts[lFontID].fattrs.fsFontUse = 0;
            goto got_it;
        }
    }
    /* Fill in the exact font metrics if we found a font */
    if (!found) {
        if (os2fonts) {
            ckfree((char *)os2fonts);
        }
        return (Font) 0;
    } else {
        logfonts[lFontID].fattrs.idRegistry = os2fonts[font].idRegistry;
        logfonts[lFontID].fattrs.lMatch = match;
        strcpy(logfonts[lFontID].fattrs.szFacename, os2fonts[font].szFacename);
#ifdef DEBUG
        printf("    using match %d (%s)\n", match,
               logfonts[lFontID].fattrs.szFacename);
#endif
        if (os2fonts[font].fsDefn & FM_DEFN_OUTLINE) {
            logfonts[lFontID].fattrs.lMaxBaselineExt = 0;
            logfonts[lFontID].fattrs.lAveCharWidth = 0;
            logfonts[lFontID].fattrs.fsFontUse |= FATTR_FONTUSE_OUTLINE;
        } else {
            logfonts[lFontID].fattrs.lMaxBaselineExt = os2fonts[font].lMaxBaselineExt;
            logfonts[lFontID].fattrs.lAveCharWidth = os2fonts[font].lAveCharWidth;
        }
    }

got_it:

#ifdef DEBUG
    printf("    length %d, sel %x, reg %d, cp %d, mbe %d, acw %d, type %x, fu %x\n",
           logfonts[lFontID].fattrs.usRecordLength,
           logfonts[lFontID].fattrs.fsSelection,
           logfonts[lFontID].fattrs.idRegistry,
           logfonts[lFontID].fattrs.usCodePage,
           logfonts[lFontID].fattrs.lMaxBaselineExt,
           logfonts[lFontID].fattrs.lAveCharWidth,
           logfonts[lFontID].fattrs.fsType, logfonts[lFontID].fattrs.fsFontUse);
#endif
    match = GpiCreateLogFont(globalPS, NULL, lFontID,
                             &(logfonts[lFontID].fattrs));
    if (match == GPI_ERROR) {
	/* Select default font by making facename empty */
#ifdef DEBUG
        printf("    GpiCreateLogFont ERROR %x\n", WinGetLastError(hab));
        printf("XLoadFont trying default font\n");
#endif
        memset(logfonts[lFontID].fattrs.szFacename, '\0', FACESIZE);
        match = GpiCreateLogFont(globalPS, NULL, lFontID,
                                 &(logfonts[lFontID].fattrs));
    }
    /* Scale the font to the right size if outline font */
    if (logfonts[lFontID].outline) {
        rc = TkOS2ScaleFont(globalPS, logfonts[lFontID].deciPoints, 0);
#ifdef DEBUG
        if (rc!=TRUE) {
            printf("TkOS2ScaleFont %d ERROR %x\n", logfonts[lFontID].deciPoints,
                   WinGetLastError(hab));
        } else {
            printf("TkOS2ScaleFont %d OK\n", logfonts[lFontID].deciPoints);
        }
        rc = GpiQueryCharBox(globalPS, &charBox);
        if (rc!=TRUE) {
            printf("GpiQueryCharBox ERROR %x\n");
        } else {
            printf("GpiQueryCharBox OK: now cx %d (%d,%d), cy %d (%d,%d)\n",
                    charBox.cx, FIXEDINT(charBox.cx), FIXEDFRAC(charBox.cx),
                    charBox.cy, FIXEDINT(charBox.cy), FIXEDFRAC(charBox.cy));
        }
#endif
    }

#ifdef DEBUG
        printf("XLoadFont match %s lFontID %d\n", match==FONT_MATCH ?
               "FONT_MATCH" : (match==FONT_DEFAULT ? "FONT_DEFAULT" :
                               "GPI_ERROR"), lFontID);
#endif
    if (match == GPI_ERROR) {
        if (os2fonts) {
            ckfree((char *)os2fonts);
        }
        return (Font) 0;
    } else {
#ifdef DEBUG
        if (match==FONT_DEFAULT) {
            FONTMETRICS fm;
            rc= GpiQueryFontMetrics(globalPS, sizeof(FONTMETRICS), &fm);
            if (rc==TRUE) {
                printf("    metrics: family [%s], name [%s], %s\n",
                       fm.szFamilyname,
                       fm.szFacename, fm.fsType & FM_TYPE_FACETRUNC ?
                       "truncated" : "org");
                printf("             avg width %d, maxBasel %d, %s\n",
                       fm.lAveCharWidth,
                       fm.lMaxBaselineExt, fm.fsDefn & FM_DEFN_OUTLINE ?
                       "outline" : "bitmap");
            }
        }
#endif
        ckfree((char *)os2fonts);
        nextLogicalFont++;
        return (Font) lFontID;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * XQueryFont --
 *
 *	Retrieve information about the specified font.
 *
 * Results:
 *	Returns a newly allocated XFontStruct.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

XFontStruct *
XQueryFont(display, font_ID)
    Display* display;
    XID font_ID;
{
    XFontStruct *fontPtr = (XFontStruct *) ckalloc(sizeof(XFontStruct));
    LONG oldFont;
    FONTMETRICS fm;
    XCharStruct bounds;
    BOOL rc;
    POINTL noShear= {0, 1};

#ifdef DEBUG
printf("XQueryFont FID %d\n", font_ID);
#endif

    if (!fontPtr) {
	return NULL;
    }
    
    fontPtr->fid = font_ID;

    oldFont = GpiQueryCharSet(globalPS);
    rc = GpiSetCharSet(globalPS, (LONG) fontPtr->fid);
#ifdef DEBUG
printf("GpiSetCharSet %d returns %d\n", fontPtr->fid, rc);
if (rc==FALSE) printf("Error=%x\n", WinGetLastError(hab));
#endif
    /* Set slant if necessary */
    if (logfonts[(LONG)fontPtr->fid].setShear) {
        GpiSetCharShear(globalPS, &(logfonts[(LONG)fontPtr->fid].shear));
    }

    /*
     * Determine the font metrics and store the values into the appropriate
     * X data structures.
     */

    /* If this is an outline font, set the char box */
    if (logfonts[font_ID].outline) {
#ifdef DEBUG
        SIZEF charBox;
#endif
        rc = TkOS2ScaleFont(globalPS, logfonts[font_ID].deciPoints, 0);
#ifdef DEBUG
        if (rc!=TRUE) {
            printf("TkOS2ScaleFont %d ERROR %x\n", logfonts[font_ID].deciPoints,
                   WinGetLastError(hab));
        } else {
            printf("TkOS2ScaleFont %d OK\n", logfonts[font_ID].deciPoints);
        }
        rc = GpiQueryCharBox(globalPS, &charBox);
        if (rc!=TRUE) {
            printf("GpiQueryCharBox ERROR %x\n");
        } else {
            printf("GpiQueryCharBox OK: now cx %d (%d,%d), cy %d (%d,%d)\n",
                   charBox.cx, FIXEDINT(charBox.cx), FIXEDFRAC(charBox.cx),
                   charBox.cy, FIXEDINT(charBox.cy), FIXEDFRAC(charBox.cy));
        }
#endif
    }

    if (GpiQueryFontMetrics(globalPS, sizeof(FONTMETRICS), &fm)) {
#ifdef DEBUG
        printf("Font metrics: first %c (%d), last %c (%d), def %c (%d),
               maxCharInc %d, maxAsc %d, maxDesc %d, maxBaselineExt %d,
               bounds %d\n", fm.sFirstChar, fm.sFirstChar, fm.sLastChar,
               fm.sLastChar, fm.sDefaultChar, fm.sDefaultChar, fm.lMaxCharInc,
               fm.lMaxAscender, fm.lMaxDescender, fm.lMaxBaselineExt, bounds);
        printf("    sNominalPointSize %d, size %d\n", fm.sNominalPointSize, 
               fm.lMaxBaselineExt);
        printf("    lEmInc %d, underpos %d, size %d\n", fm.lEmInc,
               fm.lUnderscorePosition, fm.lUnderscoreSize);
#endif

        /*
         * Copy fontmetrics into own structure, so we remember scalable font
         * modifications.
         */
        memcpy((void *)&logfonts[font_ID].fm, (void *)&fm, sizeof(FONTMETRICS));
#ifdef DEBUG
        printf("    sXDeviceRes %d, sYDeviceRes %d\n",
               logfonts[font_ID].fm.sXDeviceRes,
               logfonts[font_ID].fm.sYDeviceRes);
#endif

	fontPtr->direction = LOBYTE(fm.sInlineDir) < 90 || LOBYTE(fm.sInlineDir) > 270
	                     ? FontLeftToRight : FontRightToLeft;
	fontPtr->min_byte1 = 0;
	fontPtr->max_byte1 = 0;
	fontPtr->min_char_or_byte2 = fm.sFirstChar;
	/* sLastChar is *offset* from sFirstChar! */
	fontPtr->max_char_or_byte2 = fm.sFirstChar + fm.sLastChar;
	fontPtr->all_chars_exist = True;
	fontPtr->default_char = fm.sDefaultChar;
	fontPtr->n_properties = 0;
	fontPtr->properties = NULL;
	bounds.lbearing = 0;
	bounds.rbearing = fm.lMaxCharInc;
	bounds.width = fm.lMaxCharInc;
	bounds.ascent = fm.lMaxAscender;
	bounds.descent = fm.lMaxDescender;
	bounds.attributes = 0;
	fontPtr->ascent = fm.lMaxAscender;
	fontPtr->descent = fm.lMaxDescender;
	fontPtr->min_bounds = bounds;
	fontPtr->max_bounds = bounds;

	/*
	 * If the font is not fixed pitch, then we need to construct
	 * the per_char array.
	 */

	if ( !(fm.fsType & FM_TYPE_FIXED) ) {
	    int i;
	    char c;
            /*
             * sLastChar is offset from sFirstChar
             * Only 256 code points (0 - 255) allowed in a code page.
             */
	    int nchars = MIN(((int)fm.sLastChar + 1), 255);
	    int minWidth = 30000;
POINTL endPoint[2];
            PLONG widths;
	    fontPtr->per_char =
		(XCharStruct *)ckalloc(sizeof(XCharStruct) * nchars);

            /*
             * GpiQueryCharStringPos seems to be more precise than
             * GpiQueryWidthTable. In the widget demo, the first only exhibits
             * some "dancing" when highlighting, while the latter gives a very
             * noticeable space before the cursor.
             * On the other hand, it is abysmal for raster fonts.
             */
            if (logfonts[font_ID].outline) {
                for (i = 0, c = fm.sFirstChar; i < nchars ; i++, c++ ) {
                    GpiQueryCharStringPos(globalPS, 0L, 1, &c, NULL, endPoint);
                    /*
                    fontPtr->per_char[i] = bounds;
                    */
                    fontPtr->per_char[i].width = endPoint[1].x - endPoint[0].x;
#ifdef DEBUG
                    printf("    width [%c] %d (endP1.x %d, endP0.x %d)\n",
                           fm.sFirstChar+i, fontPtr->per_char[i].width,
                           endPoint[1].x, endPoint[0].x);
#endif
                    if (minWidth > fontPtr->per_char[i].width) {
                        minWidth = fontPtr->per_char[i].width;
                    }
                }
            } else {
                /* Bitmap -> use GpiQueryWidthTable */
                widths = (PLONG) ckalloc(sizeof(LONG)*nchars);
                if (widths != NULL) {
                    rc= GpiQueryWidthTable(globalPS, fm.sFirstChar, nchars,
                                           widths);
                    if (rc == TRUE) {
                        for (i = 0; i < nchars ; i++) {
                            fontPtr->per_char[i] = bounds;
                            fontPtr->per_char[i].width = widths[i];
#ifdef DEBUG
                            printf("    width (bmp) [%c] %d\n", fm.sFirstChar+i,
                                   fontPtr->per_char[i].width);
#endif
                            if (minWidth > fontPtr->per_char[i].width) {
                	            minWidth = fontPtr->per_char[i].width;
                            }
                        }
#ifdef DEBUG
                        printf("variable pitch, nchars %d, minWidth %d\n", nchars,
                               minWidth);
#endif
                    } else {
    	                ckfree((char *)fontPtr);
                        fontPtr = NULL;
#ifdef DEBUG
                        printf("    GpiQueryWidthTable %d ERROR %x\n", nchars,
                               WinGetLastError(hab));
#endif
                    }
                } else {
    	            ckfree((char *)fontPtr);
                    fontPtr = NULL;
#ifdef DEBUG
                    printf("    couldn't allocate memory for widths\n");
#endif
                    goto restore;
                }
            }

	    fontPtr->min_bounds.width = minWidth;
	} else {
#ifdef DEBUG
            printf("    Fixed pitch font\n");
#endif
	    fontPtr->per_char = NULL;
	}
    } else {
	ckfree((char *)fontPtr);
	fontPtr = NULL;
    }    

restore:

    /* Restore font */
    if (logfonts[(LONG)fontPtr->fid].setShear) {
        GpiSetCharShear(globalPS, &noShear);
    }
    GpiSetCharSet(globalPS, oldFont);
    
    return fontPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * XLoadQueryFont --
 *
 *	Finds the closest available OS/2 font for the specified
 *	font name.
 *
 * Results:
 *	Allocates and returns an XFontStruct containing a description
 *	of the matching font.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

XFontStruct *
XLoadQueryFont(display, name)
    Display* display;
    _Xconst char* name;
{
    Font font;

#ifdef DEBUG
printf("XLoadQueryFont %s\n", name);
#endif
    font = XLoadFont(display, name);
    return XQueryFont(display, font);
}

/*
 *----------------------------------------------------------------------
 *
 * XFreeFont --
 *
 *	Releases resources associated with the specified font.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Frees the memory referenced by font_struct.
 *
 *----------------------------------------------------------------------
 */

void
XFreeFont(display, font_struct)
    Display* display;
    XFontStruct* font_struct;
{
#ifdef DEBUG
printf("XFreeFont\n");
#endif

    /* Only deleting the last ID can be done in this array-approach */
    if (nextLogicalFont-1 == (LONG)font_struct->fid) {
        GpiDeleteSetId(globalPS, (LONG)font_struct->fid);
        nextLogicalFont--;
    }
#ifdef DEBUG
else printf("      Logical ID %d leaves hole\n", font_struct->fid);
#endif
    if (font_struct->per_char != NULL) {
        ckfree((char *) font_struct->per_char);
    }
    ckfree((char *) font_struct);
}

/*
 *----------------------------------------------------------------------
 *
 * XTextWidth --
 *
 *	Compute the width of an 8-bit character string.
 *
 * Results:
 *	Returns the computed width of the specified string.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
XTextWidth(font_struct, string, count)
    XFontStruct* font_struct;
    _Xconst char* string;
    int count;
{
    /*
    FONTMETRICS fm;
    */
    LONG oldFont;
    POINTL aSize[TXTBOX_COUNT];
    POINTL noShear= {0, 1};

#ifdef DEBUG
    printf("XTextWidth [%s], font ID [%d]\n", string, (LONG)font_struct->fid);
#endif
    oldFont = GpiQueryCharSet(globalPS);
    GpiSetCharSet(globalPS, (LONG) font_struct->fid);
    /* Set slant if necessary */
    if (logfonts[(LONG)font_struct->fid].setShear) {
        GpiSetCharShear(globalPS, &(logfonts[(LONG)font_struct->fid].shear));
    }
    /* If this is an outline font, set the char box */
    if (logfonts[(LONG)font_struct->fid].outline) {
#ifdef DEBUG
        SIZEF charBox;
#endif
        rc = TkOS2ScaleFont(globalPS,
                            logfonts[(LONG)font_struct->fid].deciPoints, 0);
#ifdef DEBUG
        if (rc!=TRUE) {
            printf("TkOS2ScaleFont %d ERROR %x\n",
                     logfonts[(LONG)font_struct->fid].deciPoints,
                     WinGetLastError(hab));
        } else {
            printf("TkOS2ScaleFont %d OK\n", logfonts[(LONG)font_struct->fid].deciPoints);
        }
        rc = GpiQueryCharBox(globalPS, &charBox);
if (rc!=TRUE) printf("GpiQueryCharBox ERROR %x\n");
else printf("GpiQueryCharBox OK: now cx %d (%d,%d), cy %d (%d,%d)\n", charBox.cx,
            FIXEDINT(charBox.cx), FIXEDFRAC(charBox.cx), charBox.cy,
            FIXEDINT(charBox.cy), FIXEDFRAC(charBox.cy));
#endif
    }
else {
/*
 * XTextWidth is only used by Tk as an average number, for '0'
 * -> use lAveCharWidth
 */
#ifdef DEBUG
    printf("XTextWidth returns AveCharWidth %d (font %d)\n",
    logfonts[(LONG)font_struct->fid].fattrs.lAveCharWidth,  font_struct->fid);
#endif
return logfonts[(LONG)font_struct->fid].fattrs.lAveCharWidth;
}

    GpiQueryTextBox(globalPS, count, (PCH)string, TXTBOX_COUNT, aSize);
    /* OS/2 PM does not have ths overhang
     * GpiQueryFontMetrics(globalPS, sizeof(FONTMETRICS), &fm);
     * size.cx -= fm.fmOverhang;
     */

    /* Restore font */
    if (logfonts[(LONG)font_struct->fid].setShear) {
        GpiSetCharShear(globalPS, &noShear);
    }
    GpiSetCharSet(globalPS, oldFont);

#ifdef DEBUG
printf("XTextWidth %s (font %d) returning %d\n", string, font_struct->fid,
       aSize[TXTBOX_BOTTOMRIGHT].x - aSize[TXTBOX_BOTTOMLEFT].x);
#endif

    return aSize[TXTBOX_BOTTOMRIGHT].x - aSize[TXTBOX_BOTTOMLEFT].x;
}

/*
 *----------------------------------------------------------------------
 *
 * XTextExtents --
 *
 *	Compute the bounding box for a string.
 *
 * Results:
 *	Sets the direction_return, ascent_return, descent_return, and
 *	overall_return values as defined by Xlib.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
XTextExtents(font_struct, string, nchars, direction_return,
	font_ascent_return, font_descent_return, overall_return)
    XFontStruct* font_struct;
    _Xconst char* string;
    int nchars;
    int* direction_return;
    int* font_ascent_return;
    int* font_descent_return;
    XCharStruct* overall_return;
{
    LONG oldFont;
    POINTL aSize[TXTBOX_COUNT];
    POINTL noShear= {0, 1};

#ifdef DEBUG
printf("XTextExtents\n");
#endif

    *direction_return = font_struct->direction;
    *font_ascent_return = font_struct->ascent;
    *font_descent_return = font_struct->descent;

    oldFont = GpiQueryCharSet(globalPS);
    GpiSetCharSet(globalPS, (LONG) font_struct->fid);
    /* Set slant if necessary */
    if (logfonts[(LONG)font_struct->fid].setShear) {
        GpiSetCharShear(globalPS, &(logfonts[(LONG)font_struct->fid].shear));
    }
    /* If this is an outline font, set the char box */
    if (logfonts[(LONG)font_struct->fid].outline) {
#ifdef DEBUG
        SIZEF charBox;
#endif
        rc = TkOS2ScaleFont(globalPS,
                            logfonts[(LONG)font_struct->fid].deciPoints, 0);
#ifdef DEBUG
        if (rc!=TRUE) {
            printf("TkOS2ScaleFont %d ERROR %x\n",
                   logfonts[(LONG)font_struct->fid].deciPoints,
                   WinGetLastError(hab));
        } else {
            printf("TkOS2ScaleFont %d OK\n",
                   logfonts[(LONG)font_struct->fid].deciPoints);
        }
        rc = GpiQueryCharBox(globalPS, &charBox);
        if (rc!=TRUE) {
            printf("GpiQueryCharBox ERROR %x\n", WinGetLastError(hab));
        } else {
            printf("GpiQueryCharBox OK: now cx %d (%d,%d), cy %d (%d,%d)\n",
                   charBox.cx, FIXEDINT(charBox.cx), FIXEDFRAC(charBox.cx),
                   charBox.cy, FIXEDINT(charBox.cy), FIXEDFRAC(charBox.cy));
        }
#endif
    }

    overall_return->ascent = logfonts[(LONG)font_struct->fid].fm.lMaxAscender;
    overall_return->descent = logfonts[(LONG)font_struct->fid].fm.lMaxDescender;
    GpiQueryTextBox(globalPS, nchars, (char *)string, TXTBOX_COUNT, aSize);
    overall_return->width = aSize[TXTBOX_BOTTOMRIGHT].x - aSize[TXTBOX_BOTTOMLEFT].x;
    overall_return->lbearing = 0;
    overall_return->rbearing = overall_return->width;

    /* Restore font */
    if (logfonts[(LONG)font_struct->fid].setShear) {
        GpiSetCharShear(globalPS, &noShear);
    }
    GpiSetCharSet(globalPS, oldFont);
}

/*
 *----------------------------------------------------------------------
 *
 * XGetFontProperty --
 *
 *	Called to get font properties.
 *
 * Results:
 *	Return true for known properties, false otherwise
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Bool
XGetFontProperty(font_struct, atom, value_return)
    XFontStruct* font_struct;
    Atom atom;
    unsigned long* value_return;
{
    FONTMETRICS fm;

#ifdef DEBUG
printf("XGetFontProperty %d\n", (LONG)atom);
#endif

    switch (atom) {
        case XA_SUPERSCRIPT_X:
            *value_return = (unsigned long) logfonts[(LONG)font_struct->fid].fm.lSuperscriptXOffset;
            return True;
            break;
        case XA_SUPERSCRIPT_Y:
            *value_return = (unsigned long) logfonts[(LONG)font_struct->fid].fm.lSuperscriptYOffset;
            return True;
            break;
        case XA_SUBSCRIPT_X:
            *value_return = (unsigned long) logfonts[(LONG)font_struct->fid].fm.lSubscriptXOffset;
            return True;
            break;
        case XA_SUBSCRIPT_Y:
            *value_return = (unsigned long) logfonts[(LONG)font_struct->fid].fm.lSubscriptYOffset;
            return True;
            break;
        case XA_UNDERLINE_POSITION:
            *value_return = (unsigned long) logfonts[(LONG)font_struct->fid].fm.lUnderscorePosition;
            return True;
            break;
        case XA_UNDERLINE_THICKNESS:
            *value_return = (unsigned long) logfonts[(LONG)font_struct->fid].fm.lUnderscoreSize;
            return True;
            break;
        case XA_STRIKEOUT_ASCENT:
            *value_return = (unsigned long) logfonts[(LONG)font_struct->fid].fm.lStrikeoutPosition;
            return True;
            break;
        case XA_STRIKEOUT_DESCENT:
            *value_return = (unsigned long) logfonts[(LONG)font_struct->fid].fm.lStrikeoutSize
                                            - logfonts[(LONG)font_struct->fid].fm.lStrikeoutPosition;
            return True;
            break;
        case XA_ITALIC_ANGLE: /* scaled by 64 */
            /* Degrees in sCharSlope second byte, minutes first byte */
/* Endian-ness!!! */
            *value_return = (unsigned long) 64 * 90 - (logfonts[(LONG)font_struct->fid].fm.sCharSlope >> 2);
            return True;
            break;
        case XA_X_HEIGHT:
            *value_return = (unsigned long) logfonts[(LONG)font_struct->fid].fm.lXHeight;
            return True;
            break;
        case XA_QUAD_WIDTH:
            *value_return = (unsigned long) logfonts[(LONG)font_struct->fid].fm.lEmInc;
            return True;
            break;
        case XA_CAP_HEIGHT:
            /* Same as max height */
            *value_return = (unsigned long) logfonts[(LONG)font_struct->fid].fm.lMaxAscender;
            return True;
            break;
        case XA_WEIGHT:
            /* Scale of 0 to 1000 */
            *value_return = (unsigned long) 100 * logfonts[(LONG)font_struct->fid].fm.usWeightClass;
            return True;
            break;
        case XA_POINT_SIZE:
            *value_return = (unsigned long) logfonts[(LONG)font_struct->fid].fm.sNominalPointSize;
            return True;
            break;
        case XA_RESOLUTION:
            /* expressed in hundredths */
            /* Fontmetrics give two fields: sXDeviceRes and sYDeviceRes */
            /* in pels/inch for bitmap fonts, notional units/Em for outline */
            if ( logfonts[(LONG)font_struct->fid].fm.fsDefn & FM_DEFN_OUTLINE ) {
                /* Em size gives point size */
                *value_return = (unsigned long) logfonts[(LONG)font_struct->fid].fm.sXDeviceRes* fm.lEmInc* 100;
#ifdef DEBUG
                printf("    XA_RESOLUTION %d (OUTLINE, xres %d * lEmInc %d * 100)\n",
                       *value_return, logfonts[(LONG)font_struct->fid].fm.sXDeviceRes, fm.lEmInc);
#endif
            } else {
                /* multiply by number of points in one inch, 72, + 100ths */
                *value_return = (unsigned long) logfonts[(LONG)font_struct->fid].fm.sXDeviceRes * 722818;
#ifdef DEBUG
                printf("    XA_RESOLUTION %d (xres %d * 72\n", *value_return,
                       logfonts[(LONG)font_struct->fid].fm.sXDeviceRes);
#endif
            }
            return True;
            break;
        default:
            return False;
            break;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkOS2ScaleFont --
 *
 *      Adapted from "OS/2 Presentation Manager Programming" by Petzold.
 *	Called to scale a presentation space's font to a desired size.
 *
 * Results:
 *	Return true if successful.
 *
 * Side effects:
 *	Sets the character box attribute of a presentation space.
 *
 *----------------------------------------------------------------------
 */

BOOL
TkOS2ScaleFont(hps, pointSize, pointWidth)
    HPS hps;
    ULONG pointSize;	/* in decipixels */
    ULONG pointWidth;	/* 0 means "same as pointSize" */
{
    HDC hdc;
    LONG xRes, yRes;
    POINTL points[2];
    SIZEF sizef;

#ifdef DEBUG
    printf("TkOS2ScaleFont hps %x, pointSize %d, pointWidth %d\n", hps,
           pointSize, pointWidth);
    rc = GpiQueryCharBox(hps, &sizef);
    if (rc!=TRUE) printf("GpiQueryCharBox ERROR %x\n", WinGetLastError(hab));
    else printf("GpiQueryCharBox OK: cx %d (%d,%d), cy %d (%d,%d)\n", sizef.cx,
                FIXEDINT(sizef.cx), FIXEDFRAC(sizef.cx), sizef.cy,
                FIXEDINT(sizef.cy), FIXEDFRAC(sizef.cy));
#endif
    /* If pointWidth defaulted, set it to pointSize */
    if (pointWidth == 0) {
        pointWidth = pointSize;
    }

    /* Determine device and its resolutions */
    hdc = GpiQueryDevice(hps);
    if (hdc == HDC_ERROR) {
#ifdef DEBUG
        printf("    GpiQueryDevice ERROR %x\n", WinGetLastError(hab));
#endif
        return FALSE;
    } else if (hdc == NULLHANDLE) {
        /* No device context associated, assume the screen */
        xRes = aDevCaps[CAPS_HORIZONTAL_FONT_RES];
        yRes = aDevCaps[CAPS_VERTICAL_FONT_RES];
    } else {
        rc = DevQueryCaps(hdc, CAPS_HORIZONTAL_FONT_RES, 1, &xRes);
        if (rc != TRUE) {
#ifdef DEBUG
            printf("    DevQueryCaps xRes ERROR %x\n", WinGetLastError(hab));
#endif
            xRes = aDevCaps[CAPS_HORIZONTAL_FONT_RES];
        }
        rc = DevQueryCaps(hdc, CAPS_VERTICAL_FONT_RES, 1, &yRes);
        if (rc != TRUE) {
#ifdef DEBUG
            printf("    DevQueryCaps yRes ERROR %x\n", WinGetLastError(hab));
#endif
            yRes = aDevCaps[CAPS_VERTICAL_FONT_RES];
        }
    }

    /*
     * Determine desired point size in pixels with device resolution.
     * Font resolution is returned by PM in pels per inch, device resolution
     * is in dots per inch. 722.818 decipoints in an inch.
     * Add 361.409 for correct rounding.
     */
    points[0].x = 0;
    points[0].y = 0;
    points[1].x = (xRes * pointWidth + 361.409) / 722.818;
    points[1].y = (yRes * pointSize + 361.409) / 722.818;

    /* Convert to page coordinates */
    rc = GpiConvert(hps, CVTC_DEVICE, CVTC_PAGE, 2L, points);
#ifdef DEBUG
    if (rc!=TRUE) printf("GpiConvert ERROR %x\n", WinGetLastError(hab));
    else printf("GpiConvert OK: (%d,%d) -> (%d,%d)\n",
                (aDevCaps[CAPS_HORIZONTAL_FONT_RES] * pointWidth + 360) / 720,
                (aDevCaps[CAPS_VERTICAL_FONT_RES] * pointSize + 360) / 720,
                points[1].x, points[1].y);
#endif

    /* Now set the character box */
    sizef.cx = MAKEFIXED((points[1].x - points[0].x), 0);
    sizef.cy = MAKEFIXED((points[1].y - points[0].y), 0);
#ifdef DEBUG
    printf("after GpiConvert: cx FIXED(%d), cy FIXED(%d)\n",
           points[1].x - points[0].x, points[1].y - points[0].y);
#endif

    return GpiSetCharBox(hps, &sizef);
}
