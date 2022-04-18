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
#include "lyos/const.h"
#include "string.h"
#include <kernel/proc.h>
#include <kernel/global.h>
#include <kernel/proto.h>
#include <kernel/irq.h>
#include <asm/const.h>
#include <asm/proto.h>
#include <asm/byteorder.h>
#include <asm/csr.h>
#include <asm/sbi.h>
#ifdef CONFIG_SMP
#include <asm/smp.h>
#endif
#include <asm/div64.h>
#include <lyos/clocksource.h>
#include "asm/cpulocals.h"

static unsigned long riscv_timebase;

static u64 tsc_per_tick[CONFIG_SMP_MAX_CPUS];
static DEFINE_CPULOCAL(irq_handler_t, timer_handler);

static u64 read_cycles()
{
    u64 n;
    __asm__ __volatile__("rdtime %0" : "=r"(n));
    return n;
}

static struct clocksource riscv_clocksource = {
    .name = "riscv",
    .rating = 300,
    .read = read_cycles,
    .mask = 0xffffffffffffffff,
};

int arch_init_time()
{
    unsigned long cpu = fdt_path_offset(initial_boot_params, "/cpus");

    const uint32_t* prop;
    if ((prop = fdt_getprop(initial_boot_params, cpu, "timebase-frequency",
                            NULL)) != NULL) {
        riscv_timebase = be32_to_cpup(prop);
    } else {
        panic("RISC-V system with no 'timebase-frequency' in DTS");
    }

    register_clocksource_hz(&riscv_clocksource, riscv_timebase);

    return 0;
}

int init_local_timer(int freq)
{
    tsc_per_tick[cpuid] = riscv_timebase;
    do_div(tsc_per_tick[cpuid], freq);

    return 0;
}

void setup_local_timer_one_shot(void) {}

void setup_local_timer_periodic(void) {}

void restart_local_timer()
{
    csr_set(sie, SIE_STIE);
    u64 cycles = read_cycles();
    sbi_set_timer(cycles + tsc_per_tick[cpuid]);
}

void stop_local_timer() {}

int put_local_timer_handler(irq_handler_t handler)
{
    get_cpulocal_var(timer_handler) = handler;

    return 0;
}

void riscv_timer_interrupt(void)
{
    irq_handler_t handler = get_cpulocal_var(timer_handler);

    csr_clear(sie, SIE_STIE);
    handler(NULL);
}

void arch_stop_context(struct proc* p, u64 delta) {}

void get_cpu_ticks(unsigned int cpu, u64 ticks[CPU_STATES]) {}
