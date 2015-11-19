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
#include <errno.h>
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "lyos/fs.h"
#include "arch_const.h"
#include "arch_proto.h"
#ifdef CONFIG_SMP
#include "arch_smp.h"
#endif
#include "lyos/cpulocals.h"
#include <lyos/param.h>
#include <lyos/vm.h>
#include "libexec/libexec.h"
#include "arch_type.h"

extern char _KERN_OFFSET;

#define PSR_I           (1 << 7)
#define PSR_F           (1 << 6)
#define PSR_USR32_MODE  0x00000010
#define PSR_SVC32_MODE  0x00000013

PUBLIC struct cpu_info cpu_info[CONFIG_SMP_MAX_CPUS];

PUBLIC reg_t svc_stack;

PUBLIC struct proc * arch_switch_to_user()
{
    struct proc * p;
    char* stk;

#ifdef CONFIG_SMP
    stk = (char*) tss[cpuid].sp0;
#else
    stk = (char*) tss[0].sp0;
#endif
    svc_stack = (reg_t) stk;

    p = get_cpulocal_var(proc_ptr);
    *((reg_t *)stk) = (reg_t) p;

    p->regs.psr &= ~(PSR_I | PSR_F);

    return p;
}

PRIVATE int kernel_clearmem(struct exec_info * execi, int vaddr, size_t len)
{
    memset((void *)vaddr, 0, len);
    return 0;
}

PRIVATE int kernel_allocmem(struct exec_info * execi, int vaddr, size_t len)
{
    pg_map(0, vaddr, vaddr + len, &kinfo);
    reload_ttbr0();
    memset((void *)vaddr, 0, len);

    return 0;
}

PRIVATE int read_segment(struct exec_info *execi, off_t offset, int vaddr, size_t len)
{
    if (offset + len > execi->header_len) return ENOEXEC;
    memcpy((void *)vaddr, (char*)execi->header + offset, len);

    return 0;
}

PUBLIC void arch_boot_proc(struct proc * p, struct boot_proc * bp)
{
    struct kinfo_module * mod = &kinfo.modules[bp->proc_nr];

    /* make MM run */
    if (bp->proc_nr == TASK_MM) {
        struct exec_info execi;
        memset(&execi, 0, sizeof(execi));

        execi.stack_top = VM_STACK_TOP;
        execi.stack_size = PROC_ORIGIN_STACK;

        /* header */
        execi.header = bp->base;
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

        p->regs.pc = (u32)execi.entry_point;
        p->regs.sp = (u32)VM_STACK_TOP;
    }
}

/*****************************************************************************
 *                                identify_cpu
 *****************************************************************************/
/**
 * <Ring 0> Identify a cpu.
 *
 *****************************************************************************/
PUBLIC void identify_cpu()
{

}

PUBLIC int arch_reset_proc(struct proc * p)
{
    memset(&p->regs, 0, sizeof(p->regs));

    if (p->endpoint == TASK_MM) {
        /* use bootstrap page table */ 
        p->seg.ttbr_phys = (u32)initial_pgd;
        p->seg.ttbr_vir = (u32 *)((phys_bytes)initial_pgd + (phys_bytes) &_KERN_OFFSET);
    } 
    
    if (is_kerntaske(p->endpoint))
        p->regs.psr = PSR_F | PSR_SVC32_MODE;
    else
        p->regs.psr = PSR_F | PSR_USR32_MODE;

    return 0;
}

PUBLIC void arch_set_syscall_result(struct proc * p, int result)
{

}

PUBLIC int arch_init_proc(struct proc * p, void * sp, void * ip, struct ps_strings * ps, char * name)
{
    memcpy(p->name, name, PROC_NAME_LEN);
    
    return 0;
}

PUBLIC void idle_stop()
{
#if CONFIG_SMP
    int cpu = cpuid;
#endif

    int is_idle = get_cpu_var(cpu, cpu_is_idle);
    get_cpu_var(cpu, cpu_is_idle) = 0;

    if (is_idle) restart_local_timer();
}
