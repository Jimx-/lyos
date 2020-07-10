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

#include <lyos/types.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include <errno.h>
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "tty.h"
#include "console.h"
#include "lyos/global.h"
#include "keymap.h"
#include "lyos/proto.h"
#include <lyos/interrupt.h>
#include <lyos/portio.h>
#include <lyos/input.h>
#include <libsysfs/libsysfs.h>
#include "proto.h"

#define RELEASE_BIT (0x10000)

#define KB_INBUF_SIZE 256

struct kb_inbuf {
    u32* p_head;
    u32* p_tail;
    size_t count;
    u32 buf[KB_INBUF_SIZE];
};

static struct kb_inbuf kb_in;

static int shift_l;     /* l shift state	*/
static int shift_r;     /* r shift state	*/
static int shift;       /* shift state	*/
static int alt_l;       /* l alt state		*/
static int alt_r;       /* r alt state		*/
static int alt;         /* alt state */
static int ctrl_l;      /* l ctrl state		*/
static int ctrl_r;      /* l ctrl state		*/
static int ctrl;        /* ctrl state		*/
static int caps_lock;   /* Caps Lock		*/
static int num_lock;    /* Num Lock		*/
static int scroll_lock; /* Scroll Lock		*/

static endpoint_t input_endpoint = NO_TASK;

static char* fn_map[12] = {"11", "12", "13", "14", "15", "17",  /* F1-F6 */
                           "18", "19", "20", "21", "23", "24"}; /* F7-F12 */
static char numpad_map[10] = {'@', ' ', 'H', 'Y', 'G', 'S', 'A', 'B', 'D', 'C'};

static u32 get_keycode_from_kb_buf();
static int map_key(u32 keycode);
static int parse_keycode(u32 keycode);

static void set_leds();

/*****************************************************************************
 *                                do_input
 *****************************************************************************/
/**
 * Handle input requests.
 *
 * @param msg Ptr to message.
 *****************************************************************************/
void do_input(MESSAGE* msg)
{
    u32 v;
    int retval;
    u16 keycode;
    int release;

    switch (msg->type) {
    case INPUT_TTY_UP:
        retval = sysfs_retrieve_u32("services.input.endpoint", &v);

        if (retval == 0) input_endpoint = (endpoint_t)v;
        break;

    case INPUT_TTY_EVENT:
        if (msg->source != input_endpoint) return;
        if (msg->u.m_input_tty_event.type != EV_KEY) return;

        keycode = msg->u.m_input_tty_event.code;
        release = !(msg->u.m_input_tty_event.value);

        if (kb_in.count < KB_INBUF_SIZE) {
            *(kb_in.p_head) = keycode | (release ? RELEASE_BIT : 0);
            kb_in.p_head++;
            if (kb_in.p_head == kb_in.buf + KB_INBUF_SIZE)
                kb_in.p_head = kb_in.buf;
            kb_in.count++;
        }

        break;

    default:
        return;
    }
}

/*****************************************************************************
 *                                init_keyboard
 *****************************************************************************/
/**
 * <Ring 1> Initialize some variables and set keyboard interrupt handler.
 *
 *****************************************************************************/
void init_keyboard()
{
    kb_in.count = 0;
    kb_in.p_head = kb_in.p_tail = kb_in.buf;

    shift = shift_l = shift_r = 0;
    alt = alt_l = alt_r = 0;
    ctrl = ctrl_l = ctrl_r = 0;

    caps_lock = 0;
    num_lock = 1;
    scroll_lock = 0;

    set_leds();
}

void kb_init(TTY* tty) { tty->tty_devread = keyboard_read; }

/*****************************************************************************
 *                                keyboard_read
 *****************************************************************************/
/**
 * Yes, it is an ugly function.
 *
 * @param tty  Which TTY is reading the keyboard input.
 *****************************************************************************/
