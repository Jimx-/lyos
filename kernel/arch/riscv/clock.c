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
#include <kernel/clockevent.h>
#include "asm/cpulocals.h"

static unsigned long riscv_timebase;

static u64 read_cycles()
{
    u64 n;
    __asm__ __volatile__("rdtime %0" : "=r"(n));
    return n;
}

static int riscv_clock_next_event(struct clock_event_device* evt,
                                  unsigned long delta)
{
    csr_set(sie, SIE_STIE);
    u64 cycles = read_cycles();
    sbi_set_timer(cycles + delta);
}

static DEFINE_CPULOCAL(struct clock_event_device, riscv_clock_event) = {
    .rating = 100,
    .set_next_event = riscv_clock_next_event,
};

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

    clocksource_register_hz(&riscv_clocksource, riscv_timebase);

    setup_riscv_timer();

    return 0;
}

int setup_riscv_timer(void)
{
    struct clock_event_device* evt = get_cpulocal_var_ptr(riscv_clock_event);

    evt->cpumask = cpumask_of(cpuid);
    clockevents_config_and_register(evt, riscv_timebase, 100, 0x7fffffff);

    return 0;
}

void riscv_timer_interrupt(void)
{
    struct clock_event_device* evt = get_cpulocal_var_ptr(riscv_clock_event);

    csr_clear(sie, SIE_STIE);
    evt->event_handler(evt);
}

void arch_stop_context(struct proc* p, u64 delta) {}

void get_cpu_ticks(unsigned int cpu, u64 ticks[CPU_STATES]) {}
