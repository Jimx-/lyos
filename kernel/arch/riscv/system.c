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

#include <lyos/type.h>
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

PUBLIC struct cpu_info cpu_info[CONFIG_SMP_MAX_CPUS];

PUBLIC struct proc * arch_switch_to_user()
{
}

PRIVATE int kernel_clearmem(struct exec_info * execi, int vaddr, size_t len)
{
    memset((void *)vaddr, 0, len);
    return 0;
}

PRIVATE int kernel_allocmem(struct exec_info * execi, int vaddr, size_t len)
{
    pg_map(0, vaddr, vaddr + len, &kinfo);
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
    return 0;
}

PUBLIC void arch_set_syscall_result(struct proc * p, int result)
{
}

PUBLIC int arch_init_proc(struct proc * p, void * sp, void * ip, struct ps_strings * ps, char * name)
{
    return 0;
}

PUBLIC void idle_stop()
{
}

PUBLIC void arch_init_profile_clock()
{
}

PUBLIC void arch_stop_profile_clock()
{
}

PUBLIC void arch_ack_profile_clock()
{
}