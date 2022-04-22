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
#include "lyos/const.h"
#include <kernel/proc.h>
#include <kernel/global.h>
#include <kernel/proto.h>
#include <asm/const.h>
#include <asm/proto.h>
#ifdef CONFIG_SMP
#include <asm/smp.h>
#endif
#include "apic.h"
#include <asm/tsc.h>
#include <asm/hpet.h>
#include <asm/div64.h>
#include <lyos/kvm_para.h>
#include <kernel/irq.h>
#include <kernel/clockevent.h>

struct clock_event_device* global_clock_event;
static struct clock_event_device i8253_clockevent;

static irq_hook_t timer_irq_hook;

static u64 tsc_per_tick[CONFIG_SMP_MAX_CPUS];
static u64 tsc_per_state[CONFIG_SMP_MAX_CPUS][CPU_STATES];

int arch_init_time()
{
    init_hpet();
    init_tsc();

#ifdef CONFIG_KVM_GUEST
    init_kvmclock();
#endif

    return 0;
}

/*****************************************************************************
 *                                init_clock
 *****************************************************************************/
/**
 * <Ring 0> Initialize 8253/8254 PIT (Programmable Interval Timer).
 *
 *****************************************************************************/
int init_8253_timer(void)
{
    i8253_clockevent.cpumask = cpumask_of(bsp_cpu_id);
    clockevents_config_and_register(&i8253_clockevent, TIMER_FREQ, 0xF, 0x7FFF);

    global_clock_event = &i8253_clockevent;

    return 0;
}

static int set_8253_periodic(struct clock_event_device* evt)
{
    out_byte(TIMER_MODE, RATE_GENERATOR);
    out_byte(TIMER0, (u8)(TIMER_FREQ / system_hz));
    out_byte(TIMER0, (u8)((TIMER_FREQ / system_hz) >> 8));
    return 0;
}

static int set_8253_next_event(struct clock_event_device* evt,
                               unsigned long delta)
{
    out_byte(TIMER_MODE, RATE_GENERATOR);
    out_byte(TIMER0, (u8)(delta & 0xff));
    out_byte(TIMER0, (u8)(delta >> 8));
    return 0;
}

static int stop_8253_timer(struct clock_event_device* evt)
{
    out_byte(TIMER_MODE, 0x36);
    out_byte(TIMER0, 0);
    out_byte(TIMER0, 0);
    return 0;
}

static struct clock_event_device i8253_clockevent = {
    .set_state_periodic = set_8253_periodic,
    .set_state_shutdown = stop_8253_timer,
    .set_next_event = set_8253_next_event,
};

void arch_stop_context(struct proc* p, u64 delta)
{
    int counter;

    if (p->endpoint >= 0) {
        if (p->priv != priv_addr(PRIV_ID_USER)) {
            counter = CPS_SYS;
        } else {
            counter = CPS_USER;
        }
    } else {
        if (p == get_cpulocal_var_ptr(idle_proc)) {
            counter = CPS_IDLE;
        } else {
            counter = CPS_INTR;
        }
    }

    tsc_per_state[cpuid][counter] += delta;
}

void get_cpu_ticks(unsigned int cpu, u64 ticks[CPU_STATES])
{
    int i;
    for (i = 0; i < CPU_STATES; i++) {
        ticks[i] = tsc_per_state[cpu][i];
        do_div(ticks[i], tsc_per_tick[cpu]);
    }
}
