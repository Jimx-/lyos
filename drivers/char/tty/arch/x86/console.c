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

#include "lyos/type.h"
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "assert.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "tty.h"
#include "console.h"
#include "lyos/global.h"
#include "keyboard.h"
#include "lyos/proto.h"
#include <lyos/portio.h>
#include <lyos/vm.h>
#include <sys/mman.h>
#include "proto.h"
#include "global.h"

#define	V_MEM_BASE	0xB8000 /* base of color video memory */
#define	V_MEM_SIZE	0x8000	/* 32K: B8000H -> BFFFFH */

PUBLIC char * console_mem = NULL;

/* #define __TTY_DEBUG__ */

/* local routines */
PRIVATE void 	cons_write(TTY * tty);
PRIVATE void	parse_escape(CONSOLE * con, char c);
PRIVATE void 	do_escape(CONSOLE * con, char c);
PRIVATE void	flush(CONSOLE* con);
PRIVATE	void	w_copy(unsigned int dst, const unsigned int src, int size);
PRIVATE void	clear_screen(int pos, int len);

#define buflen(buf)	(sizeof(buf) / sizeof((buf)[0]))
#define bufend(buf)	((buf) + buflen(buf))

/*****************************************************************************
 *                                cons_write
 *****************************************************************************/
/**
 * Write chars to console.
 * 
 * @param tty The TTY struct.
 *****************************************************************************/
PRIVATE void cons_write(TTY * tty)
{
	char buf[TTY_OUT_BUF_LEN];
	char * p = tty->tty_outbuf;
	int i = tty->tty_outleft;
	int j;

	/* Nothing to write */
	if (i == 0) return;
	while (i) {
		int bytes = min(TTY_OUT_BUF_LEN, i);
		data_copy(TASK_TTY, buf, tty->tty_outprocnr, p, bytes);
		for (j = 0; j < bytes; j++)
		{
			tty->tty_outcnt++;
			out_char(tty, buf[j]);
		}
		tty->tty_outleft -= bytes;
		i -= bytes;
		p += bytes;
	}

	flush((CONSOLE *)tty->tty_dev);

	if (tty->tty_outleft == 0 && tty->tty_outcaller != KERNEL) {	/* done, reply to caller */
		MESSAGE msg;
		msg.type = tty->tty_outreply;
		msg.PROC_NR = tty->tty_outprocnr;
		msg.CNT = tty->tty_outcnt;
		send_recv(SEND, tty->tty_outcaller, &msg); 
	}
}

/*****************************************************************************
 *                                init_screen
 *****************************************************************************/
/**
 * Initialize the console of a certain tty.
 * 
 * @param tty  Whose console is to be initialized.
 *****************************************************************************/
PUBLIC void init_screen(TTY* tty)
{
	int nr_tty = tty - tty_table;
	CONSOLE * con = console_table + nr_tty;
	con->con_tty = tty;
	tty->tty_devwrite = cons_write;
	tty->tty_echo = out_char;

	current_console = 0;
	
	int using_fb = 0;
	static int first = 0;
	if (!first) {
		using_fb = fbcon_init();
		first = 1;
	}	

	if (!console_mem) {
		console_mem = mm_map_phys(SELF, V_MEM_BASE, V_MEM_SIZE);
		if (console_mem == MAP_FAILED) panic("can't map console memory");
	}

	/* 
	 * NOTE:
	 *   variables related to `position' and `size' below are
	 *   in WORDs, but not in BYTEs.
	 */

	if (using_fb) {
		fbcon_init_con(con);
	} else {
		vgacon_init_con(con);
	}

	con->row_size = con->cols << 1;
	con->screenbuf_size = con->rows * con->row_size;
	con->is_full = 0;

	tty->tty_dev = con;
	if (nr_tty == 0) {
		((CONSOLE *)tty->tty_dev)->cursor = 0;
	}

	flush((CONSOLE*)tty->tty_dev);
}


/*****************************************************************************
 *                                out_char
 *****************************************************************************/
/**
 * Print a char in a certain console.
 * 
 * @param con  The console to which the char is printed.
 * @param ch   The char to print.
 *****************************************************************************/
