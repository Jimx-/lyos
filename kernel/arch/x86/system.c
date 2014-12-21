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
#include "stddef.h"
#include "protect.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "arch_const.h"
#include "arch_proto.h"
#ifdef CONFIG_SMP
#include "arch_smp.h"
#endif
#include "lyos/cpulocals.h"
#include <lyos/param.h>
#include <lyos/vm.h>

/**
 * <Ring 0> Switch back to user.
 */
PUBLIC struct proc * arch_switch_to_user()
{
    char * stack;
    struct proc * p;

#ifdef CONFIG_SMP
    stack = (char *)tss[cpuid].esp0;
#else
    stack = (char *)tss[0].esp0;
#endif

    p = get_cpulocal_var(proc_ptr);
    /* save the proc ptr on the stack */
    *((reg_t *)stack) = (reg_t)p;

    return p;
}

PUBLIC int arch_reset_proc(struct proc * p)
{
    u32 codeseg, dataseg;

    if (p->endpoint == TASK_MM || p->endpoint == TASK_TTY) {     /* TASK */
        codeseg = SELECTOR_TASK_CS | RPL_TASK;
        dataseg = SELECTOR_TASK_DS | RPL_TASK;
    } else {                  /* USER PROC */
        codeseg = SELECTOR_USER_CS | RPL_USER;
        dataseg = SELECTOR_USER_DS | RPL_USER;
    }

    p->regs.eip = 0;
    p->regs.esp = 0;

    if (p->endpoint == TASK_MM) {
        /* use kernel page table */ 

        p->seg.cr3_phys = (u32)initial_pgd;
        p->seg.cr3_vir = (u32 *)((int)initial_pgd + KERNEL_VMA);

        strcpy(p->name, "MM");  /* name of the process */
        p->regs.eip = (u32)task_mm;
        p->regs.esp = (u32)task_stack;
    } 
    
    p->regs.cs = codeseg;
    p->regs.ds =
    p->regs.es =
    p->regs.fs =
    p->regs.gs =
    p->regs.ss = dataseg;

    p->regs.eflags  = 0x202;    /* IF=1, bit 2 is always 1 */
    p->seg.trap_style = KTS_INT;

    return 0;
}

/**
 * <Ring 0> Restore user context according to proc's kernel trap type.
 * 
 * @param proc Which proc to restore.
 */
PUBLIC void restore_user_context(struct proc * p)
{
    int trap_style = p->seg.trap_style;
    p->seg.trap_style = KTS_NONE;

    switch (trap_style) {
    case KTS_NONE:
        panic("no trap type recorded");
    case KTS_INT:
        restore_user_context_int(p);
        break;
    default:
        panic("unknown trap type recorded");
    }
}

/**
 * <Ring 0> Initialize FPU.
 */
PUBLIC void fpu_init()
{
    fninit();
}

PUBLIC void idle_stop()
{
#if CONFIG_SMP
    int cpu = cpuid;
#endif

    int is_idle = get_cpu_var(cpu, cpu_is_idle);
    if (is_idle) get_cpu_var(cpu, cpu_is_idle) = 0;
}

PUBLIC int arch_init_proc(struct proc * p, void * sp, void * ip, struct ps_strings * ps, char * name)
{
    memcpy(p->name, name, PROC_NAME_LEN);

    p->regs.esp = (reg_t)sp;
    p->regs.eip = (reg_t)ip;
    p->regs.eax = ps->ps_nargvstr;
    p->regs.edx = (reg_t)ps->ps_argvstr;
    p->regs.ecx = (reg_t)ps->ps_envstr;

    return 0;
}