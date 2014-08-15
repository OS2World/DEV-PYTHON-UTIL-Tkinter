/*
 * tclOS2Console.c --
 *
 *    OS/2 PM console window class definition.
 *
 * Copyright (c) 1994 Software Research Associates, Inc.
 * Copyright (c) 1995 Sun Microsystems, Inc.
 * Copyright (c) 1996-1997 Illya Vaes
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"
#include "tclPort.h"
#include "tclOS2Console.h"
#include <string.h>

/*
 * Predefined control identifiers.
 */
#define IDC_EDIT    1
#define MLN_USER    0xff

#define MAX(a,b)  ( (a) > (b) ? (a) : (b) )

/*
 * Initial screen size.
 */

#define INIT_SCREEN_CX    80
#define INIT_SCREEN_CY    25

static HWND hwndEdit;        /* Handle for edit control. */
#define APP_NAME "Tclsh " ## TCL_VERSION
static char szAppName[] = APP_NAME;
static int cxFrame, cyFrame, cyCaption, cxVScroll;
static Tcl_DString command;     /* Used to buffer incomplete commands. */

char cmdBuf[256];        /* Line buffer for commands */
IPT insPoint;

PFNWP oldEditProc = NULL;    /* Pointer to system Edit control procedure */

/*
 * Forward references for procedures defined later in this file:
 */

static void             DisplayString _ANSI_ARGS_((char *str, int newline));
static MRESULT EXPENTRY TerminalProc _ANSI_ARGS_((HWND, ULONG, MPARAM, MPARAM));
static MRESULT EXPENTRY EditProc _ANSI_ARGS_((HWND, ULONG, MPARAM, MPARAM));
static int              TerminalPutsCmd _ANSI_ARGS_((ClientData clientData,
                                Tcl_Interp *interp, int argc, char **argv));


/*
 *----------------------------------------------------------------------
 *
 * RegisterTerminalClass --
 *
 *    Creates the application class for the console window.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    The global window class "Tclsh" is created.
 *
 *----------------------------------------------------------------------
 */

BOOL
RegisterTerminalClass(hab)
    HAB hab;
{            
    return WinRegisterClass(hab, "Terminal", TerminalProc, CS_SIZEREDRAW,
                            sizeof(Tcl_Interp*));
}

/*
 *----------------------------------------------------------------------
 *
 * CreateTerminal --
 *
 *    Creates an instance of the Tclsh console window.
 *
 * Results:
 *    A Window handle for the newly created instance.
 *
 * Side effects:
 *    Creates a new window instance with a pointer to the
 *    interpreter stored in the window instance data.
 *
 *----------------------------------------------------------------------
 */

HWND
CreateTerminal(hab, interp)
    HAB hab;
    Tcl_Interp *interp;
{
    HPS hps;
    FONTMETRICS fm;
    HWND hwnd, hFrame;
    ULONG flags = FCF_TITLEBAR | FCF_SYSMENU | FCF_MINMAX | FCF_SHELLPOSITION |
                  FCF_SIZEBORDER | FCF_TASKLIST;

    Tcl_DStringInit(&command);
    hwnd = hFrame = NULLHANDLE;
    hps = WinGetPS(HWND_DESKTOP);
    /* Select system font? */
    if (GpiQueryFontMetrics(hps, sizeof(FONTMETRICS), &fm)) {
        cxFrame = WinQuerySysValue(HWND_DESKTOP, SV_CXSIZEBORDER);
        cyFrame = WinQuerySysValue(HWND_DESKTOP, SV_CYSIZEBORDER);
        cyCaption = WinQuerySysValue(HWND_DESKTOP, SV_CYTITLEBAR);
        cxVScroll = WinQuerySysValue(HWND_DESKTOP, SV_CXVSCROLL);

        hFrame= WinCreateStdWindow(HWND_DESKTOP, 0, &flags, "Terminal",
                                   szAppName, 0, NULLHANDLE, 1, &hwnd);
        if (hwnd != NULLHANDLE) {
            WinSetWindowULong(hwnd, QWL_USER, (ULONG)interp);
            WinSetWindowPos(hFrame, HWND_TOP, 0, 0,
                  (fm.lAveCharWidth * INIT_SCREEN_CX)+(cxFrame * 2)+ cxVScroll,
                  (fm.lMaxBaselineExt * INIT_SCREEN_CY)+cyCaption+(cyFrame * 2), 
                  SWP_SIZE | SWP_SHOW);
            Tcl_CreateCommand(interp, "puts", TerminalPutsCmd, NULL, NULL);
        }
    }
    WinReleasePS(hps);
    return hwnd;
}

