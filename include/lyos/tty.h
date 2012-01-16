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

#define TTY_IN_BYTES		256	/* tty input queue size */
#define TTY_OUT_BUF_LEN		2	/* tty output buffer size */

struct s_tty;
struct s_console;

/* TTY */
typedef struct s_tty
{
	struct termios termios;
	u32	ibuf[TTY_IN_BYTES];	/* TTY input buffer */
	u32*	ibuf_head;		/* the next free slot */
	u32*	ibuf_tail;		/* the val to be processed by TTY */
	int	ibuf_cnt;		/* how many */

	int	tty_caller;
	int	tty_procnr;
	void*	tty_req_buf;
	int	tty_left_cnt;
	int	tty_trans_cnt;

	struct s_console *	console;
}TTY;

#endif /* _TTY_H_ */
