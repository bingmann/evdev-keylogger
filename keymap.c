/*******************************************************************************
 * keymap.c
 *
 * Translate keyboard scan codes into keys, modifiers, and compose them.
 *
 *******************************************************************************
 * Copyright (C) 2012 Jason A. Donenfeld <Jason@zx2c4.com>
 * Copyright (C) 2014 Timo Bingmann <tb@panthema.net>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA 02111-1307 USA
 ******************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include <assert.h>
#include <linux/input.h>

#include "keymap.h"

/* Mysteriously missing defines from <linux/input.h>. */
#define EV_MAKE 1
#define EV_BREAK 0
#define EV_REPEAT 2

/* Default US keymap */
static wchar_t char_keys[49] = L"1234567890-=qwertyuiop[]asdfghjkl;'`\\zxcvbnm,./<";
static wchar_t shift_keys[49] = L"!@#$%^&*()_+QWERTYUIOP{}ASDFGHJKL:\"~|ZXCVBNM<>?>";
static wchar_t altgr_keys[49] = {0};

static wchar_t func_keys[][8] = {
    L"<Esc>", L"<BckSp>", L"<Tab>", L"<Enter>", L"<LCtrl>", L"<LShft>",
    L"<RShft>", L"<KP*>", L"<LAlt>", L" ", L"<CpsLk>", L"<F1>", L"<F2>",
    L"<F3>", L"<F4>", L"<F5>", L"<F6>", L"<F7>", L"<F8>", L"<F9>", L"<F10>",
    L"<NumLk>", L"<ScrLk>", L"<KP7>", L"<KP8>", L"<KP9>", L"<KP->", L"<KP4>",
    L"<KP5>", L"<KP6>", L"<KP+>", L"<KP1>", L"<KP2>", L"<KP3>", L"<KP0>",
    L"<KP.>", /*"<",*/ L"<F11>", L"<F12>", L"<KPEnt>", L"<RCtrl>", L"<KP/>",
    L"<PrtSc>", L"<AltGr>", L"<Break>" /*linefeed?*/, L"<Home>", L"<Up>",
    L"<PgUp>", L"<Left>", L"<Right>", L"<End>", L"<Down>", L"<PgDn>",
    L"<Ins>", L"<Del>", L"<Pause>", L"<LMeta>", L"<RMeta>", L"<Menu>"
};

/* c = character key
 * f = function key
 * _ = blank/error
 *
 * Source: KEY_* defines from <linux/input.h>
 */
static const char char_or_func[] =
/*   0123456789ABCDEF */
    "_fccccccccccccff"  /* 0 */
    "ccccccccccccfFcc"  /* 1 */
    "ccccccccccFccccc"  /* 2 */
    "ccccccFfFfFfffff"  /* 3 */
    "fffffFFfffffffff"  /* 4 */
    "ffff__cff_______"  /* 5 */
    "fFffFfffffffffff"  /* 6 */
    "_______f_____FFF"; /* 7 */

static int is_char_key(unsigned int code)
{
    assert(code < sizeof(char_or_func));
    return char_or_func[code] == 'c';
}
static int is_func_key(unsigned int code)
{
    assert(code < sizeof(char_or_func));
    return char_or_func[code] == 'f' || char_or_func[code] == 'F';
}
static int is_modfunc_key(unsigned int code)
{
    assert(code < sizeof(char_or_func));
    return char_or_func[code] == 'F';
}
int is_used_key(unsigned int code)
{
    assert(code < sizeof(char_or_func));
    return char_or_func[code] != '_';
}

