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
#include <stdlib.h>
#include "unistd.h"
#include "assert.h"
#include "errno.h"
#include "fcntl.h"
#include <sys/ioctl.h>
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "tty.h"
#include "console.h"
#include "lyos/global.h"
#include "keyboard.h"
#include "lyos/proto.h"
#include "termios.h"
#include "proto.h"
#include <lyos/driver.h>
#include <lyos/param.h>
#include <lyos/log.h>
#include <lyos/sysutils.h>
#include <lyos/timer.h>
#include "global.h"

PRIVATE struct sysinfo * _sysinfo;
PRIVATE dev_t cons_minor = CONS_MINOR + 1;

#define TTY_FIRST	(tty_table)
#define TTY_END		(tty_table + NR_CONSOLES + NR_SERIALS)

/* Default termios */
PRIVATE struct termios termios_defaults = {
  TINPUT_DEF, TOUTPUT_DEF, TCTRL_DEF, TLOCAL_DEF, TSPEED_DEF, TSPEED_DEF,
  {
	TINTR_DEF, TQUIT_DEF, TERASE_DEF, TKILL_DEF, TEOF_DEF, TTIME_DEF, TMIN_DEF,
	0, TSTART_DEF, TSTOP_DEF, TSUSP_DEF, TEOL_DEF, TREPRINT_DEF,  TDISCARD_DEF, 0, TLNEXT_DEF,
  },
};

PRIVATE void	init_tty	();
PRIVATE TTY * 	minor2tty	(dev_t minor);
PRIVATE void 	set_console_line(char val[CONS_ARG]);
PRIVATE void	tty_dev_read	(TTY* tty);
PRIVATE void	tty_dev_write	(TTY* tty);
PRIVATE void 	in_transfer	(TTY* tty);
PRIVATE void	tty_do_read	(TTY* tty, MESSAGE* msg);
PRIVATE void	tty_do_write	(TTY* tty, MESSAGE* msg);
PRIVATE void 	tty_do_ioctl(TTY* tty, MESSAGE* msg);
PRIVATE void 	tty_do_kern_log();
PRIVATE void	tty_echo	(TTY* tty, char c);
PRIVATE void	tty_sigproc (TTY * tty, int signo);
PRIVATE void 	erase		(TTY * tty);
PRIVATE void	put_key		(TTY* tty, u32 key);


/*****************************************************************************
 *                                task_tty
 *****************************************************************************/
/**
 * <Ring 1> Main loop of task TTY.
 *****************************************************************************/
PUBLIC int main()
{
	TTY *	tty;
	MESSAGE msg;

	init_tty();

	while (1) {
		for (tty = TTY_FIRST; tty < TTY_END; tty++) {
			handle_events(tty);
		}
		
		send_recv(RECEIVE, ANY, &msg);

		int src = msg.source;
		assert(src != TASK_TTY);

		/* notify */
		if (msg.type == NOTIFY_MSG) {
			switch (msg.source) {
			case KERNEL:
				tty_do_kern_log();
				break;
			case INTERRUPT:
				if (msg.INTERRUPTS & rs_irq_set)
					rs_interrupt(&msg);
				break;
			case CLOCK:
				expire_timer(msg.TIMESTAMP);
				break;
			}
			continue;
		}

		TTY* ptty = minor2tty(msg.DEVICE);

		switch (msg.type) {
		case CDEV_OPEN:
			msg.type = SYSCALL_RET;
			send_recv(SEND, src, &msg);
			break;
		case CDEV_READ:
			tty_do_read(ptty, &msg);
			break;
		case CDEV_WRITE:
			tty_do_write(ptty, &msg);
			break;
		case CDEV_IOCTL:
			tty_do_ioctl(ptty, &msg);
			break;
		case INPUT_TTY_UP:
		case INPUT_TTY_EVENT:
			do_input(&msg);
			break;
		default:
			msg.type = SYSCALL_RET;
			msg.RETVAL = ENOSYS;
			send_recv(SEND, src, &msg);
			break;
		}
	}

	return 0;
}


/*****************************************************************************
 *                                init_tty
 *****************************************************************************/