PUBLIC void out_char(TTY* tty, char ch)
{
	CONSOLE * con = tty->tty_dev;

	assert(con->cursor - con->origin < con->con_size);

	/*
	 * calculate the coordinate of cursor in current console (not in
	 * current screen)
	 */
	int cursor_x = (con->cursor - con->origin) % con->cols;
	int cursor_y = (con->cursor - con->origin) / con->cols;

	if (con->c_esc_state > 0) {	/* check for escape sequences */
		parse_escape(con, ch);
		return;
	}
  
	switch(ch) {
	case 000:
		return;
	case 007:		/* beep */
		//beep();
		break;
	case '\b':		/* backspace */
		if (con->cursor > con->origin) {
			if (con->outchar) con->outchar(con, ch);
			con->cursor--;
		}
		break;
	case '\n':		/* line feed */
		if ((con->con_tty->tty_termios.c_oflag & (OPOST|ONLCR))
						== (OPOST|ONLCR)) {
			con->cursor = con->origin + con->cols * cursor_y;
		}
	case 013:		/* CTRL-K */
	case 014:		/* CTRL-L */
		con->cursor = con->cursor + con->cols;
		break;
	case '\r':		/* carriage return */
		con->cursor = con->origin + con->cols * cursor_y;
		break;
	case '\t':		/* tab */
		con->cursor = con->origin + con->cols * cursor_y + ((cursor_x + TAB_SIZE) & ~TAB_MASK);
		break;
	case 033:		/* ESC - start of an escape sequence */
		con->c_esc_state = 1;
		return;
	default:
		if (con->outchar) con->outchar(con, ch);
		con->cursor++;
		break;
	}

	if (con->cursor - con->origin >= con->con_size) {
		cursor_x = (con->cursor - con->origin) % con->cols;
		cursor_y = (con->cursor - con->origin) / con->cols;
		int cp_origin = con->origin + (cursor_y + 1) * con->cols - SCR_SIZE;
		w_copy(con->origin, cp_origin, SCR_SIZE - con->cols);
		con->visible_origin = con->origin;
		con->scr_end = con->origin + SCR_SIZE;
		con->cursor = con->origin + (SCR_SIZE - con->cols) + cursor_x;
		clear_screen(con->cursor, con->cols);
		if (!con->is_full)
			con->is_full = 1;
	}

	assert(con->cursor - con->origin < con->con_size);

	while (con->cursor >= con->scr_end ||
	       con->cursor < con->visible_origin) {
		scroll_screen(con, SCR_UP);

		clear_screen(con->cursor, con->cols);
	}

	flush(con);
}

/*****************************************************************************
 *                                clear_screen
 *****************************************************************************/
/**
 * Write whitespaces to the screen.
 * 
 * @param pos  Write from here.
 * @param len  How many whitespaces will be written.
 *****************************************************************************/
PRIVATE void clear_screen(int pos, int len)
{
	u8 * pch = (u8*)(console_mem + pos * 2);
	while (--len >= 0) {
		*pch++ = ' ';
		*pch++ = DEFAULT_CHAR_COLOR;
	}
}


/*****************************************************************************
 *                            is_current_console
 *****************************************************************************/
/**
 * Uses `nr_current_console' to determine if a console is the current one.
 * 
 * @param con   Ptr to console.
 * 
 * @return   TRUE if con is the current console.
 *****************************************************************************/
PUBLIC int is_current_console(CONSOLE* con)
{
	return (con == &console_table[current_console]);
}


/*****************************************************************************
 *                     parse_escape
 *****************************************************************************/
PRIVATE void parse_escape(CONSOLE * con, char c)
{

	switch (con->c_esc_state){
		case 1:
		con->c_esc_intro = '\0';
		con->c_esc_paramp = bufend(con->c_esc_params);
		do {
			*--con->c_esc_paramp = 0;
		} while (con->c_esc_paramp > con->c_esc_params);
		switch (c) {
			case '[':	/* Control Sequence Introducer */
				con->c_esc_intro = c;
				con->c_esc_state = 2;
				break;
			case 'M':	/* Reverse Index */
				do_escape(con, c);
				break;
			default:
				con->c_esc_state = 0;
		}
		break;

		case 2:
		if (c >= '0' && c <= '9') {
			if (con->c_esc_paramp < bufend(con->c_esc_params))
				*con->c_esc_paramp = *con->c_esc_paramp * 10 + (c-'0');
		} else
		if (c == ';') {
			if (con->c_esc_paramp < bufend(con->c_esc_params))
				con->c_esc_paramp++;
		} else {
			do_escape(con, c);
		}
		break;
	}
}

/*****************************************************************************
 *						do_escape
 *****************************************************************************/
