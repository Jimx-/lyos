/*  This file is part of Lyos.

    Lyos is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Lyos is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Lyos.  If not, see <http://www.gnu.org/licenses/>. */

/********************************************************************/
/*    "key code" <--> "key" map.                                    */
/*    It should be and can only be included by keyboard.c!          */
/********************************************************************/

#ifndef _KEYMAP_H_
#define _KEYMAP_H_

#include <lyos/types.h>
#include <lyos/input.h>

/************************************************************************/
/*                          Macros Declaration                          */
/************************************************************************/

#define K(key) [KEY_##key]

#define C(ch) ((ch)&0x1F)
#define A(ch) ((ch) | 0x80)
#define CA(ch) A(C(ch))
#define L(ch) ((ch) | FLAG_HASCAPS)

#define FLAG_EXT 0x0100     /* Normal function keys		*/
#define FLAG_SHIFT 0x0200   /* Shift key			*/
#define FLAG_CTRL 0x0400    /* Shift key			*/
#define FLAG_ALT 0x0800     /* Control key			*/
#define FLAG_HASCAPS 0x1000 /* Caps Lock has effect */

/* Special keys */
#define GUI_L (0x05 + FLAG_EXT) /* L GUI	*/
#define GUI_R (0x06 + FLAG_EXT) /* R GUI	*/
#define APPS (0x07 + FLAG_EXT)  /* APPS	*/

/* Shift, Ctrl, Alt */
#define SHIFT_L (0x08 + FLAG_EXT) /* L Shift	*/
#define SHIFT_R (0x09 + FLAG_EXT) /* R Shift	*/
#define CTRL_L (0x0A + FLAG_EXT)  /* L Ctrl	*/
#define CTRL_R (0x0B + FLAG_EXT)  /* R Ctrl	*/
#define ALT_L (0x0C + FLAG_EXT)   /* L Alt	*/
#define ALT_R (0x0D + FLAG_EXT)   /* R Alt	*/

/* Lock keys */
#define CAPS_LOCK (0x0E + FLAG_EXT)   /* Caps Lock	*/
#define NUM_LOCK (0x0F + FLAG_EXT)    /* Number Lock	*/
#define SCROLL_LOCK (0x10 + FLAG_EXT) /* Scroll Lock	*/

/* Function keys */
#define F1 (0x11 + FLAG_EXT)  /* F1		*/
#define F2 (0x12 + FLAG_EXT)  /* F2		*/
#define F3 (0x13 + FLAG_EXT)  /* F3		*/
#define F4 (0x14 + FLAG_EXT)  /* F4		*/
#define F5 (0x15 + FLAG_EXT)  /* F5		*/
#define F6 (0x16 + FLAG_EXT)  /* F6		*/
#define F7 (0x17 + FLAG_EXT)  /* F7		*/
#define F8 (0x18 + FLAG_EXT)  /* F8		*/
#define F9 (0x19 + FLAG_EXT)  /* F9		*/
#define F10 (0x1A + FLAG_EXT) /* F10		*/
#define F11 (0x1B + FLAG_EXT) /* F11		*/
#define F12 (0x1C + FLAG_EXT) /* F12		*/

/* Ctrl + Fn */
#define CF1 (0x11 + FLAG_CTRL)
#define CF2 (0x12 + FLAG_CTRL)
#define CF3 (0x13 + FLAG_CTRL)
#define CF4 (0x14 + FLAG_CTRL)
#define CF5 (0x15 + FLAG_CTRL)
#define CF6 (0x16 + FLAG_CTRL)
#define CF7 (0x17 + FLAG_CTRL)
#define CF8 (0x18 + FLAG_CTRL)
#define CF9 (0x19 + FLAG_CTRL)
#define CF10 (0x1A + FLAG_CTRL)
#define CF11 (0x1B + FLAG_CTRL)
#define CF12 (0x1C + FLAG_CTRL)

/* Alt + Fn */
#define AF1 (0x11 + FLAG_ALT)
#define AF2 (0x12 + FLAG_ALT)
#define AF3 (0x13 + FLAG_ALT)
#define AF4 (0x14 + FLAG_ALT)
#define AF5 (0x15 + FLAG_ALT)
#define AF6 (0x16 + FLAG_ALT)
#define AF7 (0x17 + FLAG_ALT)
#define AF8 (0x18 + FLAG_ALT)
#define AF9 (0x19 + FLAG_ALT)
#define AF10 (0x1A + FLAG_ALT)
#define AF11 (0x1B + FLAG_ALT)
#define AF12 (0x1C + FLAG_ALT)