/**
 * Things to be initialized before a tty can be used:
 *   -# the input buffer
 *   -# the corresponding console
 * 
 *****************************************************************************/
PRIVATE void init_tty()
{
	TTY * tty;
	int i;

	get_sysinfo(&_sysinfo);

	char val[CONS_ARG];
	if (env_get_param("console", val, CONS_ARG) == 0) {
		set_console_line(val);
	}

	init_keyboard();

	for (tty = TTY_FIRST, i = 0; tty < TTY_END; tty++, i++) {

		tty->ibuf_cnt = tty->tty_eotcnt = 0;
		tty->ibuf_head = tty->ibuf_tail = tty->ibuf;

		tty->tty_min = 1;
		
		tty->tty_termios = termios_defaults;

		if (i < NR_CONSOLES) {	/* consoles */
			init_screen(tty);
			kb_init(tty);

			/* announce the device */
			char name[6];
			sprintf(name, "tty%d", (int)(tty - TTY_FIRST + 1));
			announce_chardev(name, MAKE_DEV(DEV_CHAR_TTY, (tty - TTY_FIRST + 1)));
		} else {	/* serial ports */
			init_rs(tty);

			char name[6];
			sprintf(name, "ttyS%d", i - NR_CONSOLES);
			announce_chardev(name, MAKE_DEV(DEV_CHAR_TTY, i - NR_CONSOLES + SERIAL_MINOR));
		}
	}

	select_console(0);

	/* start to handle kernel logging request */
	kernlog_register();
}


PRIVATE TTY * minor2tty(dev_t minor)
{
	TTY * ptty = NULL;

	if (minor == CONS_MINOR || minor == LOG_MINOR) minor = cons_minor;

	if (minor - CONS_MINOR - 1 < NR_CONSOLES) ptty = &tty_table[minor - CONS_MINOR - 1];
	else if (minor - SERIAL_MINOR < NR_SERIALS) ptty = &tty_table[NR_CONSOLES + minor - SERIAL_MINOR];

	return ptty;
}

PRIVATE void set_console_line(char val[CONS_ARG])
{
	if (!strncmp(val, "console", CONS_ARG - 1)) cons_minor = CONS_MINOR;

	char * pv = val;
	if (!strncmp(pv, "tty", 3)) {
		pv += 3;
		if (*pv == 'S') {	/* serial */
			pv++;
			if (*pv >= '0' && *pv <= '9') {
				int minor = atoi(pv);
				if (minor <= NR_SERIALS) cons_minor = minor + SERIAL_MINOR;
			}
		} else if (*pv >= '0' && *pv <= '9') {	/* console */
			int minor = atoi(pv);
			if (minor <= NR_CONSOLES) cons_minor = minor;
		}
	}
}

/*****************************************************************************
 *                                in_process
 *****************************************************************************/
/**
 * keyboard_read() will invoke this routine after having recognized a key press.
 * 
 * @param tty  The key press is for whom.
 * @param key  The integer key with metadata.
 *****************************************************************************/
PUBLIC int in_process(TTY* tty, char * buf, int count)
{
	int cnt, key;

	for (cnt = 0; cnt < count; cnt++) {	
		key = *buf++ & 0xFF;

		if (tty->tty_termios.c_iflag & ISTRIP) key &= 0x7F;

		if (tty->tty_termios.c_lflag & IEXTEN) {
			/* Previous character was a character escape? */
			if (tty->tty_escaped) {
				tty->tty_escaped = 0;
				put_key(tty, key);
				tty_echo(tty, key);
				continue;
			}
			
			/* LNEXT (^V) to escape the next character? */
			if (key == tty->tty_termios.c_cc[VLNEXT]) {
				tty->tty_escaped = 1;
				tty_echo(tty, key);
				continue;
			}
		}

		/* Map CR to LF, ignore CR, or map LF to CR. */
		if (key == '\r') {
			if (tty->tty_termios.c_iflag & IGNCR) return;
			if (tty->tty_termios.c_iflag & ICRNL) key = '\n';
		} else if (key == '\n') {
			if (tty->tty_termios.c_iflag & INLCR) key = '\r';
		}
		
		if (tty->tty_termios.c_lflag & ICANON) {
			if (key == tty->tty_termios.c_cc[VERASE]) {
				erase(tty);
				if (!(tty->tty_termios.c_lflag & ECHOE)) {
					tty_echo(tty, key);
				}
				continue;
			}

			if (key == '\n') key |= IN_EOT;

			if (key == tty->tty_termios.c_cc[VEOL]) key |= IN_EOT;
		}

		if (tty->tty_termios.c_lflag & ISIG) {
			int signo = 0;

			if (key == tty->tty_termios.c_cc[VINTR]) {
				signo = SIGINT;
			} else if (key == tty->tty_termios.c_cc[VQUIT]) {
				signo = SIGQUIT;
			}

			if (signo > 0) {
				tty_echo(tty, key);
				tty_sigproc(tty, signo);
				continue;
			}
		}

		put_key(tty, key);
		tty_echo(tty, key);

		if (key & IN_EOT) tty->tty_eotcnt++;
	}

	return cnt;
}


