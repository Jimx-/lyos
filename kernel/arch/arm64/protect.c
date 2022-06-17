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
#include "lyos/const.h"
#include <kernel/proc.h>
#include "string.h"
#include <errno.h>
#include <signal.h>
#include <kernel/global.h>
#include <kernel/proto.h>
#include <asm/const.h>
#include <asm/proto.h>
#include <asm/smp.h>
#include <asm/cpulocals.h>
#include <lyos/cpufeature.h>
#include <lyos/vm.h>
#include <asm/sysreg.h>
#include <asm/esr.h>
#include <asm/mach.h>

struct fault_info {
    int (*fn)(int in_kernel, unsigned long far, unsigned int esr,
              struct stackframe* frame);
    int sig;
    const char* name;
};

static const struct fault_info fault_info[];

DEFINE_CPULOCAL(unsigned long, percpu_kstack);

void copy_user_message_end();
void copy_user_message_fault();
void phys_copy_fault();
void phys_copy_fault_in_kernel();

int handle_sys_call(int call_nr, MESSAGE* m_user, struct proc* p_proc);

static void __panic_unhandled(struct stackframe* frame, const char* vector,
                              unsigned int esr)
{
    printk("Unhandled %s exception on CPU%d, ESR 0x%08x\n", vector, cpuid, esr);
    panic("Unhandled exception");
}

#define UNHANDLED(el, regsize, vector)                                 \
    void el##_##regsize##_##vector##_handler(struct stackframe* frame) \
    {                                                                  \
        const char* desc = #regsize "-bit " #el " " #vector;           \
        __panic_unhandled(frame, desc, read_sysreg(esr_el1));          \
    }

static inline const struct fault_info* esr_to_fault_info(unsigned int esr)
{
    return fault_info + (esr & ESR_ELx_FSC);
}

int init_tss(unsigned int cpu, void* kernel_stack)
{
    struct tss* t = &tss[cpu];

    get_cpu_var(cpu, percpu_kstack) =
        (unsigned long)kernel_stack - ARM64_STACK_TOP_RESERVED;

    memset(t, 0, sizeof(struct tss));
    t->sp0 = get_cpu_var(cpu, percpu_kstack);

    return 0;
}

static int is_el0_instruction_abort(unsigned int esr)
{
    return ESR_ELx_EC(esr) == ESR_ELx_EC_IABT_LOW;
}

static int is_write_abort(unsigned int esr)
{
    return (esr & ESR_ELx_WNR) && !(esr & ESR_ELx_CM);
}

static int is_el1_instruction_abort(unsigned int esr)
{
    return ESR_ELx_EC(esr) == ESR_ELx_EC_IABT_CUR;
}

static int is_el1_data_abort(unsigned int esr)
{
    return ESR_ELx_EC(esr) == ESR_ELx_EC_DABT_CUR;
}

static int do_bad(int in_kernel, unsigned long far, unsigned int esr,
                  struct stackframe* frame)
{
    return 1;
}

static int do_page_fault(int in_kernel, unsigned long far, unsigned int esr,
                         struct stackframe* frame)
{
    struct proc* fault_proc = get_cpulocal_var(proc_ptr);
    int fault_flags = 0;
    MESSAGE msg;

    if (in_kernel || is_kerntaske(fault_proc->endpoint)) {
        if (!is_el1_instruction_abort(esr)) {
            int in_phys_copy = (frame->pc >= (reg_t)phys_copy) &&
                               (frame->pc < (reg_t)phys_copy_fault);
            int in_phys_set = 0; /* Not implemented. */
            int in_copy_message = (frame->pc >= (reg_t)copy_user_message) &&
                                  (frame->pc < (reg_t)copy_user_message_end);

            if (in_phys_copy || in_phys_set || in_copy_message) {
                if (in_phys_copy) {
                    frame->pc = (reg_t)phys_copy_fault_in_kernel;
                    frame->regs[0] = far;
                } else if (in_copy_message) {
                    frame->pc = (reg_t)copy_user_message_fault;
                }

                return 0;
            }
        }

        panic("unhandled page fault in kernel, pc: %lx, esr: %lx, far: %lx",
              frame->pc, esr, far);
    }

    if (fault_proc->endpoint == TASK_MM) {
        panic("unhandled page fault in MM, pc: %lx, esr: %lx, far: %lx",
              frame->pc, esr, far);
    }

    if (is_el0_instruction_abort(esr)) {
        fault_flags |= FAULT_FLAG_INSTRUCTION;
    } else if (is_write_abort(esr)) {
        fault_flags |= FAULT_FLAG_WRITE;
    }

    if (!in_kernel) fault_flags |= FAULT_FLAG_USER;

