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
#include "stddef.h"
#include "protect.h"
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

PRIVATE irq_hook_t timer_irq_hook;

/*****************************************************************************
 *                                init_clock
 *****************************************************************************/
/**
 * <Ring 0> Initialize 8253/8254 PIT (Programmable Interval Timer).
 * 
 *****************************************************************************/
PUBLIC int init_8253_timer(int freq)
{
    /* 初始化 8253 PIT */
    out_byte(TIMER_MODE, RATE_GENERATOR);
    out_byte(TIMER0, (u8) (TIMER_FREQ/freq) );
    out_byte(TIMER0, (u8) ((TIMER_FREQ/freq) >> 8));

    return 0;
}

PUBLIC void stop_8253_timer()
{
    out_byte(TIMER_MODE, 0x36);
    out_byte(TIMER0, 0);
    out_byte(TIMER0, 0);
}

PUBLIC int init_local_timer(int freq)
{
#if CONFIG_X86_LOCAL_APIC
    if (lapic_addr) {
        unsigned cpu = cpuid;
        stop_8253_timer();
    } else 
#endif
    {
        init_8253_timer(freq);
    }

    return 0;
}

PUBLIC int put_local_timer_handler(irq_handler_t handler)
{
#if CONFIG_X86_LOCAL_APIC
    if (lapic_addr) {

    } else
#endif
    {
        put_irq_handler(CLOCK_IRQ, &timer_irq_hook, handler);
    }

    return 0;
}