/*****************************************************************************
 *                                put_key
 *****************************************************************************/
/**
 * Put a key into the in-buffer of TTY.
 *
 * @callergraph
 * 
 * @param tty  To which TTY the key is put.
 * @param key  The key. It's an integer whose higher 24 bits are metadata.
 *****************************************************************************/
PRIVATE void put_key(TTY* tty, u32 key)
{
	if (tty->ibuf_cnt < TTY_IN_BYTES) {
		*(tty->ibuf_head) = key;
		tty->ibuf_head++;
		if (tty->ibuf_head == tty->ibuf + TTY_IN_BYTES)
			tty->ibuf_head = tty->ibuf;
		tty->ibuf_cnt++;
	}
}


/*****************************************************************************
 *                                tty_dev_read
 *****************************************************************************/
/**
 * Get chars from the keyboard buffer if the TTY::console is the `current'
 * console.
 *
 * @see keyboard_read()
 * 
 * @param tty  Ptr to TTY.
 *****************************************************************************/
PRIVATE void tty_dev_read(TTY* tty)
{
	if (tty->tty_devread) tty->tty_devread(tty);
}


/*****************************************************************************
 *                                tty_dev_write
 *****************************************************************************/
/**
 * Echo the char just pressed and transfer it to the waiting process.
 * 
 * @param tty   Ptr to a TTY struct.
 *****************************************************************************/
PRIVATE void tty_dev_write(TTY* tty)
{
	if (tty->tty_devwrite) tty->tty_devwrite(tty);
}

/*****************************************************************************
 *                                in_transfer
 *****************************************************************************/
/**
 * Transfer chars to the waiting process.
 * 
 * @param tty   Ptr to a TTY struct.
 *****************************************************************************/
PRIVATE void in_transfer(TTY* tty)
{
	char buf[64], *bp;

	if (tty->tty_inleft == 0 || tty->tty_eotcnt < tty->tty_min) return;

	bp = buf;
	while (tty->tty_inleft > 0 && tty->tty_eotcnt > 0) {
		char ch = *(tty->ibuf_tail);
		tty->ibuf_tail++;
		if (tty->ibuf_tail == tty->ibuf + TTY_IN_BYTES)
			tty->ibuf_tail = tty->ibuf;
		tty->ibuf_cnt--;

		*bp = ch & IN_CHAR;
		tty->tty_inleft--;
		if (++bp == buf + sizeof(buf)) {	/* buffer full */
			void * p = tty->tty_inbuf +
					   tty->tty_trans_cnt;
			data_copy(tty->tty_inprocnr, p, TASK_TTY, buf, sizeof(buf));
			tty->tty_trans_cnt += sizeof(buf);
			bp = buf;
		}

		if (ch == '\n') {
			tty->tty_eotcnt--;
			if (tty->tty_termios.c_lflag & ICANON) tty->tty_inleft = 0;
		}
	}

	if (bp > buf) {
		int count = bp - buf;
		void * p = tty->tty_inbuf +
					   tty->tty_trans_cnt;
		data_copy(tty->tty_inprocnr, p, TASK_TTY, buf, count);
		tty->tty_trans_cnt += count;
	}

	if (tty->tty_inleft == 0) {
		MESSAGE msg;
		msg.type = tty->tty_inreply;
		msg.PROC_NR = tty->tty_inprocnr;
		msg.CNT = tty->tty_trans_cnt;
		send_recv(SEND, tty->tty_incaller, &msg);
		tty->tty_inleft = 0;
	}
}

