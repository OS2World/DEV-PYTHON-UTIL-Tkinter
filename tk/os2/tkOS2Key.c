/* 
 * tkOS2Key.c --
 *
 *	This file contains X emulation routines for keyboard related
 *	functions.
 *
 * Copyright (c) 1996-1997 Illya Vaes
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */


#include "tkOS2Int.h"

typedef struct {
    unsigned int keycode;
    KeySym keysym;
} Keys;

static Keys keymap[] = {
    {VK_BREAK, XK_Cancel},
    {VK_BACKSPACE, XK_BackSpace},
    {VK_TAB, XK_Tab},
    {VK_CLEAR, XK_Clear},
    {VK_NEWLINE, XK_Return},
    {VK_ENTER, XK_Return},
    {VK_SHIFT, XK_Shift_L},
    {VK_CTRL, XK_Control_L},
    /*
    {VK_MENU, XK_Alt_L},
    */
    {VK_ALT, XK_Alt_L},
    /*
    {VK_MENU, XK_Alt_R},
    */
    {VK_ALTGRAF, XK_Alt_R},
    {VK_PAUSE, XK_Pause},
    {VK_CAPSLOCK, XK_Caps_Lock},
    {VK_ESC, XK_Escape},
    {VK_SPACE, XK_space},
    {VK_PAGEUP, XK_Prior},
    {VK_PAGEDOWN, XK_Next},
    {VK_END, XK_End},
    {VK_HOME, XK_Home},
    {VK_LEFT, XK_Left},
    {VK_UP, XK_Up},
    {VK_RIGHT, XK_Right},
    {VK_DOWN, XK_Down},
/*    {0, XK_Select}, */
    {VK_PRINTSCRN, XK_Print},
/*    {0, XK_Execute}, */
    {VK_INSERT, XK_Insert},
    {VK_DELETE, XK_Delete},
/*    {0, XK_Help}, */
    {VK_F1, XK_F1},
    {VK_F2, XK_F2},
    {VK_F3, XK_F3},
    {VK_F4, XK_F4},
    {VK_F5, XK_F5},
    {VK_F6, XK_F6},
    {VK_F7, XK_F7},
    {VK_F8, XK_F8},
    {VK_F9, XK_F9},
    {VK_F10, XK_F10},
    {VK_F11, XK_F11},
    {VK_F12, XK_F12},
    {VK_F13, XK_F13},
    {VK_F14, XK_F14},
    {VK_F15, XK_F15},
    {VK_F16, XK_F16},
    {VK_F17, XK_F17},
    {VK_F18, XK_F18},
    {VK_F19, XK_F19},
    {VK_F20, XK_F20},
    {VK_F21, XK_F21},
    {VK_F22, XK_F22},
    {VK_F23, XK_F23},
    {VK_F24, XK_F24},
    {VK_NUMLOCK, XK_Num_Lock}, 
    {VK_SCRLLOCK, XK_Scroll_Lock},
    {0, NoSymbol}
};


/*
 *----------------------------------------------------------------------
 *
 * XLookupString --
 *
 *	Retrieve the string equivalent for the given keyboard event.
 *
 * Results:
 *	Returns the number of characters stored in buffer_return.
 *
 * Side effects:
 *	Retrieves the characters stored in the event and inserts them
 *	into buffer_return.
 *
 *----------------------------------------------------------------------
 */

int
XLookupString(event_struct, buffer_return, bytes_buffer, keysym_return,
	status_in_out)
    XKeyEvent* event_struct;
    char* buffer_return;
    int bytes_buffer;
    KeySym* keysym_return;
    XComposeStatus* status_in_out;
{
    int i, limit;

#ifdef DEBUG
    printf("XLookupString\n");
#endif

    if ((event_struct->nchars <= 0) || (buffer_return == NULL)) {
	return 0;
    }
    limit = (event_struct->nchars < bytes_buffer) ? event_struct->nchars :
	bytes_buffer;

    for (i = 0; i < limit; i++) {
	buffer_return[i] = event_struct->trans_chars[i];
    }

    if (keysym_return != NULL) {
	*keysym_return = NoSymbol;
    }
    return i;
}

