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
#include <lyos/time.h>
#include <lyos/clocksource.h>
#include "acpi.h"
#include <asm/hpet.h>
#include <asm/div64.h>

void* hpet_addr;
void* hpet_vaddr;

static int hpet_enabled = 0;
static u64 hpet_freq;

static u64 read_hpet(struct clocksource* cs)
{
    return hpet_readl(HPET_COUNTER);
}

static struct clocksource hpet_clocksource = {
    .name = "hpet",
    .rating = 250,
    .read = read_hpet,
    .mask = 0xffffffffffffffff,
};

int init_hpet()
{
    struct acpi_hpet* hpet = acpi_get_hpet();
    if (!hpet) return 0;

    hpet_addr = (void*)((uintptr_t)hpet->address.address);
    printk("ACPI: HPET id: 0x%x base: %p\n", hpet->block_id, hpet_addr);

    /* enable hpet */
    u32 conf = hpet_read(HPET_CFG);
    conf |= HPET_CFG_ENABLE;
    hpet_write(HPET_CFG, conf);

    u32 hpet_period = hpet_read(HPET_PERIOD);
    u64 freq;
    freq = FSEC_PER_SEC;
    do_div(freq, hpet_period);
    hpet_freq = freq;

    clocksource_register_hz(&hpet_clocksource, (u32)hpet_freq);
    hpet_enabled = 1;

    return hpet_enabled;
}

int is_hpet_enabled() { return hpet_enabled; }
