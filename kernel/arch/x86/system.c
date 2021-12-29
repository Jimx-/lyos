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
#include <asm/protect.h>
#include <errno.h>
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "lyos/fs.h"
#include <lyos/sysutils.h>
#include <asm/const.h>
#include <asm/proto.h>
#ifdef CONFIG_SMP
#include <asm/smp.h>
#endif
#include "lyos/cpulocals.h"
#include <lyos/param.h>
#include <lyos/vm.h>
#include "libexec/libexec.h"
#include <asm/cpu_info.h>
#include <asm/cmos.h>
#include <asm/fpu.h>
#include <asm/prctl.h>

struct cpu_info cpu_info[CONFIG_SMP_MAX_CPUS];

void _cpuid(u32* eax, u32* ebx, u32* ecx, u32* edx);

/**
 * <Ring 0> Switch back to user.
 */
struct proc* arch_switch_to_user()
{
    char* stack;
    struct proc* p;

#ifdef CONFIG_X86_32
    stack = (char*)tss[cpuid].esp0;
#else
    stack = (char*)tss[cpuid].sp0;
#endif

    p = get_cpulocal_var(proc_ptr);
    /* save the proc ptr on the stack */
    *((reg_t*)stack) = (reg_t)p;

    load_tls(p, cpuid);

#ifdef CONFIG_X86_64
    ia32_write_msr(AMD_MSR_FS_BASE, (u32)(p->seg.fsbase >> 32),
                   (u32)p->seg.fsbase);
    ia32_write_msr(AMD_MSR_KERNEL_GS_BASE, (u32)(p->seg.gsbase >> 32),
                   (u32)p->seg.gsbase);
#endif

    return p;
}

static union fpregs_state fpu_state[NR_PROCS];

int arch_reset_proc(struct proc* p)
{
    union fpregs_state* fpu = NULL;

    p->regs.ip = 0;
    p->regs.sp = 0;

    if (p->endpoint == TASK_MM) {
        /* use bootstrap page table */
        p->seg.cr3_phys = __pa(initial_pgd);
        p->seg.cr3_vir = (unsigned long*)initial_pgd;
    }

    p->regs.cs = SELECTOR_USER_CS | RPL_USER;
#ifdef CONFIG_X86_32
    p->regs.ds = p->regs.es = p->regs.fs = p->regs.gs =
        SELECTOR_USER_DS | RPL_USER;
#endif
    p->regs.ss = SELECTOR_USER_DS | RPL_USER;

    p->regs.flags = 0x202; /* IF=1, bit 2 is always 1 */
    p->seg.trap_style = KTS_INT;

    if (p->slot >= 0) {
        fpu = &fpu_state[p->slot];
    }

    p->seg.fpu_state = (void*)fpu;

#ifdef CONFIG_X86_64
    p->seg.fsbase = 0;
    p->seg.gsbase = 0;
#endif

    memset(p->seg.tls_array, 0, GDT_TLS_ENTRIES * sizeof(struct descriptor));

    return 0;
}

/**
 * <Ring 0> Restore user context according to proc's kernel trap type.
 *
 * @param proc Which proc to restore.
 */
void restore_user_context(struct proc* p)
{
    int trap_style = p->seg.trap_style;
    p->seg.trap_style = KTS_NONE;

    switch (trap_style) {
    case KTS_NONE:
        panic("no trap type recorded");
    case KTS_INT:
        restore_user_context_int(p);
        break;
    case KTS_SYSENTER:
        restore_user_context_sysenter(p);
        break;
    case KTS_SYSCALL:
        restore_user_context_syscall(p);
        break;
    default:
        panic("unknown trap type recorded");
    }
}

void idle_stop()
{
#if CONFIG_SMP
    int cpu = cpuid;
#endif

    int is_idle = get_cpu_var(cpu, cpu_is_idle);
    get_cpu_var(cpu, cpu_is_idle) = 0;

    if (is_idle) restart_local_timer();
}

int arch_init_proc(struct proc* p, void* sp, void* ip, struct ps_strings* ps,
                   char* name)
{
    arch_reset_proc(p);

    strlcpy(p->name, name, sizeof(p->name));

    p->regs.sp = (reg_t)sp;
    p->regs.ip = (reg_t)ip;
    p->regs.ax = ps->ps_nargvstr;
    p->regs.dx = (reg_t)ps->ps_argvstr;
    p->regs.cx = (reg_t)ps->ps_envstr;

    return 0;
}

int arch_fork_proc(struct proc* p, struct proc* parent, int flags, void* newsp,
                   void* tls)
{
    int retval = 0;

    if (newsp != NULL) {
        p->regs.sp = (reg_t)newsp;
    }

    if (flags & KF_SETTLS) {
#ifdef CONFIG_X86_64
        p->seg.fsbase = (unsigned long)tls;
#else
        retval =
            do_set_thread_area(p, -1, tls, FALSE /* can_allocate */, parent);
#endif
    }

    return retval;
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
    reload_cr3();
    memset(vaddr, 0, len);

    return 0;
}

