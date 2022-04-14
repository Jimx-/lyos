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
#include <asm/cpulocals.h>
#include <lyos/cpufeature.h>
#include <lyos/vm.h>
#include <asm/sysreg.h>
#include <asm/esr.h>

DEFINE_CPULOCAL(unsigned long, percpu_kstack);

int init_tss(unsigned int cpu, void* kernel_stack)
{
    struct tss* t = &tss[cpu];

    get_cpu_var(cpu, percpu_kstack) =
        (unsigned long)kernel_stack - ARM64_STACK_TOP_RESERVED;

    memset(t, 0, sizeof(struct tss));
    t->sp0 = get_cpu_var(cpu, percpu_kstack);

    return 0;
}

static void el0_svc(struct proc* p)
{
    handle_sys_call((int)p->regs.regs[8], (MESSAGE*)p->regs.regs[0], p);
}

void el1t_64_sync_handler(struct proc* p) {}

void el1t_64_irq_handler(struct proc* p) {}

void el1t_64_fiq_handler(struct proc* p) {}

void el1t_64_error_handler(struct proc* p) {}

void el1h_64_sync_handler(struct proc* p) {}

void el1h_64_irq_handler(struct proc* p) {}

void el1h_64_fiq_handler(struct proc* p) {}

void el1h_64_error_handler(struct proc* p) {}

void el0t_64_sync_handler(struct proc* p)
{
    unsigned long esr = read_sysreg(esr_el1);

    switch (ESR_ELx_EC(esr)) {
    case ESR_ELx_EC_SVC64:
        el0_svc(p);
        break;
    }
}

void el0t_64_irq_handler(struct proc* p) {}

void el0t_64_fiq_handler(struct proc* p) {}

void el0t_64_error_handler(struct proc* p) {}

void el0t_32_sync_handler(struct proc* p) {}

void el0t_32_irq_handler(struct proc* p) {}

void el0t_32_fiq_handler(struct proc* p) {}

void el0t_32_error_handler(struct proc* p) {}
