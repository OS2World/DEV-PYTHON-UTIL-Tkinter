/* 
 * tclOS2Pipe.c -- This file implements the OS/2-specific pipeline exec
 *                 functions.
 *      
 *
 * Copyright (c) 1996 Sun Microsystems, Inc.
 * Copyright (c) 1996-1997 Illya Vaes
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */


#include "tclInt.h"
#include "tclPort.h"

#ifndef __IBMC__
#include <dos.h>
#endif

#define HF_STDIN  0
#define HF_STDOUT 1
#define HF_STDERR 2


/*
 *----------------------------------------------------------------------
 *
 * TclSpawnPipeline --
 *
 *      Given an argc/argv array, instantiate a pipeline of processes
 *      as described by the argv.
 *
 * Results:
 *      The return value is 1 on success, 0 on error
 *
 * Side effects:
 *      Processes and pipes are created.
 *
 *----------------------------------------------------------------------
 */

int
TclSpawnPipeline(interp, pidPtr, numPids, argc, argv, inputFile, outputFile,
        errorFile, intIn, finalOut)
    Tcl_Interp *interp;         /* Interpreter in which to process pipeline. */
    int *pidPtr;                /* Array of pids which are created. */
    int *numPids;               /* Number of pids created. */
    int argc;                   /* Number of entries in argv. */
    char **argv;                /* Array of strings describing commands in
                                 * pipeline plus I/O redirection with <,
                                 * <<, >, etc. argv[argc] must be NULL. */
    Tcl_File inputFile; /* If >=0, gives file id to use as input for
                                 * first process in pipeline (specified via <
                                 * or <@). */
    Tcl_File outputFile;        /* Writable file id for output from last
                                 * command in pipeline (could be file or
                                 * pipe). NULL means use stdout. */
    Tcl_File errorFile; /* Writable file id for error output from all
                                 * commands in the pipeline. NULL means use
                                 * stderr */
    char *intIn;                /* File name for initial input (for Win32s). */
    char *finalOut;             /* File name for final output (for Win32s). */
{
    Tcl_Channel channel;
    UCHAR toBeFound[MAXPATHLEN];    /* Buffer for full path of program */
    UCHAR fullPath[MAXPATHLEN];    /* Buffer for full path of program */
    PSZ path;                      /* Pointer to environment variable value */
    ULONG pid;                     /* Child process ID */
    int firstArg, lastArg;         /* Indices of first and last arguments
				    * in current command. */
    int i, type;
    APIRET rc;
    Tcl_DString buffer;
    char *execName;
    int joinThisError;
    Tcl_File pipeIn = NULL;
    Tcl_File curOutFile = NULL;
    Tcl_File curInFile = inputFile;
    Tcl_File stdInFile = NULL, stdOutFile = NULL, stdErrFile = NULL;
    HFILE stdIn = HF_STDIN, stdOut = HF_STDOUT, stdErr = HF_STDERR;
    HFILE orgIn = -1, orgOut = -1, orgErr = -1;
    HFILE tmp;
    ULONG action;
    BOOL hStdInput, hStdOutput, hStdError;
    char *cmdLine[256];
    int cmdLineCount = 0;
    char *cmdexe = "CMD";
    char *cmdarg = "/C";
#ifdef DEBUG
    printf("curInFile [%x]\n", curInFile);
#endif

    /* Backup original stdin, stdout, stderr by Dup-ing to new handle */
    rc = DosDupHandle(stdIn, &orgIn);
#ifdef DEBUG
    printf("DosDupHandle stdIn %d returns %d orgIn %d\n", stdIn, rc, orgIn);
#endif
    rc = DosDupHandle(stdOut, &orgOut);
#ifdef DEBUG
    printf("DosDupHandle stdOut %d returns %d, orgOut %d\n", stdOut, rc, orgOut);
#endif
    rc = DosDupHandle(stdErr, &orgErr);
#ifdef DEBUG
    printf("DosDupHandle stdErr %x returns %d orgErr %d\n", stdErr, rc, orgErr);
#endif

    /*
     * Fetch the current standard channels.  Note that we have to check the
     * type of each file, since we cannot duplicate some file types.
     */

    channel = Tcl_GetStdChannel(TCL_STDIN);
#ifdef DEBUG
    printf("channel [%x]\n", channel);
#endif
    if (channel) {
        stdInFile = Tcl_GetChannelFile(channel, TCL_READABLE);
#ifdef DEBUG
        printf("stdInFile [%x]\n", stdInFile);
#endif
        if (stdInFile) {
            Tcl_GetFileInfo(stdInFile, &type);
#ifdef DEBUG
            printf("filetype [%x]\n", type);
#endif
            if ((type < TCL_OS2_PIPE) || (type > TCL_OS2_CONSOLE)) {
                stdInFile = NULL;
#ifdef DEBUG
                printf("stdInFile NULL [%x]\n", stdInFile);
#endif
            }
        }
    }
    channel = Tcl_GetStdChannel(TCL_STDOUT);
    if (channel) {
        stdOutFile = Tcl_GetChannelFile(channel, TCL_WRITABLE);
        if (stdOutFile) {
            Tcl_GetFileInfo(stdOutFile, &type);
            if ((type < TCL_OS2_PIPE) || (type > TCL_OS2_CONSOLE)) {
                stdOutFile = NULL;
            }
        }
    }
    channel = Tcl_GetStdChannel(TCL_STDERR);
    if (channel) {
        stdErrFile = Tcl_GetChannelFile(channel, TCL_WRITABLE);
        if (stdErrFile) {
            Tcl_GetFileInfo(stdErrFile, &type);
            if ((type < TCL_OS2_PIPE) || (type > TCL_OS2_CONSOLE)) {
                stdErrFile = NULL;
            }
        }
    }

    /*
     * If the current process has a console attached, let the child inherit
     * it.  Otherwise, create the child as a detached process.
     */

    Tcl_DStringInit(&buffer);

    for (firstArg = 0; firstArg < argc; firstArg = lastArg+1) { 

        cmdLineCount = 0;
        hStdInput = FALSE;
        hStdOutput = FALSE;
        hStdError = FALSE;

        /*
         * Convert the program name into native form.  Also, ensure that
         * the argv entry was copied into the DString.
         */

        Tcl_DStringFree(&buffer);
        execName = Tcl_TranslateFileName(interp, argv[firstArg], &buffer);
        if (execName == NULL) {
            goto error;
        } else if (execName == argv[firstArg]) {
            Tcl_DStringAppend(&buffer, argv[firstArg], -1);
        }
        cmdLine[cmdLineCount] = argv[firstArg];
        cmdLineCount++;

        /*
         * Find the end of the current segment of the pipeline.
         */

	joinThisError = 0;
	for (lastArg = firstArg; lastArg < argc; lastArg++) {
	    if (argv[lastArg][0] == '|') { 
		if (argv[lastArg][1] == 0) { 
		    break;
		}
		if ((argv[lastArg][1] == '&') && (argv[lastArg][2] == 0)) {
		    joinThisError = 1;
		    break;
		}
	    }
	}

       /*
        * Now append the rest of the command line arguments.
        */

       for (i = firstArg + 1; i < lastArg; i++) {
           cmdLine[cmdLineCount] = argv[i];
           cmdLineCount++;
           Tcl_DStringAppend(&buffer, " ", -1);
           Tcl_DStringAppend(&buffer, argv[i], -1);
       }

       /*
        * If this is the last segment, use the specified outputFile.
        * Otherwise create an intermediate pipe.
        */

	if (lastArg == argc) { 
	    curOutFile = outputFile;
	} else {
#ifdef DEBUG
            printf("Creating pipe\n");
#endif
	    if (!TclCreatePipe(&pipeIn, &curOutFile)) {
		Tcl_AppendResult(interp, "couldn't create pipe: ",
			Tcl_PosixError(interp), (char *) NULL);
		goto error;
	    }
	}

        /*
         * In the absence of any redirections, use the standard handles.
         */

#ifdef DEBUG
        printf("curInFile [%x], curOutFile [%x]\n", curInFile, curOutFile);
#endif
        if (!curInFile) {
#ifdef DEBUG
            printf("Making curInFile stdin\n");
#endif
            curInFile = stdInFile;
        }
        if (!curOutFile) {
#ifdef DEBUG
            printf("Making curInFile stderr\n");
#endif
            curOutFile = stdOutFile;
        }
        if (!curOutFile) {
#ifdef DEBUG
            printf("Making curInFile stderr\n");
#endif
            errorFile = stdErrFile;
        }
#ifdef DEBUG
        printf("curInFile [%x], curOutFile [%x]\n", curInFile, curOutFile);
#endif

        /*
         * Duplicate all the handles which will be passed off as stdin, stdout
         * and stderr of the child process. The duplicate handles are set to
         * be inheritable, so the child process can use them.
         */

        if (curInFile) {
            tmp = (HFILE) Tcl_GetFileInfo(curInFile, NULL);
            if (tmp != (HFILE)0) {
                /* Duplicate standard input handle */
                rc = DosDupHandle(tmp, &stdIn);
#ifdef DEBUG
                printf("DosDupHandle curInFile [%x] returned [%d], handle [%d]\n",
                       tmp, rc, stdIn);
#endif
                if (rc != NO_ERROR) {
                    TclOS2ConvertError(rc);
                    Tcl_AppendResult(interp, "couldn't duplicate input handle: ",
                            Tcl_PosixError(interp), (char *) NULL);
                    goto error;
                }
                hStdInput = TRUE;
            }
        }
#ifdef DEBUG
        printf("before curOutFile\n");
#endif
        if (curOutFile) {
            tmp = (HFILE) Tcl_GetFileInfo(curOutFile, NULL);
            if (tmp != (HFILE)1) {
                /* Duplicate standard output handle */
                rc = DosDupHandle(tmp, &stdOut);
#ifdef DEBUG
                printf("DosDupHandle curOutFile [%x] returned [%d], handle [%d]\n",
                       tmp, rc, stdOut);
#endif
                if (rc != NO_ERROR) {
                    TclOS2ConvertError(rc);
                    Tcl_AppendResult(interp, "couldn't duplicate output handle: ",
                            Tcl_PosixError(interp), (char *) NULL);
                    goto error;
                }
                hStdOutput = TRUE;
            }
        }
#ifdef DEBUG
        printf("before joinThisError\n");
#endif
        if (joinThisError) {
            tmp = (HFILE) Tcl_GetFileInfo(curOutFile, NULL);
            if (tmp != (HFILE)1) {
                /* Duplicate standard output handle */
                rc = DosDupHandle(tmp, &stdOut);
#ifdef DEBUG
                printf("DosDupHandle curOutFile [%x] returned [%d], handle [%d]\n",
                       tmp, rc, stdErr);
#endif
                if (rc != NO_ERROR) {
                    TclOS2ConvertError(rc);
                    Tcl_AppendResult(interp, "couldn't duplicate output handle: ",
                            Tcl_PosixError(interp), (char *) NULL);
                    goto error;
                }
                hStdOutput = TRUE;
            }
        } else {
            if (errorFile) {
                tmp = (HFILE) Tcl_GetFileInfo(errorFile, NULL);
                if (tmp != (HFILE)2) {
                    /* Duplicate standard error handle */
                    rc = DosDupHandle(tmp, &stdErr);
#ifdef DEBUG
                    printf("DosDupHandle errorFile [%x] returned [%d], handle [%d]\n",
                           tmp, rc, stdErr);
#endif
                    if (rc != NO_ERROR) {
                        TclOS2ConvertError(rc);
                        Tcl_AppendResult(interp, "couldn't duplicate error handle: ",
                                Tcl_PosixError(interp), (char *) NULL);
                        goto error;
                    }
                    hStdError = TRUE;
                }
            }
        }

       /*
        * If any handle was not set, open the null device instead.
        */

       if (!hStdInput) {
#ifdef DEBUG
           printf("opening NUL as stdin\n");
#endif
           rc = DosOpen((PSZ)"NUL", &tmp, &action, 0, FILE_NORMAL,
                        OPEN_ACTION_CREATE_IF_NEW,
                        OPEN_SHARE_DENYNONE | OPEN_ACCESS_READONLY, NULL);
#ifdef DEBUG
           printf("DosOpen NUL returned %d\n", rc);
#endif
           /* Duplicate standard input handle */
           rc = DosDupHandle(tmp, &stdIn);
#ifdef DEBUG
           printf("DosDupHandle NUL returned %d\n", rc);
#endif
       }
       if (!hStdOutput) {
#ifdef DEBUG
           printf("opening NUL as stdout\n");
#endif
           rc = DosOpen((PSZ)"NUL", &tmp, &action, 0, FILE_NORMAL,
                        OPEN_ACTION_CREATE_IF_NEW,
                        OPEN_SHARE_DENYNONE | OPEN_ACCESS_WRITEONLY, NULL);
#ifdef DEBUG
           printf("DosOpen NUL returned %d\n", rc);
#endif
           /* Duplicate standard output handle */
           rc = DosDupHandle(tmp, &stdOut);
#ifdef DEBUG
           printf("DosDupHandle NUL returned %d\n", rc);
#endif
       }
       if (!hStdError) {
#ifdef DEBUG
           printf("opening NUL as stderr\n");
#endif
           rc = DosOpen((PSZ)"NUL", &tmp, &action, 0, FILE_NORMAL,
                        OPEN_ACTION_CREATE_IF_NEW,
                        OPEN_SHARE_DENYNONE | OPEN_ACCESS_WRITEONLY, NULL);
#ifdef DEBUG
           printf("DosOpen NUL returned %d\n", rc);
#endif
           /* Duplicate standard error handle */
           rc = DosDupHandle(tmp, &stdErr);
#ifdef DEBUG
           printf("DosDupHandle NUL returned %d\n", rc);
#endif
       }

       /*
        * Start the subprocess by invoking the executable directly.  If that
        * fails, then attempt to use cmd.exe.
        */

        cmdLine[cmdLineCount] = NULL;
        cmdLineCount++;
        /* See if we can find the program as a "real" executable */
        rc = DosScanEnv("PATH", &path);
#ifdef DEBUG
        printf("DosScanEnv PATH returned %x (%s)\n", rc, path);
#endif
        strcpy(toBeFound, cmdLine[0]);
        rc = DosSearchPath(SEARCH_CUR_DIRECTORY | SEARCH_IGNORENETERRS, path,
                           toBeFound, (PBYTE)&fullPath, (ULONG) sizeof(fullPath));
        if (rc != NO_ERROR) {
            /* Couldn't find it "literally" */
#ifdef DEBUG
            printf("DosSearchPath %s ERROR %d\n", toBeFound, rc);
#endif
            strcpy(toBeFound, cmdLine[0]);
            strcat(toBeFound, ".EXE");
            rc = DosSearchPath(SEARCH_CUR_DIRECTORY | SEARCH_IGNORENETERRS,
                               path, toBeFound, (PBYTE)&fullPath,
                               (ULONG) sizeof(fullPath));
#ifdef DEBUG
            if (rc != NO_ERROR) {
                printf("DosSearchPath %s ERROR %d\n", toBeFound, rc);
            } else {
                printf("DosSearchPath %s OK: %s\n", toBeFound, fullPath);
            }
#endif
        }
#ifdef DEBUG
          else {
             printf("DosSearchPath %s OK (%s)\n", toBeFound, fullPath);
        }
#endif
        if (rc == NO_ERROR) {
            cmdLine[0] = fullPath;
#ifdef DEBUG
            printf("before spawnv, cmdLine[0] [%s] cmdLine[1] [%s]\n",
                   cmdLine[0], cmdLine[1]);
#endif
/* *MM* created ifdef */
#ifdef __IBMC__
            pid = spawnv(P_WAIT,
                         cmdLine[0], cmdLine);
#else
            pid = spawnv(P_SESSION | P_DEFAULT | P_MINIMIZE | P_BACKGROUND,
                         cmdLine[0], cmdLine);
#endif
#ifdef DEBUG
            printf("after spawnv, pid [%d]\n", pid);
#endif
	}
	if (rc != NO_ERROR || pid == -1) {
            /*
             * DosSearchPath didn't find match in path, have the system command
             * processor (in environment variable COMSPEC) try it in case
             * it's an internal command.
             */
/* *MM* added ifdef - IBMC doesn't support variable length local arrays */
#ifdef __IBMC__
	    char buffer[MAXPATHLEN];
#else
	    char buffer[maxPath];
#endif
	    PSZ comspec = buffer;
	    char *cmdcmdLine[258];
	    int i;

            rc = DosScanEnv("COMSPEC", &comspec);
            if (rc != NO_ERROR) {
#ifdef DEBUG
                printf("DosScanEnv COMSPEC ERROR %d (203=Var not found)\n", rc);
#endif
                cmdcmdLine[0] = cmdexe;
            } else {
#ifdef DEBUG
                printf("DosScanEnv COMSPEC OK: %s\n", (char *)comspec);
#endif
                cmdcmdLine[0] = comspec;
            }
            cmdcmdLine[1] = cmdarg;
            for (i= 0; i<cmdLineCount; i++) {
                cmdcmdLine[i+2] = cmdLine[i];
            }
            /* Spawn, with CMD /C at beginning */
#ifdef DEBUG
            printf("before spawnv cmdcmdLine [%s] [%s] [%s]\n",
                   cmdcmdLine[0], cmdcmdLine[1], cmdcmdLine[2]);
#endif
/* *MM* created ifdef */
#ifdef __IBMC__
            pid = spawnv(P_WAIT,
                         cmdexe, cmdcmdLine);
#else
            pid = spawnv(P_SESSION | P_DEFAULT | P_MINIMIZE | P_BACKGROUND,
                         cmdexe, cmdcmdLine);
#endif
#ifdef DEBUG
            printf("after spawnv, pid [%d]\n", pid);
#endif
	    if (pid == -1) {
                Tcl_AppendResult(interp, "couldn't execute \"", argv[firstArg],
                        "\": ", Tcl_PosixError(interp), (char *) NULL);
                goto error;
	    }
	}
        Tcl_DStringFree(&buffer);

        /*
         * Add the child process to the list of those to be reaped.
         */

#ifdef DEBUG
        printf("numPids: %d\n", *numPids);
#endif
        pidPtr[*numPids] = pid;
        (*numPids)++;
#ifdef DEBUG
        printf("    now: %d\n", *numPids);
#endif

        /*
         * Close off our copies of file descriptors that were set up for
         * this child, then set up the input for the next child.
         */

        if (curInFile && (curInFile != inputFile)
                && (curInFile != stdInFile)) {
            TclCloseFile(curInFile);
        }
        curInFile = pipeIn;
        pipeIn = NULL;

        if (curOutFile && (curOutFile != outputFile)
                && (curOutFile != stdOutFile)) {
            TclCloseFile(curOutFile);
        }
        curOutFile = NULL;

    }

    /* Restore original stdin, stdout, stderr by Dup-ing from new handle */
    stdIn = HF_STDIN; stdOut = HF_STDOUT; stdErr = HF_STDERR;
    rc = DosDupHandle(orgIn, &stdIn);
#ifdef DEBUG
    printf("DosDupHandle orgIn [%x] returned [%d]\n", orgIn, rc);
#endif
    rc = DosDupHandle(orgOut, &stdOut);
#ifdef DEBUG
    printf("DosDupHandle orgOut [%x] returned [%d]\n", orgOut, rc);
#endif
    rc = DosDupHandle(orgErr, &stdErr);
#ifdef DEBUG
    printf("DosDupHandle orgErr [%x] returned [%d]\n", orgErr, rc);
#endif
    rc = DosClose(orgIn);
    rc = DosClose(orgOut);
    rc = DosClose(orgErr);

    return 1;

    /*
     * An error occured, so we need to clean up any open pipes.
     */

error:
    Tcl_DStringFree(&buffer);
    if (pipeIn) {
        TclCloseFile(pipeIn);
    }
    if (curOutFile && (curOutFile != outputFile)
                && (curOutFile != stdOutFile)) {
        TclCloseFile(curOutFile);
    }
    if (curInFile && (curInFile != inputFile)
                && (curInFile != stdInFile)) {
        TclCloseFile(curInFile);
    }
    /* Restore original stdin, stdout, stderr by Dup-ing from new handle */
    stdIn = HF_STDIN; stdOut = HF_STDOUT; stdErr = HF_STDERR;
    rc = DosDupHandle(orgIn, &stdIn);
#ifdef DEBUG
    printf("DosDupHandle orgIn [%x] returned [%d]\n", orgIn, rc);
#endif
    rc = DosDupHandle(orgOut, &stdOut);
#ifdef DEBUG
    printf("DosDupHandle orgOut [%x] returned [%d]\n", orgOut, rc);
#endif
    rc = DosDupHandle(orgErr, &stdErr);
#ifdef DEBUG
    printf("DosDupHandle orgErr [%x] returned [%d]\n", orgErr, rc);
#endif
    rc = DosClose(orgIn);
    rc = DosClose(orgOut);
    rc = DosClose(orgErr);
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TclHasPipes --
 *
 *      Determines if pipes are available.
 *
 * Results:
 *      Returns 1 if pipes are available.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
TclHasPipes(void)
{
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCreatePipe --
 *
 *      Creates an anonymous pipe.
 *
 * Results:
 *      Returns 1 on success, 0 on failure. 
 *
 * Side effects:
 *      Creates a pipe.
 *
 *----------------------------------------------------------------------
 */

int
TclCreatePipe(readPipe, writePipe)
    Tcl_File *readPipe; /* Location to store file handle for
                                 * read side of pipe. */
    Tcl_File *writePipe;        /* Location to store file handle for
                                 * write side of pipe. */
{
    HFILE readHandle, writeHandle;
    APIRET rc;

    rc = DosCreatePipe(&readHandle, &writeHandle, 1024);
#ifdef DEBUG
    printf("DosCreatePipe returned [%x], read [%x], write [%x]\n",
           rc, readHandle, writeHandle);
#endif

    *readPipe = Tcl_GetFile((ClientData)readHandle, TCL_OS2_PIPE);
    *writePipe = Tcl_GetFile((ClientData)writeHandle, TCL_OS2_PIPE);
    return 1;
}
