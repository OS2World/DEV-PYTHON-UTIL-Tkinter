/* 
 * tclOS2Main.c --
 *
 *	Main program for Tcl shells and other Tcl-based applications.
 *
 * Copyright (c) 1988-1994 The Regents of the University of California.
 * Copyright (c) 1994 Sun Microsystems, Inc.
 * Copyright (c) 1996-1997 Illya Vaes
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include        "tclInt.h"      /* Internal definitions for Tcl. */
#include        "tclPort.h"     /* Portability features for Tcl. */
#include "tclOS2Console.h"

static Tcl_Interp *interp;	/* Interpreter for application. */
static Tcl_DString command;	/* Used to buffer incomplete commands being
				 * read from stdin. */

#ifdef TCL_MEM_DEBUG
static char dumpFile[100];	/* Records where to dump memory allocation
				 * information. */
static int quitFlag = 0;	/* 1 means the "checkmem" command was
				 * invoked, so the application should quit
				 * and dump memory allocation information. */
#endif

/*
 * Forward references for procedures defined later in this file:
 */

static void TclOS2Panic TCL_VARARGS(char *,format);
#ifdef TCL_MEM_DEBUG
static int		CheckmemCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char *argv[]));
#endif


/*
 *----------------------------------------------------------------------
 *
 * Tcl_Main --
 *
 *	Main program for tclsh and most other Tcl-based applications.
 *
 * Results:
 *	None. This procedure never returns (it exits the process when
 *	it's done.
 *
 * Side effects:
 *	This procedure initializes the Tcl world and then starts
 *	interpreting commands;  almost anything could happen, depending
 *	on the script being interpreted.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_Main(argc, argv, appInitProc)
    int argc;				/* Number of arguments. */
    char **argv;			/* Array of argument strings. */
    Tcl_AppInitProc *appInitProc;	/* Application-specific initialization
					 * procedure to call after most
					 * initialization but before starting
					 * to execute commands. */
{
    char cbuf[1000], *args, *fileName;
    int code, gotPartial;
#ifdef CLI_VERSION
    char *cmd;
    int length;
    int tty = 1;
#endif
    int exitCode = 0;
    Tcl_Channel inChannel, outChannel, errChannel;
    HWND hTerminal;

    /* Initialize PM */
    if (!PMInitialize()) {
        return;
    }
    /* Set Panic procedure */
    Tcl_SetPanicProc(TclOS2Panic);
#ifndef CLI_VERSION
    /* Register "Terminal" class */
    if (!RegisterTerminalClass(TclOS2GetHAB())) {
        WinMessageBox(HWND_DESKTOP, NULLHANDLE, "Cannot register Terminal",
                      "Tclsh", 0, MB_OK | MB_ERROR | MB_APPLMODAL);
        /* Don't forget to cleanly exit PM */
        PMShutdown();
        return;
    }
#endif

    Tcl_FindExecutable(argv[0]);

    interp = Tcl_CreateInterp();
#ifdef TCL_MEM_DEBUG
    Tcl_InitMemory(interp);
    Tcl_CreateCommand(interp, "checkmem", CheckmemCmd, (ClientData) 0,
	    (Tcl_CmdDeleteProc *) NULL);
#endif

    /*
     * Make command-line arguments available in the Tcl variables "argc"
     * and "argv".  If the first argument doesn't start with a "-" then
     * strip it off and use it as the name of a script file to process.
     */

    fileName = NULL;
    if ((argc > 1) && (argv[1][0] != '-')) {
	fileName = argv[1];
	argc--;
	argv++;
    }
    args = Tcl_Merge(argc-1, argv+1);
    Tcl_SetVar(interp, "argv", args, TCL_GLOBAL_ONLY);
    ckfree(args);
    sprintf(cbuf, "%d", argc-1);
    Tcl_SetVar(interp, "argc", cbuf, TCL_GLOBAL_ONLY);
    Tcl_SetVar(interp, "argv0", (fileName != NULL) ? fileName : argv[0],
	    TCL_GLOBAL_ONLY);

    /*
     * Set the "tcl_interactive" variable.
     */

    Tcl_SetVar(interp, "tcl_interactive",
	    (fileName == NULL) ? "1" : "0", TCL_GLOBAL_ONLY);

    /*
     * Invoke application-specific initialization.
     */

    if ((*appInitProc)(interp) != TCL_OK) {
#ifndef CLI_VERSION
	sprintf(cbuf, "application-specific initialization failed: %s\n",
		interp->result);
        WinMessageBox(HWND_DESKTOP, NULLHANDLE, cbuf, "Tclsh",
                      0, MB_OK | MB_ICONEXCLAMATION | MB_APPLMODAL);
#else
        fprintf(stderr, "application-specific initialization failed: %s\n",
                interp->result);
#endif
    }

    /*
     * If a script file was specified then just source that file
     * and quit.
     */

    if (fileName != NULL) {
	code = Tcl_EvalFile(interp, fileName);
	if (code != TCL_OK) {
#ifndef CLI_VERSION
	    sprintf(cbuf, "%s\n", interp->result);
            WinMessageBox(HWND_DESKTOP, NULLHANDLE, cbuf, "Tclsh", 0,
                          MB_OK | MB_ERROR | MB_APPLMODAL);
#else
            fprintf(stderr, "%s\n", interp->result);
#endif
	    exitCode = 1;
	}
	goto done;
    }

    /*
     * We're running interactively.  Source a user-specific startup
     * file if the application specified one and if the file exists.
     */

    fileName = Tcl_GetVar(interp, "tcl_rcFileName", TCL_GLOBAL_ONLY);

    if (fileName != NULL) {
	Tcl_DString buffer;
	char *fullName;
	FILE *f;

	fullName = Tcl_TildeSubst(interp, fileName, &buffer);
	if (fullName == NULL) {
#ifndef CLI_VERSION
	    sprintf(cbuf, "%s\n", interp->result);
            WinMessageBox(HWND_DESKTOP, NULLHANDLE, cbuf, "Tclsh", 0,
                          MB_OK | MB_ICONEXCLAMATION | MB_APPLMODAL);
#else
            fprintf(stderr, "%s\n", interp->result);
#endif
	} else {
	    f = fopen(fullName, "r");
	    if (f != NULL) {
		code = Tcl_EvalFile(interp, fullName);
		if (code != TCL_OK) {
#ifndef CLI_VERSION
	            sprintf(cbuf, "%s\n", interp->result);
                    WinMessageBox(HWND_DESKTOP, NULLHANDLE, cbuf, "Tclsh", 0,
                                  MB_OK | MB_ICONEXCLAMATION | MB_APPLMODAL);
#else
                    fprintf(stderr, "%s\n", interp->result);
#endif
		}
		fclose(f);
	    }
	}
	Tcl_DStringFree(&buffer);
    }

    /*
     * Create and display the console window.
     */

#ifndef CLI_VERSION
    hTerminal = CreateTerminal(TclOS2GetHAB(), interp);
    if (hTerminal == NULLHANDLE) {
        WinMessageBox(HWND_DESKTOP, NULLHANDLE, "Cannot create Terminal",
                      "Tclsh", 0, MB_OK | MB_ERROR | MB_APPLMODAL);
        /* Don't forget to cleanly exit PM */
        PMShutdown();
        return;
    }
#endif

    /*
     * Process commands from stdin until there's an end-of-file.
     */

    gotPartial = 0;
    Tcl_DStringInit(&command);
    inChannel = Tcl_GetStdChannel(TCL_STDIN);
    outChannel = Tcl_GetStdChannel(TCL_STDOUT);
    errChannel = Tcl_GetStdChannel(TCL_STDERR);
    while (1) {
        Tcl_DoOneEvent(TCL_ALL_EVENTS);
#ifdef CLI_VERSION

	if (tty) {
	    char *promptCmd;

	    promptCmd = Tcl_GetVar(interp,
		gotPartial ? "tcl_prompt2" : "tcl_prompt1", TCL_GLOBAL_ONLY);
	    if (promptCmd == NULL) {
defaultPrompt:
		if (!gotPartial && outChannel) {
		    Tcl_Write(outChannel, "% ", 2);
		}
	    } else {
		code = Tcl_Eval(interp, promptCmd);
		inChannel = Tcl_GetStdChannel(TCL_STDIN);
		outChannel = Tcl_GetStdChannel(TCL_STDOUT);
		errChannel = Tcl_GetStdChannel(TCL_STDERR);
		if (code != TCL_OK) {
		    if (errChannel) {
		        Tcl_Write(errChannel, interp->result, -1);
		        Tcl_Write(errChannel, "\n", 1);
		    }
		    Tcl_AddErrorInfo(interp,
			    "\n    (script that generates prompt)");
		    goto defaultPrompt;
		}
	    }
	    if (outChannel) {
	        Tcl_Flush(outChannel);
	    }
	}
        if (!inChannel) {
            goto done;
        }
        length = Tcl_Gets(inChannel, &command);
        if (length < 0) {
            goto done;
        }
        if ((length == 0) && Tcl_Eof(inChannel) && (!gotPartial)) {
            goto done;
        }

        /*
         * Add the newline removed by Tcl_Gets back to the string.
         */

        (void) Tcl_DStringAppend(&command, "\n", -1);

        cmd = Tcl_DStringValue(&command);
        if (!Tcl_CommandComplete(cmd)) {
            gotPartial = 1;
            continue;
        }

        gotPartial = 0;
        code = Tcl_RecordAndEval(interp, cmd, 0);
        inChannel = Tcl_GetStdChannel(TCL_STDIN);
        outChannel = Tcl_GetStdChannel(TCL_STDOUT);
        errChannel = Tcl_GetStdChannel(TCL_STDERR);
        Tcl_DStringFree(&command);
        if (code != TCL_OK) {
            if (errChannel) {
                Tcl_Write(errChannel, interp->result, -1);
                Tcl_Write(errChannel, "\n", 1);
            }
        } else if (tty && (*interp->result != 0)) {
            if (outChannel) {
                Tcl_Write(outChannel, interp->result, -1);
                Tcl_Write(outChannel, "\n", 1);
            }
        }

   #ifdef TCL_MEM_DEBUG
	if (quitFlag) {
	    Tcl_DeleteInterp(interp);
	    Tcl_DumpActiveMemory(dumpFile);
	    /* Don't forget to cleanly exit PM */
	    PMShutdown();
	    exit(0);
	}
   #endif
#endif
    }

    /*
     * Rather than calling exit, invoke the "exit" command so that
     * users can replace "exit" with some other command to do additional
     * cleanup on exit.  The Tcl_Eval call should never return.
     */

    done:
    /* Don't forget to cleanly exit PM */
    PMShutdown();
    sprintf(cbuf, "exit %d", exitCode);
    Tcl_Eval(interp, cbuf);
}