/* Shift + Fn */
#define SF1 (0x11 + FLAG_SHIFT)
#define SF2 (0x12 + FLAG_SHIFT)
#define SF3 (0x13 + FLAG_SHIFT)
#define SF4 (0x14 + FLAG_SHIFT)
#define SF5 (0x15 + FLAG_SHIFT)
#define SF6 (0x16 + FLAG_SHIFT)
#define SF7 (0x17 + FLAG_SHIFT)
#define SF8 (0x18 + FLAG_SHIFT)
#define SF9 (0x19 + FLAG_SHIFT)
#define SF10 (0x1A + FLAG_SHIFT)
#define SF11 (0x1B + FLAG_SHIFT)
#define SF12 (0x1C + FLAG_SHIFT)

/* Alt + Shift + Fn */
#define ASF1 (0x11 + FLAG_SHIFT + FLAG_ALT)
#define ASF2 (0x12 + FLAG_SHIFT + FLAG_ALT)
#define ASF3 (0x13 + FLAG_SHIFT + FLAG_ALT)
#define ASF4 (0x14 + FLAG_SHIFT + FLAG_ALT)
#define ASF5 (0x15 + FLAG_SHIFT + FLAG_ALT)
#define ASF6 (0x16 + FLAG_SHIFT + FLAG_ALT)
#define ASF7 (0x17 + FLAG_SHIFT + FLAG_ALT)
#define ASF8 (0x18 + FLAG_SHIFT + FLAG_ALT)
#define ASF9 (0x19 + FLAG_SHIFT + FLAG_ALT)
#define ASF10 (0x1A + FLAG_SHIFT + FLAG_ALT)
#define ASF11 (0x1B + FLAG_SHIFT + FLAG_ALT)
#define ASF12 (0x1C + FLAG_SHIFT + FLAG_ALT)

/* Control Pad */
#define PRINTSCREEN (0x1D + FLAG_EXT) /* Print Screen	*/
#define PAUSEBREAK (0x1E + FLAG_EXT)  /* Pause/Break	*/
#define INSERT (0x1F + FLAG_EXT)      /* Insert	*/
#define DELETE (0x20 + FLAG_EXT)      /* Delete	*/
#define HOME (0x21 + FLAG_EXT)        /* Home		*/
#define END (0x22 + FLAG_EXT)         /* End		*/
#define PAGEUP (0x23 + FLAG_EXT)      /* Page Up	*/
#define PAGEDOWN (0x24 + FLAG_EXT)    /* Page Down	*/
#define UP (0x25 + FLAG_EXT)          /* Up		*/
#define DOWN (0x26 + FLAG_EXT)        /* Down		*/
#define LEFT (0x27 + FLAG_EXT)        /* Left		*/
#define RIGHT (0x28 + FLAG_EXT)       /* Right	*/

/* Ctrl + Pad */
#define CINSERT (0x1F + FLAG_CTRL)
#define CHOME (0x21 + FLAG_CTRL)
#define CEND (0x22 + FLAG_CTRL)
#define CPAGEUP (0x23 + FLAG_CTRL)
#define CPAGEDOWN (0x24 + FLAG_CTRL)
#define CUP (0x25 + FLAG_CTRL)
#define CDOWN (0x26 + FLAG_CTRL)
#define CLEFT (0x27 + FLAG_CTRL)
#define CRIGHT (0x28 + FLAG_CTRL)

/* Alt + Pad */
#define AINSERT (0x1F + FLAG_ALT)
#define AHOME (0x21 + FLAG_ALT)
#define AEND (0x22 + FLAG_ALT)
#define APAGEUP (0x23 + FLAG_ALT)
#define APAGEDOWN (0x24 + FLAG_ALT)
#define AUP (0x25 + FLAG_ALT)
#define ADOWN (0x26 + FLAG_ALT)
#define ALEFT (0x27 + FLAG_ALT)
#define ARIGHT (0x28 + FLAG_ALT)

/* ACPI keys */
#define POWER (0x29 + FLAG_EXT) /* Power	*/
#define SLEEP (0x2A + FLAG_EXT) /* Sleep	*/
#define WAKE (0x2B + FLAG_EXT)  /* Wake Up	*/

