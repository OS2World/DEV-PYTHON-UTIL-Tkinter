/* 
 * tclOS2Dll.c --
 *
 *	This file contains the DLL entry point.
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 * Copyright (c) 1996-1997 Illya Vaes
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */


#include "tclInt.h"
#include "tclPort.h"

int _CRT_init(void);
void _CRT_term(void);

/*
 * The following data structure is used to keep track of all of the DLL's
 * opened by Tcl so that they can be freed with the Tcl.dll is unloaded.
 */

typedef struct LibraryList {
    HMODULE handle;
    struct LibraryList *nextPtr;
} LibraryList;

static LibraryList *libraryList = NULL;	/* List of currently loaded DLL's. */

static HMODULE tclInstance;	/* Global library instance handle */

static void 		UnloadLibraries _ANSI_ARGS_((void));


/*
 *----------------------------------------------------------------------
 *
 * _DLL_InitTerm --
 *
 *	DLL entry point.
 *
 * Results:
 *	TRUE on sucess, FALSE on failure.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
unsigned long /*MM* add { */
#ifdef __IBMC__
_System
#endif
/*MM* } */
_DLL_InitTerm(
    unsigned long hInst,	/* Library instance handle. */
    unsigned long reason	/* Reason this function is being called. */
)
{
    switch (reason) {
    case 0:	/* INIT */
	tclInstance = (HMODULE)hInst;
/*MM* add { */
#if defined(__IBMC__) && !defined(DYNAMICLIBS)
	_CRT_init();
#endif
/*MM* } */
        return TRUE; 
    case 1:	/* TERM */
        UnloadLibraries();
/*MM* add { */
#if defined(__IBMC__) && !defined(DYNAMICLIBS)
	_CRT_term();
#endif
/*MM* } */
        return TRUE; 
    }

    return FALSE; 
}

/*
 *----------------------------------------------------------------------
 *
 * TclOS2LoadLibrary --
 *
 *	This function is a wrapper for the system DosLoadModule.  It is
 *	responsible for adding library handles to the library list so
 *	the libraries can be freed when tcl.dll is unloaded.
 *
 * Results:
 *	Returns the handle of the newly loaded library, or NULL on
 *	failure.
 *
 * Side effects:
 *	Loads the specified library into the process.
 *
 *----------------------------------------------------------------------
 */

HMODULE
TclOS2LoadLibrary(name)
    char *name;			/* Library file to load. */
{
    HMODULE handle;
    LibraryList *ptr;
    APIRET rc;
    UCHAR LoadError[256];	/* Area for name of DLL that we failed on */

    rc = DosLoadModule(LoadError, sizeof(LoadError), name, &handle);
    if (rc == NO_ERROR) {
#ifdef DEBUG
        printf("DosLoadModule %s OK\n", name);
#endif
	ptr = (LibraryList*) ckalloc(sizeof(LibraryList));
	ptr->handle = handle;
	ptr->nextPtr = libraryList;
	libraryList = ptr;
        return handle;
    } else {
#ifdef DEBUG
        printf("DosLoadModule %s ERROR %d on %s\n", name, rc, LoadError);
#endif
        return NULLHANDLE;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * UnloadLibraries --
 *
 *	Frees any dynamically allocated libraries loaded by Tcl.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Frees the libraries on the library list as well as the list.
 *
 *----------------------------------------------------------------------
 */

static void
UnloadLibraries()
{
    LibraryList *ptr;

    while (libraryList != NULL) {
	DosFreeModule(libraryList->handle);
	ptr = libraryList->nextPtr;
	ckfree((char *)libraryList);
	libraryList = ptr;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclOS2GetTclInstance --
 *
 *      Retrieves the global library instance handle.
 *
 * Results:
 *      Returns the global library instance handle.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

HMODULE
TclOS2GetTclInstance()
{
    return tclInstance;
}