/*
 *----------------------------------------------------------------------
 *
 * TerminalProc --
 *
 *    Window procedure for the Tclsh "Terminal" class.
 *
 * Results:
 *    The usual Window procedure values.
 *
 * Side effects:
 *    On window creation, it creates an edit child window.  Most
 *    of the messages are forwarded to the child.
 *
 *----------------------------------------------------------------------
 */

static MRESULT EXPENTRY
TerminalProc(hwnd, message, param1, param2)
    HWND hwnd;
    ULONG message;
    MPARAM param1;
    MPARAM param2;
{
    switch(message) {
    case WM_CREATE: {
        MLECTLDATA mleCtlData;
        IPT firstPos;

        mleCtlData.cbCtlData = sizeof(mleCtlData);
        mleCtlData.afIEFormat = MLFIE_CFTEXT;
        /*
         * Don't specify unbounded text limit by giving -1, so we don't
         * groooowwwwwww the swapfile. The limit will be raised by overflows
         * that don't fit into this limit; other overflows will cause silent
         * deletion of that amount from the beginning.
         */
        mleCtlData.cchText = 1024 * 1024;   /* 1 MB */
        mleCtlData.iptAnchor = 0;
        mleCtlData.iptCursor = 0;
        mleCtlData.cxFormat = 0;
        mleCtlData.cyFormat = 0;
        mleCtlData.afFormatFlags = MLFFMTRECT_MATCHWINDOW;
        hwndEdit = WinCreateWindow(hwnd, WC_MLE, NULL,
                                   WS_VISIBLE | MLS_HSCROLL | MLS_VSCROLL |
                                   MLS_BORDER | MLS_WORDWRAP, 0, 0, 0, 0, hwnd,
                                   HWND_TOP, IDC_EDIT, &mleCtlData, NULL);
        oldEditProc = WinSubclassWindow(hwndEdit, EditProc);
        /* Have the first prompt displayed */
        insPoint = (IPT) WinSendMsg(hwndEdit, MLM_QUERYFIRSTCHAR, (MPARAM)0,
                                    (MPARAM)0);
        firstPos = insPoint;
        DisplayString("Welcome to the Tcl shell "TCL_VERSION" for OS/2 Presentation Manager", 1);
        DisplayString("% ", 0);
        WinSendMsg(hwndEdit, MLM_SETFIRSTCHAR, (MPARAM)firstPos, (MPARAM)0);
        /*
        insPoint = (IPT)WinSendMsg(hwndEdit, MLM_CHARFROMLINE, MPFROMLONG(-1),
                                   0);
        */
#ifdef DEBUG
        {
        LONG limit = (LONG) WinSendMsg(hwndEdit, MLM_QUERYTEXTLIMIT, (MPARAM)0,
                                       (MPARAM)0);
        printf("MLE text limit is %d\n", limit);
        fflush(stdout);
        }
#endif
        return 0;
    }

    case WM_CONTROL:
        if (SHORT1FROMMP(param1) == IDC_EDIT) {
            /* This is our MLE calling */
            switch (SHORT2FROMMP(param1)) {
            case MLN_USER: {
                int length, offset, exp;
                char *cmd;

                /*
                 * Get line containing cursor.
                 */

                /* Get line length */
                length = (int)WinSendMsg(hwndEdit, MLM_QUERYLINELENGTH,
                                         MPFROMLONG(insPoint), 0);
                /* Set export buffer */
                if (!WinSendMsg(hwndEdit, MLM_SETIMPORTEXPORT, MPFROMP(cmdBuf),
                                MPFROMLONG(sizeof(cmdBuf)))) {
                    break;
                }
                /* Export the text from the MLE */
                exp = (ULONG)WinSendMsg(hwndEdit, MLM_EXPORT,
                                        MPFROMP(&insPoint), MPFROMP(&length));
                cmdBuf[exp] = '\n';
                cmdBuf[exp+1] = '\0';
                if (cmdBuf[0] == '%' || cmdBuf[0] == '>') {
                    if (cmdBuf[0] == ' ') offset = 2;
                    else offset = 1;
                } else {
                    offset = 0;
                }
                cmd = Tcl_DStringAppend(&command, cmdBuf + offset, -1);
                DisplayString("", 1);
                if (Tcl_CommandComplete(cmd)) {
                    Tcl_Interp* interp = (Tcl_Interp*) WinQueryWindowULong(hwnd,
                                         QWL_USER);
                    Tcl_RecordAndEval(interp, cmd, 0);
                    Tcl_DStringFree(&command);
                    if (interp->result != NULL && *interp->result != '\0') {
                        DisplayString(interp->result, 1);
                    }
                    DisplayString("% ", 0);
                } else {
                    DisplayString("> ", 0);
                }
                break;
            }

            case MLN_TEXTOVERFLOW:
                /*
                 * Character(s) typed causing overflow, delete a whole block
                 * of text at the beginning so the next character won't cause
                 * this message again, or the amount of overflow (in param2)
                 * if that's larger.
                 * Return TRUE to signal that corrective action has been taken.
                 */
#ifdef DEBUG
                printf("MLN_TEXTOVERFLOW %d\n", MAX(1024, LONGFROMMP(param2)));
                fflush(stdout);
#endif
                WinSendMsg(hwndEdit, MLM_DELETE,
                           (MPARAM) WinSendMsg(hwndEdit, MLM_CHARFROMLINE,
                                           (MPARAM)0, (MPARAM)0),
                           (MPARAM) MAX(1024, LONGFROMMP(param2)));
                return (MRESULT)1;

            case MLN_OVERFLOW: {
                /*
                 * Some action != typing character has caused overflow, delete
                 * the amount of overflow (in MLEOVERFLOW structure pointed to
                 * by param2) at the beginning if this is because of a paste.
                 * Return TRUE to signal that corrective action has been taken.
                 */
                POVERFLOW pOverflow = (POVERFLOW) PVOIDFROMMP(param2);
                if (pOverflow->afErrInd & MLFETL_TEXTBYTES) {
                    /*
                     * If the overflow is larger than the text limit, increase
                     * it to the overflow, so it will fit entirely. Otherwise,
                     * delete the first <overflow> bytes.
                     */
                    IPT firstPoint;
                    LONG limit = (LONG) WinSendMsg(hwndEdit, MLM_QUERYTEXTLIMIT,
                                            (MPARAM)0, (MPARAM)0);
#ifdef DEBUG
                    printf("MLE text limit is %d\n", limit);
                    fflush(stdout);
#endif
                    /* limit is < 0 for unbounded, but then we can't overflow */
                    if (pOverflow->nBytesOver > limit) {
#ifdef DEBUG
                        printf("Increasing MLE text limit by %d to %d\n",
                               pOverflow->nBytesOver,
                               pOverflow->nBytesOver + limit);
                        fflush(stdout);
#endif
                        WinSendMsg(hwndEdit, MLM_SETTEXTLIMIT,
                                   /* *MM* added parens around expression */
                                   (MPARAM) (pOverflow->nBytesOver + limit),
                                   (MPARAM)0);
                    }
#ifdef DEBUG
                    printf("MLN_OVERFLOW %d\n",
                           MAX(1024, pOverflow->nBytesOver));
                    fflush(stdout);
#endif
                    firstPoint = (IPT) WinSendMsg(hwndEdit, MLM_CHARFROMLINE,
                                                  (MPARAM)0, (MPARAM)0);
                    firstPoint = 0;
                    WinSendMsg(hwndEdit, MLM_DELETE, (MPARAM)firstPoint,
                               (MPARAM) MAX(1024, pOverflow->nBytesOver));
                    insPoint = (IPT)WinSendMsg(hwndEdit, MLM_CHARFROMLINE,
                                          (MPARAM)WinSendMsg(hwndEdit,
                                                             MLM_QUERYLINECOUNT,
                                                             (MPARAM)0,
                                                             (MPARAM)0),
                                          (MPARAM)0);
                    insPoint += (int)WinSendMsg(hwndEdit, MLM_QUERYLINELENGTH,
                                                MPFROMLONG(insPoint), 0);
#ifdef DEBUG
                    printf("lineCount %d\n", (long)WinSendMsg(hwndEdit,
                           MLM_QUERYLINECOUNT, (MPARAM)0, (MPARAM)0));
                    printf("firstPoint %d, insPoint %d\n", firstPoint,
                           insPoint);
                    fflush(stdout);
#endif
                } else {
                    /* What to do? */
#ifdef DEBUG
                    printf("non-textlimit MLN_OVERFLOW %d\n",
                           pOverflow->nBytesOver);
                    fflush(stdout);
#endif
                    return (MRESULT)0;
                }
                return (MRESULT)1;
            }

            case MLN_MEMERROR:
                WinMessageBox(HWND_DESKTOP, hwnd,
                              "MLE says \"MLN_MEMERROR\"",
                              szAppName, 0, MB_OK | MB_ERROR | MB_APPLMODAL);
                return (MRESULT)0;
        }
    }
    break;

    case WM_CHAR: {
#ifdef DEBUG
        USHORT flags= CHARMSG(&message)->fs;
        USHORT charcode= CHARMSG(&message)->chr;
        printf("WM_CHAR flags %x, charcode %d\n", flags, charcode);
#endif
        if ((CHARMSG(&message)->fs) & KC_CTRL &&
            (CHARMSG(&message)->chr) == 'c') {
            Tcl_Interp* interp = (Tcl_Interp*) WinQueryWindowULong(hwnd,
                                 QWL_USER);
            int length, exp;

            /*
             * Get line containing cursor.
             */

            /* Get line length */
            length = (int)WinSendMsg(hwndEdit, MLM_QUERYLINELENGTH,
                                     MPFROMLONG(insPoint), 0);
            /* Set export buffer */
            WinSendMsg(hwndEdit, MLM_SETIMPORTEXPORT, MPFROMP(cmdBuf),
                            MPFROMLONG(sizeof(cmdBuf)));
            /* Export the text from the MLE */
            exp = (ULONG)WinSendMsg(hwndEdit, MLM_EXPORT, MPFROMP(&insPoint),
                                    MPFROMP(&length));
            Tcl_DStringFree(&command);
            Tcl_Eval(interp, "break");
            DisplayString("", 1);
            DisplayString("% ", 0);
        }
        break;
    }

    case WM_SETFOCUS:
        WinSetFocus(HWND_DESKTOP, hwndEdit);
        return 0;
        
    case WM_SIZE:
        WinSetWindowPos(hwndEdit, HWND_TOP, 0, 0, SHORT1FROMMP(param2),
                        SHORT2FROMMP(param2), SWP_MOVE | SWP_SIZE);
        return 0;

    case WM_CLOSE:
        if (WinMessageBox(HWND_DESKTOP, hwnd, "Do you really want to exit?",
                          szAppName, 0, MB_YESNO|MB_ICONQUESTION|MB_APPLMODAL)
            == MBID_YES) {
            Tcl_Interp* interp= (Tcl_Interp*) WinQueryWindowULong(hwnd,
                                                                  QWL_USER);
#ifdef DEBUG
            printf("WM_CLOSE, exiting, interp %x\n", (long)interp);
#endif
            Tcl_Eval(interp, "exit");
            /* Don't risk not cleanly exiting PM */
            PMShutdown();
            exit(0);
        }
#ifdef DEBUG
        printf("WM_CLOSE, not exiting\n");
#endif
        return 0 ;

    }
#ifdef DEBUG
    printf("Returning WinDefWindowProc(%x, %x (%s), %x, %x)\n", hwnd, message,
           message == WM_CONTROL ? "WM_CONTROL" : "unknown",
           param1, param2);
    fflush(stdout);
#endif
    return WinDefWindowProc(hwnd, message, param1, param2);
}

