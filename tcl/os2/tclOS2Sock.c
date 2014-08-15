/* 
 * tclOS2Sock.c --
 *
 *	This file contains OS/2-specific socket related code.
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 * Copyright (c) 1996-1997 Illya Vaes
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 */

#define FD_READ 0
#define FD_WRITE 1
#define FD_CLOSE 2
#define FD_ACCEPT 3
#define FD_CONNECT 4

#include "tclInt.h"
#include "tclPort.h"

/* defines for how a socket is to be closed */
#define HOW_RCV    0	/* further receives disabled */
#define HOW_SND    1	/* further sends disabled */
#define HOW_SNDRCV 2	/* further send and receives disabled */
/* ERROR return value */
#define SOCKET_ERROR -1
/* INVALID SOCKET return value */
#define INVALID_SOCKET -1

/*
 * The following structure contains pointers to all of the WinSock API entry
 * points used by Tcl.  It is initialized by InitSockets.  Since we
 * dynamically load Winsock.dll on demand, we must use this function table
 * to refer to functions in the socket API.
 */

static struct {
    SOCKET (APIENTRY  *accept)(SOCKET s, struct sockaddr  *addr,
	    int  *addrlen);
    int (APIENTRY  *bind)(SOCKET s, const struct sockaddr  *addr,
	    int namelen);
    int (APIENTRY  *soclose)(SOCKET s);
    int (APIENTRY  *connect)(SOCKET s, const struct sockaddr  *name,
	    int namelen);
    int (APIENTRY  *ioctlsocket)(SOCKET s, long cmd, u_long  *argp);
    int (APIENTRY  *getsockopt)(SOCKET s, int level, int optname,
	    void  * optval, int  *optlen);
    u_short (APIENTRY  *htons)(u_short hostshort);
    unsigned long (APIENTRY  *inet_addr)(const char  * cp);
    char  * (APIENTRY  *inet_ntoa)(struct in_addr in);
    int (APIENTRY  *listen)(SOCKET s, int backlog);
    u_short (APIENTRY  *ntohs)(u_short netshort);
    int (APIENTRY  *recv)(SOCKET s, void  * buf, int len, int flags);
    int (APIENTRY  *send)(SOCKET s, const void  * buf, int len, int flags);
    int (APIENTRY  *setsockopt)(SOCKET s, int level, int optname,
	    const void  * optval, int optlen);
    int (APIENTRY  *shutdown)(SOCKET s, int how);
    SOCKET (APIENTRY  *socket)(int af, int type, int protocol);
    struct hostent  * (APIENTRY  *gethostbyname)(const char  * name);
    struct hostent  * (APIENTRY  *gethostbyaddr)(const char  *addr,
            int addrlen, int addrtype);
    int (APIENTRY  *gethostname)(char  * name, int namelen);
    int (APIENTRY  *getpeername)(SOCKET sock, struct sockaddr  *name,
            int  *namelen);
    struct servent  * (APIENTRY  *getservbyname)(const char  * name,
	    const char  * proto);
    int (APIENTRY  *getsockname)(SOCKET sock, struct sockaddr  *name,
            int  *namelen);
} OS2Sock;

/*
 * The following define declares a new user message for use on the
 * socket window.
 */

#define SOCKET_MESSAGE	WM_USER+1

/*
 * The following structure is used to store the data associated with
 * each socket.  A Tcl_File of type TCL_OS2_SOCKET will contain a
 * pointer to one of these structures in the clientdata slot.
 */

typedef struct SocketInfo {
    SOCKET socket;		   /* Windows SOCKET handle. */
    int flags;			   /* Bit field comprised of the flags
				    * described below.  */
    int checkMask;		   /* OR'ed combination of TCL_READABLE and
				    * TCL_WRITABLE as set by an asynchronous
				    * event handler. */
    int watchMask;		   /* OR'ed combination of TCL_READABLE and
				    * TCL_WRITABLE as set by Tcl_WatchFile. */
    int eventMask;		   /* OR'ed combination of FD_READ, FD_WRITE,
                                    * FD_CLOSE, FD_ACCEPT and FD_CONNECT. */
    Tcl_File file;		   /* The file handle for the socket. */
    Tcl_TcpAcceptProc *acceptProc; /* Proc to call on accept. */
    ClientData acceptProcData;	   /* The data for the accept proc. */
    struct SocketInfo *nextPtr;	   /* The next socket on the global socket
				    * list. */
} SocketInfo;

/*
 * This defines the minimum buffersize maintained by the kernel.
 */

#define TCP_BUFFER_SIZE 4096

/*
 * The following macros may be used to set the flags field of
 * a SocketInfo structure.
 */

#define SOCKET_WATCH 	(1<<1)
				/* TclOS2WatchSocket has been called since the
				 * last time we entered Tcl_WaitForEvent. */
