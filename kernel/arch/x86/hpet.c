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
#include "apic.h"
#include "acpi.h"
#include "hpet.h"

PRIVATE vir_bytes   hpet_addr;
PRIVATE vir_bytes   hpet_vaddr;

PRIVATE u8 hpet_enabled = 0;

PUBLIC int init_hpet()
{
    struct acpi_hpet * hpet = acpi_get_hpet();
    if (!hpet) return 0;

    hpet_addr = hpet->address.address;
    printk("ACPI: HPET id: 0x%x base: 0x%x\n", hpet->block_id, hpet_addr);
    
    /* enable hpet */
    u32 conf = hpet_read(HPET_CFG);
    conf |= HPET_CFG_ENABLE;
    hpet_write(HPET_CFG, conf);

    hpet_enabled = 1;

    return hpet_enabled;
}

PUBLIC u8 is_hpet_enabled()
{
    return hpet_enabled;
}

PUBLIC u32 hpet_read(u32 a)
{
    return *(u32 *)(hpet_addr + a);
}

PUBLIC u32 hpet_write(u32 a, u32 v)
{
    return *(u32 *)(hpet_addr + a) = v;
}
