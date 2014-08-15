/* 
 * tclOS2File.c --
 *
 *      This file contains temporary wrappers around UNIX file handling
 *      functions. These wrappers map the UNIX functions to OS/2 HFILE-style
 *      files, which can be manipulated through the OS/2 console redirection
 *      interfaces.
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 * Copyright (c) 1996-1997 Illya Vaes
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */


#include "tclInt.h"
#include "tclPort.h"

/*
 * Local functions
 */
void      TclOS2DeleteTempFile _ANSI_ARGS_((ClientData name));

/*
 * The variable below caches the name of the current working directory
 * in order to avoid repeated calls to getcwd.  The string is malloc-ed.
 * NULL means the cache needs to be refreshed.
 */

static char *currentDir =  NULL;

/*
 * Mapping of drive numbers to drive letters
 */
static char drives[] = {'0', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
                        'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U',
                        'V', 'W', 'X', 'Y', 'Z'};


/*
 *----------------------------------------------------------------------
 *
 * TclCreateTempFile --
 *
 *	This function opens a unique file with the property that it
 *	will be deleted when its file handle is closed.  The temporary
 *	file is created in the system temporary directory.
 *
 * Results:
 *	Returns a valid C file descriptor, or -1 on failure.
 *
 * Side effects:
 *	Creates a new temporary file.
 *
 *----------------------------------------------------------------------
 */