void keyboard_read(TTY* tty)
{
    char buf[7], suffix;
    u32 keycode;
    unsigned int ch;

    if (!is_current_console((CONSOLE*)tty->tty_dev)) return;

    while (kb_in.count > 0) {
        keycode = get_keycode_from_kb_buf();

        ch = parse_keycode(keycode);

        if (ch <= 0xff) {
            buf[0] = ch;
            in_process(tty, buf, 1);
        } else if ((ch >= INSERT) && (ch <= RIGHT)) {
            buf[0] = '\033';
            buf[1] = '[';
            buf[2] = numpad_map[ch - INSERT];
            in_process(tty, buf, 3);
        } else if (((ch >= F1) && (ch <= F12)) ||
                   ((ch >= SF1) && (ch <= SF12)) ||
                   ((ch >= CF1) && (ch <= CF12))) {
            /* function keys */
            if ((ch >= F1) && (ch <= F12)) {
                ch -= F1;
                suffix = 0;
            } else if ((ch >= SF1) && (ch <= SF12)) {
                ch -= SF1;
                suffix = '2';
            } else {
                ch -= CF1;
                suffix = shift ? '6' : '5';
            }

            buf[0] = '\033';
            buf[1] = '[';
            buf[2] = fn_map[ch][0];
            buf[3] = fn_map[ch][1];

            if (suffix) {
                buf[4] = ';';
                buf[5] = suffix;
                buf[6] = '~';
            } else {
                buf[4] = '~';
            }

            in_process(tty, buf, suffix ? 7 : 5);
        }
    }
}

static int map_key(u32 keycode)
{
    int column, caps;
    u16* row = keymap[keycode];

    caps = shift;
    if (caps_lock && (row[0] & FLAG_HASCAPS)) caps = !caps;

    if (alt) {
        column = 2;
        if (ctrl || alt_r) {
            column = 3;
        }
        if (caps) column = 4;
    } else {
        column = 0;
        if (caps) column = 1;
        if (ctrl) column = 5;
    }

    return row[column] & ~FLAG_HASCAPS;
}

static int parse_keycode(u32 keycode)
{
    int release;
    int ch;

    release = !!(keycode & RELEASE_BIT);
    ch = map_key(keycode & ~RELEASE_BIT);

    switch (ch) {
    case CTRL_L:
    case CTRL_R:
        *(ch == CTRL_L ? &ctrl_l : &ctrl_r) = !release;
        ctrl = ctrl_l || ctrl_r;
        break;
    case ALT_L:
    case ALT_R:
        *(ch == ALT_L ? &alt_l : &alt_r) = !release;
        alt = alt_l || alt_r;
        break;
    case SHIFT_L:
    case SHIFT_R:
        *(ch == SHIFT_L ? &shift_l : &shift_r) = !release;
        shift = shift_l || shift_r;
        break;
    case CAPS_LOCK:
        caps_lock = !caps_lock;
        break;
    case NUM_LOCK:
        num_lock = !num_lock;
        break;
    case SCROLL_LOCK:
        scroll_lock = !scroll_lock;
        break;
    default:
        if (release) return -1;
        if (ch) return ch;
    }

    return -1;
}

/*****************************************************************************
 *                                get_byte_from_kb_buf
 *****************************************************************************/
/**
 * Read a byte from the keyboard buffer.
 *
 * @return The byte read.
 *****************************************************************************/
static u32 get_keycode_from_kb_buf()
{
    u32 keycode;

    if (input_endpoint == NO_TASK) return 0;

    keycode = *(kb_in.p_tail);
    kb_in.p_tail++;
    if (kb_in.p_tail == kb_in.buf + KB_INBUF_SIZE) {
        kb_in.p_tail = kb_in.buf;
    }
    kb_in.count--;

    return keycode;
}

/*****************************************************************************
 *                                set_leds
 *****************************************************************************/
/**
 * Set the leds according to: caps_lock, num_lock & scroll_lock.
 *
 *****************************************************************************/
static void set_leds() {}
