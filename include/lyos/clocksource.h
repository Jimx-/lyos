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

struct clocksource;

struct clocksource {
    struct list_head list;

    u64 (*read)(struct clocksource* cs);
    u32 mul;
    u32 shift;
    u64 mask;

    char* name;
    u32 rating;
};

PUBLIC void init_clocksource();
PUBLIC void register_clocksource(struct clocksource* cs);
PUBLIC void register_clocksource_hz(struct clocksource* cs, u32 hz);
PUBLIC void register_clocksource_khz(struct clocksource* cs, u32 khz);
PUBLIC u64 clocksource_cyc2ns(struct clocksource* cs, u64 cycles);

#endif