Tcl_File
TclCreateTempFile(char *contents)
	/* string to write into tempfile, or NULL */
{
    unsigned char *name;
    PSZ tmpVal;
    HFILE handle;
    APIRET rc;
    ULONG timeVal[2], action;
    ULONG result, length;
    Tcl_File tclFile;

    /* Determine TEMP-path */
    rc = DosScanEnv("TEMP", &tmpVal);
#ifdef DEBUG
    printf("DosScanEnv TEMP returned [%d]\n", rc);
#endif
    if ( rc != NO_ERROR ) {
        /* Try TMP instead */
        rc = DosScanEnv("TMP", &tmpVal);
#ifdef DEBUG
        printf("DosScanEnv TMP returned [%d]\n", rc);
#endif
        if ( rc != NO_ERROR ) {
            TclOS2ConvertError(rc);
            return NULL;
        }
    }
    /* Determine unique value from time */
    rc = DosQuerySysInfo(QSV_TIME_LOW, QSV_TIME_HIGH, (PVOID)timeVal,
                         sizeof(timeVal));
    /* Alloc space for name, free in TclOS2DeleteTempFile */
    name = (unsigned char *)ckalloc(maxPath);
    if (name == (unsigned char *)NULL) return NULL;
    /* Add unique name to path */
    sprintf(name, "%s\\Tcl%04hx.TMP", tmpVal, (SHORT)timeVal[0]);

    rc = DosOpen(name, &handle, &action, 0, FILE_NORMAL,
                 OPEN_ACTION_CREATE_IF_NEW | OPEN_ACTION_REPLACE_IF_EXISTS,
                 OPEN_SHARE_DENYNONE | OPEN_ACCESS_READWRITE, NULL);

#ifdef DEBUG
    printf("TclCreateTempFile: DosOpen [%s] handle [%x] rc [%d]\n",
           name, handle, rc);
#endif
    if (rc != NO_ERROR) {
        goto error;
    }

    /*
     * Write the file out, doing line translations on the way.
     */

    if (contents != NULL) {
        char *p;

        for (p = contents; *p != '\0'; p++) {
            if (*p == '\n') {
                length = p - contents;
                if (length > 0) {
                    rc = DosWrite(handle, (PVOID)contents, length, &result);
#ifdef DEBUG
                    printf("DosWrite handle %x [%s] returned [%d]\n",
                           handle, contents, rc);
#endif
                    if (rc != NO_ERROR) {
                        goto error;
                    }
                }
                if (DosWrite(handle, "\r\n", 2, &result) != NO_ERROR) {
                    goto error;
                }
                contents = p+1;
            }
        }
        length = p - contents;
        if (length > 0) {
            rc = DosWrite(handle, (PVOID)contents, length, &result);
#ifdef DEBUG
            printf("DosWrite handle %x [%s] returned [%d]\n",
                   handle, contents, rc);
#endif
            if (rc != NO_ERROR) {
                goto error;
            }
        }
    }
    rc = DosSetFilePtr(handle, 0, FILE_BEGIN, &result);
#ifdef DEBUG
    printf("DosSetFilePtr handle [%x] returned [%d]\n", handle, rc);
#endif
    if (rc != NO_ERROR) {
        goto error;
    }
    tclFile = Tcl_GetFile((ClientData) handle, TCL_OS2_FILE);
    /* Set up deletion procedure for the file */
    Tcl_SetNotifierData(tclFile, (Tcl_FileFreeProc *)TclOS2DeleteTempFile,
                        (ClientData)name);

    return tclFile;

  error:
    TclOS2ConvertError(rc);
    rc = DosClose(handle);
    DosDelete(name);
    ckfree((char *)name);;
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TclOpenFile --
 *
 *      This function wraps the normal system open() to ensure that
 *      files are opened with the _O_NOINHERIT flag set.
 *
 * Results:
 *      Same as open().
 *
 * Side effects:
 *      Same as open().
 *
 *----------------------------------------------------------------------
 */

Tcl_File
TclOpenFile(path, mode)
    char *path;
    int mode;
{
    HFILE handle;
    ULONG accessMode, createMode, flags, exist;
    APIRET rc;

    /*
     * Map the access bits to the OS/2 access mode.
     */

    switch (mode & (O_RDONLY | O_WRONLY | O_RDWR)) {
        case O_RDONLY:
           accessMode = OPEN_ACCESS_READONLY;
           break;
       case O_WRONLY:
           accessMode = OPEN_ACCESS_WRITEONLY;
           break;
       case O_RDWR:
           accessMode = OPEN_ACCESS_READWRITE;
           break;
       default:
           TclOS2ConvertError(ERROR_INVALID_FUNCTION);
           return NULL;
    }
    /*
     * Map the creation flags to the OS/2 open mode.
     */

    switch (mode & (O_CREAT | O_EXCL | O_TRUNC)) {
        case (O_CREAT | O_EXCL):
        case (O_CREAT | O_EXCL | O_TRUNC):
            createMode = OPEN_ACTION_CREATE_IF_NEW | OPEN_ACTION_REPLACE_IF_EXISTS;
            break;
        case (O_CREAT | O_TRUNC):
            createMode = OPEN_ACTION_CREATE_IF_NEW | OPEN_ACTION_REPLACE_IF_EXISTS;
            break;
        case O_CREAT:
            createMode = OPEN_ACTION_CREATE_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS;
            break;
        case O_TRUNC:
        case (O_TRUNC | O_EXCL):
            createMode = OPEN_ACTION_CREATE_IF_NEW | OPEN_ACTION_REPLACE_IF_EXISTS;
            break;
        default:
            createMode = OPEN_ACTION_FAIL_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS;
            break;
    }

    /*
     * If the file is not being created, use the existing file attributes.
     */

    flags = 0;
    if (!(mode & O_CREAT)) {
        FILESTATUS3 infoBuf;

        rc = DosQueryPathInfo(path, FIL_STANDARD, &infoBuf, sizeof(infoBuf));
        if (rc == NO_ERROR) {
            flags = infoBuf.attrFile;
        } else {
            flags = 0;
        }
    }


   /*
    * Set up the attributes so this file is not inherited by child processes.
    */

   accessMode |= OPEN_FLAGS_NOINHERIT;

   /*
    * Set up the file sharing mode.  We want to allow simultaneous access.
    */

   accessMode |= OPEN_SHARE_DENYNONE;

   /*
    * Now we get to create the file.
    */

   rc = DosOpen(path, &handle, &exist, 0, flags, createMode, accessMode,
                 (PEAOP2)NULL);
#ifdef DEBUG
   printf("TclOpenFile: DosOpen [%s] returns [%d]\n", path, rc);
#endif
   if (rc != NO_ERROR) {
       ULONG err = 0;

       switch (rc) {
           case ERROR_FILE_NOT_FOUND:
           case ERROR_PATH_NOT_FOUND:
               err = ERROR_FILE_NOT_FOUND;
               break;
           case ERROR_ACCESS_DENIED:
           case ERROR_INVALID_ACCESS:
           case ERROR_SHARING_VIOLATION:
           case ERROR_CANNOT_MAKE:
               err = (mode & O_CREAT) ? ERROR_FILE_EXISTS
                                      : ERROR_FILE_NOT_FOUND;
               break;
       }
       TclOS2ConvertError(err);
       return NULL;
    }

    return Tcl_GetFile((ClientData) handle, TCL_OS2_FILE);
}

/*
 *----------------------------------------------------------------------
 *
 * TclCloseFile --
 *
 *      Closes a file on OS/2.
 *
 * Results:
 *      0 on success, -1 on failure.
 *
 * Side effects:
 *      The file is closed.
 *
 *----------------------------------------------------------------------
 */

int
TclCloseFile(file)
    Tcl_File file;      /* The file to close. */
{
    HFILE handle;
    int type;
    APIRET rc;

    handle = (HFILE) Tcl_GetFileInfo(file, &type);

    if (type == TCL_OS2_FILE || type == TCL_OS2_PIPE) {
        rc = DosClose(handle);
#ifdef DEBUG
        printf("TclCloseFile: DosClose handle [%x] returned [%d]\n",
               handle, rc);
#endif
        if (rc != NO_ERROR) {
            TclOS2ConvertError(rc);
            return -1;
        }
    } else {
#ifdef DEBUG
        printf("TclCloseFile: unexpected file type %x for handle %x\n",
               type, handle);
#endif
        panic("Tcl_CloseFile: unexpected file type");
    }

    Tcl_FreeFile(file);
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TclSeekFile --
 *
 *      Sets the file pointer on a file indicated by the file.
 *
 * Results:
 *      The new position at which the file pointer is after it was
 *      moved, or -1 on failure.
 *
 * Side effects:
 *      May move the position at which subsequent operations on the
 *      file access it.
 *
 *----------------------------------------------------------------------
 */

int
TclSeekFile(file, offset, whence)
    Tcl_File file;      /* File to seek on. */
    int offset;                 /* How much to move. */
    int whence;                 /* Relative to where? */
{
    ULONG moveMethod;
    ULONG newPos;
    HFILE handle;
    int type;
    APIRET rc;

    handle = (HFILE) Tcl_GetFileInfo(file, &type);
    if (type != TCL_OS2_FILE) {
        panic("TclSeekFile: unexpected file type");
    }

    if (whence == SEEK_SET) {
        moveMethod = FILE_BEGIN;
    } else if (whence == SEEK_CUR) {
        moveMethod = FILE_CURRENT;
    } else {
        moveMethod = FILE_END;
    }

    rc = DosSetFilePtr(handle, offset, moveMethod, &newPos);
#ifdef DEBUG
    printf("DosSetFilePtr handle [%x] returned [%d]\n", handle, rc);
#endif
    if (rc != NO_ERROR) {
        TclOS2ConvertError(rc);
        return -1;
    }
    return newPos;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FindExecutable --
 *
 *	This procedure computes the absolute path name of the current
 *	application, given its argv[0] value.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The variable tclExecutableName gets filled in with the file
 *	name for the application, if we figured it out.  If we couldn't
 *	figure it out, Tcl_FindExecutable is set to NULL.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_FindExecutable(argv0)
    char *argv0;		/* The value of the application's argv[0]. */
{
    char *p;

    if (tclExecutableName != NULL) {
	ckfree(tclExecutableName);
	tclExecutableName = NULL;
    }

    tclExecutableName = (char *) ckalloc((unsigned) (strlen(argv0) + 1));
    strcpy(tclExecutableName, argv0);
    /* Convert backslahes to slashes */
    for (p= tclExecutableName; *p != '\0'; p++) {
        if (*p == '\\') *p = '/';
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclMatchFiles --
 *
 *      This routine is used by the globbing code to search a
 *      directory for all files which match a given pattern.
 *
 * Results:
 *      If the tail argument is NULL, then the matching files are
 *      added to the interp->result.  Otherwise, TclDoGlob is called
 *      recursively for each matching subdirectory.  The return value
 *      is a standard Tcl result indicating whether an error occurred
 *      in globbing.
 *
 * Side effects:
 *      None.
 *
 *---------------------------------------------------------------------- */

int
TclMatchFiles(interp, separators, dirPtr, pattern, tail)
    Tcl_Interp *interp;         /* Interpreter to receive results. */
    char *separators;           /* Directory separators to pass to TclDoGlob. */
    Tcl_DString *dirPtr;        /* Contains path to directory to search. */
    char *pattern;              /* Pattern to match against. */
    char *tail;                 /* Pointer to end of pattern.  Tail must
                                 * point to a location in pattern. */
{
    char drivePattern[4] = "?:\\";
    char *newPattern, *p, *dir, *root, c;
    int length, matchDotFiles;
    int result = TCL_OK;
    int baseLength = Tcl_DStringLength(dirPtr);
    Tcl_DString buffer;
    ULONG volFlags;
    HDIR handle;
    FILESTATUS3 infoBuf;
    FILEFINDBUF3 data;
    ULONG filesAtATime = 1;
    APIRET rc;
    ULONG diskNum = 3;		/* Assume C: for errors */
    BYTE fsBuf[1024];		/* Info about file system */
    ULONG bufSize;

#ifdef DEBUG
    printf("TclMatchFiles path [%s], pat [%s]\n", Tcl_DStringValue(dirPtr),
           pattern);
#endif

    /*
     * Convert the path to normalized form since some interfaces only
     * accept backslashes.  Also, ensure that the directory ends with a
     * separator character.
     */

    Tcl_DStringInit(&buffer);
    if (baseLength == 0) {
        Tcl_DStringAppend(&buffer, ".", 1);
    } else {
        Tcl_DStringAppend(&buffer, Tcl_DStringValue(dirPtr),
                Tcl_DStringLength(dirPtr));
    }
    for (p = Tcl_DStringValue(&buffer); *p != '\0'; p++) {
        if (*p == '/') {
            *p = '\\';
        }
    }
/*
    p--;
    if (*p != '\\' && (strcmp(Tcl_DStringValue(&buffer), ".") != 0)) {
        Tcl_DStringAppend(&buffer, "\\", 1);
        p++;
    }
*/
    p--;
    /*
     * DosQueryPathInfo can only handle a trailing (back)slash for the root
     * of a drive, so cut it off in other case.
     */
    if ((*p == '\\') && (*(p-1) != ':') && (*p != '.')) {
        Tcl_DStringSetLength(&buffer, Tcl_DStringLength(&buffer)-1);
        p--;
    }
    /*
     * In cases of eg. "c:filespec", we need to put the current dir for that
     * disk after the drive specification.
     */
    if (*p == ':') {
        char wd[256];
        ULONG len = 256;
        ULONG drive;

        if (*(p-1) > 'Z') drive = *(p-1) - 'a' + 1;
        else drive = *(p-1) - 'A' + 1;
        rc = DosQueryCurrentDir(drive, (PBYTE)wd, &len);
#ifdef DEBUG
        printf("DosQueryCurrentDir drive %c (%d) returns %d [%s] (len %d)\n",
               *(p-1), drive, rc, wd, len);
#endif
        if (rc == NO_ERROR) {
            Tcl_DStringAppend(&buffer, "\\", 1);
            len = strlen(wd);
            Tcl_DStringAppend(&buffer, wd, len);
            p += len+1;
        }
#ifdef DEBUG
        printf("    *p now %c\n", *p);
#endif
    }

    /*
     * First verify that the specified path is actually a directory.
     */

    dir = Tcl_DStringValue(&buffer);
    rc = DosQueryPathInfo(dir, FIL_STANDARD, &infoBuf, sizeof(infoBuf));
#ifdef DEBUG
    printf("DosQueryPathInfo [%s] returned [%d]\n", dir, rc);
    fflush(stdout);
#endif
    if ( (rc != NO_ERROR) || ((infoBuf.attrFile & FILE_DIRECTORY) == 0)) {
        Tcl_DStringFree(&buffer);
        return TCL_OK;
    }

    if (*p != '\\') {
        Tcl_DStringAppend(&buffer, "\\", 1);
    }
    dir = Tcl_DStringValue(&buffer);

    /*
     * Next check the volume information for the directory to see whether
     * comparisons should be case sensitive or not.  If the root is null, then
     * we use the root of the current directory.  If the root is just a drive
     * specifier, we use the root directory of the given drive.
     * There's no API for determining case sensitivity and preservation (that
     * I've found) perse. We can determine the File System Driver though, and
     * assume correct values for some file systems we know, eg. FAT, HPFS,
     * NTFS, ext2fs.
     */

    switch (Tcl_GetPathType(dir)) {
        case TCL_PATH_RELATIVE: {
            ULONG logical;
            /* Determine current drive */
            DosQueryCurrentDisk(&diskNum, &logical);
#ifdef DEBUG
            printf("TCL_PATH_RELATIVE, disk %d\n", diskNum);
#endif

            break;
        }
        case TCL_PATH_VOLUME_RELATIVE: {
            ULONG logical;
            /* Determine current drive */
            DosQueryCurrentDisk(&diskNum, &logical);
#ifdef DEBUG
            printf("TCL_PATH_VOLUME_RELATIVE, disk %d\n", diskNum);
#endif

            if (*dir == '\\') {
                root = NULL;
            } else {
                root = drivePattern;
                *root = *dir;
            }
            break;
        }
        case TCL_PATH_ABSOLUTE:
            /* Use given drive */
            diskNum = (ULONG) dir[0] - 'A' + 1;
            if (dir[0] >= 'a') {
                diskNum -= ('a' - 'A');
            }
#ifdef DEBUG
            printf("TCL_PATH_ABSOLUTE, disk %d\n", diskNum);
#endif

            if (dir[1] == ':') {
                root = drivePattern;
                *root = *dir;
            } else if (dir[1] == '\\') {
                p = strchr(dir+2, '\\');
                p = strchr(p+1, '\\');
                p++;
                c = *p;
                *p = 0;
                *p = c;
            }
            break;
    }
    /* Now determine file system driver name and hack the case stuff */
    bufSize = sizeof(fsBuf);
    rc = DosQueryFSAttach(NULL, diskNum, FSAIL_DRVNUMBER, ((PFSQBUFFER2)fsBuf),
                          &bufSize);
    if (rc != NO_ERROR) {
        /* Error, assume FAT */
#ifdef DEBUG
        printf("DosQueryFSAttach %d ERROR %d (bufsize %d)\n", diskNum, rc,
               bufSize);
#endif
        volFlags = 0;
    } else {
        USHORT cbName = ((PFSQBUFFER2) fsBuf)->cbName;
#ifdef DEBUG
        printf("DosQueryFSAttach %d OK, szN [%s], szFSDN [%s] (bufsize %d)\n",
               diskNum, ((PFSQBUFFER2)fsBuf)->szName,
               ((PFSQBUFFER2)(fsBuf+cbName))->szFSDName, bufSize);
#endif
        if (strcmp(((PFSQBUFFER2)(fsBuf+cbName))->szFSDName, "FAT") == 0) {
            volFlags = 0;
        } else
        if (strcmp(((PFSQBUFFER2)(fsBuf+cbName))->szFSDName, "HPFS") == 0) {
            volFlags = FS_CASE_IS_PRESERVED;
        } else
        if (strcmp(((PFSQBUFFER2)(fsBuf+cbName))->szFSDName, "NFS") == 0) {
            volFlags = FS_CASE_SENSITIVE | FS_CASE_IS_PRESERVED;
        } else
        if (strcmp(((PFSQBUFFER2)(fsBuf+cbName))->szFSDName, "EXT2FS") == 0) {
            volFlags = FS_CASE_SENSITIVE | FS_CASE_IS_PRESERVED;
        } else
        if (strcmp(((PFSQBUFFER2)(fsBuf+cbName))->szFSDName, "VINES") == 0) {
            volFlags = 0;
        } else
        if (strcmp(((PFSQBUFFER2)(fsBuf+cbName))->szFSDName, "NTFS") == 0) {
            volFlags = FS_CASE_IS_PRESERVED;
        } else {
            volFlags = 0;
        }
    }

    /*
     * If the volume is not case sensitive, then we need to convert the pattern
     * to lower case.
     */

    length = tail - pattern;
    newPattern = ckalloc(length+1);
    if (volFlags & FS_CASE_SENSITIVE) {
        strncpy(newPattern, pattern, length);
        newPattern[length] = '\0';
    } else {
        char *src, *dest;
        for (src = pattern, dest = newPattern; src < tail; src++, dest++) {
            *dest = (char) tolower(*src);
        }
        *dest = '\0';
    }

    /*
     * We need to check all files in the directory, so append a *
     * to the path. Not "*.*".
     */


    dir = Tcl_DStringAppend(&buffer, "*", 3);

    /*
     * Now open the directory for reading and iterate over the contents.
     */

    handle = HDIR_SYSTEM;
    rc = DosFindFirst(dir, &handle, FILE_NORMAL | FILE_DIRECTORY, &data, sizeof(data),
                       &filesAtATime, FIL_STANDARD);
#ifdef DEBUG
    printf("DosFindFirst %s returns %x (%s)\n", dir, rc, data.achName);
#endif
    Tcl_DStringFree(&buffer);

    if (rc != NO_ERROR) {
        TclOS2ConvertError(rc);
        Tcl_ResetResult(interp);
        Tcl_AppendResult(interp, "couldn't read directory \"",
                dirPtr->string, "\": ", Tcl_PosixError(interp), (char *) NULL);
        ckfree(newPattern);
        return TCL_ERROR;
    }

    /*
     * Clean up the tail pointer.  Leave the tail pointing to the
     * first character after the path separator or NULL.
     */

    if (*tail == '\\') {
        tail++;
    }
    if (*tail == '\0') {
        tail = NULL;
    } else {
        tail++;
    }

    /*
     * Check to see if the pattern needs to compare with dot files.
     */

    if ((newPattern[0] == '.')
            || ((pattern[0] == '\\') && (pattern[1] == '.'))) {
        matchDotFiles = 1;
    } else {
        matchDotFiles = 0;
    }

    /*
     * Now iterate over all of the files in the directory.
     */

    Tcl_DStringInit(&buffer);
#ifdef DEBUG
    for ( rc = NO_ERROR;
          rc == NO_ERROR;
          printf("DosFindNext returns %x (%s)\n",
                 rc = DosFindNext(handle, &data, sizeof(data), &filesAtATime),
                 data.achName)) {
#else
    for (   rc = NO_ERROR;
            rc == NO_ERROR;
            rc = DosFindNext(handle, &data, sizeof(data), &filesAtATime)) {
#endif
        char *matchResult;

        /*
         * Ignore hidden files.
         */

        if ((data.attrFile & FILE_HIDDEN)
                || (!matchDotFiles && (data.achName[0] == '.'))) {
            continue;
        }

        /*
         * Check to see if the file matches the pattern.  If the volume is not
         * case sensitive, we need to convert the file name to lower case.  If
         * the volume also doesn't preserve case, then we return the lower case
         * form of the name, otherwise we return the system form.
         */

        matchResult = NULL;
        if (!(volFlags & FS_CASE_SENSITIVE)) {
            Tcl_DStringSetLength(&buffer, 0);
            Tcl_DStringAppend(&buffer, data.achName, -1);
            for (p = buffer.string; *p != '\0'; p++) {
                *p = (char) tolower(*p);
            }
            if (Tcl_StringMatch(buffer.string, newPattern)) {
                if (volFlags & FS_CASE_IS_PRESERVED) {
                    matchResult = data.achName;
                } else {
                    matchResult = buffer.string;
                }
            }
        } else {
            if (Tcl_StringMatch(data.achName, newPattern)) {
                matchResult = data.achName;
            }
        }

        if (matchResult == NULL) {
            continue;
        }

        /*
         * If the file matches, then we need to process the remainder of the
         * path.  If there are more characters to process, then ensure matching
         * files are directories and call TclDoGlob. Otherwise, just add the
         * file to the result.
         */

        Tcl_DStringSetLength(dirPtr, baseLength);
        Tcl_DStringAppend(dirPtr, matchResult, -1);
        if (tail == NULL) {
            Tcl_AppendElement(interp, dirPtr->string);
        } else {
            if ((DosQueryPathInfo(dirPtr->string, FIL_STANDARD, &infoBuf,
                    sizeof(infoBuf)) == NO_ERROR) &&
                    (infoBuf.attrFile & FILE_DIRECTORY)) {
                Tcl_DStringAppend(dirPtr, "/", 1);
                result = TclDoGlob(interp, separators, dirPtr, tail);
                if (result != TCL_OK) {
                    break;
                }
            }
        }
    }

    Tcl_DStringFree(&buffer);
    DosFindClose(handle);
    ckfree(newPattern);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetStdHandles --
 *
 *      This function returns the file handles for standard I/O.
 *
 * Results:
 *      Sets the arguments to the standard file handles.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
TclGetStdHandles(stdinPtr, stdoutPtr, stderrPtr)
    Tcl_File *stdinPtr;
    Tcl_File *stdoutPtr;
    Tcl_File *stderrPtr;
{
    HFILE hStdInput = (HFILE) 0;
    HFILE hStdOutput = (HFILE) 1;
    HFILE hStdError = (HFILE) 2;

    *stdinPtr = Tcl_GetFile((ClientData) hStdInput, TCL_OS2_FILE);
    *stdoutPtr = Tcl_GetFile((ClientData) hStdOutput, TCL_OS2_FILE);
    *stderrPtr = Tcl_GetFile((ClientData) hStdError, TCL_OS2_FILE);
}

/*
 *----------------------------------------------------------------------
 *
 * TclChdir --
 *
 *      Change the current working directory.
 *
 * Results:
 *      The result is a standard Tcl result.  If an error occurs and
 *      interp isn't NULL, an error message is left in interp->result.
 *
 * Side effects:
 *      The working directory for this application is changed.  Also
 *      the cache maintained used by TclGetCwd is deallocated and
 *      set to NULL.
 *
 *----------------------------------------------------------------------
 */

int
TclChdir(interp, dirName)
    Tcl_Interp *interp;         /* If non NULL, used for error reporting. */
    char *dirName;              /* Path to new working directory. */
{
    APIRET rc;

#ifdef DEBUG
    printf("TclChDir %s\n", dirName);
#endif
    if (currentDir != NULL) {
        ckfree(currentDir);
        currentDir = NULL;
    }
    /* Set drive, if present */
    if (dirName[1] == ':') {
        ULONG ulDriveNum;

        /* Determine disk number */
        for (ulDriveNum=1;
             ulDriveNum < 27 && strnicmp(&drives[ulDriveNum], dirName, 1) != 0;
             ulDriveNum++)
            /* do nothing */;
        if (ulDriveNum == 27) {
            if (interp != NULL) {
                Tcl_AppendResult(interp, "invalid drive specification \'",
                        dirName[0], "\': ",
                        Tcl_PosixError(interp), (char *) NULL);
            }
            return TCL_ERROR;
        }
        rc = DosSetDefaultDisk(ulDriveNum);
#ifdef DEBUG
        printf("DosSetDefaultDisk %c (%d) returned [%d]\n", dirName[0],
               ulDriveNum, rc);
#endif
        dirName += 2;
    }
    /* Set directory if specified (not just a drive spec) */
    if (strcmp(dirName, "") != 0) {
        rc = DosSetCurrentDir(dirName);
#ifdef DEBUG
        printf("DosSetCurrentDir [%s] returned [%d]\n", dirName, rc);
#endif
        if (rc != NO_ERROR) {
            TclOS2ConvertError(rc);
            if (interp != NULL) {
                Tcl_AppendResult(interp,
                        "couldn't change working directory to \"",
                        dirName, "\": ", Tcl_PosixError(interp), (char *) NULL);
            }
            return TCL_ERROR;
        }
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetCwd --
 *
 *      Return the path name of the current working directory.
 *
 * Results:
 *      The result is the full path name of the current working
 *      directory, or NULL if an error occurred while figuring it
 *      out.  If an error occurs and interp isn't NULL, an error
 *      message is left in interp->result.
 *
 * Side effects:
 *      The path name is cached to avoid having to recompute it
 *      on future calls;  if it is already cached, the cached
 *      value is returned.
 *
 *----------------------------------------------------------------------
 */

char *
TclGetCwd(interp)
    Tcl_Interp *interp;         /* If non NULL, used for error reporting. */
{
    char buffer[MAXPATHLEN+1], *bufPtr;
    ULONG length = MAXPATHLEN+1;
    ULONG ulDriveNum = 0;	/* A=1, B=2, ... */
    ULONG ulDriveMap = 0;	/* Bitmap of valid drives */
    APIRET rc;

#ifdef DEBUG
    printf("TclGetCwd\n");
#endif
    if (currentDir == NULL) {
        rc = DosQueryCurrentDisk(&ulDriveNum, &ulDriveMap);
#ifdef DEBUG
        printf("DosQueryCurrentDir returned [%d], drive %d (%c)\n", rc,
               ulDriveNum, drives[ulDriveNum]);
#endif
        if (rc != NO_ERROR) {
            TclOS2ConvertError(rc);
            if (interp != NULL) {
                Tcl_AppendResult(interp,
                        "error getting default drive: ",
                        Tcl_PosixError(interp), (char *) NULL);
            }
            return NULL;
        }
        rc = DosQueryCurrentDir(0, buffer, &length);
#ifdef DEBUG
        printf("DosQueryCurrentDir returned [%d], dir %s\n", rc, buffer);
#endif
        if (rc != NO_ERROR) {
            TclOS2ConvertError(rc);
            if (interp != NULL) {
                if (length > MAXPATHLEN+1) {
                    interp->result = "working directory name is too long";
                } else {
                    Tcl_AppendResult(interp,
                            "error getting working directory name: ",
                            Tcl_PosixError(interp), (char *) NULL);
                }
            }
            return NULL;
        }
        bufPtr = buffer;
        /* OS/2 returns pwd *without* leading slash!, so add it */
        currentDir = (char *) ckalloc((unsigned) (strlen(bufPtr) + 4));
        currentDir[0] = drives[ulDriveNum];
        currentDir[1] = ':';
        currentDir[2] = '/';
        strcpy(currentDir+3, bufPtr);

        /*
         * Convert to forward slashes for easier use in scripts.
         */

        for (bufPtr = currentDir; *bufPtr != '\0'; bufPtr++) {
            if (*bufPtr == '\\') {
                *bufPtr = '/';
            }
        }
    }
    return currentDir;
}

/*
 *----------------------------------------------------------------------
 *
 * TclOS2DeleteTempFile --
 *
 *      Callback for deleting a temporary file when closing it.
 *
 * Results:
 *      The named file is deleted.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
TclOS2DeleteTempFile(name)
    ClientData name;         /* Name of file to be deleted. */
{
    APIRET rc;
#ifdef DEBUG
    printf("TclOS2DeleteTempFile %s\n", (PSZ)name);
#endif
    rc = DosDelete((PSZ)name);
#ifdef DEBUG
    if (rc != NO_ERROR) {
        printf("    DosDelete ERROR %d\n", rc);
    } else {
        printf("    DosDelete OK\n");
    }
#endif
    ckfree((char *)name);
}
