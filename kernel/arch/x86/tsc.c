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
#include "protect.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <lyos/interrupt.h>
#include "arch_const.h"
#include "arch_proto.h"
#ifdef CONFIG_SMP
#include "arch_smp.h"
#endif
#include "lyos/cpulocals.h"
#include "apic.h"
#include "acpi.h"
#include "hpet.h"
#include "div64.h"
#include <lyos/clocksource.h>

PUBLIC u32 tsc_khz;

PRIVATE u64 tsc_read(struct clocksource * cs)
{
    u64 tsc;
    read_tsc_64(&tsc);
    return tsc;
}

PRIVATE struct clocksource tsc_clocksource = {
    .name = "tsc",
    .rating = 300,
    .read = read_tsc_64,
    .mask = 0xffffffffffffffff,
};

PRIVATE u32 calibrate_tsc();
PRIVATE u32 pit_calibrate_tsc();

PRIVATE void init_tsc_clocksource()
{
    /* TSC is not an ideal clocksource on SMP systems */
#if CONFIG_SMP
    tsc_clocksource.rating = 0;
#endif
    register_clocksource_khz(&tsc_clocksource, tsc_khz);
}

PUBLIC void init_tsc()
{
    tsc_khz = calibrate_tsc();
    init_tsc_clocksource();
}

#if 0
/* Calculate TSC frequency from HPET reference */
PRIVATE u64 hpet_ref_tsc(u64 tsc_delta, u64 hpet1, u64 hpet2)
{
    u64 tmp;

    if (hpet2 < hpet1)
        hpet2 += 0x100000000ULL;
    hpet2 -= hpet1;
    tmp = ((u64)hpet2 * hpet_read(HPET_PERIOD));
    do_div(tmp, 1000000);
    do_div(tsc_delta, tmp);

    return (u64)tsc_delta;
}
#endif

PRIVATE u32 calibrate_tsc()
{
    u32 pit_tsc_khz = pit_calibrate_tsc();
    if (pit_tsc_khz) return pit_tsc_khz;

    return 0;
}

PRIVATE u32 probe_ticks;
#define PROBE_TICKS (system_hz / 10)
PRIVATE u64 tsc0, tsc1;

PRIVATE int tsc_calibrate_handler(irq_hook_t * hook)
{
    u64 tsc;

    probe_ticks++;
    read_tsc_64(&tsc);


    if (probe_ticks == 1) {
        tsc0 = tsc;
    }
    else if (probe_ticks == PROBE_TICKS) {
        tsc1 = tsc;
    }

    return 1;
}

PRIVATE u32 pit_calibrate_tsc()
{
    irq_hook_t calibrate_hook;

    probe_ticks = 0;

    put_irq_handler(CLOCK_IRQ, &calibrate_hook, tsc_calibrate_handler);
    init_8253_timer(system_hz);

    enable_int();
    while (probe_ticks < PROBE_TICKS) enable_int();
    disable_int();

    rm_irq_handler(&calibrate_hook);

    u64 tsc_delta = tsc1 - tsc0;
    do_div(tsc_delta, PROBE_TICKS - 1);
    tsc_delta *= make64(0, system_hz);
    do_div(tsc_delta, 1000);
    printk("tsc: TSC calibration using PIT\n", tsc_delta);

    return tsc_delta;
}
