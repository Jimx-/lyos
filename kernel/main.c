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
#include <asm/const.h>
#include <asm/proto.h>
#ifdef CONFIG_SMP
#include <asm/smp.h>
#include <lyos/smp.h>
#endif
#include "lyos/cpulocals.h"
#include <lyos/log.h>

#define LYOS_BANNER                                              \
    "Lyos version " UTS_RELEASE " (compiled by " LYOS_COMPILE_BY \
    "@" LYOS_COMPILE_HOST ")(" LYOS_COMPILER ") " UTS_VERSION "\n"

void init_arch();

/*****************************************************************************
 *                               kernel_main
 *****************************************************************************/
/**
 * jmp from kernel.asm::_start.
 *
 *****************************************************************************/
int kernel_main()
{
    init_arch();
    arch_setup_cpulocals();
    init_sched();
    init_proc();
    init_system();
    init_memory();
    init_irq();
    init_time();

    printk(LYOS_BANNER);

    int i;
    struct boot_proc* bp = boot_procs;
    for (i = 0; i < NR_BOOT_PROCS; i++, bp++) {
        struct proc* p = proc_addr(bp->proc_nr);
        bp->endpoint = p->endpoint;

        /* set task name */
        if (i < NR_TASKS) {
            strlcpy(p->name, bp->name, sizeof(p->name));
        }

        /* set module info */
        if (i >= NR_TASKS) {
            struct kinfo_module* mb_mod = &kinfo.modules[i - NR_TASKS];
            bp->base = mb_mod->start_addr;
            bp->len = mb_mod->end_addr - mb_mod->start_addr;
        }

        if (is_kerntaske(bp->endpoint) || bp->endpoint == TASK_MM ||
            bp->endpoint == TASK_SERVMAN) {
            /* assign priv structure */
            set_priv(p, static_priv_id(bp->endpoint));
            p->priv->flags |= PRF_PRIV_PROC;

            int allowed_calls;
            if (bp->endpoint == TASK_MM) {
                allowed_calls = TASK_CALLS;
            }
            if (bp->endpoint == TASK_SERVMAN) {
                allowed_calls = TASK_CALLS;
            } else {
                allowed_calls = KERNTASK_CALLS;
            }

            int j;
            for (j = 0; j < BITCHUNKS(NR_SYS_CALLS); j++) {
                p->priv->syscall_mask[j] =
                    (allowed_calls == ALL_CALLS_ALLOWED) ? (~0) : 0;
            }
        } else {
            p->state |= PST_NO_PRIV;
        }

        arch_boot_proc(p, bp);
    }

    memcpy(&kinfo.boot_procs, boot_procs, sizeof(kinfo.boot_procs));

#ifdef CONFIG_SMP
    smp_init();
#endif

    /* failed to init smp */
    finish_bsp_booting();

    while (TRUE) {
    }
}

void finish_bsp_booting()
{
    identify_cpu();

    if (init_bsp_timer(system_hz) != 0) panic("unable to init bsp timer");

    fpu_init();

    /* proc_ptr should point to somewhere */
    get_cpulocal_var(proc_ptr) = get_cpulocal_var_ptr(idle_proc);
    get_cpulocal_var(fpu_owner) = NULL;

    int i;
    /* enqueue runnable process */
    for (i = 0; i < NR_BOOT_PROCS - NR_TASKS; i++) {
        PST_UNSET(proc_addr(i), PST_STOPPED);
    }

#if CONFIG_SMP
    set_cpu_flag(cpuid, CPU_IS_READY);
#endif

    switch_to_user();
}

/*****************************************************************************
 *                                get_ticks
 *****************************************************************************/
int get_ticks()
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
void panic(const char* fmt, ...)
{
    char buf[256];

    /* 4 is the size of fmt in the stack */
    va_list arg = (va_list)((char*)&fmt + 4);

    vsprintf(buf, fmt, arg);

    direct_cls();
    kern_log.buf[kern_log.size] = 0;
    direct_put_str(kern_log.buf);
#if CONFIG_SMP
    direct_print("\nKernel panic on CPU %d: %s\n", cpuid, buf);
#else
    direct_print("\nKernel panic: %s\n", buf);
#endif

    while (1)
        ;
    /* should never arrive here */
    __asm__ __volatile__("ud2");
}