    /* inform MM to handle this page fault */
    memset(&msg, 0, sizeof(msg));
    msg.type = FAULT;
    msg.FAULT_ADDR = (void*)far;
    msg.FAULT_PROC = fault_proc->endpoint;
    msg.FAULT_ERRCODE = fault_flags;
    msg.FAULT_PC = (void*)frame->pc;

    msg_send(fault_proc, TASK_MM, &msg, IPCF_FROMKERNEL);

    /* block the process */
    PST_SET(fault_proc, PST_PAGEFAULT);

    return 0;
}

static int do_translation_fault(int in_kernel, unsigned long far,
                                unsigned int esr, struct stackframe* frame)
{
    return do_page_fault(in_kernel, far, esr, frame);
}

static const struct fault_info fault_info[] = {
    {do_bad, SIGKILL, "ttbr address size fault"},
    {do_bad, SIGKILL, "level 1 address size fault"},
    {do_bad, SIGKILL, "level 2 address size fault"},
    {do_bad, SIGKILL, "level 3 address size fault"},
    {do_translation_fault, SIGSEGV, "level 0 translation fault"},
    {do_translation_fault, SIGSEGV, "level 1 translation fault"},
    {do_translation_fault, SIGSEGV, "level 2 translation fault"},
    {do_translation_fault, SIGSEGV, "level 3 translation fault"},
    {do_bad, SIGKILL, "unknown 8"},
    {do_page_fault, SIGSEGV, "level 1 access flag fault"},
    {do_page_fault, SIGSEGV, "level 2 access flag fault"},
    {do_page_fault, SIGSEGV, "level 3 access flag fault"},
    {do_bad, SIGKILL, "unknown 12"},
    {do_page_fault, SIGSEGV, "level 1 permission fault"},
    {do_page_fault, SIGSEGV, "level 2 permission fault"},
    {do_page_fault, SIGSEGV, "level 3 permission fault"},
    {do_bad, SIGBUS, "synchronous external abort"},
    {do_bad, SIGSEGV, "synchronous tag check fault"},
    {do_bad, SIGKILL, "unknown 18"},
    {do_bad, SIGKILL, "unknown 19"},
    {do_bad, SIGKILL, "level 0 (translation table walk)"},
    {do_bad, SIGKILL, "level 1 (translation table walk)"},
    {do_bad, SIGKILL, "level 2 (translation table walk)"},
    {do_bad, SIGKILL, "level 3 (translation table walk)"},
    {do_bad, SIGBUS, "synchronous parity or ECC error"},
    {do_bad, SIGKILL, "unknown 25"},
    {do_bad, SIGKILL, "unknown 26"},
    {do_bad, SIGKILL, "unknown 27"},
    {do_bad, SIGKILL,
     "level 0 synchronous parity error (translation table walk)"},
    {do_bad, SIGKILL,
     "level 1 synchronous parity error (translation table walk)"},
    {do_bad, SIGKILL,
     "level 2 synchronous parity error (translation table walk)"},
    {do_bad, SIGKILL,
     "level 3 synchronous parity error (translation table walk)"},
    {do_bad, SIGKILL, "unknown 32"},
    {do_bad, SIGBUS, "alignment fault"},
    {do_bad, SIGKILL, "unknown 34"},
    {do_bad, SIGKILL, "unknown 35"},
    {do_bad, SIGKILL, "unknown 36"},
    {do_bad, SIGKILL, "unknown 37"},
    {do_bad, SIGKILL, "unknown 38"},
    {do_bad, SIGKILL, "unknown 39"},
    {do_bad, SIGKILL, "unknown 40"},
    {do_bad, SIGKILL, "unknown 41"},
    {do_bad, SIGKILL, "unknown 42"},
    {do_bad, SIGKILL, "unknown 43"},
    {do_bad, SIGKILL, "unknown 44"},
    {do_bad, SIGKILL, "unknown 45"},
    {do_bad, SIGKILL, "unknown 46"},
    {do_bad, SIGKILL, "unknown 47"},
    {do_bad, SIGKILL, "TLB conflict abort"},
    {do_bad, SIGKILL, "Unsupported atomic hardware update fault"},
    {do_bad, SIGKILL, "unknown 50"},
    {do_bad, SIGKILL, "unknown 51"},
    {do_bad, SIGKILL, "implementation fault (lockdown abort)"},
    {do_bad, SIGBUS, "implementation fault (unsupported exclusive)"},
    {do_bad, SIGKILL, "unknown 54"},
    {do_bad, SIGKILL, "unknown 55"},
    {do_bad, SIGKILL, "unknown 56"},
    {do_bad, SIGKILL, "unknown 57"},
    {do_bad, SIGKILL, "unknown 58"},
    {do_bad, SIGKILL, "unknown 59"},
    {do_bad, SIGKILL, "unknown 60"},
    {do_bad, SIGKILL, "section domain fault"},
    {do_bad, SIGKILL, "page domain fault"},
    {do_bad, SIGKILL, "unknown 63"},
};