PRIVATE void do_escape(CONSOLE * con, char c)
{
	int value, m, n;
	unsigned src, dst, count;
	//int *paramp;
	
	int cursor_x = (con->cursor - con->origin) % con->cols;
	int cursor_y = (con->cursor - con->origin) / con->cols;

	flush(con);

	if (con->c_esc_intro == '\0') {
		switch (c) {
			case 'M':		/* Reverse Index */
			if (cursor_y == 0) {
				scroll_screen(con, SCR_DN);
			} else {
				con->cursor = con->origin + con->cols * (cursor_y - 1);
			}
			flush(con);
			break;
			default: break;
		}
	} else
	if (con->c_esc_intro == '[') {
		value = con->c_esc_params[0];
		switch (c) {
	    case 'A':		/* ESC [nA moves up n lines */
			n = (value == 0 ? 1 : value);
			con->cursor = con->origin + con->cols * (cursor_y - n);
			flush(con);
			break;
		case 'B':		/* ESC [nB moves down n lines */
			n = (value == 0 ? 1 : value);
			con->cursor = con->origin + con->cols * (cursor_y + n);
			flush(con);
			break;
	    case 'C':		/* ESC [nC moves right n spaces */
			n = (value == 0 ? 1 : value);
			con->cursor = con->origin + con->cols * cursor_y + cursor_x + n;
			flush(con);
			break;
		case 'D':		/* ESC [nD moves left n spaces */
			n = (value == 0 ? 1 : value);
			con->cursor = con->origin + con->cols * cursor_y + cursor_x - n;
			flush(con);
			break;
		case 'H':		/* ESC [m;nH" moves cursor to (m,n) */
			m = con->c_esc_params[0] - 1;
			n = con->c_esc_params[1] - 1;
			con->cursor = con->origin + con->cols * m + n;
			flush(con);
		break;
	    case 'J':		/* ESC [sJ clears in display */
		switch (value) {
		    case 0:	/* Clear from cursor to end of screen */
			count = SCR_SIZE - (con->cursor - con->origin);
			dst = con->cursor;
			break;
		    case 1:	/* Clear from start of screen to cursor */
			count = con->cursor - con->origin;
			dst = con->origin;
			break;
		    case 2:	/* Clear entire screen */
			count = SCR_SIZE;
			dst = con->origin;
			break;
		    default:	/* Do nothing */
			count = 0;
			dst = con->origin;
		}
		clear_screen(dst, count);
		break;
	    case 'K':		/* ESC [sK clears line from cursor */
		switch (value) {
		    case 0:	/* Clear from cursor to end of line */
			count = con->cols - cursor_x;
			dst = con->cursor;
			break;
		    case 1:	/* Clear from beginning of line to cursor */
			count = cursor_x;
			dst = con->cursor - cursor_x;
			break;
		    case 2:	/* Clear entire line */
			count = con->cols;
			dst = con->cursor - cursor_x;
			break;
		    default:	/* Do nothing */
			count = 0;
			dst = con->cursor;
		}
		clear_screen(dst, count);
		break;
	    case 'L':		/* ESC [nL inserts n lines at cursor */
		n = value;
		if (n < 1) n = 1;
		if (n > (25 - cursor_y))
			n = 25 - cursor_y;

		src = con->origin + cursor_y * con->cols;
		dst = src + n * con->cols;
		count = n * con->cols;
		w_copy(dst, src, count);
		clear_screen(src, count);
		break;
	    case 'M':		/* ESC [nM deletes n lines at cursor */
		n = value;
		if (n < 1) n = 1;
		if (n > (25 - cursor_y))
			n = 25 - cursor_y;

		dst = con->origin + cursor_y * con->cols;
		src = dst + n * con->cols;
		count = (25 - cursor_y - n) * con->cols;
		w_copy(dst, src, count);
		clear_screen(dst + count, n * con->cols);
		break;
	    case '@':		/* ESC [n@ inserts n chars at cursor */
		n = value;
		if (n < 1) n = 1;
		if (n > (con->cols - cursor_x))
			n = con->cols - cursor_x;

		src = con->cursor;
		dst = src + n;
		count = con->cols - cursor_x - n;
		w_copy(dst, src, count);
		clear_screen(src, n);
		break;
	    case 'P':		/* ESC [nP deletes n chars at cursor */
		n = value;
		if (n < 1) n = 1;
		if (n > (con->cols - cursor_x))
			n = con->cols - cursor_x;

		dst = con->cursor;
		src = dst + n;
		count = con->cols - cursor_x - n;
		w_copy(dst, src, count);
		clear_screen(dst + count, n);
		break;
		}
	}
	con->c_esc_state = 0;
}

