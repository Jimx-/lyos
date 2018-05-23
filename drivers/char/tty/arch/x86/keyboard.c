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

#include <lyos/type.h>
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
#include "keyboard.h"
#include "keymap.h"
#include "lyos/proto.h"
#include <lyos/interrupt.h>
#include <lyos/portio.h>
#include <lyos/input.h>
#include "proto.h"

PRIVATE	struct kb_inbuf	kb_in;
PRIVATE	int		code_with_E0;
PRIVATE	int		shift_l;	/* l shift state	*/
PRIVATE	int		shift_r;	/* r shift state	*/
PRIVATE	int		alt_l;		/* l alt state		*/
PRIVATE	int		alt_r;		/* r left state		*/
PRIVATE	int		ctrl_l;		/* l ctrl state		*/
PRIVATE	int		ctrl_r;		/* l ctrl state		*/
PRIVATE	int		caps_lock;	/* Caps Lock		*/
PRIVATE	int		num_lock;	/* Num Lock		*/
PRIVATE	int		scroll_lock;	/* Scroll Lock		*/
PRIVATE	int		column;
PRIVATE endpoint_t  input_endpoint = NO_TASK;

PRIVATE u8	get_byte_from_kb_buf();
PRIVATE void	set_leds();
PRIVATE void	kb_wait();
PRIVATE void	kb_ack();

/*****************************************************************************
 *                                do_input
 *****************************************************************************/
/**
 * Handle input requests.
 * 
 * @param msg Ptr to message.
 *****************************************************************************/
