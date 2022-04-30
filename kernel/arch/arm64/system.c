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
#include <kernel/proc.h>
#include <kernel/global.h>
#include <kernel/proto.h>
#include "lyos/fs.h"
#include <asm/const.h>
#include <asm/proto.h>
#ifdef CONFIG_SMP
#include <asm/smp.h>
#endif
#include "asm/cpulocals.h"
#include <lyos/param.h>
#include <lyos/vm.h>
#include <lyos/sysutils.h>
#include "libexec/libexec.h"
#include <asm/cpu_info.h>
#include <asm/cpu_type.h>
#include <asm/sysreg.h>

struct cpu_info cpu_info[CONFIG_SMP_MAX_CPUS];

struct proc* arch_switch_to_user(void)
{
    struct proc* p;
    reg_t* stk;

    stk = (reg_t*)tss[cpuid].sp0;
    p = get_cpulocal_var(proc_ptr);

    p->regs.kernel_sp = (reg_t)stk;
    p->regs.cpu = cpuid;

    write_sysreg(p->seg.tp_value, tpidr_el0);

    return p;
}

void identify_cpu()
{
    struct cpu_info* ci = &cpu_info[cpuid];
    ci->reg_midr = read_cpuid_id();
}

static int kernel_clearmem(struct exec_info* execi, void* vaddr, size_t len)
{
    memset(vaddr, 0, len);

    return 0;
}

static int kernel_allocmem(struct exec_info* execi, void* vaddr, size_t len,
                           unsigned int prot_flags)
{
    pg_map(0, vaddr, vaddr + len, &kinfo);

    return 0;
}

static int read_segment(struct exec_info* execi, off_t offset, void* vaddr,
                        size_t len)
{
    if (offset + len > execi->header_len) return ENOEXEC;

    memcpy((void*)vaddr, (char*)execi->header + offset, len);

    return 0;
}

void arch_boot_proc(struct proc* p, struct boot_proc* bp)
{
    /* make MM run */
    if (bp->proc_nr == TASK_MM) {
        struct exec_info execi;
        memset(&execi, 0, sizeof(execi));

        execi.stack_top = (void*)VM_STACK_TOP;
        execi.stack_size = PROC_ORIGIN_STACK;

        /* header */
        execi.header = __va(bp->base);
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

        p->regs.pc = (reg_t)execi.entry_point;
        p->regs.sp = (reg_t)VM_STACK_TOP;
        p->regs.regs[2] = (reg_t)0; // environ
    }
}

static struct fpu_state fpu_state[NR_PROCS];

int arch_reset_proc(struct proc* p)
{
    struct fpu_state* fpu;

    memset(&p->regs, 0, sizeof(p->regs));

    if (p->endpoint == TASK_MM) {
        /* use bootstrap page table */
        p->seg.ttbr_phys = (reg_t)__pa_symbol(mm_pg_dir);
        p->seg.ttbr_vir = (reg_t*)mm_pg_dir;
    }

    if (p->slot >= 0) {
        fpu = &fpu_state[p->slot];
    }

    p->seg.fpu_state = (void*)fpu;

    return 0;
}

void arch_set_syscall_result(struct proc* p, int result)
{
    p->regs.regs[0] = (reg_t)result;
}

int arch_init_proc(struct proc* p, void* sp, void* ip, struct ps_strings* ps,
                   char* name)
{
    arch_reset_proc(p);

    memcpy(p->name, name, PROC_NAME_LEN);

    p->regs.sp = (reg_t)sp;
    p->regs.pc = (reg_t)ip;
    p->regs.regs[0] = (reg_t)ps->ps_nargvstr;
    p->regs.regs[1] = (reg_t)ps->ps_argvstr;
    p->regs.regs[2] = (reg_t)ps->ps_envstr;

    return 0;
}

int arch_fork_proc(struct proc* p, struct proc* parent, int flags, void* newsp,
                   void* tls)
{
    if (newsp) {
        p->regs.sp = (reg_t)newsp;
    }

    if (parent->flags & PF_FPU_INITIALIZED) {
        memcpy(p->seg.fpu_state, parent->seg.fpu_state,
               sizeof(struct fpu_state));
    }

    if (flags & KF_SETTLS) p->seg.tp_value = (unsigned long)tls;

    return 0;
}

void idle_stop() {}

void arch_init_profile_clock() {}

void arch_stop_profile_clock() {}

void arch_ack_profile_clock() {}