/*
 *----------------------------------------------------------------------
 *
 * CheckmemCmd --
 *
 *	This is the command procedure for the "checkmem" command, which
 *	causes the application to exit after printing information about
 *	memory usage to the file passed to this command as its first
 *	argument.
 *
 * Results:
 *	Returns a standard Tcl completion code.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
#ifdef TCL_MEM_DEBUG

	/* ARGSUSED */
static int
CheckmemCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Not used. */
    Tcl_Interp *interp;			/* Interpreter for evaluation. */
    int argc;				/* Number of arguments. */
    char *argv[];			/* String values of arguments. */
{
    if (argc != 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" fileName\"", (char *) NULL);
	return TCL_ERROR;
    }
    strcpy(dumpFile, argv[1]);
    quitFlag = 1;
    return TCL_OK;
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * TclOS2Panic --
 *
 *	Display a message and exit.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Exits the program.
 *
 *----------------------------------------------------------------------
 */

void
TclOS2Panic TCL_VARARGS_DEF(char *,arg1)
{
    va_list argList;
    char buf[1024];
    char *format;
    
    format = TCL_VARARGS_START(char *,arg1,argList);
    vsprintf(buf, format, argList);

#ifdef DEBUG
    printf("TclOS2Panic: %s\n", buf);
    fflush(stdout);
    fflush(stderr);
#endif

#ifndef CLI_VERSION
    /* Make sure pointer is not captured (for WinMessageBox) */
    WinSetCapture(HWND_DESKTOP, NULLHANDLE);
    WinAlarm(HWND_DESKTOP, WA_ERROR);
    WinMessageBox(HWND_DESKTOP, NULLHANDLE, buf, "Fatal Error in Tclsh", 0,
	    MB_OK | MB_ERROR | MB_APPLMODAL);
    PMShutdown();
#else
    fprintf(stderr, "FATAL: %s\n", buf);
#endif
    exit(1);
}
