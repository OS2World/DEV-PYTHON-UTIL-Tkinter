/* 
 * tclOS2Init.c --
 *
 *	Contains the OS/2-specific interpreter initialization functions.
 *
 * Copyright (c) 1994-1996 Sun Microsystems, Inc.
 * Copyright (c) 1996-1997 Illya Vaes
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"
#include "tclPort.h"

/* Global PM variables, necessary because of event loop and thus console */
HAB hab;
HMQ hmq;
/* Other global variables */
ULONG maxPath;

/*
 * The following arrays contain the human readable strings for the OS/2
 * version values.
 */

static char* processors[] = { "intel", "ppc" };
static const int numProcessors = sizeof(processors);

#ifndef PROCESSOR_ARCHITECTURE_INTEL
#define PROCESSOR_ARCHITECTURE_INTEL 0
#endif
#ifndef PROCESSOR_ARCHITECTURE_PPC
#define PROCESSOR_ARCHITECTURE_PPC   1
#endif
#ifndef PROCESSOR_ARCHITECTURE_UNKNOWN
#define PROCESSOR_ARCHITECTURE_UNKNOWN 0xFFFF
#endif


/*
 * The following string is the startup script executed in new
 * interpreters.  It looks on disk in several different directories
 * for a script "init.tcl" that is compatible with this version
 * of Tcl.  The init.tcl script does all of the real work of
 * initialization.
 */

static char *initScript =
"proc init {} {\n\
    global tcl_library tcl_version tcl_patchLevel env\n\
    rename init {}\n\
    set dirs {}\n\
    if [info exists env(TCL_LIBRARY)] {\n\
        lappend dirs $env(TCL_LIBRARY)\n\
    }\n\
    lappend dirs [info library]\n\
    lappend dirs [file dirname [file dirname [info nameofexecutable]]]/lib/tcl$tcl_version\n\
    if [string match {*[ab]*} $tcl_patchLevel] {\n\
        set lib tcl$tcl_patchLevel\n\
    } else {\n\
        set lib tcl$tcl_version\n\
    }\n\
    lappend dirs [file dirname [file dirname [pwd]]]/$lib/library\n\
    lappend dirs [file dirname [pwd]]/library\n\
    foreach i $dirs {\n\
        set tcl_library $i\n\
        if ![catch {uplevel #0 source [list $i/init.tcl]}] {\n\
            return\n\
        }\n\
    }\n\
    set msg \"Can't find a usable init.tcl in the following directories: \n\"\n\
    append msg \"    $dirs\n\"\n\
    append msg \"This probably means that Tcl wasn't installed properly.\n\"\n\
    error $msg\n\
}\n\
init";

/*
 *----------------------------------------------------------------------
 *
 * TclPlatformInit --
 *
 *	Performs OS/2-specific interpreter initialization related to the
 *	tcl_library variable.  Also sets up the HOME environment variable
 *	if it is not already set.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Sets "tcl_library" and "env(HOME)" Tcl variables
 *
 *----------------------------------------------------------------------
 */

void
TclPlatformInit(interp)
    Tcl_Interp *interp;
{
    char *ptr;
    char buffer[13];
    Tcl_DString ds;
    ULONG sysInfo[QSV_MAX];   /* System Information Data Buffer */
    APIRET rc;
    int cpu = PROCESSOR_ARCHITECTURE_INTEL;
    
    tclPlatform = TCL_PLATFORM_OS2;

    Tcl_DStringInit(&ds);

    /*
     * Find out what kind of system we are running on.
     */

    /* Request all available system information */
    rc= DosQuerySysInfo (1L, QSV_MAX, (PVOID)sysInfo, sizeof(ULONG)*QSV_MAX);
    maxPath = sysInfo[QSV_MAX_PATH_LENGTH - 1];
#ifdef DEBUG
    printf("major version [%d], minor version [%d], rev. [%d], maxPath [%d]\n",
           sysInfo[QSV_VERSION_MAJOR - 1], sysInfo[QSV_VERSION_MINOR - 1],
           sysInfo[QSV_VERSION_REVISION - 1], sysInfo[QSV_MAX_PATH_LENGTH - 1]);
#endif

    /*
     * Define the tcl_platform array.
     */

    Tcl_SetVar2(interp, "tcl_platform", "platform", "OS/2", TCL_GLOBAL_ONLY);
    Tcl_SetVar2(interp, "tcl_platform", "os", "OS/2", TCL_GLOBAL_ONLY);
    /*
     * Hack for LX-versions above 2.11
     *  OS/2 version    MAJOR MINOR
     *  2.0             20    0
     *  2.1             20    10
     *  2.11            20    11
     *  3.0             20    30
     *  4.0             20    40
     */
    if (sysInfo[QSV_VERSION_MINOR - 1] > 11) {
        int major = (int) (sysInfo[QSV_VERSION_MINOR - 1] / 10);
        sprintf(buffer, "%d.%d", major,
                (int) sysInfo[QSV_VERSION_MINOR - 1] - major * 10);
    } else {
        sprintf(buffer, "%d.%d", (int) (sysInfo[QSV_VERSION_MAJOR - 1] / 10),
                (int)sysInfo[QSV_VERSION_MINOR - 1]);
    }
    Tcl_SetVar2(interp, "tcl_platform", "osVersion", buffer, TCL_GLOBAL_ONLY);
    /* No API for determining processor (yet) */
    Tcl_SetVar2(interp, "tcl_platform", "machine", processors[cpu],
                TCL_GLOBAL_ONLY);

    Tcl_SetVar(interp, "tcl_library", ".", TCL_GLOBAL_ONLY);

    /*
     * Set up the HOME environment variable from the HOMEDRIVE & HOMEPATH
     * environment variables, if necessary.
     */

    ptr = Tcl_GetVar2(interp, "env", "HOME", TCL_GLOBAL_ONLY);
    if (ptr == NULL) {
	Tcl_DStringSetLength(&ds, 0);
	ptr = Tcl_GetVar2(interp, "env", "HOMEDRIVE", TCL_GLOBAL_ONLY);
	if (ptr != NULL) {
	    Tcl_DStringAppend(&ds, ptr, -1);
	}
	ptr = Tcl_GetVar2(interp, "env", "HOMEPATH", TCL_GLOBAL_ONLY);
	if (ptr != NULL) {
	    Tcl_DStringAppend(&ds, ptr, -1);
	}
	if (Tcl_DStringLength(&ds) > 0) {
	    Tcl_SetVar2(interp, "env", "HOME", Tcl_DStringValue(&ds),
		    TCL_GLOBAL_ONLY);
	} else {
	    Tcl_SetVar2(interp, "env", "HOME", "c:/", TCL_GLOBAL_ONLY);
	}
    }

    Tcl_DStringFree(&ds);

}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_Init --
 *
 *	This procedure is typically invoked by Tcl_AppInit procedures
 *	to perform additional initialization for a Tcl interpreter,
 *	such as sourcing the "init.tcl" script.
 *
 * Results:
 *	Returns a standard Tcl completion code and sets interp->result
 *	if there is an error.
 *
 * Side effects:
 *	Depends on what's in the init.tcl script.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_Init(interp)
    Tcl_Interp *interp;		/* Interpreter to initialize. */
{
    return Tcl_Eval(interp, initScript);
}

