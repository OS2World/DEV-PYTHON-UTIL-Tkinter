/* 
 * tclOS2NoSock.c --
 *
 *	This file contains stubs in absence of sockets.
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 * Copyright (c) 1996-1997 Illya Vaes
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
 * Tcl_OpenTcpClient --
 *
 *	Opens a TCP client socket and creates a channel around it.
 *
 * Results:
 *	The channel or NULL if failed.  An error message is returned
 *	in the interpreter on failure.
 *
 * Side effects:
 *	Opens a client socket and creates a new channel.
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
Tcl_OpenTcpClient(interp, port, host, myaddr, myport, async)
    Tcl_Interp *interp;			/* For error reporting; can be NULL. */
    int port;				/* Port number to open. */
    char *host;				/* Host on which to open port. */
    char *myaddr;			/* Client-side address */
    int myport;				/* Client-side port */
    int async;				/* If nonzero, should connect
                                         * client socket asynchronously. */
{
	return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_OpenTcpServer --
 *
 *	Opens a TCP server socket and creates a channel around it.
 *
 * Results:
 *	The channel or NULL if failed.  An error message is returned
 *	in the interpreter on failure.
 *
 * Side effects:
 *	Opens a server socket and creates a new channel.
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
Tcl_OpenTcpServer(interp, port, host, acceptProc, acceptProcData)
    Tcl_Interp *interp;			/* For error reporting - may be
                                         * NULL. */
    int port;				/* Port number to open. */
    char *host;				/* Name of local host. */
    Tcl_TcpAcceptProc *acceptProc;	/* Callback for accepting connections
                                         * from new clients. */
    ClientData acceptProcData;		/* Data for the callback. */
{
	return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TclOS2NotifySocket --
 *
 *	Set up event notifiers for any sockets that are being watched.
 *	Also, clean up any sockets that are no longer being watched.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Adds and removes asynch select handlers.
 *
 *----------------------------------------------------------------------
 */

void
TclOS2NotifySocket()
{
    return;
}

/*
 *----------------------------------------------------------------------
 *
 * TclHasSockets --
 *
 *	This function determines whether sockets are available on the
 *	current system and returns an error in interp if they are not.
 *	Note that interp may be NULL.
 *
 * Results:
 *	Returns TCL_OK if the system supports sockets, or TCL_ERROR with
 *	an error in interp.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclHasSockets(interp)
    Tcl_Interp *interp;
{
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TclOS2SocketReady --
 *
 *	This function is invoked by Tcl_FileReady to check whether
 *	the specified conditions are present on a socket.
 *
 * Results:
 *	The return value is 0 if none of the conditions specified by
 *	mask were true for socket the last time the system checked.
 *	If any of the conditions were true, then the return value is a
 *	mask of those that were true.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclOS2SocketReady(file, mask)
    Tcl_File file;	/* File handle for a stream. */
    int mask;			/* OR'ed combination of TCL_READABLE,
				 * TCL_WRITABLE, and TCL_EXCEPTION:
				 * indicates conditions caller cares about. */
{
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetHostName --
 *
 *	Returns the name of the local host.
 *
 * Results:
 *	Returns a string containing the host name, or NULL on error.
 *	The returned string must be freed by the caller.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
Tcl_GetHostName()
{
    return (char *) NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TclOS2WatchSocket --
 *
 *	This function imlements the socket specific portion of the
 *	Tcl_WatchFile function in the notifier.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The watched socket will be placed into non-blocking mode, and
 *	an entry on the asynch handler list will be created if necessary. 
 *
 *----------------------------------------------------------------------
 */

void
TclOS2WatchSocket(file, mask)
    Tcl_File file;		/* Socket to watch. */
    int mask;			/* OR'ed combination of TCL_READABLE,
				 * TCL_WRITABLE, and TCL_EXCEPTION:
				 * indicates conditions to wait for
				 * in select. */
{
    return;
}
