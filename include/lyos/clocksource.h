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

#ifndef _CLOCKSOURCE_H_
#define _CLOCKSOURCE_H_

#include <lyos/list.h>

struct clocksource {
    struct list_head list;

    u64 (*read)(struct clocksource* cs);
    u32 mul;
    u32 shift;
    u64 mask;

    char* name;
    u32 rating;
};

void clocksource_register(struct clocksource* cs);
void clocksource_register_hz(struct clocksource* cs, u32 hz);
void clocksource_register_khz(struct clocksource* cs, u32 khz);

static __attribute__((always_inline)) inline u64
clocksource_cyc2ns(struct clocksource* cs, u64 cycles)
{
    return (cycles * cs->mul) >> cs->shift;
}

#endif
