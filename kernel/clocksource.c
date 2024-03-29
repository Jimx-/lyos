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
#include <kernel/proto.h>
#ifdef CONFIG_SMP
#include <asm/smp.h>
#endif
#include <lyos/clocksource.h>
#include <lyos/time.h>
#include <asm/div64.h>
#include <lyos/spinlock.h>

struct clocksource* curr_clocksource;
static DEF_SPINLOCK(clocksource_lock);
static DEF_LIST(clocksource_list);

/*****************************************************************************
 *                                clocksource_enqueue
 *****************************************************************************/
/**
 * <Ring 0> Puts a clocksource into the queue.
 *
 * @param clocksource The clocksource to be put into the queue.
 *****************************************************************************/
static void clocksource_enqueue(struct clocksource* cs)
{
    spinlock_lock(&clocksource_lock);
    list_add(&cs->list, &clocksource_list);
    spinlock_unlock(&clocksource_lock);
}

/*****************************************************************************
 *                                clocksource_select
 *****************************************************************************/
/**
 * <Ring 0> Select the most accurate clocksource as current clocksource.
 *
 *****************************************************************************/
static void clocksource_select()
{
    spinlock_lock(&clocksource_lock);

    struct clocksource *cs, *selected = NULL;
    int rating = 0;
    list_for_each_entry(cs, &clocksource_list, list)
    {
        if (cs->rating > rating) {
            rating = cs->rating;
            selected = cs;
        }
    }
    if (curr_clocksource != NULL && curr_clocksource != selected)
        printk("kernel: switched to clocksource %s\n", selected->name);
    curr_clocksource = selected;

    spinlock_unlock(&clocksource_lock);
}

/*****************************************************************************
 *                                clocksource_register
 *****************************************************************************/
/**
 * <Ring 0> Register a clocksource.
 *
 * @param clocksource The clocksource to be registered.
 *****************************************************************************/
void clocksource_register(struct clocksource* cs)
{
    clocksource_enqueue(cs);
    clocksource_select();
}

/*****************************************************************************
 *                                calc_clock_mul_shift
 *****************************************************************************/
/**
 * <Ring 0> Calculates the mul and shift value for a clocksource.
 *
 *****************************************************************************/
static void calc_clock_mul_shift(u32* mul, u32* shift, u32 from, u32 to,
                                 u32 maxsec)
{
    u64 tmp;
    u32 sft, sftacc = 32;

    tmp = ((u64)maxsec * from) >> 32;
    while (tmp) {
        tmp >>= 1;
        sftacc--;
    }

    for (sft = 32; sft > 0; sft--) {
        tmp = (u64)to << sft;
        tmp += from / 2;
        do_div(tmp, from);
        if ((tmp >> sftacc) == 0) break;
    }
    *mul = tmp;
    *shift = sft;
}

/*****************************************************************************
 *                                update_clocksource_freq
 *****************************************************************************/
/**
 * <Ring 0> Sets frequency for a clocksource.
 *
 * @param clocksource The clocksource.
 * @param scale Frequency scale(1 or 1000).
 * @param freq The frequency.
 */
static void update_clocksource_freq(struct clocksource* cs, u32 scale, u32 freq)
{
    u64 sec;
    sec = cs->mask - (cs->mask >> 3);
    do_div(sec, scale);
    do_div(sec, freq);
    if (sec == 0)
        sec = 1;
    else if (sec > 600 && cs->mask > 0xffffffff)
        sec = 600;

    calc_clock_mul_shift(&cs->mul, &cs->shift, freq, NSEC_PER_SEC / scale,
                         sec * scale);
}

/*****************************************************************************
 *                                clocksource_register_scale
 *****************************************************************************/
/**
 * <Ring 0> Register a clocksource with frequency and scale.
 *
 *****************************************************************************/
static void clocksource_register_scale(struct clocksource* cs, u32 scale,
                                       u32 freq)
{
    update_clocksource_freq(cs, scale, freq);
    clocksource_register(cs);
}

/*****************************************************************************
 *                                clocksource_register_hz
 *****************************************************************************/
/**
 * <Ring 0> Register a clocksource with frequency in HZ.
 *
 *****************************************************************************/
void clocksource_register_hz(struct clocksource* cs, u32 hz)
{
    clocksource_register_scale(cs, 1, hz);
}

/*****************************************************************************
 *                                clocksource_register_khz
 *****************************************************************************/
/**
 * <Ring 0> Register clocksource with frequency in kHZ.
 *
 *****************************************************************************/
void clocksource_register_khz(struct clocksource* cs, u32 khz)
{
    clocksource_register_scale(cs, 1000, khz);
}