#define SOCKET_REGISTERED (1<<2)
				/* A valid WSAAsyncSelect handler is
				 * registered. */
#define SOCKET_ASYNCH	(1<<3)
				/* The socket is in asynch mode. */

/*
 * Every open socket has an entry on the following list.
 */

static SocketInfo *socketList = NULL;

/*
 * Static functions defined in this file.
 */

static SocketInfo *	CreateSocket _ANSI_ARGS_((Tcl_Interp *interp,
			    int port, char *host, int server, char *myaddr,
			    int myport, int async));
static int		CreateSocketAddress _ANSI_ARGS_(
			    (struct sockaddr_in *sockaddrPtr,
			    char *host, int port));
static int		InitSockets _ANSI_ARGS_((void));
static SocketInfo *	NewSocketInfo _ANSI_ARGS_((Tcl_File file));
static void		TcpAccept _ANSI_ARGS_((ClientData data, int mask));
static int		TcpCloseProc _ANSI_ARGS_((ClientData instanceData,
	            	    Tcl_Interp *interp, Tcl_File inFile,
                            Tcl_File outFile));
static int		TcpGetOptionProc _ANSI_ARGS_((ClientData instanceData,
		            char *optionName, Tcl_DString *optionValue));
static int		TcpInputProc _ANSI_ARGS_((ClientData instanceData,
	            	    Tcl_File inFile, char *buf, int toRead,
	            	    int *errorCode));
static int		TcpOutputProc _ANSI_ARGS_((ClientData instanceData,
	            	    Tcl_File outFile, char *buf, int toWrite,
	            	    int *errorCode));

/*
 * This structure describes the channel type structure for TCP socket
 * based IO.
 */

static Tcl_ChannelType tcpChannelType = {
    "tcp",		/* Type name. */
    NULL,		/* Block: Not used. */
    TcpCloseProc,	/* Close proc. */
    TcpInputProc,	/* Input proc. */
    TcpOutputProc,	/* Output proc. */
    NULL,		/* Seek proc. */
    NULL,		/* Set option proc. */
    TcpGetOptionProc,	/* Get option proc. */
};

/*
 * Socket notification window.  This window is used to receive socket
 * notification events.
 */


/*
 *----------------------------------------------------------------------
 *
 * InitSockets --
 *
 *	Initialize the socket module.  Attempts to load the wsock32.dll
 *	library and set up the OS2Sock function table.  If successful,
 *	registers the event window for the socket notifier code.
 *
 * Results:
 *	Returns 1 on successful initialization, 0 on failure.
 *
 * Side effects:
 *	Dynamically loads wsock32.dll, and registers a new window
 *	class and creates a window for use in asynchronous socket
 *	notification.
 *
 *----------------------------------------------------------------------
 */

