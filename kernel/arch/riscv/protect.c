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
#include "lyos/proc.h"
#include "string.h"
#include <errno.h>
#include <signal.h>
#include "lyos/global.h"
#include "lyos/proto.h"
#include <asm/const.h>
#include <asm/proto.h>
#include <asm/smp.h>
#include <asm/csr.h>
#include <lyos/cpulocals.h>
#include <lyos/cpufeature.h>
#include <lyos/vm.h>

struct tss tss[CONFIG_SMP_MAX_CPUS];

struct exception_frame {
    reg_t sepc;
    reg_t sbadaddr;
    reg_t scause;
};

extern void trap_entry(void);

void copy_user_message_end();
void copy_user_message_fault();
void phys_copy_fault();
void phys_copy_fault_in_kernel();

int init_tss(unsigned int cpu, void* kernel_stack)
{
    struct tss* t = &tss[cpu];

    t->sp0 = (reg_t)kernel_stack;
    return 0;
}

void init_prot()
{
    csr_write(stvec, &trap_entry);
    csr_write(sie, -1);

    plic_scan();
}

void do_trap_unknown(int in_kernel, struct proc* p)
{
    printk("unknown trap\n");
}

void do_trap_insn_misaligned(int in_kernel, struct proc* p)
{
    printk("insn misaligned\n");
}

void do_trap_insn_fault(int in_kernel, struct proc* p)
{
    printk("insn fault\n");
}

void do_trap_insn_illegal(int in_kernel, struct proc* p,
                          struct exception_frame* frame)
{
    printk("insn fault %p\n", frame->sepc);
    ksig_proc(p->endpoint, SIGILL);
}

void do_trap_break(int in_kernel, struct proc* p) { printk("break\n"); }

void do_trap_load_misaligned(int in_kernel, struct proc* p)
{
    printk("load misaligned\n");
}

void do_trap_load_fault(int in_kernel, struct proc* p)
{
    printk("load fault\n");
}

void do_trap_store_misaligned(int in_kernel, struct proc* p)
{
    printk("store misaligned\n");
}

void do_trap_store_fault(int in_kernel, struct proc* p)
{
    printk("store fault\n");
}

void do_trap_ecall_u(int in_kernel, struct proc* p) { printk("ecall u\n"); }

void do_trap_ecall_s(int in_kernel, struct proc* p) { printk("ecall s\n"); }

void do_trap_ecall_m(int in_kernel, struct proc* p) { printk("ecall m\n"); }

void do_page_fault(int in_kernel, struct proc* p, struct exception_frame* frame)
{
    struct proc* fault_proc = get_cpulocal_var(proc_ptr);
    void* fault_addr = (void*)frame->sbadaddr;

    int in_phys_copy = (frame->sepc >= (uintptr_t)phys_copy) &&
                       (frame->sepc < (uintptr_t)phys_copy_fault);
    int in_phys_set = 0; /* Not implemented. */
    int in_copy_message = (frame->sepc >= (reg_t)copy_user_message) &&
                          (frame->sepc < (reg_t)copy_user_message_end);

    if ((in_kernel || is_kerntaske(fault_proc->endpoint)) &&
        (in_phys_copy || in_phys_set || in_copy_message)) {
        if (in_kernel) {
            if (in_phys_copy) {
                frame->sepc = (reg_t)phys_copy_fault_in_kernel;
            } else if (in_copy_message) {
                frame->sepc = (reg_t)copy_user_message_fault;
            }
        } else {
            fault_proc->regs.sepc = (reg_t)phys_copy_fault;
            fault_proc->regs.a0 = fault_addr;
        }

        return;
    }

    if (in_kernel) {
        panic("unhandled page fault in kernel, epc: %p, tval: %p",
              (void*)frame->sepc, fault_addr);
    }

    if (fault_proc->endpoint == TASK_MM) {
        panic("unhandled page fault in MM, epc: %p, tval: %p",
              (void*)fault_proc->regs.sepc, fault_proc->regs.sbadaddr);
    }

    int fault_flags = 0;
    if (frame->scause == EXC_STORE_PAGE_FAULT)
        fault_flags |= FAULT_FLAG_WRITE;
    else if (frame->scause == EXC_INST_PAGE_FAULT)
        fault_flags |= FAULT_FLAG_INSTRUCTION;

    if (!in_kernel) fault_flags |= FAULT_FLAG_USER;

    /* inform MM to handle this page fault */
    MESSAGE msg;
    msg.type = FAULT;
    msg.FAULT_ADDR = (void*)frame->sbadaddr;
    msg.FAULT_PROC = fault_proc->endpoint;
    msg.FAULT_ERRCODE = fault_flags;
    msg.FAULT_PC = (void*)frame->sepc;

    msg_send(fault_proc, TASK_MM, &msg, IPCF_FROMKERNEL);

    /* block the process */
    PST_SET(fault_proc, PST_PAGEFAULT);
}