/*
 *----------------------------------------------------------------------
 *
 * XKeycodeToKeysym --
 *
 *	Translate from a system-dependent keycode to a
 *	system-independent keysym.
 *
 * Results:
 *	Returns the translated keysym, or NoSymbol on failure.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

KeySym
XKeycodeToKeysym(display, keycode, index)
    Display* display;
    unsigned int keycode;
    int index;
{
    Keys* key;
    BYTE keys[256];
    int result;

    memset(keys, 0, 256);
    if (index & 0x02) {
	keys[VK_NUMLOCK] = 1;
    }
    if (index & 0x01) {
	keys[VK_SHIFT] = 0x80;
    }
    result = keycode > 32 && keycode < 255;

    /*
     * Keycode mapped to a valid Latin-1 character.  Since the keysyms
     * for alphanumeric characters map onto Latin-1, we just return it.
     */

    if (result == 1 && keycode >= 0x20) {
#ifdef DEBUG
        printf("XKeycodeToKeysym returning char %x (%c)\n", keycode, keycode);
#endif
	return (KeySym) keycode;
    }

    /*
     * Keycode is a non-alphanumeric key, so we have to do the lookup.
     */

    for (key = keymap; key->keycode != 0; key++) {
	if (key->keycode == keycode) {
#ifdef DEBUG
            printf("XKeycodeToKeysym keycode %x -> keysym %x\n", keycode,
                   key->keysym);
#endif
	    return key->keysym;
	}
    }

    return NoSymbol;
}

/*
 *----------------------------------------------------------------------
 *
 * XKeysymToKeycode --
 *
 *	Translate a keysym back into a keycode.
 *
 * Results:
 *	Returns the keycode that would generate the specified keysym.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

KeyCode
XKeysymToKeycode(display, keysym)
    Display* display;
    KeySym keysym;
{
    Keys* key;
    SHORT result;

#ifdef DEBUG
    printf("XKeysymToKeycode\n");
#endif

    if (keysym >= 0x20) {
/*
	result = VkKeyScan(keysym);
*/
result= (SHORT)keysym;
	if (result != -1) {
	    return (KeyCode) (result & 0xff);
	}
    }

    /*
     * Couldn't map the character to a virtual keycode, so do a
     * table lookup.
     */

    for (key = keymap; key->keycode != 0; key++) {
	if (key->keysym == keysym) {
	    return key->keycode;
	}
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * XGetModifierMapping --
 *
 *	Fetch the current keycodes used as modifiers.
 *
 * Results:
 *	Returns a new modifier map.
 *
 * Side effects:
 *	Allocates a new modifier map data structure.
 *
 *----------------------------------------------------------------------
 */

XModifierKeymap	*
XGetModifierMapping(display)
    Display* display;
{
    XModifierKeymap *map = (XModifierKeymap *)ckalloc(sizeof(XModifierKeymap));

#ifdef DEBUG
    printf("XGetModifierMapping\n");
#endif

    map->max_keypermod = 1;
    map->modifiermap = (KeyCode *) ckalloc(sizeof(KeyCode)*8);
    map->modifiermap[ShiftMapIndex] = VK_SHIFT;
    map->modifiermap[LockMapIndex] = VK_CAPSLOCK;
    map->modifiermap[ControlMapIndex] = VK_CTRL;
    map->modifiermap[Mod1MapIndex] = VK_NUMLOCK;
    map->modifiermap[Mod2MapIndex] = VK_MENU;
    map->modifiermap[Mod3MapIndex] = VK_SCRLLOCK;
    map->modifiermap[Mod4MapIndex] = 0;
    map->modifiermap[Mod5MapIndex] = 0;
    return map;
}

/*
 *----------------------------------------------------------------------
 *
 * XFreeModifiermap --
 *
 *	Deallocate a modifier map that was created by
 *	XGetModifierMapping.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Frees the datastructure referenced by modmap.
 *
 *----------------------------------------------------------------------
 */

void
XFreeModifiermap(modmap)
    XModifierKeymap* modmap;
{
    ckfree((char *) modmap->modifiermap);
    ckfree((char *) modmap);
}

/*
 *----------------------------------------------------------------------
 *
 * XStringToKeysym --
 *
 *	Translate a keysym name to the matching keysym. 
 *
 * Results:
 *	Returns the keysym.  Since this is already handled by
 *	Tk's StringToKeysym function, we just return NoSymbol.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

KeySym
XStringToKeysym(string)
    _Xconst char *string;
{
    return NoSymbol;
}

/*
 *----------------------------------------------------------------------
 *
 * XKeysymToString --
 *
 *	Convert a keysym to character form.
 *
 * Results:
 *	Returns NULL, since Tk will have handled this already.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
XKeysymToString(keysym)
    KeySym keysym;
{
    return NULL;
}
