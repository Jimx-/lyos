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

#ifndef _HPET_H_
#define _HPET_H_

#define HPET_ID 0x000
#define HPET_PERIOD 0x004
#define HPET_CFG 0x010
#define HPET_STATUS 0x020
#define HPET_COUNTER 0x0f0

#define HPET_CFG_ENABLE 0x001
#define HPET_CFG_LEGACY 0x002

extern void* hpet_addr;

PUBLIC int init_hpet();
PUBLIC int is_hpet_enabled();

static __attribute__((always_inline)) inline u32 hpet_read(u32 a)
{
    return *(volatile u32*)(hpet_addr + a);
}

static __attribute__((always_inline)) inline u64 hpet_readl(u32 a)
{
    return *(volatile u64*)(hpet_addr + a);
}

static __attribute__((always_inline)) inline u32 hpet_write(u32 a, u32 v)
{
    return *(volatile u32*)(hpet_addr + a) = v;
}

#endif