/* Num Pad */
#define PAD_SLASH (0x2C + FLAG_EXT) /* /		*/
#define PAD_STAR (0x2D + FLAG_EXT)  /* *		*/
#define PAD_MINUS (0x2E + FLAG_EXT) /* -		*/
#define PAD_PLUS (0x2F + FLAG_EXT)  /* +		*/
#define PAD_ENTER (0x30 + FLAG_EXT) /* Enter	*/
#define PAD_DOT (0x31 + FLAG_EXT)   /* .		*/
#define PAD_0 (0x32 + FLAG_EXT)     /* 0		*/
#define PAD_1 (0x33 + FLAG_EXT)     /* 1		*/
#define PAD_2 (0x34 + FLAG_EXT)     /* 2		*/
#define PAD_3 (0x35 + FLAG_EXT)     /* 3		*/
#define PAD_4 (0x36 + FLAG_EXT)     /* 4		*/
#define PAD_5 (0x37 + FLAG_EXT)     /* 5		*/
#define PAD_6 (0x38 + FLAG_EXT)     /* 6		*/
#define PAD_7 (0x39 + FLAG_EXT)     /* 7		*/
#define PAD_8 (0x3A + FLAG_EXT)     /* 8		*/
#define PAD_9 (0x3B + FLAG_EXT)     /* 9		*/
#define PAD_UP PAD_8                /* Up		*/
#define PAD_DOWN PAD_2              /* Down		*/
#define PAD_LEFT PAD_4              /* Left		*/
#define PAD_RIGHT PAD_6             /* Right	*/
#define PAD_HOME PAD_7              /* Home		*/
#define PAD_END PAD_1               /* End		*/
#define PAD_PAGEUP PAD_9            /* Page Up	*/
#define PAD_PAGEDOWN PAD_3          /* Page Down	*/
#define PAD_INS PAD_0               /* Ins		*/
#define PAD_MID PAD_5               /* Middle key	*/
#define PAD_DEL PAD_DOT             /* Del		*/

#define MAP_COLS 6
#define NR_KEY_CODES 256

