// Escape sequence parser for xterm function keys.
// Copyright 2018 Craig Barnes.
// SPDX-License-Identifier: GPL-2.0-only
// Generated by: src/terminal/xterm-keys.lua
// See also: https://invisible-island.net/xterm/ctlseqs/ctlseqs.html

#include "xterm.h"

static KeyCode mod_enum_to_mod_mask(char mod_enum)
{
    switch (mod_enum) {
    case '2': return MOD_SHIFT;
    case '3': return MOD_META;
    case '4': return MOD_SHIFT | MOD_META;
    case '5': return MOD_CTRL;
    case '6': return MOD_SHIFT | MOD_CTRL;
    case '7': return MOD_META | MOD_CTRL;
    case '8': return MOD_SHIFT | MOD_META | MOD_CTRL;
    default:  return 0;
    }
}

ssize_t xterm_parse_key(const char *buf, size_t length, KeyCode *k)
{
    if (length == 0 || buf[0] != '\033') {
        return 0;
    }
    KeyCode tmp;
    size_t i = 1;
    if (i >= length) return -1;
    switch(buf[i++]) {
    case 'O':
        if (i >= length) return -1;
        switch(buf[i++]) {
        case ' ':
            *k = ' ';
            return i;
        case 'A':
            *k = KEY_UP;
            return i;
        case 'B':
            *k = KEY_DOWN;
            return i;
        case 'C':
            *k = KEY_RIGHT;
            return i;
        case 'D':
            *k = KEY_LEFT;
            return i;
        case 'F':
            *k = KEY_END;
            return i;
        case 'H':
            *k = KEY_HOME;
            return i;
        case 'I':
            *k = '\t';
            return i;
        case 'M':
            *k = '\r';
            return i;
        case 'P':
            *k = KEY_F1;
            return i;
        case 'Q':
            *k = KEY_F2;
            return i;
        case 'R':
            *k = KEY_F3;
            return i;
        case 'S':
            *k = KEY_F4;
            return i;
        case 'j':
            *k = '*';
            return i;
        case 'k':
            *k = '+';
            return i;
        case 'm':
            *k = '-';
            return i;
        case 'o':
            *k = '/';
            return i;
        }
        return 0;
    case '[':
        if (i >= length) return -1;
        switch(buf[i++]) {
        case '1':
            if (i >= length) return -1;
            switch(buf[i++]) {
            case '1':
                tmp = KEY_F1;
                goto check_trailing_tilde;
            case '2':
                tmp = KEY_F2;
                goto check_trailing_tilde;
            case '3':
                tmp = KEY_F3;
                goto check_trailing_tilde;
            case '4':
                tmp = KEY_F4;
                goto check_trailing_tilde;
            case '5':
                tmp = KEY_F5;
                goto check_delim;
            case '7':
                tmp = KEY_F6;
                goto check_delim;
            case '8':
                tmp = KEY_F7;
                goto check_delim;
            case '9':
                tmp = KEY_F8;
                goto check_delim;
            case ';':
                if (i >= length) {
                    return -1;
                } else {
                    const KeyCode mods = mod_enum_to_mod_mask(buf[i++]);
                    if (mods == 0) {
                        return 0;
                    } else if (i >= length) {
                        return -1;
                    }
                    switch(buf[i++]) {
                    case 'A':
                        *k = mods | KEY_UP;
                        return i;
                    case 'B':
                        *k = mods | KEY_DOWN;
                        return i;
                    case 'C':
                        *k = mods | KEY_RIGHT;
                        return i;
                    case 'D':
                        *k = mods | KEY_LEFT;
                        return i;
                    case 'F':
                        *k = mods | KEY_END;
                        return i;
                    case 'H':
                        *k = mods | KEY_HOME;
                        return i;
                    case 'P':
                        *k = mods | KEY_F1;
                        return i;
                    case 'Q':
                        *k = mods | KEY_F2;
                        return i;
                    case 'R':
                        *k = mods | KEY_F3;
                        return i;
                    case 'S':
                        *k = mods | KEY_F4;
                        return i;
                    }
                }
                return 0;
            case '~':
                *k = KEY_HOME;
                return i;
            }
            return 0;
        case '2':
            if (i >= length) return -1;
            switch(buf[i++]) {
            case '0':
                tmp = KEY_F9;
                goto check_delim;
            case '1':
                tmp = KEY_F10;
                goto check_delim;
            case '3':
                tmp = KEY_F11;
                goto check_delim;
            case '4':
                tmp = KEY_F12;
                goto check_delim;
            case ';':
                tmp = KEY_INSERT;
                goto check_modifiers;
            case '~':
                *k = KEY_INSERT;
                return i;
            }
            return 0;
        case '3':
            tmp = KEY_DELETE;
            goto check_delim;
        case '4':
            tmp = KEY_END;
            goto check_trailing_tilde;
        case '5':
            tmp = KEY_PAGE_UP;
            goto check_delim;
        case '6':
            tmp = KEY_PAGE_DOWN;
            goto check_delim;
        case 'A':
            *k = KEY_UP;
            return i;
        case 'B':
            *k = KEY_DOWN;
            return i;
        case 'C':
            *k = KEY_RIGHT;
            return i;
        case 'D':
            *k = KEY_LEFT;
            return i;
        case 'F':
            *k = KEY_END;
            return i;
        case 'H':
            *k = KEY_HOME;
            return i;
        case 'L':
            *k = KEY_INSERT;
            return i;
        case 'Z':
            *k = MOD_SHIFT | '\t';
            return i;
        case '[':
            if (i >= length) return -1;
            switch(buf[i++]) {
            case 'A':
                *k = KEY_F1;
                return i;
            case 'B':
                *k = KEY_F2;
                return i;
            case 'C':
                *k = KEY_F3;
                return i;
            case 'D':
                *k = KEY_F4;
                return i;
            case 'E':
                *k = KEY_F5;
                return i;
            }
            return 0;
        }
        return 0;
    }
    return 0;
check_delim:
    if (i >= length) return -1;
    switch(buf[i++]) {
    case ';':
        goto check_modifiers;
    case '~':
        *k = tmp;
        return i;
    }
    return 0;
check_modifiers:
    if (i >= length) {
        return -1;
    }
    const KeyCode mods = mod_enum_to_mod_mask(buf[i++]);
    if (mods == 0) {
        return 0;
    }
    tmp |= mods;
check_trailing_tilde:
    if (i >= length) {
        return -1;
    } else if (buf[i++] == '~') {
        *k = tmp;
        return i;
    }
    return 0;
}