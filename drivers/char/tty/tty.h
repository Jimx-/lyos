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

#ifndef _TTY_H_
#define _TTY_H_

#include "termios.h"
#include <sys/types.h>

#define TTY_IN_BYTES		256	/* tty input queue size */
#define TTY_OUT_BUF_LEN		2	/* tty output buffer size */

#define IN_CHAR       		0x00FF	/* low 8 bits */
#define IN_EOT				0x1000	/* End Of Text */

#define TAB_SIZE           8	/* distance between tab stops */
#define TAB_MASK           7	/* mask to compute a tab stop position */

/* minor device nr */
#define CONS_MINOR		0
#define LOG_MINOR		15
#define SERIAL_MINOR	16

#define CONS_ARG		20
    
struct s_tty;
struct s_console;

typedef void(*devfun_t)(struct s_tty *tp);
typedef void(*devfunarg_t)(struct s_tty *tp, char c);

/* TTY */
typedef struct s_tty
{
	u32	ibuf[TTY_IN_BYTES];	/* TTY input buffer */
	u32*	ibuf_head;		/* the next free slot */
	u32*	ibuf_tail;		/* the val to be processed by TTY */
	int	ibuf_cnt;		/* how many */

	int	tty_events;
	int	tty_incaller;
	int	tty_inid;
	void*	tty_inbuf;
	int	tty_inleft;
	int	tty_trans_cnt;
	int	tty_outcaller;
	int	tty_outid;
	void*	tty_outbuf;
	int	tty_outleft;
	int	tty_outcnt;
	pid_t 	tty_pgrp;
	int 	tty_min;
	int 	tty_eotcnt;

	int 	tty_select_ops;
	endpoint_t 	tty_select_proc;
	dev_t 	tty_select_minor;

	struct termios tty_termios;

	int 	tty_escaped;

	void *		tty_dev;
	devfun_t 	tty_devread;
	devfun_t 	tty_devwrite;
	devfunarg_t tty_echo;
}TTY;

#define buflen(buf)	(sizeof(buf) / sizeof((buf)[0]))
#define bufend(buf)	((buf) + buflen(buf))

#endif /* _TTY_H_ */