/* Translates character keycodes to continuous array indexes. */
static int to_char_keys_index(unsigned int keycode)
{
    // keycodes 2-13: US keyboard: 1, 2, ..., 0, -, =
    if (keycode >= KEY_1 && keycode <= KEY_EQUAL)
        return keycode - 2;
    // keycodes 16-27: q, w, ..., [, ]
    if (keycode >= KEY_Q && keycode <= KEY_RIGHTBRACE)
        return keycode - 4;
    // keycodes 30-41: a, s, ..., ', `
    if (keycode >= KEY_A && keycode <= KEY_GRAVE)
        return keycode - 6;
    // keycodes 43-53: \, z, ..., ., /
    if (keycode >= KEY_BACKSLASH && keycode <= KEY_SLASH)
        return keycode - 7;
    // key right to the left of 'Z' on US layout
    if (keycode == KEY_102ND)
        return 47;

    return -1; // not character keycode
}
/* Translates function keys keycodes to continuous array indexes. */
static int to_func_keys_index(unsigned int keycode)
{
    // 1
    if (keycode == KEY_ESC)
        return 0;
    // 14-15
    if (keycode >= KEY_BACKSPACE && keycode <= KEY_TAB)
        return keycode - 13;
    // 28-29
    if (keycode >= KEY_ENTER && keycode <= KEY_LEFTCTRL)
        return keycode - 25;
    // 42
    if (keycode == KEY_LEFTSHIFT)
        return keycode - 37;
    // 54-83
    if (keycode >= KEY_RIGHTSHIFT && keycode <= KEY_KPDOT)
        return keycode - 48;
    // 87-88
    if (keycode >= KEY_F11 && keycode <= KEY_F12)
        return keycode - 51;
    // 96-111
    if (keycode >= KEY_KPENTER && keycode <= KEY_DELETE)
        return keycode - 58;
    // 119
    if (keycode == KEY_PAUSE)
        return keycode - 65;
    // 125-127
    if (keycode >= KEY_LEFTMETA && keycode <= KEY_COMPOSE)
        return keycode - 70;

    return -1; // not function key keycode
}

/* Translates struct input_event *event into the string of size buffer_length
 * pointed to by buffer. Stores state originating from sequence of event
 * structs in struc input_event_state *state. Returns the number of bytes
 * written to buffer.
 */
size_t translate_eventw(struct input_event *event,
                        struct input_event_state *state,
                        wchar_t *wbuffer, size_t wbuffer_len)
{
    wchar_t wch;
    size_t len = 0;
    static const size_t wprefix_len = 64;
    wchar_t wprefix[wprefix_len];
    wprefix[0] = 0;

    if (event->type != EV_KEY) {
        // not a keyboard event
    }
    else if (event->code >= sizeof(char_or_func)) {
        len += swprintf(&wbuffer[len], wbuffer_len, L"<E-%x>", event->code);
    }
    else if (event->value == EV_MAKE || event->value == EV_REPEAT) {
        if (event->code == KEY_LEFTSHIFT || event->code == KEY_RIGHTSHIFT) {
            state->shift = 1;
        }
        else if (event->code == KEY_RIGHTALT) {
            state->altgr = 1;
        }
        else if (event->code == KEY_LEFTALT) {
            state->alt = 1;
        }
        else if (event->code == KEY_LEFTCTRL || event->code == KEY_RIGHTCTRL) {
            state->ctrl = 1;
        }       
        else if (event->code == KEY_LEFTMETA || event->code == KEY_RIGHTMETA) {
            state->meta = 1;
        }
        else {
            if (state->ctrl && state->alt && state->meta)
                swprintf(wprefix, wprefix_len, L"<CTRL,ALT,META>+");
            else if (state->ctrl && state->alt)
                swprintf(wprefix, wprefix_len, L"<CTRL,ALT>+");
            else if (state->alt && state->meta)
                swprintf(wprefix, wprefix_len, L"<ALT,META>+");
            else if (state->ctrl && state->meta)
                swprintf(wprefix, wprefix_len, L"<CTRL,META>+");
            else if (state->meta)
                swprintf(wprefix, wprefix_len, L"<META>+");
            else if (state->ctrl)
                swprintf(wprefix, wprefix_len, L"<CTRL>+");
            else if (state->alt)
                swprintf(wprefix, wprefix_len, L"<ALT>+");
        }
        if (is_char_key(event->code)) {
            if (state->altgr) {
                wch = altgr_keys[to_char_keys_index(event->code)];
                if (wch == L'\0') {
                    if (state->shift)
                        wch = shift_keys[to_char_keys_index(event->code)];
                    else
                        wch = char_keys[to_char_keys_index(event->code)];
                }
            }
            else if (state->shift) {
                wch = shift_keys[to_char_keys_index(event->code)];
                if (wch == L'\0')
                    wch = char_keys[to_char_keys_index(event->code)];
            }
            else
                wch = char_keys[to_char_keys_index(event->code)];

            if (wch == L'<') {
                // escape < to <<, else parsing the output stream is impossible
                len += swprintf(&wbuffer[len], wbuffer_len, L"%ls<<", wprefix);
            }
            else if (wch != L'\0') {
                len += swprintf(&wbuffer[len], wbuffer_len, L"%ls%lc",
                                wprefix, wch);
            }
        }
        else if (is_modfunc_key(event->code)) {
            if (event->value == EV_MAKE)
                len += swprintf(&wbuffer[len], wbuffer_len, L"%ls%ls",
                                wprefix, func_keys[to_func_keys_index(event->code)]);
        }
        else if (is_func_key(event->code)) {
            len += swprintf(&wbuffer[len], wbuffer_len, L"%ls%ls",
                            wprefix, func_keys[to_func_keys_index(event->code)]);
        }
        else {
            len += swprintf(&wbuffer[len], wbuffer_len, L"%ls<E-%x>",
                            wprefix, event->code);
        }
    }
    else if (event->value == EV_BREAK) {
        if (event->code == KEY_LEFTSHIFT || event->code == KEY_RIGHTSHIFT)
            state->shift = 0;
        else if (event->code == KEY_RIGHTALT)
            state->altgr = 0;
        else if (event->code == KEY_LEFTALT)
            state->alt = 0;
        else if (event->code == KEY_LEFTCTRL || event->code == KEY_RIGHTCTRL)
            state->ctrl = 0;
        else if (event->code == KEY_LEFTMETA || event->code == KEY_RIGHTMETA)
            state->meta = 0;
    }

    return len;
}