static int read_segment(struct exec_info* execi, off_t offset, void* vaddr,
                        size_t len)
{
    if (offset + len > execi->header_len) return ENOEXEC;
    memcpy(vaddr, (void*)(execi->header + offset), len);

    return 0;
}

void arch_boot_proc(struct proc* p, struct boot_proc* bp)
{
    /* make MM run */
    if (bp->proc_nr == TASK_MM) {
        struct exec_info execi;
        memset(&execi, 0, sizeof(execi));

        execi.stack_top = (void*)VM_STACK_TOP;
        execi.stack_size = PROC_ORIGIN_STACK * 2;

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

        p->regs.ip = (reg_t)execi.entry_point;
        p->regs.sp = (reg_t)VM_STACK_TOP;
        p->regs.ax = (reg_t)0; // argc
        p->regs.cx = (reg_t)0; // environ
    }
}

void arch_set_syscall_result(struct proc* p, int result)
{
    p->regs.ax = (reg_t)result;
}

/*****************************************************************************
 *                                identify_cpu
 *****************************************************************************/
/**
 * <Ring 0> Identify a cpu.
 *
 *****************************************************************************/
void identify_cpu()
{
    u32 eax, ebx, ecx, edx;
    u32 cpu = cpuid;

    eax = 0;
    _cpuid(&eax, &ebx, &ecx, &edx);

    if (ebx == INTEL_CPUID_EBX && ecx == INTEL_CPUID_ECX &&
        edx == INTEL_CPUID_EDX) {
        cpu_info[cpu].vendor = CPU_VENDOR_INTEL;
    } else if (ebx == AMD_CPUID_EBX && ecx == AMD_CPUID_ECX &&
               edx == AMD_CPUID_EDX) {
        cpu_info[cpu].vendor = CPU_VENDOR_AMD;
    } else
        cpu_info[cpu].vendor = CPU_VENDOR_UNKNOWN;

    if (eax == 0) return;

    eax = 1;
    _cpuid(&eax, &ebx, &ecx, &edx);

    cpu_info[cpu].family = (eax >> 8) & 0xf;
    if (cpu_info[cpu].family == 0xf) cpu_info[cpu].family += (eax >> 20) & 0xff;
    cpu_info[cpu].model = (eax >> 4) & 0xf;
    if (cpu_info[cpu].model == 0xf || cpu_info[cpu].model == 0x6)
        cpu_info[cpu].model += ((eax >> 16) & 0xf) << 4;
    cpu_info[cpu].stepping = eax & 0xf;
    cpu_info[cpu].flags[0] = ecx;
    cpu_info[cpu].flags[1] = edx;
}

#if CONFIG_PROFILING

int arch_init_profile_clock(u32 freq)
{
    out_byte(RTC_INDEX, RTC_REG_A);
    out_byte(RTC_IO, RTC_A_DV_OK | freq);
    out_byte(RTC_INDEX, RTC_REG_B);
    int r = in_byte(RTC_IO);
    out_byte(RTC_INDEX, RTC_REG_B);
    out_byte(RTC_IO, r | RTC_B_PIE);
    out_byte(RTC_INDEX, RTC_REG_C);
    in_byte(RTC_IO);

    return CMOS_CLOCK_IRQ;
}

void arch_stop_profile_clock()
{
    int r;
    out_byte(RTC_INDEX, RTC_REG_B);
    r = in_byte(RTC_IO);
    out_byte(RTC_INDEX, RTC_REG_B);
    out_byte(RTC_IO, r & ~RTC_B_PIE);
}

void arch_ack_profile_clock()
{
    out_byte(RTC_INDEX, RTC_REG_C);
    in_byte(RTC_IO);
}

#endif

#ifdef CONFIG_X86_64

static int do_arch_prctl_64(struct proc* proc, int option, unsigned long arg2)
{
    int retval = 0;

    switch (option) {
    case ARCH_SET_FS:
        proc->seg.fsbase = arg2;
        break;
    case ARCH_SET_GS:
        proc->seg.gsbase = arg2;
        break;
    default:
        retval = EINVAL;
        break;
    }

    return retval;
}

#endif

int sys_arch_prctl(MESSAGE* m, struct proc* p_proc)
{
    int option = m->u.m3.m3i1;
    unsigned long arg2 = (unsigned long)m->u.m3.m3l1;
    int retval = EINVAL;

#ifdef CONFIG_X86_64
    retval = do_arch_prctl_64(p_proc, option, arg2);
#endif

    return retval;
}
