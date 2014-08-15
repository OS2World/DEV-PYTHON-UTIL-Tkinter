/* 
 * tkOS2Dll.c --
 *
 *	This file contains a stub dll entry point.
 *
 * Copyright (c) 1996-1997 Illya Vaes
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tkPort.h"
#include "tkOS2Int.h"

int _CRT_init(void);
void _CRT_term(void);

/* Save the Tk DLL handle for TkPerl */
unsigned long dllHandle = (unsigned long) NULLHANDLE;


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
/*MM* } */ _DLL_InitTerm(unsigned long modHandle, unsigned long flag)
{
    /*
     * If we are attaching to the DLL from a new process, tell Tk about
     * the hInstance to use. If we are detaching then clean up any
     * data structures related to this DLL.
     */

    switch (flag) {
    case 0:     /* INIT */
        /* Save handle */
        dllHandle = modHandle;
        TkOS2InitPM();
        TkOS2XInit(TkOS2GetAppInstance());
/*MM* add { */
#if defined(__IBMC__) && !defined(DYNAMICLIBS)
	_CRT_init();
#endif
/*MM* } */
        return TRUE;

    case 1:     /* TERM */
        TkOS2ExitPM();
        /* Invalidate handle */
        dllHandle = (unsigned long)NULLHANDLE;
/*MM* add { */
#if defined(__IBMC__) && !defined(DYNAMICLIBS)
	_CRT_term();
#endif
/*MM* } */
        return TRUE;
    }

    return FALSE;
}
