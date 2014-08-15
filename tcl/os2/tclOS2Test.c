/* 
 * tclOS2Test.c --
 *
 *	Contains commands for platform specific tests on OS/2.
 *
 * Copyright (c) 1996 Sun Microsystems, Inc.
 * Copyright (c) 1996-1997 Illya Vaes
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 */

#include "tclInt.h"
#include "tclPort.h"

/*
 * Forward declarations of procedures defined later in this file:
 */
int			TclplatformtestInit _ANSI_ARGS_((Tcl_Interp *interp));

/*
 *----------------------------------------------------------------------
 *
 * TclplatformtestInit --
 *
 *	Defines commands that test platform specific functionality for
 *	OS/2 platforms.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Defines new commands.
 *
 *----------------------------------------------------------------------
 */

int
TclplatformtestInit(interp)
    Tcl_Interp *interp;		/* Interpreter to add commands to. */
{
    /*
     * Add commands for platform specific tests for OS/2 here.
     */
    
    return TCL_OK;
}