PUBLIC void do_input(MESSAGE* msg)
{
	u32 v;
	int retval;
	u8 scan_code;

	switch (msg->type) {
	case INPUT_TTY_UP:
		retval = sysfs_retrieve_u32("services.input.endpoint", &v);

		if (retval == 0) input_endpoint = (endpoint_t)v;
		break;
	case INPUT_TTY_EVENT:
		if (msg->source != input_endpoint) return;
		if (msg->IEV_TYPE != EV_KEY) return;

		scan_code = (u8)msg->IEV_CODE;

		if (msg->IEV_VALUE == 0) scan_code |= FLAG_BREAK;

		if (kb_in.count < KB_IN_BYTES) {
			*(kb_in.p_head) = scan_code;
			kb_in.p_head++;
			if (kb_in.p_head == kb_in.buf + KB_IN_BYTES)
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
PUBLIC void init_keyboard()
{
	kb_in.count = 0;
	kb_in.p_head = kb_in.p_tail = kb_in.buf;

	shift_l	= shift_r = 0;
	alt_l	= alt_r   = 0;
	ctrl_l	= ctrl_r  = 0;

	caps_lock	= 0;
	num_lock	= 1;
	scroll_lock	= 0;

	column		= 0;

	set_leds();
}

PUBLIC void kb_init(TTY * tty)
{
	tty->tty_devread = keyboard_read;
}

/*****************************************************************************
 *                                keyboard_read
 *****************************************************************************/
/**
 * Yes, it is an ugly function.
 *
 * @param tty  Which TTY is reading the keyboard input.
 *****************************************************************************/
PUBLIC void keyboard_read(TTY* tty)
{
	if (!is_current_console((CONSOLE *)tty->tty_dev)) return;

	u8	scan_code;

	/**
	 * 1 : make
	 * 0 : break
	 */
	int	make;

	/**
	 * We use a integer to record a key press.
	 * For instance, if the key HOME is pressed, key will be evaluated to
	 * `HOME' defined in keyboard.h.
	 */
	u32	key = 0;


	/**
	 * This var points to a row in keymap[]. I don't use two-dimension
	 * array because I don't like it.
	 */
	u32*	keyrow;

	while (kb_in.count > 0) {
		code_with_E0 = 0;
		scan_code = get_byte_from_kb_buf();

		if ((key != PAUSEBREAK) && (key != PRINTSCREEN)) {
			int caps;

			/* make or break */
			make = (scan_code & FLAG_BREAK ? 0 : 1);
			
			keyrow = &keymap[(scan_code & 0177) * MAP_COLS];

			column = 0;

			caps = shift_l || shift_r;
			if (caps_lock &&
			    keyrow[0] >= 'a' && keyrow[0] <= 'z')
				caps = !caps;

			if (caps)
				column = 1;

			if (code_with_E0)
				column = 2;

			if (ctrl_l || ctrl_r) column = 3;

			key = keyrow[column];

			switch(key) {
			case SHIFT_L:
				shift_l	= make;
				break;
			case SHIFT_R:
				shift_r	= make;
				break;
			case CTRL_L:
				ctrl_l	= make;
				break;
			case CTRL_R:
				ctrl_r	= make;
				break;
			case ALT_L:
				alt_l	= make;
				break;
			case ALT_R:
				alt_l	= make;
				break;
			case CAPS_LOCK:
				if (make) {
					caps_lock   = !caps_lock;
					set_leds();
				}
				break;
			case NUM_LOCK:
				if (make) {
					num_lock    = !num_lock;
					set_leds();
				}
				break;
			case SCROLL_LOCK:
				if (make) {
					scroll_lock = !scroll_lock;
					set_leds();
				}
				break;
			default:
				break;
			}
		}

		if(make){ /* Break Code is ignored */
			int pad = 0;

			/* deal with the numpad first */
			if ((key >= PAD_SLASH) && (key <= PAD_9)) {
				pad = 1;
				switch(key) {	/* '/', '*', '-', '+',
						 * and 'Enter' in num pad
						 */
				case PAD_SLASH:
					key = '/';
					break;
				case PAD_STAR:
					key = '*';
					break;
				case PAD_MINUS:
					key = '-';
					break;
				case PAD_PLUS:
					key = '+';
					break;
				case PAD_ENTER:
					key = ENTER;
					break;
				default:
					/* the value of these keys
					 * depends on the Numlock
					 */
					if (num_lock) {	/* '0' ~ '9' and '.' in num pad */
						if (key >= PAD_0 && key <= PAD_9)
							key = key - PAD_0 + '0';
						else if (key == PAD_DOT)
							key = '.';
					}
					else{
						switch(key) {
						case PAD_HOME:
							key = HOME;
							break;
						case PAD_END:
							key = END;
							break;
						case PAD_PAGEUP:
							key = PAGEUP;
							break;
						case PAD_PAGEDOWN:
							key = PAGEDOWN;
							break;
						case PAD_INS:
							key = INSERT;
							break;
						case PAD_UP:
							key = UP;
							break;
						case PAD_DOWN:
							key = DOWN;
							break;
						case PAD_LEFT:
							key = LEFT;
							break;
						case PAD_RIGHT:
							key = RIGHT;
							break;
						case PAD_DOT:
							key = DELETE;
							break;
						default:
							break;
						}
					}
					break;
				}
			}

			int raw_code = key & MASK_RAW;
			switch(raw_code) {
			case UP:
				if (shift_l || shift_r) {	/* Shift + Up */
					scroll_screen((CONSOLE *)tty->tty_dev, SCR_DN);
				}
				break;
			case DOWN:
				if (shift_l || shift_r) {	/* Shift + Down */
					scroll_screen((CONSOLE *)tty->tty_dev, SCR_UP);
				}
				break;
			case F1:
			case F2:
			case F3:
			case F4:
			case F5:
			case F6:
			case F7:
			case F8:
			case F9:
			case F10:
			case F11:
			case F12:
				if (alt_l ||
				    alt_r) {	/* Alt + F1~F12 */
					select_console(raw_code - F1);
				}
				break;
			default:
				break;
			}

			/* normal key */
			if (key < 0xFF) {
				char rawchar = key;
				in_process(tty, &rawchar, 1);
			}
		}
	}
}

/*****************************************************************************
 *                                get_byte_from_kb_buf
 *****************************************************************************/
/**
 * Read a byte from the keyboard buffer.
 * 
 * @return The byte read.
 *****************************************************************************/
PRIVATE u8 get_byte_from_kb_buf()
{
	u8	scan_code;

	if (input_endpoint == NO_TASK) return 0;

	while (kb_in.count <= 0) {	 /* wait for a byte to arrive */
		MESSAGE input_msg;
		send_recv(RECEIVE, input_endpoint, &input_msg);
		do_input(&input_msg);
	}

	scan_code = *(kb_in.p_tail);
	kb_in.p_tail++;
	if (kb_in.p_tail == kb_in.buf + KB_IN_BYTES) {
		kb_in.p_tail = kb_in.buf;
	}
	kb_in.count--;

	return scan_code;
}


/*****************************************************************************
 *                                kb_wait
 *****************************************************************************/
/**
 * Wait until the input buffer of 8042 is empty.
 * 
 *****************************************************************************/
PRIVATE void kb_wait()
{
	u8 kb_stat;

	do {
		portio_inb(KB_CMD, &kb_stat);
	} while (kb_stat & 0x02);
}


/*****************************************************************************
 *                                kb_ack
 *****************************************************************************/
/**
 * Read from the keyboard controller until a KB_ACK is received.
 * 
 *****************************************************************************/
PRIVATE void kb_ack()
{
	u8 kb_read;

	do {
		portio_inb(KB_DATA, &kb_read);
	} while (kb_read != KB_ACK);
}


/*****************************************************************************
 *                                set_leds
 *****************************************************************************/
/**
 * Set the leds according to: caps_lock, num_lock & scroll_lock.
 * 
 *****************************************************************************/
PRIVATE void set_leds()
{
	u8 leds = (caps_lock << 2) | (num_lock << 1) | scroll_lock;

	kb_wait();
	portio_outb(KB_DATA, LED_CODE);
	kb_ack();

	kb_wait();
	portio_outb(KB_DATA, leds);
	kb_ack();
}