/*****************************************************************************
 *                                handle_events
 *****************************************************************************/
/**
 * Handle all events pending on a tty.
 * 
 * @param tty   Ptr to a TTY struct.
 *****************************************************************************/
PUBLIC void handle_events(TTY * tty)
{
	do {
		tty_dev_read(tty);
		tty_dev_write(tty);
	} while (tty->tty_events);
	in_transfer(tty);
}

/*****************************************************************************
 *                                tty_do_read
 *****************************************************************************/
/**
 * Invoked when task TTY receives DEV_READ message.
 *
 * @note The routine will return immediately after setting some members of
 * TTY struct, telling FS to suspend the proc who wants to read. The real
 * transfer (tty buffer -> proc buffer) is not done here.
 * 
 * @param tty  From which TTY the caller proc wants to read.
 * @param msg  The MESSAGE just received.
 *
 * @see documentation/tty/
 *****************************************************************************/
PRIVATE void tty_do_read(TTY* tty, MESSAGE* msg)
{
	/* tell the tty: */
	tty->tty_inreply = SYSCALL_RET;
	tty->tty_incaller   = msg->source;  /* who called, usually FS */
	tty->tty_inprocnr   = msg->PROC_NR; /* who wants the chars */
	tty->tty_inbuf  = msg->BUF;/* where the chars should be put */
	tty->tty_inleft = msg->CNT; /* how many chars are requested */
	tty->tty_trans_cnt= 0; /* how many chars have been transferred */

	msg->type = SUSPEND_PROC;
	msg->CNT = tty->tty_inleft;
	send_recv(SEND, tty->tty_incaller, msg);
	tty->tty_inreply = RESUME_PROC;
}


/*****************************************************************************
 *                                tty_do_write
 *****************************************************************************/
/**
 * Invoked when task TTY receives DEV_WRITE message.
 * 
 * @param tty  To which TTY the calller proc is bound.
 * @param msg  The MESSAGE.
 *****************************************************************************/
PRIVATE void tty_do_write(TTY* tty, MESSAGE* msg)
{
	/* tell the tty: */
	tty->tty_outreply    = SYSCALL_RET;
	tty->tty_outcaller   = msg->source;  /* who called, usually FS */
	tty->tty_outprocnr   = msg->PROC_NR; /* who wants to output the chars */
	tty->tty_outbuf  = msg->BUF;/* where are the chars */
	tty->tty_outleft = msg->CNT; /* how many chars are requested */
	tty->tty_outcnt = 0;

	handle_events(tty);
	if (tty->tty_outleft == 0) return;	/* already done, just return */

	if (msg->FLAGS & O_NONBLOCK) {	/* do not block */	
		tty->tty_outleft = tty->tty_outcnt = 0;
		msg->type = tty->tty_outreply;
		msg->STATUS = tty->tty_outcnt > 0 ? tty->tty_outcnt : EAGAIN;
		send_recv(SEND, tty->tty_outcaller, msg);
	} else {	/* block */
		msg->type = SUSPEND_PROC;
		msg->CNT = tty->tty_outleft;
		send_recv(SEND, tty->tty_outcaller, msg);
		tty->tty_outreply = RESUME_PROC;
	}
}

/*****************************************************************************
 *                                tty_do_ioctl
 *****************************************************************************/
/**
 * Invoked when task TTY receives DEV_IOCTL message.
 * 
 * @param tty  To which TTY the calller proc is bound.
 * @param msg  The MESSAGE.
 *****************************************************************************/
