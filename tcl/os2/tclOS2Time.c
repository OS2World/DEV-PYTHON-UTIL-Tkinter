/* 
 * tclOS2Time.c --
 *
 *	Contains OS/2 specific versions of Tcl functions that
 *	obtain time values from the operating system.
 *
 * Copyright 1995 by Sun Microsystems, Inc.
 * Copyright 1996-1997 Illya Vaes
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 */

#include "tclInt.h"
#include "tclPort.h"

/*
 *----------------------------------------------------------------------
 *
 * TclGetSeconds --
 *
 *	This procedure returns the number of seconds from the epoch.
 *	On most Unix systems the epoch is Midnight Jan 1, 1970 GMT.
 *      This goes for OS/2 with EMX too.
 *
 * Results:
 *	Number of seconds from the epoch.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

unsigned long
TclGetSeconds()
{
    return (unsigned long) time((time_t *) NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetClicks --
 *
 *	This procedure returns a value that represents the highest
 *	resolution clock available on the system.  There are no
 *	guarantees on what the resolution will be.  In Tcl we will
 *	call this value a "click".  The start time is also system
 *	dependant.
 *
 * Results:
 *	Number of clicks from some start time.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

unsigned long
TclGetClicks()
{
#ifndef CLI_VERSION
    return WinGetCurrentTime(hab);
#else
    QWORD timer;
    DosTmrQueryTime(&timer);
    return timer.ulLo;
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetTimeZone --
 *
 *	Determines the current timezone.  The method varies wildly
 *	between different Platform implementations, so its hidden in
 *	this function.
 *
 * Results:
 *	Hours east of GMT.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclGetTimeZone (currentTime)
    unsigned long  currentTime;
{
    static int setTZ = 0;
    int timeZone;

    if (!setTZ) {
        tzset();
        setTZ = 1;
    }
    /* EMX has "timezone" variable with seconds *west* of Greenwich */
    /* include <time.h> */
    timeZone = - (timezone / 3600);

    return timeZone;
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetTime --
 *
 *	Gets the current system time in seconds and microseconds
 *	since the beginning of the epoch: 00:00 UCT, January 1, 1970.
 *
 * Results:
 *	Returns the current time in timePtr.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TclGetTime(timePtr)
    Tcl_Time *timePtr;		/* Location to store time information. */
{
    struct timeb t;

    ftime(&t);
    timePtr->sec = t.time;
    timePtr->usec = t.millitm * 1000;
}