/*****************************************************************************
 *                                select_console
 *****************************************************************************/
/**
 * Select a console as the current.
 * 
 * @param nr_console   Console nr, range in [0, NR_CONSOLES-1].
 *****************************************************************************/
PUBLIC void select_console(int nr_console)
{
	if ((nr_console < 0) || (nr_console >= NR_CONSOLES)) return;

	flush(&console_table[current_console = nr_console]);
}


/*****************************************************************************
 *                                scroll_screen
 *****************************************************************************/
/**
 * Scroll the screen.
 *
 * Note that scrolling UP means the content of the screen will go upwards, so
 * that the user can see lines below the bottom. Similarly scrolling DOWN means
 * the content of the screen will go downwards so that the user can see lines
 * above the top.
 *
 * When there is no line below the bottom of the screen, scrolling UP takes no
 * effects; when there is no line above the top of the screen, scrolling DOWN
 * takes no effects.
 * 
 * @param con   The console whose screen is to be scrolled.
 * @param dir   SCR_UP : scroll the screen upwards;
 *              SCR_DN : scroll the screen downwards
 *****************************************************************************/
PUBLIC void scroll_screen(CONSOLE* con, int dir)
{
	/*
	 * variables below are all in-console-offsets (based on con->origin)
	 */
	int oldest; /* addr of the oldest available line in the console */
	int newest; /* .... .. ... latest ......... .... .. ... ....... */
	int scr_top;/* position of the top of current screen */
	int scr_end;/* position of the bottom of current screen */

	newest = (con->cursor - con->origin) / con->cols * con->cols;
	oldest = con->is_full ? (newest + con->cols) % con->con_size : 0;
	scr_top = con->visible_origin - con->origin;
	scr_end = con->scr_end - con->origin;

	if (dir == SCR_DN) {
		if (!con->is_full && scr_top > 0) {
			con->visible_origin -= con->cols;
			con->scr_end -= con->cols;
		}
		else if (con->is_full && scr_top != oldest) {
			if (con->cursor - con->origin >= con->con_size - SCR_SIZE) {
				if (con->visible_origin != con->origin)
					con->visible_origin -= con->cols;
			}
			else if (con->visible_origin == con->origin) {
				scr_top = con->con_size - SCR_SIZE;
				con->visible_origin = con->origin + scr_top;
			}
			else {
				con->visible_origin -= con->cols;
			}
		}
	}
	else if (dir == SCR_UP) {
		if (!con->is_full && newest >= scr_end) {
			con->visible_origin += con->cols;
			con->scr_end += con->cols;
		}
		else if (con->is_full && scr_end - con->cols != newest) {
			if (scr_end == con->con_size) {
				int scr_size = scr_end - scr_top;
				con->visible_origin = con->origin;
				con->scr_end = con->visible_origin + scr_size;
			}
			else {
				con->visible_origin += con->cols;
				con->scr_end += con->cols;
			}
		}
	}
	else {
		assert(dir == SCR_DN || dir == SCR_UP);
	}

	flush(con);
}


/*****************************************************************************
 *                                flush
 *****************************************************************************/
/**
 * Set the cursor and starting address of a console by writing the
 * CRT Controller Registers.
 * 
 * @param con  The console to be set.
 *****************************************************************************/
PRIVATE void flush(CONSOLE* con)
{
	if (is_current_console(con) && con->flush) {
		con->flush(con);
	}

#ifdef __TTY_DEBUG__
	int lineno = 0;
	for (lineno = 0; lineno < con->con_size / con->cols; lineno++) {
		u8 * pch = (u8*)(console_mem +
				   (con->origin + (lineno + 1) * con->cols) * 2
				   - 4);
		*pch++ = lineno / 10 + '0';
		*pch++ = RED_CHAR;
		*pch++ = lineno % 10 + '0';
		*pch++ = RED_CHAR;
	}
#endif
}

/*****************************************************************************
 *                                w_copy
 *****************************************************************************/
/**
 * Copy data in WORDS.
 *
 * Note that the addresses of dst and src are not pointers, but integers, 'coz
 * in most cases we pass integers into it as parameters.
 * 
 * @param dst   Addr of destination.
 * @param src   Addr of source.
 * @param size  How many words will be copied.
 *****************************************************************************/
PRIVATE	void w_copy(unsigned int dst, const unsigned int src, int size)
{
	memcpy((void*)(console_mem + (dst << 1)), (void*)(console_mem + (src << 1)), size << 1);
}