/*
 *----------------------------------------------------------------------
 *
 * EditProc --
 *
 *    Edit subclass window procedure.
 *
 * Results:
 *    The usual Window procedure values.
 *
 * Side effects:
 *    Allows user to edit commands.  Sends a double click event to
 *    the main window when the user presses enter.
 *
 *----------------------------------------------------------------------
 */

static MRESULT EXPENTRY
EditProc(hwnd, message, param1, param2)
    HWND hwnd;
    ULONG message;
    MPARAM param1;
    MPARAM param2;
{
    if (message == WM_CHAR && SHORT1FROMMP(param1) & KC_CHAR &&
            ((USHORT)SHORT1FROMMP(param2) == '\r'
             || (USHORT)SHORT1FROMMP(param2) == '\n')) {
#ifdef DEBUG
        printf("short1(param1) [%x], char3(param1) [%x], char4(param1) [%x]\n",
               SHORT1FROMMP(param1), CHAR3FROMMP(param1), CHAR4FROMMP(param1));
        printf("short1(param2) [%x], short2(param2) [%x]\n",
               SHORT1FROMMP(param2), SHORT2FROMMP(param2));
#endif
    WinSendMsg(WinQueryWindow(hwnd, QW_PARENT), WM_CONTROL,
               MPFROM2SHORT(IDC_EDIT,MLN_USER), (MPARAM)hwnd);
        return 0 ;
    } else {
#ifdef DEBUG
        printf("Returning oldEditProc (%x, %x (%s), %x, %x)\n", hwnd,
               message, message == WM_CONTROL ? "WM_CONTROL" :
               (message == MLM_SETSEL ? "MLM_SETSEL" :
                (message == MLM_QUERYLINECOUNT ? "MLM_QUERYLINECOUNT" :
                 (message == MLM_CHARFROMLINE ? "MLM_CHARFROMLINE" :
                  (message == MLM_PASTE ? "MLM_PASTE" : "unknown")))),
               param1, param2);
        fflush(stdout);
#endif
        return oldEditProc(hwnd, message, param1, param2);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TerminalPutsCmd --
 *
 *    Replacement for Tcl "puts" command that writes output to the
 *    terminal instead of stdout/stderr.
 *
 * Results:
 *    A standard Tcl result.
 *
 * Side effects:
 *    Same as Tcl_PutsCmd, except that it puts the output string
 *    into the terminal if the specified file is stdout.
 *
 *----------------------------------------------------------------------
 */

int
TerminalPutsCmd(clientData, interp, argc, argv)
    ClientData clientData;        /* Not used. */
    Tcl_Interp *interp;            /* Current interpreter. */
    int argc;                /* Number of arguments. */
    char **argv;            /* Argument strings. */
{
    Tcl_Channel chan;                   /* The channel to puts on. */
    int i;                              /* Counter. */
    int newline;                        /* Add a newline at end? */
    char *channelId;                    /* Name of channel for puts. */
    int result;                         /* Result of puts operation. */
    int mode;                           /* Mode in which channel is opened. */

#ifdef DEBUG
    printf("TerminalPutsCmd ");
    for (i=0; i<argc; i++) {
       printf("[%s] ", argv[i]);
    }
    printf("\n");
#endif
    
    i = 1;
    newline = 1;
    if ((argc >= 2) && (strcmp(argv[1], "-nonewline") == 0)) {
    newline = 0;
    i++;
    }
    if ((i < (argc-3)) || (i >= argc)) {
        Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
                " ?-nonewline? ?channelId? string\"", (char *) NULL);
        return TCL_ERROR;
    }

    /*
     * The code below provides backwards compatibility with an old
     * form of the command that is no longer recommended or documented.
     */

    if (i == (argc-3)) {
        if (strncmp(argv[i+2], "nonewline", strlen(argv[i+2])) != 0) {
            Tcl_AppendResult(interp, "bad argument \"", argv[i+2],
                "\": should be \"nonewline\"", (char *) NULL);
            return TCL_ERROR;
        }
        newline = 0;
    }
    if (i == (argc-1)) {
        /* Output on console terminal */
        DisplayString(argv[i], newline);
    } else {
        /* Other channel specified, use standard (tclIOCmd) stuff */
        channelId = argv[i];
        i++;
        chan = Tcl_GetChannel(interp, channelId, &mode);
        if (chan == (Tcl_Channel) NULL) {
            return TCL_ERROR;
        }
        if ((mode & TCL_WRITABLE) == 0) {
            Tcl_AppendResult(interp, "channel \"", channelId,
                    "\" wasn't opened for writing", (char *) NULL);
            return TCL_ERROR;
        }

        result = Tcl_Write(chan, argv[i], -1);
        if (result < 0) {
            goto error;
        }
        if (newline != 0) {
            result = Tcl_Write(chan, "\n", 1);
            if (result < 0) {
                goto error;
            }
        }
    }

    return TCL_OK;

error:
    Tcl_AppendResult(interp, "error writing \"", Tcl_GetChannelName(chan),
            "\": ", Tcl_PosixError(interp), (char *) NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * DisplayString --
 *
 *    Insert a string into the console with an optional trailing
 *    newline.
 *      NOTE: the MLE control assumes the text to be in the clipboard as
 *      a single contiguous data segment, which restricts the amount to
 *      the maximum segment size (64K).
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Updates the MLE control.
 *
 *----------------------------------------------------------------------
 */

void
DisplayString(str, newline)
    char *str;
    int newline;
{
    char *p;
    const char *tmp;
    PVOID clipmem;
    ULONG size, lineCnt;

    tmp = str;
    for(lineCnt = 0; *tmp; tmp++) {
        if(*tmp == '\n') {
            lineCnt++;
        }
    }
    if (newline) {
        size  = strlen(str) + lineCnt + 3;
    } else {
        size  = strlen(str) + lineCnt + 1;
    }
#ifdef DEBUG
    printf("DisplayString size %d\n", size);
    fflush(stdout);
#endif
    if ( (clipmem = ckalloc(size)) ) {
        p = (char *)clipmem;
        while(*str != '\0') {
            if(*str == '\n') {
                *p++ = '\r';
            }
            *p++ = *str++ ;
        }
        if (newline) {
            *p++ = '\r';
            *p++ = '\n';
        }
        *p = '\0';
        WinSendMsg(hwndEdit, MLM_DISABLEREFRESH, (MPARAM)0, (MPARAM)0);
        if (WinSendMsg(hwndEdit, MLM_SETIMPORTEXPORT, MPFROMP(clipmem),
                       MPFROMLONG(size))) {
#ifdef DEBUG
            ULONG imported;
            printf("before MLM_IMPORT, insPoint %d, size %d\n", insPoint,
                   size);
            imported = (ULONG)
#endif
            WinSendMsg(hwndEdit, MLM_IMPORT, MPFROMP(&insPoint),
                       MPFROMLONG(size));
#ifdef DEBUG
            printf("MLM_IMPORT imported %d (insPoint %d, size %d)\n",
                   imported, insPoint, size);
#endif
        }
        WinSendMsg(hwndEdit, MLM_SETSEL, (MPARAM)insPoint,
                   (MPARAM)insPoint);
        WinSendMsg(hwndEdit, MLM_ENABLEREFRESH, (MPARAM)0, (MPARAM)0);
    }
}