PRIVATE void tty_do_ioctl(TTY* tty, MESSAGE* msg)
{
	int retval = SYSCALL_RET;

	switch (msg->REQUEST) {
	case TCGETS:
		data_copy(msg->PROC_NR, msg->BUF, SELF, &(tty->tty_termios), sizeof(struct termios));
		break;
	case TCSETSW:
    case TCSETSF:
    //case TCDRAIN:
    case TCSETS:
    	data_copy(SELF, &(tty->tty_termios), msg->PROC_NR, msg->BUF, sizeof(struct termios));
    	break;
    case TIOCGPGRP:
    	data_copy(msg->PROC_NR, msg->BUF, SELF, &tty->tty_pgrp, sizeof(tty->tty_pgrp));
    	break;
    case TIOCSPGRP:
    	data_copy(SELF, &tty->tty_pgrp, msg->PROC_NR, msg->BUF, sizeof(tty->tty_pgrp));
    	break;
	default:
		break;
	}

	msg->type = retval;
	send_recv(SEND, msg->source, msg);
}

/*****************************************************************************
 *                                tty_do_kern_log
 *****************************************************************************/
/**
 * Invoked when task TTY receives KERN_LOG message.
 * 
 *****************************************************************************/
PRIVATE void tty_do_kern_log()
{
	static int prev_next = 0;
	static char kernel_log_copy[KERN_LOG_SIZE];
	struct kern_log * klog = _sysinfo->kern_log;
	int next = klog->next;
	TTY * tty;

	size_t bytes = ((next + KERN_LOG_SIZE) - prev_next) % KERN_LOG_SIZE, copy;
	if (bytes > 0) {
		copy = min(bytes, KERN_LOG_SIZE - prev_next);
		memcpy(kernel_log_copy, &klog->buf[prev_next], copy);
		if (bytes > copy) memcpy(&kernel_log_copy[copy], klog->buf, bytes - copy);
		
		tty = minor2tty(cons_minor);
		/* tell the tty: */
		tty->tty_outreply    = SYSCALL_RET;
		tty->tty_outcaller   = KERNEL;
		tty->tty_outprocnr   = TASK_TTY; /* who wants to output the chars */
		tty->tty_outbuf  = kernel_log_copy;/* where are the chars */
		tty->tty_outleft = bytes; /* how many chars are requested */
		tty->tty_outcnt = 0;

		handle_events(tty);
	}

	prev_next = next;
}

/*****************************************************************************
 *                                erase
 *****************************************************************************/
/**
 * Erase last character.
 *
 *****************************************************************************/
PRIVATE void erase(TTY * tty)
{
	if (tty->ibuf_cnt == 0) return;

	u32 * head = tty->ibuf_head;
	if (head == tty->ibuf)
			head = tty->ibuf + TTY_IN_BYTES;
	head--;
	tty->ibuf_head = head;
	tty->ibuf_cnt--;

	if (tty->tty_termios.c_lflag & ECHOE) {
		tty->tty_echo(tty, '\b');
	}
}

/*****************************************************************************
 *                                tty_echo
 *****************************************************************************/
/**
 * Echo the character is echoing is on.
 *
 *****************************************************************************/
PRIVATE void tty_echo(TTY* tty, char c)
{
	if (!(tty->tty_termios.c_lflag & ECHO)) return;

	int len;
	if ((c & IN_CHAR) < ' ') {
		switch (c & (IN_CHAR)) {
		case '\t':
			len = 0;
			do {
				tty->tty_echo(tty, ' ');
				len++;
			} while (len < TAB_SIZE);
			break;
		case '\n':
		case '\r':
			tty->tty_echo(tty, c);
			break;
		default:
			tty->tty_echo(tty, '^');
			tty->tty_echo(tty, '@' + (c & IN_CHAR));
			break;
		}
	} else {
		tty->tty_echo(tty, c);
	}
}

/*****************************************************************************
 *                                tty_sigproc
 *****************************************************************************/
/**
 * Send a signal to control proc.
 *
 *****************************************************************************/
PRIVATE void tty_sigproc(TTY * tty, int signo)
{
	endpoint_t ep;
	if (get_procep(tty->tty_pgrp, &ep) != 0) return;

	int retval;
	if ((retval = kernel_kill(ep, signo)) != 0) panic("unable to send signal(%d)", retval);
}