/* Translates event into multi-byte string description. */
size_t translate_event(struct input_event *event,
                       struct input_event_state *state,
                       char *buffer, size_t buffer_len)
{
    wchar_t *wbuffer;
    size_t wbuffer_len, len;

    wbuffer = (wchar_t*)buffer;
    wbuffer_len = buffer_len / sizeof(wchar_t);

    len = translate_eventw(event, state, wbuffer, wbuffer_len);

    if (!len)
        *buffer = 0;
    else
        wcstombs(buffer, wbuffer, buffer_len);
    return len;
}

/* Determines the system keymap via the dump keys program
 * and some disgusting parsing of it. */
int load_system_keymap()
{
    FILE *dumpkeys;
    char buffer[256];
    char *start, *end;
    unsigned int keycode;
    int index;
    wchar_t wch;
    /* HACK: This is obscenely ugly, and we should really just do this in C... */
    dumpkeys = popen("/usr/bin/dumpkeys -n | /bin/grep '^\\([[:space:]]shift[[:space:]]\\)*\\([[:space:]]altgr[[:space:]]\\)*keycode' | /bin/sed 's/U+/0x/g' 2>&1", "r");
    if (!dumpkeys) {
        perror("popen");
        return 1;
    }
    memset(char_keys, 0, sizeof(char_keys));
    memset(shift_keys, 0, sizeof(shift_keys));
    memset(altgr_keys, 0, sizeof(altgr_keys));
    for (keycode = 1; fgets(buffer, sizeof(buffer), dumpkeys)
             && keycode < sizeof(char_or_func); ++keycode) {
        if (!is_char_key(keycode))
            continue;
        if (buffer[0] == 'k') {
            index = to_char_keys_index(keycode);

            start = &buffer[14];
            wch = (wchar_t)strtoul(start, &end, 16);
            if (start[0] == '+' && (wch & 0xB00))
                wch ^= 0xB00;
            char_keys[index] = wch;

            start = end;
            while (start[0] == ' ' && start[0] != '\0')
                ++start;
            wch = (wchar_t)strtoul(start, &end, 16);
            if (start[0] == '+' && (wch & 0xB00))
                wch ^= 0xB00;
            if (wch == L'\0') {
                wch = towupper(char_keys[index]);
                if (wch == char_keys[index])
                    wch = L'\0';
            }
            shift_keys[index] = wch;

            start = end;
            while (start[0] == ' ' && start[0] != '\0')
                ++start;
            wch = (wchar_t)strtoul(start, &end, 16);
            if (start[0] == '+' && (wch & 0xB00))
                wch ^= 0xB00;
            altgr_keys[index] = wch;
        } else {
            index = to_char_keys_index(keycode == 0 ? 0 : --keycode);
            wch = (wchar_t)strtoul(&buffer[21], NULL, 16);
            if (buffer[21] == '+' && (wch & 0xB00))
                wch ^= 0xB00;
            if (buffer[1] == 's')
                shift_keys[index] = wch;
            else if (buffer[1] == 'a')
                altgr_keys[index] = wch;
        }
    }
    pclose(dumpkeys);
    return 0;

}
