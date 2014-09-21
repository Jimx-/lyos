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
#include "stdarg.h"
#include "unistd.h"
#include "assert.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/tty.h"
#include "lyos/console.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "fcntl.h"
#include "sys/wait.h"
#include "sys/utsname.h"
#include "lyos/compile.h"
#include "errno.h"
#include "multiboot.h"

#define LYOS_BANNER "Lyos version "UTS_RELEASE" (compiled by "LYOS_COMPILE_BY"@"LYOS_COMPILE_HOST")("LYOS_COMPILER") "UTS_VERSION"\n"

PUBLIC void init_arch();

/*****************************************************************************
 *                               kernel_main
 *****************************************************************************/
/**
 * jmp from kernel.asm::_start. 
 * 
 *****************************************************************************/
PUBLIC int kernel_main()
{
	disp_str(LYOS_BANNER);
	init_arch();

	jiffies = 0;

	init_clock();
    init_keyboard();

	restart();

	while(1){}
}


/*****************************************************************************
 *                                get_ticks
 *****************************************************************************/
PUBLIC int get_ticks()
{
	MESSAGE msg;
	reset_msg(&msg);
	msg.type = GET_TICKS;
	send_recv(BOTH, TASK_SYS, &msg);
	return msg.RETVAL;
}

/*****************************************************************************
 *                                Init
 *****************************************************************************/
/**
 * The hen.
 * 
 *****************************************************************************/
void Init()
{
	printl("Init() is running ...\n");

/*#define PROCFS_PATH	"/sbin/procfs"
	MESSAGE msg;
	msg.type = SERVICE_UP;
	msg.PATHNAME	= (void*)PROCFS_PATH;
	msg.NAME_LEN	= strlen(PROCFS_PATH);
	send_recv(BOTH, TASK_SERVMAN, &msg);	*/

	/* Here we go! */
	_exit(execv("/sbin/init", NULL));
}

/*****************************************************************************
 *                                panic
 *****************************************************************************/
PUBLIC void panic(const char *fmt, ...)
{
	char buf[256];

	/* 4 is the size of fmt in the stack */
	va_list arg = (va_list)((char*)&fmt + 4);

	vsprintf(buf, fmt, arg);

	printl("Kernel panic: %s", buf);

	/* should never arrive here */
	__asm__ __volatile__("ud2");
}