static int
InitSockets()
{
    /*
     * Initialize the function table.
     */

#ifdef DEBUG
    printf("InitSockets\n");
#endif

    OS2Sock.accept = accept;
    OS2Sock.bind = bind;
    OS2Sock.connect = connect;
    OS2Sock.ioctlsocket = (int (APIENTRY *)(SOCKET s, long cmd,
                           u_long *argp))ioctl;
    OS2Sock.getsockopt = getsockopt;
    OS2Sock.htons = _swaps;
    OS2Sock.inet_addr = inet_addr;
    OS2Sock.inet_ntoa = inet_ntoa;
    OS2Sock.listen = listen;
    OS2Sock.ntohs = _swaps;
    OS2Sock.recv = recv;
    OS2Sock.send = send;
    OS2Sock.shutdown = shutdown;
    OS2Sock.setsockopt = setsockopt;
    OS2Sock.socket = socket;
    OS2Sock.gethostbyaddr = gethostbyaddr;
    OS2Sock.gethostbyname = gethostbyname;
    OS2Sock.gethostname = gethostname;
    OS2Sock.getpeername = getpeername;
    OS2Sock.getservbyname = getservbyname;
    OS2Sock.getsockname = getsockname;

    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * TcpCloseProc --
 *
 *	This procedure is called by the generic IO level to perform
 *	channel type specific cleanup on a socket based channel
 *	when the channel is closed.
 *
 * Results:
 *	0 if successful, the value of errno if failed.
 *
 * Side effects:
 *	Closes the socket.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static int
TcpCloseProc(instanceData, interp, inFile, outFile)
    ClientData instanceData;	/* The socket to close. */
    Tcl_Interp *interp;		/* Unused. */
    Tcl_File inFile, outFile;	/* Unused. */
{
    SocketInfo *infoPtr = (SocketInfo *) instanceData;
    int errorCode = 0;

    /*
     * Clean up the OS socket handle.
     */
    
    if ((*OS2Sock.shutdown)(infoPtr->socket, HOW_SNDRCV) == SOCKET_ERROR) {
	errorCode = errno;
    }

    /*
     * Delete a file handler that may be active for this socket.
     * Channel handlers are already deleted in the generic IO close
     * code which called this function.
     */
    
    Tcl_DeleteFileHandler(infoPtr->file);

    /*
     * Free the file handle.  As a side effect, this will call the
     * SocketFreeProc to release the SocketInfo associated with this file.
     */

    Tcl_FreeFile(infoPtr->file);

    return errorCode;
}

/*
 *----------------------------------------------------------------------
 *
 * SocketFreeProc --
 *
 *	This callback is invoked by Tcl_FreeFile in order to delete
 *	the notifier data associated with a file handle.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Removes the SocketInfo from the global socket list.
 *
 *----------------------------------------------------------------------
 */

static void
SocketFreeProc(clientData)
    ClientData clientData;
{
    SocketInfo *infoPtr = (SocketInfo *) clientData;

    /*
     * Remove the socket from socketList.
     */

    if (infoPtr == socketList) {
	socketList = infoPtr->nextPtr;
    } else {
	SocketInfo *p;
	for (p = socketList; p != NULL; p = p->nextPtr) {
	    if (p->nextPtr == infoPtr) {
		p->nextPtr = infoPtr->nextPtr;
		break;
	    }
	}
    }
    ckfree((char *) infoPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * NewSocketInfo --
 *
 *	This function allocates and initializes a new SocketInfo
 *	structure.
 *
 * Results:
 *	Returns a newly allocated SocketInfo.
 *
 * Side effects:
 *	Adds the socket to the global socket list.
 *
 *----------------------------------------------------------------------
 */

static SocketInfo *
NewSocketInfo(file)
    Tcl_File file;
{
    SocketInfo *infoPtr;

    infoPtr = (SocketInfo *) ckalloc((unsigned) sizeof(SocketInfo));
    infoPtr->socket = (SOCKET) Tcl_GetFileInfo(file, NULL);
    infoPtr->flags = 0;
    infoPtr->checkMask = 0;
    infoPtr->watchMask = 0;
    infoPtr->eventMask = 0;
    infoPtr->file = file;
    infoPtr->acceptProc = NULL;
    infoPtr->nextPtr = socketList;
    socketList = infoPtr;

    Tcl_SetNotifierData(file, SocketFreeProc, (ClientData) infoPtr);
    return infoPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * CreateSocket --
 *
 *	This function opens a new socket and initializes the
 *	SocketInfo structure.
 *
 * Results:
 *	Returns a new SocketInfo, or NULL with an error in interp.
 *
 * Side effects:
 *	Adds a new socket to the socketList.
 *
 *----------------------------------------------------------------------
 */

static SocketInfo *
CreateSocket(interp, port, host, server, myaddr, myport, async)
    Tcl_Interp *interp;		/* For error reporting; can be NULL. */
    int port;			/* Port number to open. */
    char *host;			/* Name of host on which to open port. */
    int server;			/* 1 if socket should be a server socket,
				 * else 0 for a client socket. */
    char *myaddr;		/* Optional client-side address */
    int myport;			/* Optional client-side port */
    int async;			/* If nonzero, connect client socket
                                 * asynchronously. Unused. */
{
    int status;
    struct sockaddr_in sockaddr;	/* Socket address */
    struct sockaddr_in mysockaddr;	/* Socket address for client */
    SOCKET sock;

#ifdef DEBUG
    printf("CreateSocket port %d, host %s, server %d, cl-adr %s, cl-p %d\n",
           port, host, server, myaddr, myport);
#endif

    if (! CreateSocketAddress(&sockaddr, host, port)) {
	goto addressError;
    }
    if ((myaddr != NULL || myport != 0) &&
	    ! CreateSocketAddress(&mysockaddr, myaddr, myport)) {
	goto addressError;
    }

    sock = (*OS2Sock.socket)(PF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
	goto addressError;
    }
#ifdef DEBUG
    printf("Created socket %d\n", sock);
#endif

    /*
     * Set kernel space buffering
     */

    TclSockMinimumBuffers(sock, TCP_BUFFER_SIZE);

    if (server) {
	/*
	 * Bind to the specified port.  Note that we must not call setsockopt
	 * with SO_REUSEADDR because Microsoft allows addresses to be reused
	 * even if they are still in use.
	 */
    
	status = (*OS2Sock.bind)(sock, (struct sockaddr *) &sockaddr,
		sizeof(sockaddr));
#ifdef DEBUG
        printf("Bind: %d\n", status);
#endif
	if (status != SOCKET_ERROR) {
	    (*OS2Sock.listen)(sock, 5);
	}
    } else {
	if (myaddr != NULL || myport != 0) { 
	    status = (*OS2Sock.bind)(sock, (struct sockaddr *) &mysockaddr,
		    sizeof(struct sockaddr));
#ifdef DEBUG
            printf("Bind: %d\n", status);
#endif
	    if (status < 0) {
		goto bindError;
	    }
	}
	status = (*OS2Sock.connect)(sock, (struct sockaddr *) &sockaddr,
		sizeof(sockaddr));
#ifdef DEBUG
        printf("Connect: %d (error %d)\n", status, errno);
#endif
    }
    if (status != SOCKET_ERROR) {
	int flag = 1;
	status = (*OS2Sock.ioctlsocket)(sock, FIONBIO, (u_long *)&flag);
#ifdef DEBUG
        printf("Ioctlsocket: %d\n", status);
#endif
    }

bindError:
    if (status == SOCKET_ERROR) {
        if (interp != NULL) {
            Tcl_AppendResult(interp, "couldn't open socket: ",
                    Tcl_PosixError(interp), (char *) NULL);
        }
        (*OS2Sock.shutdown)(sock, HOW_SNDRCV);
        return NULL;
    }

    /*
     * Add this socket to the global list of sockets.
     */

    return NewSocketInfo(Tcl_GetFile((ClientData) sock, TCL_OS2_SOCKET));

addressError:
    if (interp != NULL) {
	Tcl_AppendResult(interp, "couldn't open socket: ",
		Tcl_PosixError(interp), (char *) NULL);
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * CreateSocketAddress --
 *
 *	This function initializes a sockaddr structure for a host and port.
 *
 * Results:
 *	1 if the host was valid, 0 if the host could not be converted to
 *	an IP address.
 *
 * Side effects:
 *	Fills in the *sockaddrPtr structure.
 *
 *----------------------------------------------------------------------
 */

static int
CreateSocketAddress(sockaddrPtr, host, port)
    struct sockaddr_in *sockaddrPtr;	/* Socket address */
    char *host;				/* Host.  NULL implies INADDR_ANY */
    int port;				/* Port number */
{
    struct hostent *hostent;		/* Host database entry */
    struct in_addr addr;		/* For 64/32 bit madness */

#ifdef DEBUG
    printf("CreateSocketAddress \"%s\":%d\n", host, port);
    fflush(stdout);
#endif

    (void) memset((char *) sockaddrPtr, '\0', sizeof(struct sockaddr_in));
    sockaddrPtr->sin_family = AF_INET;
    sockaddrPtr->sin_port = (*OS2Sock.htons)((short) (port & 0xFFFF));
#ifdef DEBUG
    printf("sinPort %d\n", sockaddrPtr->sin_port);
    fflush(stdout);
#endif
    if (host == NULL) {
	addr.s_addr = INADDR_ANY;
    } else {
        addr.s_addr = (*OS2Sock.inet_addr)(host);
#ifdef DEBUG
        printf("addr.s_addr %d\n", addr.s_addr);
#endif
        if (addr.s_addr == (unsigned long) -1) {
            hostent = (*OS2Sock.gethostbyname)(host);
#ifdef DEBUG
            printf("hostent %x\n", hostent);
#endif
            if (hostent != NULL) {
                memcpy((char *) &addr,
                        (char *) hostent->h_addr_list[0],
                        (size_t) hostent->h_length);
            } else {
#ifdef	EHOSTUNREACH
                errno = EHOSTUNREACH;
    #ifdef DEBUG
                printf("gethostbyname ERROR EHOSTUNREACH\n");
    #endif
#else
    #ifdef ENXIO
                errno = ENXIO;
        #ifdef DEBUG
                printf("gethostbyname ERROR ENXIO\n");
        #endif
    #else
        #ifdef DEBUG
                printf("gethostbyname ERROR ?\n");
        #endif
    #endif
#endif
		return 0;	/* Error. */
	    }
	}
    }

    /*
     * NOTE: On 64 bit machines the assignment below is rumored to not
     * do the right thing. Please report errors related to this if you
     * observe incorrect behavior on 64 bit machines such as DEC Alphas.
     * Should we modify this code to do an explicit memcpy?
     */

    sockaddrPtr->sin_addr.s_addr = addr.s_addr;
    return 1;	/* Success. */
}

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
    Tcl_Channel chan;
    SocketInfo *infoPtr;
    char channelName[20];

#ifdef DEBUG
    printf("Tcl_OpenTcpClient port %d host %s myaddr %s myport %d async %d\n",
           port, host, myaddr, myport, async);
#endif

    if (TclHasSockets(interp) != TCL_OK) {
	return NULL;
    }

    /*
     * Create a new client socket and wrap it in a channel.
     */

    infoPtr = CreateSocket(interp, port, host, 0, myaddr, myport, async);
    if (infoPtr == NULL) {
	return NULL;
    }

    sprintf(channelName, "sock%d", infoPtr->socket);
#ifdef DEBUG
    printf("Tcl_OpenTcpClient creating socket %s\n", channelName);
#endif

    chan = Tcl_CreateChannel(&tcpChannelType, channelName, infoPtr->file,
	    infoPtr->file, (ClientData) infoPtr);
    if (Tcl_SetChannelOption(interp, chan, "-translation", "auto crlf") ==
            TCL_ERROR) {
        Tcl_Close((Tcl_Interp *) NULL, chan);
#ifdef DEBUG
        printf("ERROR: Tcl_SetChannelOption translation failed\n");
#endif
        return (Tcl_Channel) NULL;
    }
    if (Tcl_SetChannelOption(NULL, chan, "-eofchar", "") == TCL_ERROR) {
        Tcl_Close((Tcl_Interp *) NULL, chan);
#ifdef DEBUG
        printf("ERROR: Tcl_SetChannelOption eofchar failed\n");
#endif
        return (Tcl_Channel) NULL;
    }
    return chan;
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
    Tcl_Channel chan;
    SocketInfo *infoPtr;
    char channelName[20];

#ifdef DEBUG
    printf("Tcl_OpenTcpServer port %d host %s\n", port, host);
#endif

    if (TclHasSockets(interp) != TCL_OK) {
	return NULL;
    }

    /*
     * Create a new server socket and wrap it in a channel.
     */

    infoPtr = CreateSocket(interp, port, host, 1, NULL, 0, 0);
    if (infoPtr == NULL) {
	return NULL;
    }

    infoPtr->acceptProc = acceptProc;
    infoPtr->acceptProcData = acceptProcData;

    /*
     * Set up the callback mechanism for accepting connections
     * from new clients. The caller will use Tcl_TcpRegisterCallback
     * to register a callback to call when a new connection is
     * accepted.
     */

    Tcl_CreateFileHandler(infoPtr->file, TCL_READABLE, TcpAccept,
            (ClientData) infoPtr);

    sprintf(channelName, "sock%d", infoPtr->socket);
#ifdef DEBUG
    printf("Tcl_OpenTcpServer creating socket %s\n", channelName);
#endif

    chan = Tcl_CreateChannel(&tcpChannelType, channelName, NULL, NULL,
	    (ClientData) infoPtr);
    if (Tcl_SetChannelOption(interp, chan, "-eofchar", "") == TCL_ERROR) {
        Tcl_Close((Tcl_Interp *) NULL, chan);
        return (Tcl_Channel) NULL;
    }

    return chan;
}

/*
 *----------------------------------------------------------------------
 *
 * TcpAccept --
 *	Accept a TCP socket connection.  This is called by the event loop,
 *	and it in turns calls any registered callbacks for this channel.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Evals the Tcl script associated with the server socket.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static void
TcpAccept(data, mask)
    ClientData data;			/* Callback token. */
    int mask;				/* Not used. */
{
    SOCKET newSocket;
    SocketInfo *infoPtr = (SocketInfo *) data;
    SocketInfo *newInfoPtr;
    struct sockaddr_in addr;
    int len;
    Tcl_Channel chan;
    char channelName[20];
    int flag = 1;

#ifdef DEBUG
    printf("TcpAccept\n");
    fflush(stdout);
#endif

    len = sizeof(struct sockaddr_in);
    newSocket = (*OS2Sock.accept)(infoPtr->socket, (struct sockaddr *)&addr,
	    &len);

    infoPtr->checkMask &= ~TCL_READABLE;

    if (newSocket == INVALID_SOCKET) {
        return;
    }

    /*
     * Clear the inherited event mask.
     */

/*
    (*OS2Sock.WSAAsyncSelect)(newSocket, socketWindow, 0, 0);
*/

    /*
     * Set the socket into non-blocking mode.
     */

    if ((*OS2Sock.ioctlsocket)(newSocket, FIONBIO, (u_long *)&flag) != 0) {
	(*OS2Sock.soclose)(newSocket);
	return;
    }

    /*
     * Add this socket to the global list of sockets.
     */

    newInfoPtr = NewSocketInfo(Tcl_GetFile((ClientData) newSocket,
	    TCL_OS2_SOCKET));


    sprintf(channelName, "sock%d", newSocket);
    chan = Tcl_CreateChannel(&tcpChannelType, channelName, newInfoPtr->file,
	    newInfoPtr->file, (ClientData) newInfoPtr);
    if (Tcl_SetChannelOption(NULL, chan, "-translation", "auto crlf") ==
            TCL_ERROR) {
        Tcl_Close((Tcl_Interp *) NULL, chan);
        return;
    }
    if (Tcl_SetChannelOption(NULL, chan, "-eofchar", "") == TCL_ERROR) {
        Tcl_Close((Tcl_Interp *) NULL, chan);
        return;
    }

    /*
     * Invoke the accept callback procedure.
     */

    if (infoPtr->acceptProc != NULL) {
	(infoPtr->acceptProc) (infoPtr->acceptProcData, chan,
		(*OS2Sock.inet_ntoa)(addr.sin_addr),
		(*OS2Sock.ntohs)(addr.sin_port));
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TcpInputProc --
 *
 *	This procedure is called by the generic IO level to read data from
 *	a socket based channel.
 *
 * Results:
 *	The number of bytes read or -1 on error.
 *
 * Side effects:
 *	Consumes input from the socket.
 *
 *----------------------------------------------------------------------
 */

static int
TcpInputProc(instanceData, inFile, buf, toRead, errorCodePtr)
    ClientData instanceData;		/* The socket state. */
    Tcl_File inFile;			/* Not used. */
    char *buf;				/* Where to store data. */
    int toRead;				/* Maximum number of bytes to read. */
    int *errorCodePtr;			/* Where to store error codes. */
{
    SocketInfo *infoPtr = (SocketInfo *) instanceData;
    int bytesRead;
    
    *errorCodePtr = 0;
    bytesRead = (*OS2Sock.recv)(infoPtr->socket, buf, toRead, 0);
    if (bytesRead == SOCKET_ERROR) {
        if (errno != ECONNRESET) {
            *errorCodePtr = errno;
        }
        bytesRead = -1;
    }

    /*
     * Clear the readable bit in the check mask.  If an async handler
     * is still registered for this socket, then it will generate a new
     * event if there is still data available.  When the event is
     * processed, the readable bit will be turned back on.
     */

    infoPtr->checkMask &= ~TCL_READABLE;
    return bytesRead;
}

/*
 *----------------------------------------------------------------------
 *
 * TcpOutputProc --
 *
 *	This procedure is called by the generic IO level to write data
 *	to a socket based channel.
 *
 * Results:
 *	The number of bytes written or -1 on failure.
 *
 * Side effects:
 *	Produces output on the socket.
 *
 *----------------------------------------------------------------------
 */

static int
TcpOutputProc(instanceData, outFile, buf, toWrite, errorCodePtr)
    ClientData instanceData;		/* The socket state. */
    Tcl_File outFile;			/* The socket to write to. */
    char *buf;				/* Where to get data. */
    int toWrite;			/* Maximum number of bytes to write. */
    int *errorCodePtr;			/* Where to store error codes. */
{
    SocketInfo *infoPtr = (SocketInfo *) instanceData;
    int bytesWritten;

    *errorCodePtr = 0;
    bytesWritten = (*OS2Sock.send)(infoPtr->socket, buf, toWrite, 0);
    if (bytesWritten == SOCKET_ERROR) {
	if (errno == EWOULDBLOCK) {
	    infoPtr->checkMask &= ~TCL_WRITABLE;
	}
        *errorCodePtr = errno;
        return -1;
    }

    /*
     * Clear the readable bit in the check mask.  If an async handler
     * is still registered for this socket, then it will generate a new
     * event if there is still data available.  When the event is
     * processed, the readable bit will be turned back on.
     */

    infoPtr->checkMask &= (~(TCL_WRITABLE));

    return bytesWritten;
}

/*
 *----------------------------------------------------------------------
 *
 * TcpGetOptionProc --
 *
 *	Computes an option value for a TCP socket based channel, or a
 *	list of all options and their values.
 *
 *	Note: This code is based on code contributed by John Haxby.
 *
 * Results:
 *	A standard Tcl result. The value of the specified option or a
 *	list of all options and	their values is returned in the
 *	supplied DString.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TcpGetOptionProc(instanceData, optionName, dsPtr)
    ClientData instanceData;		/* Socket state. */
    char *optionName;			/* Name of the option to
                                         * retrieve the value for, or
                                         * NULL to get all options and
                                         * their values. */
    Tcl_DString *dsPtr;			/* Where to store the computed
                                         * value; initialized by caller. */
{
    SocketInfo *infoPtr;
    struct sockaddr_in sockname;
    struct sockaddr_in peername;
    struct hostent *hostEntPtr;
    SOCKET sock;
    int size = sizeof(struct sockaddr_in);
    size_t len = 0;
    char buf[128];

    infoPtr = (SocketInfo *) instanceData;
    sock = (int) infoPtr->socket;
    if (optionName != (char *) NULL) {
        len = strlen(optionName);
    }

    if ((len == 0) ||
            ((len > 1) && (optionName[1] == 'p') &&
                    (strncmp(optionName, "-peername", len) == 0))) {
        if ((*OS2Sock.getpeername)(sock, (struct sockaddr *) &peername, &size)
                >= 0) {
            if (len == 0) {
                Tcl_DStringAppendElement(dsPtr, "-peername");
                Tcl_DStringStartSublist(dsPtr);
            }
            Tcl_DStringAppendElement(dsPtr,
                    (*OS2Sock.inet_ntoa)(peername.sin_addr));
            hostEntPtr = (*OS2Sock.gethostbyaddr)(
                (char *) &(peername.sin_addr), sizeof(peername.sin_addr),
                PF_INET);
            if (hostEntPtr != (struct hostent *) NULL) {
                Tcl_DStringAppendElement(dsPtr, hostEntPtr->h_name);
            } else {
                Tcl_DStringAppendElement(dsPtr,
                        (*OS2Sock.inet_ntoa)(peername.sin_addr));
            }
            sprintf(buf, "%d", (*OS2Sock.ntohs)(peername.sin_port));
            Tcl_DStringAppendElement(dsPtr, buf);
            if (len == 0) {
                Tcl_DStringEndSublist(dsPtr);
            } else {
                return TCL_OK;
            }
        }
    }

    if ((len == 0) ||
            ((len > 1) && (optionName[1] == 's') &&
                    (strncmp(optionName, "-sockname", len) == 0))) {
        if ((*OS2Sock.getsockname)(sock, (struct sockaddr *) &sockname, &size)
                >= 0) {
            if (len == 0) {
                Tcl_DStringAppendElement(dsPtr, "-sockname");
                Tcl_DStringStartSublist(dsPtr);
            }
            Tcl_DStringAppendElement(dsPtr,
                    (*OS2Sock.inet_ntoa)(sockname.sin_addr));
            hostEntPtr = (*OS2Sock.gethostbyaddr)(
                (char *) &(sockname.sin_addr), sizeof(peername.sin_addr),
                PF_INET);
            if (hostEntPtr != (struct hostent *) NULL) {
                Tcl_DStringAppendElement(dsPtr, hostEntPtr->h_name);
            } else {
                Tcl_DStringAppendElement(dsPtr,
                        (*OS2Sock.inet_ntoa)(sockname.sin_addr));
            }
            sprintf(buf, "%d", (*OS2Sock.ntohs)(sockname.sin_port));
            Tcl_DStringAppendElement(dsPtr, buf);
            if (len == 0) {
                Tcl_DStringEndSublist(dsPtr);
            } else {
                return TCL_OK;
            }
        }
    }

    if (len > 0) {
        Tcl_SetErrno(EINVAL);
        return TCL_ERROR;
    }

    return TCL_OK;
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
    SocketInfo *infoPtr = (SocketInfo *) Tcl_GetNotifierData(file, NULL);
    Tcl_Time dontBlock;

    dontBlock.sec = 0; dontBlock.usec = 0;

    /*
     * Create socket info on demand if necessary.  We should only enter this
     * code if the socket was created outside of Tcl.  Since this may be
     * the first time that the socket code has been called, we need to invoke
     * TclHasSockets to ensure that everything is initialized properly.
     */

    if (infoPtr == NULL) {
	if (TclHasSockets(NULL) != TCL_OK) {
	    return;
	}
	infoPtr = NewSocketInfo(file);
    }

    infoPtr->flags |= SOCKET_WATCH;

    /*
     * Check if any bits are set on the checkMask. If there are, this
     * means that the socket already had events on it, and we need to
     * check it immediately. To do this, set the maximum block time to
     * zero.
     */

    if (infoPtr->checkMask != 0) {
        Tcl_SetMaxBlockTime(&dontBlock);
        return;
    }
        
    /*
     * If the new mask includes more conditions than the current mask,
     * then we mark the socket as unregistered so it will be reregistered
     * the next time we enter Tcl_WaitForEvent.
     */

    mask |= infoPtr->watchMask;
    if (infoPtr->watchMask != mask) {
	infoPtr->flags &= (~(SOCKET_REGISTERED));
	infoPtr->watchMask = mask;
    }
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
    SocketInfo *infoPtr;

    if (socketList == NULL) {
	return;
    }

    /*
     * Establish or remove any notifiers.
     */

    for (infoPtr = socketList; infoPtr != NULL; infoPtr = infoPtr->nextPtr) {
	if (infoPtr->flags & SOCKET_WATCH) {
	    if (!(infoPtr->flags & SOCKET_REGISTERED)) {
		int events = 0;

		if (infoPtr->watchMask & TCL_READABLE) {
		    events |= (FD_READ | FD_ACCEPT | FD_CLOSE);
		} else if (infoPtr->watchMask & TCL_WRITABLE) {
		    events |= (FD_WRITE | FD_CONNECT);
		}

                /*
                 * If the new event interest mask does not match what is
                 * currently set into the socket, set the new mask.
                 */

                if (events != infoPtr->eventMask) {
                    infoPtr->eventMask = events;
/*
                    (*OS2Sock.WSAAsyncSelect)(infoPtr->socket, socketWindow,
                            SOCKET_MESSAGE, events);
*/
                }
	    }
	} else {
	    if (infoPtr->flags & SOCKET_REGISTERED) {
                infoPtr->eventMask = 0;
/*
		(*OS2Sock.WSAAsyncSelect)(infoPtr->socket, socketWindow, 0, 0);
*/
	    }
	}
    }
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
/*
    SocketInfo *infoPtr = (SocketInfo *) Tcl_GetNotifierData(file, NULL);

    infoPtr->flags &= (~(SOCKET_WATCH));
    return (infoPtr->checkMask & mask);
*/
    int result, ret, fd, type;
    fd_set readfd, writefd, exceptfd;
    struct timeval timeout;

    fd = (int) Tcl_GetFileInfo(file, &type);

    timeout.tv_sec = 0;
    timeout.tv_usec = 100;
    FD_ZERO(&readfd);
    FD_SET(fd, &readfd);
    FD_ZERO(&writefd);
    FD_SET(fd, &writefd);
    FD_ZERO(&exceptfd);
    FD_SET(fd, &exceptfd);

    ret = select(fd+1, &readfd, &writefd, &exceptfd, &timeout);
#ifdef DEBUG
    printf("TclOS2SocketReady, select (fd %d) returns %d\n", fd, ret);
    for (result=0; result < (FD_SETSIZE+31)/32; result++) {
        printf("    %x %x %x\n", readfd.fds_bits[result],
               writefd.fds_bits[result], exceptfd.fds_bits[result]);
    }
    printf("    mask & TCL_READABLE %x, FD_ISSET(fd, &readfd) %x\n",
           mask & TCL_READABLE, FD_ISSET(fd, &readfd));
    printf("    mask & TCL_WRITABLE %x, FD_ISSET(fd, &writefd) %x\n",
           mask & TCL_WRITABLE, FD_ISSET(fd, &writefd));
    printf("    mask & TCL_EXCEPTION %x, FD_ISSET(fd, &exceptfd) %x\n",
           mask & TCL_EXCEPTION, FD_ISSET(fd, &exceptfd));
    fflush(stdout);
#endif
    if (ret == 0 || ret == -1) {
        return 0;
    }

    result = 0;
    if ((mask & TCL_READABLE) && FD_ISSET(fd, &readfd)) {
        result |= TCL_READABLE;
    }
    if ((mask & TCL_WRITABLE) && FD_ISSET(fd, &writefd)) {
        result |= TCL_WRITABLE;
    }
    if ((mask & TCL_EXCEPTION) && FD_ISSET(fd, &exceptfd)) {
        result |= TCL_EXCEPTION;
    }
#ifdef DEBUG
    printf("    result %d (readfd %x, writefd %x, exceptfd %x)\n", result,
           readfd, writefd, exceptfd);
    fflush(stdout);
#endif
    return result;
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
    static int  hostnameInitialized = 0;
    static char hostname[255];	/* This buffer should be big enough for
                                 * hostname plus domain name. */

    if (TclHasSockets(NULL) != TCL_OK) {
	return "";
    }

    if (hostnameInitialized) {
        return hostname;
    }
    if ((*OS2Sock.gethostname)(hostname, 100) == 0) {
        hostnameInitialized = 1;
        return hostname;
    }
    return (char *) NULL;
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
    static int initialized = 0;	/* 1 if the socket system has been
				 * initialized. */
    static int hasSockets = 0;	/* 1 if the system supports sockets. */

    if (!initialized) {
        hasSockets = InitSockets();
        initialized = 1;
    }
    
    if (hasSockets) {
	return TCL_OK;
    }
    if (interp != NULL) {
	Tcl_AppendResult(interp, "sockets are not available on this system",
		NULL);
    }
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * getsockopt, et al. --
 *
 *	These functions are wrappers that let us bind the socket
 *	API dynamically so we can run on systems that don't have
 *	the socket dll.  We need wrappers for these interfaces
 *	because they are called from the generic Tcl code 
 *
 * Results:
 *	As defined for each function.
 *
 * Side effects:
 *	As defined for each function.
 *
 *----------------------------------------------------------------------
 */

/*
int APIENTRY 
getsockopt(SOCKET s, int level, int optname, char  * optval,
	int  *optlen)
{
    return (*OS2Sock.getsockopt)(s, level, optname, optval, optlen);
}

int APIENTRY 
setsockopt(SOCKET s, int level, int optname, const char  * optval,
	int optlen)
{
    return (*OS2Sock.setsockopt)(s, level, optname, optval, optlen);
}

u_short APIENTRY 
ntohs(u_short netshort)
{
    return (*OS2Sock.ntohs)(netshort);
}

struct servent  * APIENTRY 
getservbyname(const char  * name, const char  * proto)
{
    return (*OS2Sock.getservbyname)(name, proto);
}
*/
