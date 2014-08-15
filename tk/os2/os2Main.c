/* 
 * os2Main.c --
 *
 *	Main entry point for wish and other Tk-based applications.
 *
 * Copyright (c) 1996-1997 Illya Vaes
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include <tk.h>
#define INCL_PM
#include <os2.h>
#undef INCL_PM
#include <malloc.h>
#include <locale.h>
#include <stdarg.h>

/*
 * The following declarations refer to internal Tk routines.  These
 * interfaces are available for use, but are not supported.
 */

EXTERN void             TkConsoleCreate _ANSI_ARGS_((void));
EXTERN int              TkConsoleInit _ANSI_ARGS_((Tcl_Interp *interp));

/*
 * Forward declarations for procedures defined later in this file:
 */

static void WishPanic TCL_VARARGS(char *,format);

#ifdef TK_TEST
EXTERN int              Tktest_Init _ANSI_ARGS_((Tcl_Interp *interp));
#endif /* TK_TEST */


/*
 *----------------------------------------------------------------------
 *
 * main --
 *
 *	Main entry point from OS/2.
 *
 * Results:
 *	Returns false if initialization fails, otherwise it never
 *	returns. 
 *
 * Side effects:
 *	Just about anything, since from here we call arbitrary Tcl code.
 *
 *----------------------------------------------------------------------
 */

int
main( int argc, char **argv )
{
    /* Initialize PM: done in DLL */

    /*
     * Set up the default locale
     */

    setlocale(LC_ALL, "");

    Tcl_SetPanicProc(WishPanic);

    /*
     * Create the console channels and install them as the standard
     * channels.  All I/O will be discarded until TkConsoleInit is
     * called to attach the console to a text widget.
     */

    TkConsoleCreate();

    Tk_Main(argc, argv, Tcl_AppInit);

    /* Shutting down PM: done in DLL */

    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_AppInit --
 *
 *	This procedure performs application-specific initialization.
 *	Most applications, especially those that incorporate additional
 *	packages, will have their own version of this procedure.
 *
 * Results:
 *	Returns a standard Tcl completion code, and leaves an error
 *	message in interp->result if an error occurs.
 *
 * Side effects:
 *	Depends on the startup script.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_AppInit(interp)
    Tcl_Interp *interp;		/* Interpreter for application. */
{
    if (Tcl_Init(interp) == TCL_ERROR) {
	return TCL_ERROR;
    }
    if (Tk_Init(interp) == TCL_ERROR) {
	return TCL_ERROR;
    }
    Tcl_StaticPackage(interp, "Tk", Tk_Init, (Tcl_PackageInitProc *) NULL);

    /*
     * Initialize the console only if we are running as an interactive
     * application.
     */

    if (strcmp(Tcl_GetVar(interp, "tcl_interactive", TCL_GLOBAL_ONLY), "1")
            == 0) {
        if (TkConsoleInit(interp) == TCL_ERROR) {
            goto error;
        }
    }

#ifdef TK_TEST
    if (Tktest_Init(interp) == TCL_ERROR) {
	goto error;
    }
    Tcl_StaticPackage(interp, "Tktest", Tktest_Init,
            (Tcl_PackageInitProc *) NULL);
#endif /* TK_TEST */

    Tcl_SetVar(interp, "tcl_rcFileName", "~/wishrc.tcl", TCL_GLOBAL_ONLY);
    return TCL_OK;

error:
    WishPanic(interp->result);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * WishPanic --
 *
 *	Display a message and exit.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Exits the program.
 *
 *----------------------------------------------------------------------
 */

void
WishPanic TCL_VARARGS_DEF(char *,arg1)
{
    va_list argList;
    char buf[1024];
    char *format;
    
    format = TCL_VARARGS_START(char *,arg1,argList);
    vsprintf(buf, format, argList);

    /* Make sure pointer is not captured (for WinMessageBox) */
    WinSetCapture(HWND_DESKTOP, NULLHANDLE);
    WinAlarm(HWND_DESKTOP, WA_ERROR);
    WinMessageBox(HWND_DESKTOP, NULLHANDLE, buf, "Fatal Error in WISH", 0,
	    MB_OK | MB_ERROR | MB_APPLMODAL);
    exit(1);
}
