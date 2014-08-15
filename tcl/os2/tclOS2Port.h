/*
 * tclOS2Port.h --
 *
 *	This header file handles porting issues that occur because of
 *	differences between OS/2 and Unix. It should be the only file
 *	that contains #ifdefs to handle different flavors of OS.
 *
 * Copyright (c) 1996-1997 Illya Vaes
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 */

#ifndef _TCLOS2PORT
#define _TCLOS2PORT

#define INCL_BASE
#define INCL_PM
#define INCL_DOSPROCESS /*MM*/
#include <os2.h>
#undef INCL_DOSPROCESS /*MM*/
#undef INCL_PM
#undef INCL_BASE

#ifndef OS2
#define OS2
#endif

/* Global variables */
extern HAB hab;	/* Anchor block */
extern HMQ hmq;	/* Message queue */
extern ULONG maxPath;	/* Maximum path length */

/* Definitions */
#define FS_CASE_SENSITIVE    1
#define FS_CASE_IS_PRESERVED 2

#include <sys/types.h>

/* *MM* added this to get fd_set */
#ifdef __IBMC__
#include <sys/select.h>
#endif

/* BSD sockets from EMX are int */
#define SOCKET int
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <netdb.h>
//#include <arpa/inet.h> *MM*
//#include <sys/utsname.h> *MM*

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* *MM* replaced first include with this fancy wrapper */
#ifdef EMX
#include <sys/errno.h>
#else
// use TCP/IP's errno as well as libC errno
#include <errno.h>
#include <nerrno.h>
#define EPIPE SOCEPIPE
#define EFAULT SOCEFAULT
#define EPERM SOCEPERM
#define ESRCH SOCESRCH
#define EROFS 666
#define EIO 667
#define ESPIPE 668
#endif
#include <process.h>
#include <signal.h>
#include <time.h>
/* *MM* - for vacpp, map timezone variable. */
#ifdef __IBMC__
#define timezone _timezone
#endif
#include <sys/time.h>
#include <sys/timeb.h>
#include <sys/stat.h>
#include <io.h>
#include <fcntl.h>

/*
 * If ENOTSUP is not defined, define it to a value that will never occur.
 */

#ifndef ENOTSUP
#define ENOTSUP         -1030507
#endif

/*
 * The default platform eol translation on OS/2 is TCL_TRANSLATE_CRLF:
 */

#define TCL_PLATFORM_TRANSLATION        TCL_TRANSLATE_CRLF

/*
 * Declare dynamic loading extension macro.
 */

#define TCL_SHLIB_EXT ".dll"

/*
 * Declare directory manipulation routines.
 */

#ifdef HAS_DIRENT
#	include <dirent.h>
#else /* HAS_DIRENT */

#include <direct.h>
#define MAXNAMLEN 255

struct dirent {
    long d_ino;			/* Inode number of entry */
    short d_reclen;		/* Length of this record */
    short d_namlen;		/* Length of string in d_name */
    char d_name[MAXNAMLEN + 1];
				/* Name must be no longer than this */
};

typedef struct _dirdesc DIR;

EXTERN void			closedir _ANSI_ARGS_((DIR *dirp));
EXTERN DIR *			opendir _ANSI_ARGS_((char *name));
EXTERN struct dirent *		readdir _ANSI_ARGS_((DIR *dirp));
#endif /* HAS_DIRENT */

/*
 * Supply definitions for macros to query wait status, if not already
 * defined in header files above.
 */

#if TCL_UNION_WAIT
#   define WAIT_STATUS_TYPE union wait
#else
#   define WAIT_STATUS_TYPE int
#endif

#ifndef WIFEXITED
#   define WIFEXITED(stat)  (((*((int *) &(stat))) & 0xff) == 0)
#endif

#ifndef WEXITSTATUS
#   define WEXITSTATUS(stat) (((*((int *) &(stat))) >> 8) & 0xff)
#endif

#ifndef WIFSIGNALED
#   define WIFSIGNALED(stat) (((*((int *) &(stat)))) && ((*((int *) &(stat))) == ((*((int *) &(stat))) & 0x00ff)))
#endif

#ifndef WTERMSIG
#   define WTERMSIG(stat)    ((*((int *) &(stat))) & 0x7f)
#endif