/*
 *----------------------------------------------------------------------
 *
 * TclOS2GetPlatform --
 *
 *      This is a kludge that allows the test library to get access
 *      the internal tclPlatform variable.
 *
 * Results:
 *      Returns a pointer to the tclPlatform variable.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

TclPlatformType *
TclOS2GetPlatform()
{
    return &tclPlatform;
}

/*
 *----------------------------------------------------------------------
 *
 * PMInitialize --
 *
 *	Performs OS/2-specific initialization.
 *
 * Results:
 *	True or false depending on intialization.
 *
 * Side effects:
 *	Opens the "PM connection"
 *
 *----------------------------------------------------------------------
 */

BOOL
PMInitialize(void)
{
    /* Initialize PM */
    hab = WinInitialize (0);
    if (hab == NULLHANDLE) return FALSE;
    /* Create message queue, increased size from 10 */
    hmq= WinCreateMsgQueue (hab, 64);
    if (hmq == NULLHANDLE) {
        WinTerminate(hab);
        hab= (HAB)0;
        return FALSE;
    }
    return TRUE;
}

/*
 *----------------------------------------------------------------------
 *
 * PMShutdown --
 *
 *	Performs OS/2-specific cleanup.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Closes the "PM connection"
 *
 *----------------------------------------------------------------------
 */

void
PMShutdown(void)
{
    BOOL rc;

    /* Reset pointer to arrow */
    rc = WinSetPointer(HWND_DESKTOP,
                       WinQuerySysPointer(HWND_DESKTOP, SPTR_ARROW, FALSE));
#ifdef DEBUG
    if (rc != TRUE) {
        printf("WinSetPointer PMShutdown ERROR: %x\n", WinGetLastError(hab));
    } else {
        printf("WinSetPointer PMShutdown OK\n");
    }
#endif
    WinDestroyMsgQueue(hmq);
    WinTerminate(hab);
    hmq= (HMQ)0;
    hab= (HAB)0;
}

/*
 *----------------------------------------------------------------------
 *
 * TclOS2GetHAB --
 *
 *	Get the handle to the anchor block.
 *
 * Results:
 *	HAB or NULLHANDLE.
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

HAB
TclOS2GetHAB(void)
{
    return hab;
}

/*
 *----------------------------------------------------------------------
 *
 * TclPlatformExit --
 *
 *	Cleanup and exit on OS/2.
 *
 * Results:
 *	None. This procedure never returns (it exits the process when
 *	it's done).
 *
 * Side effects:
 *	This procedure terminates all relations with PM.
 *
 *----------------------------------------------------------------------
 */

void
TclPlatformExit(status)
    int status;				/* Status to exit with */
{
    /*
     * Set focus to Desktop to force the Terminal edit window to reinstate
     * the system pointer.
     */
    WinSetFocus(HWND_DESKTOP, HWND_DESKTOP);
    WinDestroyMsgQueue(hmq);
    WinTerminate(hab);
    hmq= (HAB)0;
    hab= (HMQ)0;
    exit(status);
}
