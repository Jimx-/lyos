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
#include <asm/protect.h>
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
#include <asm/cmos.h>
#include <asm/fpu.h>

struct cpu_info cpu_info[CONFIG_SMP_MAX_CPUS];

void _cpuid(u32* eax, u32* ebx, u32* ecx, u32* edx);

/**
 * <Ring 0> Switch back to user.
 */
struct proc* arch_switch_to_user()
{
    char* stack;
    struct proc* p;

#ifdef CONFIG_SMP
    stack = (char*)tss[cpuid].esp0;
#else
    stack = (char*)tss[0].esp0;
#endif

    p = get_cpulocal_var(proc_ptr);
    /* save the proc ptr on the stack */
    *((reg_t*)stack) = (reg_t)p;

    return p;
}

static char fpu_state[NR_PROCS][FPU_XFP_SIZE] __attribute__((aligned(16)));

int arch_reset_proc(struct proc* p)
{
    char* fpu = NULL;

    p->regs.eip = 0;
    p->regs.esp = 0;

    if (p->endpoint == TASK_MM) {
        /* use bootstrap page table */
        p->seg.cr3_phys = __pa(initial_pgd);
        p->seg.cr3_vir = (u32*)initial_pgd;
    }

    p->regs.cs = SELECTOR_USER_CS | RPL_USER;
    p->regs.ds = p->regs.es = p->regs.fs = p->regs.gs = p->regs.ss =
        SELECTOR_USER_DS | RPL_USER;

    p->regs.eflags = 0x202; /* IF=1, bit 2 is always 1 */
    p->seg.trap_style = KTS_INT;

    if (p->slot >= 0) {
        fpu = fpu_state[p->slot];
        memset(fpu, 0, FPU_XFP_SIZE);
    }

    p->seg.fpu_state = fpu;

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
    strlcpy(p->name, name, sizeof(p->name));

    p->regs.esp = (reg_t)sp;
    p->regs.eip = (reg_t)ip;
    p->regs.eax = ps->ps_nargvstr;
    p->regs.edx = (reg_t)ps->ps_argvstr;
    p->regs.ecx = (reg_t)ps->ps_envstr;
    p->seg.trap_style = KTS_INT;

    return 0;
}

static int kernel_clearmem(struct exec_info* execi, void* vaddr, size_t len)
{
    memset(vaddr, 0, len);
    return 0;
}

static int kernel_allocmem(struct exec_info* execi, void* vaddr, size_t len)
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

        p->regs.eip = (u32)execi.entry_point;
        p->regs.esp = (u32)VM_STACK_TOP;
        p->regs.ecx = (u32)0; // environ
    }
}

void arch_set_syscall_result(struct proc* p, int result)
{
    p->regs.eax = (u32)result;
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