static u16 keymap[NR_KEY_CODES][MAP_COLS] = {
    K(A) = {L('a'), 'A', A('a'), A('a'), A('A'), C('A')},
    K(B) = {L('b'), 'B', A('b'), A('b'), A('B'), C('B')},
    K(C) = {L('c'), 'C', A('c'), A('c'), A('C'), C('C')},
    K(D) = {L('d'), 'D', A('d'), A('d'), A('D'), C('D')},
    K(E) = {L('e'), 'E', A('e'), A('e'), A('E'), C('E')},
    K(F) = {L('f'), 'F', A('f'), A('f'), A('F'), C('F')},
    K(G) = {L('g'), 'G', A('g'), A('g'), A('G'), C('G')},
    K(H) = {L('h'), 'H', A('h'), A('h'), A('H'), C('H')},
    K(I) = {L('i'), 'I', A('i'), A('i'), A('I'), C('I')},
    K(J) = {L('j'), 'J', A('j'), A('j'), A('J'), C('J')},
    K(K) = {L('k'), 'K', A('k'), A('k'), A('K'), C('K')},
    K(L) = {L('l'), 'L', A('l'), A('l'), A('L'), C('L')},
    K(M) = {L('m'), 'M', A('m'), A('m'), A('M'), C('M')},
    K(N) = {L('n'), 'N', A('n'), A('n'), A('N'), C('N')},
    K(O) = {L('o'), 'O', A('o'), A('o'), A('O'), C('O')},
    K(P) = {L('p'), 'P', A('p'), A('p'), A('P'), C('P')},
    K(Q) = {L('q'), 'Q', A('q'), A('q'), A('Q'), C('Q')},
    K(R) = {L('r'), 'R', A('r'), A('r'), A('R'), C('R')},
    K(S) = {L('s'), 'S', A('s'), A('s'), A('S'), C('S')},
    K(T) = {L('t'), 'T', A('t'), A('t'), A('T'), C('T')},
    K(U) = {L('u'), 'U', A('u'), A('u'), A('U'), C('U')},
    K(V) = {L('v'), 'V', A('v'), A('v'), A('V'), C('V')},
    K(W) = {L('w'), 'W', A('w'), A('w'), A('W'), C('W')},
    K(X) = {L('x'), 'X', A('x'), A('x'), A('X'), C('X')},
    K(Y) = {L('y'), 'Y', A('y'), A('y'), A('Y'), C('Y')},
    K(Z) = {L('z'), 'Z', A('z'), A('z'), A('Z'), C('Z')},
    K(1) = {'1', '!', A('1'), A('1'), A('!'), C('A')},
    K(2) = {'2', '@', A('2'), A('2'), A('@'), C('@')},
    K(3) = {'3', '#', A('3'), A('3'), A('#'), C('C')},
    K(4) = {'4', '$', A('4'), A('4'), A('$'), C('D')},
    K(5) = {'5', '%', A('5'), A('5'), A('%'), C('E')},
    K(6) = {'6', '^', A('6'), A('6'), A('^'), C('^')},
    K(7) = {'7', '&', A('7'), A('7'), A('&'), C('G')},
    K(8) = {'8', '*', A('8'), A('8'), A('*'), C('H')},
    K(9) = {'9', '(', A('9'), A('9'), A('('), C('I')},
    K(0) = {'0', ')', A('0'), A('0'), A(')'), C('@')},
    K(ENTER) = {C('M'), C('M'), CA('M'), CA('M'), CA('M'), C('J')},
    K(ESC) = {C('['), C('['), CA('['), CA('['), CA('['), C('[')},
    K(BACKSPACE) = {C('H'), C('H'), CA('H'), CA('H'), CA('H'), DELETE},
    K(TAB) = {C('I'), C('I'), CA('I'), CA('I'), CA('I'), C('I')},
    K(SPACE) = {' ', ' ', A(' '), A(' '), A(' '), C('@')},
    K(MINUS) = {'-', '_', A('-'), A('-'), A('_'), C('_')},
    K(EQUAL) = {'=', '+', A('='), A('='), A('+'), C('@')},
    K(LEFTBRACE) = {'[', '{', A('['), A('['), A('{'), C('[')},
    K(RIGHTBRACE) = {']', '}', A(']'), A(']'), A('}'), C(']')},
    K(BACKSLASH) = {'\\', '|', A('\\'), A('\\'), A('|'), C('\\')},
    K(SEMICOLON) = {';', ':', A(';'), A(';'), A(':'), C('@')},
    K(APOSTROPHE) = {'\'', '"', A('\''), A('\''), A('"'), C('@')},
    K(GRAVE) = {'`', '~', A('`'), A('`'), A('~'), C('@')},
    K(COMMA) = {',', '<', A(','), A(','), A('<'), C('@')},
    K(DOT) = {'.', '>', A('.'), A('.'), A('>'), C('@')},
    K(SLASH) = {'/', '?', A('/'), A('/'), A('?'), C('@')},
    K(CAPSLOCK) = {CAPS_LOCK, CAPS_LOCK, CAPS_LOCK, CAPS_LOCK, CAPS_LOCK,
                   CAPS_LOCK},
    K(LEFTCTRL) = {CTRL_L, CTRL_L, CTRL_L, CTRL_L, CTRL_L, CTRL_L},
    K(LEFTSHIFT) = {SHIFT_L, SHIFT_L, SHIFT_L, SHIFT_L, SHIFT_L, SHIFT_L},
    K(LEFTALT) = {ALT_L, ALT_L, ALT_L, ALT_L, ALT_L, ALT_L},
    K(RIGHTCTRL) = {CTRL_R, CTRL_R, CTRL_R, CTRL_R, CTRL_R, CTRL_R},
    K(RIGHTSHIFT) = {SHIFT_R, SHIFT_R, SHIFT_R, SHIFT_R, SHIFT_R, SHIFT_R},
    K(RIGHTALT) = {ALT_R, ALT_R, ALT_R, ALT_R, ALT_R, ALT_R},
    K(F1) = {F1, SF1, AF1, AF1, ASF1, CF1},
    K(F2) = {F2, SF2, AF2, AF2, ASF2, CF2},
    K(F3) = {F3, SF3, AF3, AF3, ASF3, CF3},
    K(F4) = {F4, SF4, AF4, AF4, ASF4, CF4},
    K(F5) = {F5, SF5, AF5, AF5, ASF5, CF5},
    K(F6) = {F6, SF6, AF6, AF6, ASF6, CF6},
    K(F7) = {F7, SF7, AF7, AF7, ASF7, CF7},
    K(F8) = {F8, SF8, AF8, AF8, ASF8, CF8},
    K(F9) = {F9, SF9, AF9, AF9, ASF9, CF9},
    K(F10) = {F10, SF10, AF10, AF10, ASF10, CF10},
    K(F11) = {F11, SF11, AF11, AF11, ASF11, CF11},
    K(F12) = {F12, SF12, AF12, AF12, ASF12, CF12},
    K(RIGHT) = {RIGHT, RIGHT, ARIGHT, ARIGHT, ARIGHT, CRIGHT},
    K(LEFT) = {LEFT, LEFT, ALEFT, ALEFT, ALEFT, CLEFT},
    K(DOWN) = {DOWN, DOWN, ADOWN, ADOWN, ADOWN, CDOWN},
    K(UP) = {UP, UP, AUP, AUP, AUP, CUP},
    K(INSERT) = {INSERT, INSERT, AINSERT, AINSERT, AINSERT, CINSERT},
    K(HOME) = {HOME, HOME, AHOME, AHOME, AHOME, CHOME},
    K(PAGEUP) = {PAGEUP, PAGEUP, APAGEUP, APAGEUP, APAGEUP, CPAGEUP},
    K(DELETE) = {DELETE, DELETE, A(DELETE), A(DELETE), A(DELETE), DELETE},
    K(END) = {END, END, AEND, AEND, AEND, CEND},
    K(PAGEDOWN) = {PAGEDOWN, PAGEDOWN, APAGEDOWN, APAGEDOWN, APAGEDOWN,
                   CPAGEDOWN},
};

#endif /* _KEYMAP_H_ */