#ifndef WIFSTOPPED
#   define WIFSTOPPED(stat)  (((*((int *) &(stat))) & 0xff) == 0177)
#endif

#ifndef WSTOPSIG
#   define WSTOPSIG(stat)    (((*((int *) &(stat))) >> 8) & 0xff)
#endif

/*
 * Define constants for waitpid() system call if they aren't defined
 * by a system header file.
 */

#ifndef WNOHANG
#   define WNOHANG 1
#endif
#ifndef WUNTRACED
#   define WUNTRACED 2
#endif

/*
 * Define MAXPATHLEN in terms of MAXPATH if available
 */

#ifndef MAX_PATH
#define MAX_PATH CCHMAXPATH
#endif

#ifndef MAXPATH
#define MAXPATH MAX_PATH
#endif /* MAXPATH */

#ifndef MAXPATHLEN
#define MAXPATHLEN MAXPATH
#endif /* MAXPATHLEN */

#ifndef F_OK
#    define F_OK 00
#endif
#ifndef X_OK
#    define X_OK 01
#endif
#ifndef W_OK
#    define W_OK 02
#endif
#ifndef R_OK
#    define R_OK 04
#endif

/*
 * On systems without symbolic links (i.e. S_IFLNK isn't defined)
 * define "lstat" to use "stat" instead.
 */

#ifndef S_IFLNK
#   define lstat stat
#endif

/*
 * Define macros to query file type bits, if they're not already
 * defined.
 */

/* *MM* added this ifndef clause */
#ifndef S_IFMT
#define S_IFMT 0xFFFF
#endif

#ifndef S_ISREG
#   ifdef S_IFREG
#       define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#   else
#       define S_ISREG(m) 0
#   endif
# endif
#ifndef S_ISDIR
#   ifdef S_IFDIR
#       define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#   else
#       define S_ISDIR(m) 0
#   endif
# endif
#ifndef S_ISCHR
#   ifdef S_IFCHR
#       define S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)
#   else
#       define S_ISCHR(m) 0
#   endif
# endif
#ifndef S_ISBLK
#   ifdef S_IFBLK
#       define S_ISBLK(m) (((m) & S_IFMT) == S_IFBLK)
#   else
#       define S_ISBLK(m) 0
#   endif
# endif
#ifndef S_ISFIFO
#   ifdef S_IFIFO
#       define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#   else
#       define S_ISFIFO(m) 0
#   endif
# endif
#ifndef S_ISLNK
#   ifdef S_IFLNK
#       define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)
#   else
#       define S_ISLNK(m) 0
#   endif
# endif
#ifndef S_ISSOCK
#   ifdef S_IFSOCK
#       define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)
#   else
#       define S_ISSOCK(m) 0
#   endif
# endif

/*
 * Define pid_t and uid_t if they're not already defined.
 */

#if ! TCL_PID_T
#   define pid_t int
#endif
#if ! TCL_UID_T
#   define uid_t int
#endif

/*
 * Substitute Tcl's own versions for several system calls.
 */

#define matherr		_matherr

/*
 * Provide a stub definition for TclGetUserHome().
 */

#define TclGetUserHome(name,bufferPtr) (NULL)

/*
 * Declarations for OS/2 specific functions.
 */

EXTERN int		TclSetSystemEnv _ANSI_ARGS_((const char *variable,
                                                     const char *value));
EXTERN void		TclOS2WatchSocket _ANSI_ARGS_((Tcl_File file,
                                                       int mask));
EXTERN int		TclOS2SocketReady _ANSI_ARGS_((Tcl_File file,
                                                       int mask));
EXTERN void		TclOS2NotifySocket _ANSI_ARGS_((void));
EXTERN void		TclOS2ConvertError _ANSI_ARGS_((ULONG errCode));
EXTERN HMODULE		TclOS2LoadLibrary _ANSI_ARGS_((char *name));
EXTERN HMODULE		TclOS2GetTclInstance _ANSI_ARGS_((void));
EXTERN HAB		TclOS2GetHAB _ANSI_ARGS_((void));
EXTERN BOOL		PMInitialize _ANSI_ARGS_((void));
EXTERN void		PMShutdown _ANSI_ARGS_((void));


/*
 * The following macro defines the character that marks the end of
 * a line.
 */

#define NEWLINE_CHAR '\n'

#endif /* _TCLOS2PORT */
