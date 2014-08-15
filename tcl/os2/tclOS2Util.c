/* 
 * tclOS2Util.c --
 *
 *	This file contains a collection of utility procedures that
 *	are present in Tcl's OS/2 core but not in the generic core.
 *	For example, they do file manipulation and process manipulation.
 *
 * Copyright (c) 1996-1997 Illya Vaes
 * Copyright (c) 1994 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#define INCL_BASE /*MM*/
#include <os2.h> /*MM*/
#include "tclInt.h"
#include "tclPort.h"


/*
 *----------------------------------------------------------------------
 *
 * Tcl_WaitPid --
 *
 *	Does the waitpid system call.
 *
 * Results:
 *	Returns return value of pid it's waiting for.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
int
Tcl_WaitPid(pid, statPtr, options)
    pid_t pid;
    int *statPtr;
    int options;
{
    ULONG flags;
    APIRET ret;
    struct _RESULTCODES results; /*MM*/

    if (options & WNOHANG) {
	flags = DCWW_NOWAIT;
    } else {
	flags = DCWW_WAIT;
    }
#ifdef DEBUG
    printf("Waiting for PID %d (%s)", pid,
           options & WNOHANG ? "WNOHANG" : "WAIT");
#endif
/*MM* add { */
#ifdef __IBMC__
    DosWaitChild(DCWA_PROCESS, flags, &results, &ret, pid);
	 *statPtr = results.codeTerminate & 0xFF | (results.codeResult << 8);
#else
/*MM* } */
    ret = waitpid((int)pid, statPtr, options);
#endif /*MM*/
#ifdef DEBUG
    printf(", returns %d (*statPtr %x) %s %d\n", ret, *statPtr,
           WIFEXITED(*statPtr) ? "WIFEXITED" :
           (WIFSIGNALED(*statPtr) ? "WIFSIGNALED" :
            (WIFSTOPPED(*statPtr) ? "WIFSTOPPED" : "unknown")),
           WIFEXITED(*statPtr) ? WEXITSTATUS(*statPtr) :
           (WIFSIGNALED(*statPtr) ? WTERMSIG(*statPtr) :
            (WIFSTOPPED(*statPtr) ? WSTOPSIG(*statPtr) : 0)));
#endif
    return ret;
}
