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
#include <stdarg.h>
#include "unistd.h"
#include "assert.h"
#include "errno.h"
#include "fcntl.h"
#include <sys/ioctl.h>
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <lyos/log.h>

PRIVATE void kputc(char c);
PRIVATE void kputs(char * s);

/*****************************************************************************
 *                                sys_printx
 *****************************************************************************/
/**
 * System calls accept four parameters. `printx' needs only two, so it wastes
 * the other two.
 *
 * @note `printx' accepts only one parameter -- `char* s', the other one --
 * `struct proc * proc' -- is pushed by kernel.asm::sys_call so that the
 * kernel can easily know who invoked the system call.
 *
 * @note s[0] (the first char of param s) is a magic char. if it equals
 * MAG_CH_PANIC, then this syscall was invoked by `panic()', which means
 * something goes really wrong and the system is to be halted; if it equals
 * MAG_CH_ASSERT, then this syscall was invoked by `assert()', which means
 * an assertion failure has occured. @see kernel/main lib/misc.c.
 * 
 * @param _unused1  Ignored.
 * @param _unused2  Ignored.
 * @param s         The string to be printed.
 * @param p_proc    Caller proc.
 * 
 * @return  Zero if success.
 *****************************************************************************/
PUBLIC int sys_printx(MESSAGE * m, struct proc* p_proc)
{
	kputs(m->BUF);

	return 0;
}

/**
 * <Ring 0> Put a character into kernel log buffer.
 */
PRIVATE void kputc(char c)
{
	if (c != 0) {
		kern_log.buf[kern_log.next] = c;
		if (kern_log.size < sizeof(kern_log.buf)) kern_log.size++;
		kern_log.next = (kern_log.next + 1) % KERN_LOG_SIZE;
	} else {	/* infrom output process */
		inform_kernel_log(TASK_TTY);
	}
}

/**
 * <Ring 0> Put a string into kernel log buffer.
 */
PRIVATE void kputs(char * s)
{
	const char * p;
	char ch;

	p = s;

	spinlock_lock(&kern_log.lock);

	while ((ch = *p++) != 0) {
		kputc(ch);
	}
	kputc(0);

	spinlock_unlock(&kern_log.lock);
}

/*****************************************************************************
 *                                printk
 *****************************************************************************/
/**
 * Kernel's print
 * 
 * @param fmt  The format string
 * 
 * @return  The number of chars printed.
 *****************************************************************************/
PUBLIC int printk(const char *fmt, ...)
{
	int i;
	char buf[STR_DEFAULT_LEN];
	va_list arg;
	
	va_start(arg, fmt);	
	i = vsprintf(buf, fmt, arg);
	kputs(buf);

	va_end(arg);

	return i;
}
