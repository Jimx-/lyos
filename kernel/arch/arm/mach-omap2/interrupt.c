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
#include <sys/types.h>
#include <lyos/const.h>
#include <string.h>
#include <kernel/proc.h>
#include <kernel/global.h>
#include <kernel/proto.h>
#include "arch.h"
#include "arch_proto.h"
#include "interrupt.h"

void* intr_base_addr;

void omap3_handle_irq(void)
{
    int irq = mmio_read(intr_base_addr + OMAP3_INTCPS_SIR_IRQ) &
              OMAP3_INTR_ACTIVEIRQ_MASK;
    generic_handle_irq(irq);
    mmio_write(intr_base_addr + OMAP3_INTCPS_CONTROL, OMAP3_INTR_NEWIRQAGR);
}
