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

#include "lyos/type.h"
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "arch_const.h"
#include "arch_proto.h"
#ifdef CONFIG_SMP
#include "arch_smp.h"
#endif
#include "lyos/cpulocals.h"
#include <lyos/clocksource.h>
#include <lyos/time.h>
#include "div64.h"

extern spinlock_t clocksource_lock;

PUBLIC clock_t idle_ticks = 0;

DEF_LIST(timer_list);
PUBLIC spinlock_t timers_lock;
PUBLIC clock_t next_timeout = TIMER_UNSET;

/*****************************************************************************
 *                                read_jiffies
 *****************************************************************************/
/**
 * <Ring 0> Reads jiffies.
 * 
 * @param clocksource The jiffies clocksource.
 * @return The jiffies.
 *****************************************************************************/
PRIVATE u64 read_jiffies(struct clocksource * cs)
{
    return jiffies;
}

#define NSEC_PER_JIFFY  (NSEC_PER_SEC/DEFAULT_HZ)
#define JIFFY_SHIFT     8
PRIVATE struct clocksource jiffies_clocksource = {
    .name = "jiffies",
    .rating = 1,
    .read = read_jiffies,
    .mask = 0xffffffff,
    .mul = NSEC_PER_JIFFY << JIFFY_SHIFT,
    .shift = JIFFY_SHIFT,
};

/*****************************************************************************
 *                                clock_handler
 *****************************************************************************/
/**
 * <Ring 0> This routine handles the clock interrupt generated by 8253/8254
 *          programmable interval timer.
 * 
 * @param irq The IRQ nr, unused here.
 *****************************************************************************/
PUBLIC int clock_handler(irq_hook_t * hook)
{
#if CONFIG_SMP
    if (cpuid == bsp_cpu_id) {
#endif
	   if (++jiffies >= MAX_TICKS)
		  jiffies = 0;
#if CONFIG_SMP
    }
#endif

    if (get_cpulocal_var(proc_ptr) == get_cpulocal_var_ptr(idle_proc)) idle_ticks++;

#if CONFIG_SMP
    if (cpuid == bsp_cpu_id) {
#endif
        /* timer expired */
        if (jiffies >= next_timeout && next_timeout != TIMER_UNSET) {
            timer_expire(&timer_list, jiffies);
            if (list_empty(&timer_list)) next_timeout = TIMER_UNSET;
            else next_timeout = list_entry(timer_list.next, struct timer_list, list)->expire_time;
        }
#if CONFIG_SMP
    }
#endif

    return 1;
}

/*****************************************************************************
 *                                init_time
 *****************************************************************************/
/**
 * <Ring 0> This routine initializes time subsystem.
 * 
 *****************************************************************************/
PUBLIC int init_time()
{
    jiffies = 0;

    init_clocksource();
    arch_init_time();
    register_clocksource(&jiffies_clocksource);
    spinlock_init(&timers_lock);

    return 0;
}

/*****************************************************************************
 *                                init_bsp_timer
 *****************************************************************************/
/**
 * <Ring 0> This routine initializes timer for BSP.
 * 
 * @param freq Timer frequency.
 *****************************************************************************/
PUBLIC int init_bsp_timer(int freq)
{
    if (init_local_timer(freq)) return -1;
    if (put_local_timer_handler(clock_handler)) return -1;

    return 0;
}

/*****************************************************************************
 *                                init_ap_timer
 *****************************************************************************/
/**
 * <Ring 0> This routine initializes timer for APs.
 * 
 * @param freq Timer frequency.
 *****************************************************************************/
PUBLIC int init_ap_timer(int freq)
{
    if (init_local_timer(freq)) return -1;

    return 0;
}

/*****************************************************************************
 *                                stop_context
 *****************************************************************************/
/**
 * <Ring 0> This routine performs context switching.
 * 
 * @param p Stop context for whom.
 *****************************************************************************/
PUBLIC void stop_context(struct proc * p)
{
    spinlock_lock(&clocksource_lock);
    u64 * ctx_switch_clock = get_cpulocal_var_ptr(context_switch_clock);

    if (!curr_clocksource) return;
    u64 cycle = curr_clocksource->read(curr_clocksource);
    u64 delta = cycle - *ctx_switch_clock;
    p->cycles += delta;

    u64 nsec = clocksource_cyc2ns(curr_clocksource, delta);

    if (p->endpoint >= 0) {
        if (ex64hi(nsec) < ex64hi(p->counter_ns) ||
                (ex64hi(nsec) == ex64hi(p->counter_ns) &&
                 ex64lo(nsec) < ex64lo(p->counter_ns)))
            p->counter_ns = p->counter_ns - nsec;
        else {
            p->counter_ns = 0;
        }
    }

    *ctx_switch_clock = cycle;
    spinlock_unlock(&clocksource_lock);
}

PUBLIC void set_sys_timer(struct timer_list* timer)
{
    spinlock_lock(&timers_lock);

    timer_add(&timer_list, timer);
    next_timeout = list_entry(timer_list.next, struct timer_list, list)->expire_time;

    spinlock_unlock(&timers_lock);
}

PUBLIC void reset_sys_timer(struct timer_list* timer)
{
    spinlock_lock(&timers_lock);

    timer_remove(timer);
    if (list_empty(&timer_list)) next_timeout = TIMER_UNSET;
    else next_timeout = list_entry(timer_list.next, struct timer_list, list)->expire_time;

    spinlock_unlock(&timers_lock);
}
