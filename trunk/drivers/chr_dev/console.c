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

#include "type.h"
#include "stdio.h"
#include "unistd.h"
#include "const.h"
#include "protect.h"
#include "string.h"
#include "fs.h"
#include "proc.h"
#include "tty.h"
#include "console.h"
#include "global.h"
#include "keyboard.h"
#include "proto.h"

/* #define __TTY_DEBUG__ */

/* local routines */
PRIVATE void	set_cursor(unsigned int position);
PRIVATE void	set_video_start_addr(u32 addr);
PRIVATE void	parse_escape(CONSOLE * con, char c);
PRIVATE void 	do_escape(CONSOLE * con, char c);
PRIVATE void	flush(CONSOLE* con);
PRIVATE	void	w_copy(unsigned int dst, const unsigned int src, int size);
PRIVATE void	clear_screen(int pos, int len);

#define buflen(buf)	(sizeof(buf) / sizeof((buf)[0]))
#define bufend(buf)	((buf) + buflen(buf))

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
	tty->console = console_table + nr_tty;

	/* 
	 * NOTE:
	 *   variables related to `position' and `size' below are
	 *   in WORDs, but not in BYTEs.
	 */
	int v_mem_size = V_MEM_SIZE >> 1; /* size of Video Memory */
	int size_per_con = v_mem_size / NR_CONSOLES;
	tty->console->orig = nr_tty * size_per_con;
	tty->console->con_size = size_per_con / SCR_WIDTH * SCR_WIDTH;
	tty->console->cursor = tty->console->crtc_start = tty->console->orig;
	tty->console->is_full = 0;

	if (nr_tty == 0) {
		tty->console->cursor = disp_pos / 2;
		disp_pos = 0;
	}
	else {
		/* 
		 * `?' in this string will be replaced with 0, 1, 2, ...
		 */
		const char prompt[] = "[tty #?]\n\nWelcome to Lyos \n\n";

		const char * p = prompt;
		for (; *p; p++)
			out_char(tty->console, *p == '?' ? nr_tty + '0' : *p);
	}

	set_cursor(tty->console->cursor);
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
PUBLIC void out_char(CONSOLE* con, char ch)
{
	u8* pch = (u8*)(V_MEM_BASE + con->cursor * 2);

	assert(con->cursor - con->orig < con->con_size);

	/*
	 * calculate the coordinate of cursor in current console (not in
	 * current screen)
	 */
	int cursor_x = (con->cursor - con->orig) % SCR_WIDTH;
	int cursor_y = (con->cursor - con->orig) / SCR_WIDTH;

	if (con->c_esc_state > 0) {	/* check for escape sequences */
		parse_escape(con, ch);
		return;
	}
  
	switch(ch) {
	case 007:
		//beep();
		break;
	case '\n':
		con->cursor = con->orig + SCR_WIDTH * (cursor_y + 1);
		break;
	case '\b':
		if (con->cursor > con->orig) {
			con->cursor--;
			*(pch - 2) = ' ';
			*(pch - 1) = DEFAULT_CHAR_COLOR;
		}
		break;
	case '\r':
		con->cursor = con->orig + SCR_WIDTH * cursor_y;
		break;
	case '\t':		/* tab */
		con->cursor = con->orig + SCR_WIDTH * cursor_y + ((cursor_x + TAB_SIZE) & ~TAB_MASK);
		break;
	case 033:		/* ESC - start of an escape sequence */
		con->c_esc_state = 1;
		return;
	default:
		*pch++ = ch;
		*pch++ = DEFAULT_CHAR_COLOR;
		con->cursor++;
		break;
	}

	if (con->cursor - con->orig >= con->con_size) {
		cursor_x = (con->cursor - con->orig) % SCR_WIDTH;
		cursor_y = (con->cursor - con->orig) / SCR_WIDTH;
		int cp_orig = con->orig + (cursor_y + 1) * SCR_WIDTH - SCR_SIZE;
		w_copy(con->orig, cp_orig, SCR_SIZE - SCR_WIDTH);
		con->crtc_start = con->orig;
		con->cursor = con->orig + (SCR_SIZE - SCR_WIDTH) + cursor_x;
		clear_screen(con->cursor, SCR_WIDTH);
		if (!con->is_full)
			con->is_full = 1;
	}

	assert(con->cursor - con->orig < con->con_size);

	while (con->cursor >= con->crtc_start + SCR_SIZE ||
	       con->cursor < con->crtc_start) {
		scroll_screen(con, SCR_UP);

		clear_screen(con->cursor, SCR_WIDTH);
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
	u8 * pch = (u8*)(V_MEM_BASE + pos * 2);
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
 *                                set_cursor
 *****************************************************************************/
/**
 * Display the cursor by setting CRTC (6845 compatible) registers.
 * 
 * @param position  Position of the cursor based on the beginning of the video
 *                  memory. Note that it counts in WORDs, not in BYTEs.
 *****************************************************************************/
PRIVATE void set_cursor(unsigned int position)
{
	disable_int();
	out_byte(CRTC_ADDR_REG, CURSOR_H);
	out_byte(CRTC_DATA_REG, (position >> 8) & 0xFF);
	out_byte(CRTC_ADDR_REG, CURSOR_L);
	out_byte(CRTC_DATA_REG, position & 0xFF);
	enable_int();
}


/*****************************************************************************
 *                                set_video_start_addr
 *****************************************************************************/
/**
 * Routine for hardware screen scrolling.
 * 
 * @param addr  Offset in the video memory.
 *****************************************************************************/
PRIVATE void set_video_start_addr(u32 addr)
{
	disable_int();
	out_byte(CRTC_ADDR_REG, START_ADDR_H);
	out_byte(CRTC_DATA_REG, (addr >> 8) & 0xFF);
	out_byte(CRTC_ADDR_REG, START_ADDR_L);
	out_byte(CRTC_DATA_REG, addr & 0xFF);
	enable_int();
}

/*****************************************************************************
 *                     parse_escape
 *****************************************************************************/
PRIVATE void	parse_escape(CONSOLE * con, char c)
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
	
	int cursor_x = (con->cursor - con->orig) % SCR_WIDTH;
	int cursor_y = (con->cursor - con->orig) / SCR_WIDTH;

	flush(con);

	if (con->c_esc_intro == '\0') {
		switch (c) {
			case 'M':		/* Reverse Index */
			if (cursor_y == 0) {
				scroll_screen(con, SCR_DN);
			} else {
				con->cursor = con->orig + SCR_WIDTH * (cursor_y - 1);
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
			con->cursor = con->orig + SCR_WIDTH * (cursor_y - n);
			flush(con);
			break;
		case 'B':		/* ESC [nB moves down n lines */
			n = (value == 0 ? 1 : value);
			con->cursor = con->orig + SCR_WIDTH * (cursor_y + n);
			flush(con);
			break;
	    case 'C':		/* ESC [nC moves right n spaces */
			n = (value == 0 ? 1 : value);
			con->cursor = con->orig + SCR_WIDTH * cursor_y + cursor_x + n;
			flush(con);
			break;
		case 'D':		/* ESC [nD moves left n spaces */
			n = (value == 0 ? 1 : value);
			con->cursor = con->orig + SCR_WIDTH * cursor_y + cursor_x - n;
			flush(con);
			break;
		case 'H':		/* ESC [m;nH" moves cursor to (m,n) */
			m = con->c_esc_params[0] - 1;
			n = con->c_esc_params[1] - 1;
			con->cursor = con->orig + SCR_WIDTH * m + n;
			flush(con);
		break;
	    case 'J':		/* ESC [sJ clears in display */
		switch (value) {
		    case 0:	/* Clear from cursor to end of screen */
			count = SCR_SIZE - (con->cursor - con->orig);
			dst = con->cursor;
			break;
		    case 1:	/* Clear from start of screen to cursor */
			count = con->cursor - con->orig;
			dst = con->orig;
			break;
		    case 2:	/* Clear entire screen */
			count = SCR_SIZE;
			dst = con->orig;
			break;
		    default:	/* Do nothing */
			count = 0;
			dst = con->orig;
		}
		clear_screen(dst, count);
		break;
	    case 'K':		/* ESC [sK clears line from cursor */
		switch (value) {
		    case 0:	/* Clear from cursor to end of line */
			count = SCR_WIDTH - cursor_x;
			dst = con->cursor;
			break;
		    case 1:	/* Clear from beginning of line to cursor */
			count = cursor_x;
			dst = con->cursor - cursor_x;
			break;
		    case 2:	/* Clear entire line */
			count = SCR_WIDTH;
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

		src = con->orig + cursor_y * SCR_WIDTH;
		dst = src + n * SCR_WIDTH;
		count = n * SCR_WIDTH;
		w_copy(dst, src, count);
		clear_screen(src, count);
		break;
	    case 'M':		/* ESC [nM deletes n lines at cursor */
		n = value;
		if (n < 1) n = 1;
		if (n > (25 - cursor_y))
			n = 25 - cursor_y;

		dst = con->orig + cursor_y * SCR_WIDTH;
		src = dst + n * SCR_WIDTH;
		count = (25 - cursor_y - n) * SCR_WIDTH;
		w_copy(dst, src, count);
		clear_screen(dst + count, n * SCR_WIDTH);
		break;
	    case '@':		/* ESC [n@ inserts n chars at cursor */
		n = value;
		if (n < 1) n = 1;
		if (n > (SCR_WIDTH - cursor_x))
			n = SCR_WIDTH - cursor_x;

		src = con->cursor;
		dst = src + n;
		count = SCR_WIDTH - cursor_x - n;
		w_copy(dst, src, count);
		clear_screen(src, n);
		break;
	    case 'P':		/* ESC [nP deletes n chars at cursor */
		n = value;
		if (n < 1) n = 1;
		if (n > (SCR_WIDTH - cursor_x))
			n = SCR_WIDTH - cursor_x;

		dst = con->cursor;
		src = dst + n;
		count = SCR_WIDTH - cursor_x - n;
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
	 * variables below are all in-console-offsets (based on con->orig)
	 */
	int oldest; /* addr of the oldest available line in the console */
	int newest; /* .... .. ... latest ......... .... .. ... ....... */
	int scr_top;/* position of the top of current screen */

	newest = (con->cursor - con->orig) / SCR_WIDTH * SCR_WIDTH;
	oldest = con->is_full ? (newest + SCR_WIDTH) % con->con_size : 0;
	scr_top = con->crtc_start - con->orig;

	if (dir == SCR_DN) {
		if (!con->is_full && scr_top > 0) {
			con->crtc_start -= SCR_WIDTH;
		}
		else if (con->is_full && scr_top != oldest) {
			if (con->cursor - con->orig >= con->con_size - SCR_SIZE) {
				if (con->crtc_start != con->orig)
					con->crtc_start -= SCR_WIDTH;
			}
			else if (con->crtc_start == con->orig) {
				scr_top = con->con_size - SCR_SIZE;
				con->crtc_start = con->orig + scr_top;
			}
			else {
				con->crtc_start -= SCR_WIDTH;
			}
		}
	}
	else if (dir == SCR_UP) {
		if (!con->is_full && newest >= scr_top + SCR_SIZE) {
			con->crtc_start += SCR_WIDTH;
		}
		else if (con->is_full && scr_top + SCR_SIZE - SCR_WIDTH != newest) {
			if (scr_top + SCR_SIZE == con->con_size)
				con->crtc_start = con->orig;
			else
				con->crtc_start += SCR_WIDTH;
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
	if (is_current_console(con)) {
		set_cursor(con->cursor);
		set_video_start_addr(con->crtc_start);
	}

#ifdef __TTY_DEBUG__
	int lineno = 0;
	for (lineno = 0; lineno < con->con_size / SCR_WIDTH; lineno++) {
		u8 * pch = (u8*)(V_MEM_BASE +
				   (con->orig + (lineno + 1) * SCR_WIDTH) * 2
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
	phys_copy((void*)(V_MEM_BASE + (dst << 1)),
		  (void*)(V_MEM_BASE + (src << 1)),
		  size << 1);
}

