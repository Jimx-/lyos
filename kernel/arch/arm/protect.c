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
#include "arch.h"
#include "arch_const.h"
#include "arch_proto.h"
#include "arch_smp.h"
#include "arch_type.h"
#include <lyos/cpulocals.h>
#include <lyos/cpufeature.h>

extern int exc_vector_table;

struct tss tss[CONFIG_SMP_MAX_CPUS];

int init_tss(unsigned cpu, unsigned kernel_stack)
{
    struct tss* t = &tss[cpu];

    t->sp0 = kernel_stack - ARM_STACK_TOP_RESERVED;
    *((reg_t*)(t->sp0 + sizeof(reg_t))) = cpu;

    return 0;
}

void init_prot() { write_vbar((reg_t)&exc_vector_table); }

void irq_entry_handle()
{
    if (machine_desc->handle_irq) machine_desc->handle_irq();
}
