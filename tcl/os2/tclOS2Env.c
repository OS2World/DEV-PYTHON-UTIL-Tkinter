/* 
 * tclOS2Env.c --
 *
 *	This file defines the TclOS2SetSystemEnv function.
 *
 * Copyright (c) 1996-1997 Illya Vaes
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */


#include "tclInt.h"
#include "tclPort.h"

/*
 *----------------------------------------------------------------------
 *
 * TclSetSystemEnv --
 *
 *	Set an environment variable.
 *
 * Results:
 *	0 on sucess, -1 on failure.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
int
TclSetSystemEnv( const char *variable, const char *value )
{
    char *string;
    int ret;

    string= malloc(strlen(variable)+1+strlen(value)+1);
    if (string == (char *)NULL) return -1;
    ret = sprintf(string, "%s=%s", variable, value);
    if (ret == 0 || ret == EOF) {
#ifdef DEBUG
        printf("TclSetSystemEnv: sprintf \"%s=%s\" returns %d\n", variable,
               value, ret);
        fflush(stdout);
#endif
        free(string);
        return -1;
    }
    ret= putenv(string);
    /* String may NOT be freed or reused */
#ifdef DEBUG
    printf("putenv \"%s\" returns %d\n", string, ret);
    fflush(stdout);
#endif
    return ret;
}
