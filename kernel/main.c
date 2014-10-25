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
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "fcntl.h"
#include "sys/wait.h"
#include "sys/utsname.h"
#include "lyos/compile.h"
#include "errno.h"
#include "multiboot.h"
#include "arch_const.h"
#include "arch_proto.h"
#ifdef CONFIG_SMP
#include "arch_smp.h"
#endif
#include "lyos/cpulocals.h"

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
	printk(LYOS_BANNER);
	init_arch();
	init_proc();
	
	jiffies = 0;

	init_clock();
    init_keyboard();

#ifdef CONFIG_SMP
    smp_init();
    /* failed to init smp */
    finish_bsp_booting();
#endif

	finish_bsp_booting();
	
	while(1){}
}

PUBLIC void finish_bsp_booting()
{
	fpu_init();

	/* proc_ptr should point to somewhere */
	get_cpulocal_var(proc_ptr) = get_cpulocal_var_ptr(idle_proc);

	int i;
	/* enqueue runnable process */
	for (i = 0; i < NR_TASKS + NR_NATIVE_PROCS; i++) {
		PST_UNSET(&proc_table[i], PST_STOPPED);
	}

	switch_to_user();
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
 *                                panic
 *****************************************************************************/
PUBLIC void panic(const char *fmt, ...)
{
	char buf[256];

	/* 4 is the size of fmt in the stack */
	va_list arg = (va_list)((char*)&fmt + 4);

	vsprintf(buf, fmt, arg);

	direct_cls();
	direct_print("Kernel panic: %s", buf);

	while(1);
	/* should never arrive here */
	__asm__ __volatile__("ud2");
}
