/* 
 * tclOS2Load.c --
 *
 *	This procedure provides a version of the TclLoadFile that
 *	works with the OS/2 "DosLoadModule" and "DosQueryProcAddr"
 *	APIs for dynamic loading.
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 * Copyright (c) 1996-1997 Illya Vaes
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"
#include "tclPort.h"


/*
 *----------------------------------------------------------------------
 *
 * TclLoadFile --
 *
 *	Dynamically loads a binary code file into memory and returns
 *	the addresses of two procedures within that file, if they
 *	are defined.
 *
 * Results:
 *	A standard Tcl completion code.  If an error occurs, an error
 *	message is left in interp->result.
 *
 * Side effects:
 *	New code suddenly appears in memory.
 *
 *----------------------------------------------------------------------
 */

int
TclLoadFile(interp, fileName, sym1, sym2, proc1Ptr, proc2Ptr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    char *fileName;		/* Name of the file containing the desired
				 * code. */
    char *sym1, *sym2;		/* Names of two procedures to look up in
				 * the file's symbol table. */
    Tcl_PackageInitProc **proc1Ptr, **proc2Ptr;
				/* Where to return the addresses corresponding
				 * to sym1 and sym2. */
{
    HMODULE handle;
    APIRET rc;
    char *buffer;

#ifdef DEBUG
    printf("TclLoadFile %s %s %s\n", fileName, sym1, sym2);
#endif
    handle = TclOS2LoadLibrary(fileName);
    if (handle == NULLHANDLE) {
	Tcl_AppendResult(interp, "couldn't load file \"", fileName,
		"\": ", "file not found", (char *) NULL);
	return TCL_ERROR;
    }

    /*
     * For each symbol, check for both Symbol and _Symbol, since some
     * compilers generate C symbols with a leading '_' by default.
     */

    rc = DosQueryProcAddr(handle, 0L, sym1, (PFN *)proc1Ptr);
    if (rc != NO_ERROR) {
#ifdef DEBUG
        printf("DosQueryProcAddr %s ERROR %d\n", sym1, rc);
#endif
	buffer = ckalloc(strlen(sym1)+2);
	buffer[0] = '_';
	strcpy(buffer+1, sym1);
        rc = DosQueryProcAddr(handle, 0L, buffer, (PFN *)proc1Ptr);
	if (rc != NO_ERROR) {
#ifdef DEBUG
            printf("DosQueryProcAddr %s ERROR %d\n", buffer, rc);
#endif
	    *proc1Ptr = NULL;
	}
#ifdef DEBUG
          else {
            printf("DosQueryProcAddr %s OK\n", buffer, rc);
        }
#endif
	ckfree(buffer);
    }
#ifdef DEBUG
      else {
        printf("DosQueryProcAddr %s OK\n", sym1, rc);
    }
#endif
    
    rc = DosQueryProcAddr(handle, 0L, sym2, (PFN *)proc2Ptr);
    if (rc != NO_ERROR) {
#ifdef DEBUG
        printf("DosQueryProcAddr %s ERROR %d\n", sym2, rc);
#endif
	buffer = ckalloc(strlen(sym2)+2);
	buffer[0] = '_';
	strcpy(buffer+1, sym2);
        rc = DosQueryProcAddr(handle, 0L, buffer, (PFN *)proc2Ptr);
	if (rc != NO_ERROR) {
#ifdef DEBUG
            printf("DosQueryProcAddr %s ERROR %d\n", buffer, rc);
#endif
	    *proc2Ptr = NULL;
	}
#ifdef DEBUG
          else {
            printf("DosQueryProcAddr %s OK\n", buffer, rc);
        }
#endif
	ckfree(buffer);
    }
#ifdef DEBUG
      else {
        printf("DosQueryProcAddr %s OK\n", sym2, rc);
    }
#endif
    
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclGuessPackageName --
 *
 *      If the "load" command is invoked without providing a package
 *      name, this procedure is invoked to try to figure it out.
 *
 * Results:
 *      Always returns 0 to indicate that we couldn't figure out a
 *      package name;  generic code will then try to guess the package
 *      from the file name.  A return value of 1 would have meant that
 *      we figured out the package name and put it in bufPtr.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
TclGuessPackageName(fileName, bufPtr)
    char *fileName;             /* Name of file containing package (already
                                 * translated to local form if needed). */
    Tcl_DString *bufPtr;        /* Initialized empty dstring.  Append
                                 * package name to this if possible. */
{
    return 0;
}
