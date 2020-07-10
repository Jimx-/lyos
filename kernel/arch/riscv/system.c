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
#include "unistd.h"
#include "stddef.h"
#include <errno.h>
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "lyos/fs.h"
#include <asm/const.h>
#include <asm/proto.h>
#ifdef CONFIG_SMP
#include <asm/smp.h>
#endif
#include "lyos/cpulocals.h"
#include <lyos/param.h>
#include <lyos/vm.h>
#include "libexec/libexec.h"
#include <asm/type.h>

struct cpu_info cpu_info[CONFIG_SMP_MAX_CPUS];

struct proc* arch_switch_to_user()
{
    struct proc* p;
    reg_t stk;

    stk = tss[cpuid].sp0;
    p = get_cpulocal_var(proc_ptr);

    p->regs.kernel_sp = stk;
    p->regs.cpu = cpuid;

    return p;
}

static int kernel_clearmem(struct exec_info* execi, void* vaddr, size_t len)
{
    enable_user_access();
    memset(vaddr, 0, len);
    disable_user_access();

    return 0;
}

static int kernel_allocmem(struct exec_info* execi, void* vaddr, size_t len)
{
    pg_map(0, vaddr, vaddr + len, &kinfo);

    return 0;
}

static int read_segment(struct exec_info* execi, off_t offset, void* vaddr,
                        size_t len)
{
    if (offset + len > execi->header_len) return ENOEXEC;

    enable_user_access();
    memcpy((void*)vaddr, (char*)execi->header + offset, len);
    disable_user_access();

    return 0;
}

void arch_boot_proc(struct proc* p, struct boot_proc* bp)
{
    /* make MM run */
    if (bp->proc_nr == TASK_MM) {
        struct exec_info execi;
        memset(&execi, 0, sizeof(execi));

        execi.stack_top = VM_STACK_TOP;
        execi.stack_size = PROC_ORIGIN_STACK;

        /* header */
        execi.header = (void*)bp->base;
        execi.header_len = bp->len;

        execi.allocmem = kernel_allocmem;
        execi.allocmem_prealloc = kernel_allocmem;
        execi.copymem = read_segment;
        execi.clearproc = NULL;
        execi.clearmem = kernel_clearmem;

        execi.proc_e = bp->endpoint;
        execi.filesize = bp->len;

        if (libexec_load_elf(&execi) != 0) panic("can't load MM");
        strcpy(p->name, bp->name);

        p->regs.sepc = (reg_t)execi.entry_point;
        p->regs.sp = (reg_t)VM_STACK_TOP;
        p->regs.a2 = (reg_t)0; // environ
    }
}

/*****************************************************************************
 *                                identify_cpu
 *****************************************************************************/
/**
 * <Ring 0> Identify a cpu.
 *
 *****************************************************************************/
void identify_cpu() {}

int arch_reset_proc(struct proc* p)
{
    memset(&p->regs, 0, sizeof(p->regs));

    if (p->endpoint == TASK_MM) {
        /* use bootstrap page table */
        p->seg.ptbr_phys = (reg_t)__pa(initial_pgd);
        p->seg.ptbr_vir = (reg_t*)initial_pgd;
    }

    return 0;
}

void arch_set_syscall_result(struct proc* p, int result)
{
    p->regs.a0 = (reg_t)result;
}

int arch_init_proc(struct proc* p, void* sp, void* ip, struct ps_strings* ps,
                   char* name)
{
    memcpy(p->name, name, PROC_NAME_LEN);

    p->regs.sp = (reg_t)sp;
    p->regs.sepc = (reg_t)ip;
    p->regs.a0 = (reg_t)ps->ps_nargvstr;
    p->regs.a1 = (reg_t)ps->ps_argvstr;
    p->regs.a2 = (reg_t)ps->ps_envstr;

    return 0;
}

void idle_stop() {}

void arch_init_profile_clock() {}

void arch_stop_profile_clock() {}

void arch_ack_profile_clock() {}