void do_mem_abort(int in_kernel, unsigned long far, unsigned int esr,
                  struct stackframe* frame)
{
    const struct fault_info* inf = esr_to_fault_info(esr);
    struct proc* fault_proc = get_cpulocal_var(proc_ptr);

    if (!inf->fn(in_kernel, far, esr, frame)) return;

    if (in_kernel) {
        panic("unhandled memory abort in kernel, pc: %lx, esr: %lx, far: %lx",
              frame->pc, esr, far);
    } else {
        printk("kernel: memory abort in userspace endpoint %d, pc: %lx, esr: "
               "%lx, far: %lx",
               fault_proc->endpoint, frame->pc, esr, far);
        ksig_proc(fault_proc->endpoint, inf->sig);
    }
}

static void el1_abort(struct stackframe* frame, unsigned long esr)
{
    unsigned long far = read_sysreg(far_el1);
    do_mem_abort(TRUE, far, esr, frame);
}

static void el1_interrupt(struct stackframe* frame, void (*handler)(void))
{
    if (!handler) panic("no root IRQ handler");

    write_sysreg(PSR_I_BIT | PSR_F_BIT, daif);
    handler();
}

UNHANDLED(el1t, 64, sync)
UNHANDLED(el1t, 64, irq)
UNHANDLED(el1t, 64, fiq)
UNHANDLED(el1t, 64, error)

void el1h_64_sync_handler(struct stackframe* frame)
{
    unsigned long esr = read_sysreg(esr_el1);

    switch (ESR_ELx_EC(esr)) {
    case ESR_ELx_EC_DABT_CUR:
    case ESR_ELx_EC_IABT_CUR:
        el1_abort(frame, esr);
        break;
    }
}

void el1h_64_irq_handler(struct stackframe* frame)
{
    el1_interrupt(frame, machine_desc->handle_irq);
}

void el1h_64_fiq_handler(struct stackframe* frame)
{
    el1_interrupt(frame, machine_desc->handle_fiq);
}

void el1h_64_error_handler(struct stackframe* frame) {}

static void el0_svc(struct stackframe* frame)
{
    struct proc* p = get_cpulocal_var(proc_ptr);
    handle_sys_call((int)p->regs.regs[8], (MESSAGE*)p->regs.regs[0], p);
}

static void el0_da(struct stackframe* frame, unsigned long esr)
{
    unsigned long far = read_sysreg(far_el1);
    do_mem_abort(FALSE, far, esr, frame);
}

static void el0_ia(struct stackframe* frame, unsigned long esr)
{
    unsigned long far = read_sysreg(far_el1);
    do_mem_abort(FALSE, far, esr, frame);
}

static void el0_interrupt(struct stackframe* frame, void (*handler)(void))
{
    if (!handler) panic("no root IRQ handler");

    write_sysreg(PSR_I_BIT | PSR_F_BIT, daif);
    handler();
}

static void el0_fpsimd_acc(struct stackframe* regs, unsigned long esr)
{
    copr_not_available_handler();
}

void el0t_64_sync_handler(struct stackframe* frame)
{
    unsigned long esr = read_sysreg(esr_el1);

    switch (ESR_ELx_EC(esr)) {
    case ESR_ELx_EC_SVC64:
        el0_svc(frame);
        break;
    case ESR_ELx_EC_DABT_LOW:
        el0_da(frame, esr);
        break;
    case ESR_ELx_EC_IABT_LOW:
        el0_ia(frame, esr);
        break;
    case ESR_ELx_EC_FP_ASIMD:
        el0_fpsimd_acc(frame, esr);
        break;
    }
}

void el0t_64_irq_handler(struct stackframe* frame)
{
    el0_interrupt(frame, machine_desc->handle_irq);
}

void el0t_64_fiq_handler(struct stackframe* frame)
{
    el0_interrupt(frame, machine_desc->handle_fiq);
}

void el0t_64_error_handler(struct stackframe* frame) {}

void el0t_32_sync_handler(struct stackframe* frame) {}

void el0t_32_irq_handler(struct stackframe* frame)
{
    el0_interrupt(frame, machine_desc->handle_irq);
}

void el0t_32_fiq_handler(struct stackframe* frame)
{
    el0_interrupt(frame, machine_desc->handle_fiq);
}

void el0t_32_error_handler(struct stackframe* frame) {}
