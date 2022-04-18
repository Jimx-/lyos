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
#include <lyos/vm.h>
#include "arch.h"
#include "common.h"
#include "serial.h"
#include "interrupt.h"

static void omap3_beagle_init_serial(void)
{
    /* map UART register */
    uart_base_addr = OMAP3_BEAGLE_DEBUG_UART_BASE;
    kern_map_phys(OMAP3_BEAGLE_DEBUG_UART_BASE, ARCH_PG_SIZE, KMF_WRITE,
                  &uart_base_addr);
}

static void omap3_beagle_init_interrupt(void)
{
    intr_base_addr = OMAP3_BEAGLE_INTR_BASE;
    kern_map_phys(OMAP3_BEAGLE_INTR_BASE, ARCH_PG_SIZE, KMF_WRITE,
                  &intr_base_addr);
}

static void omap3_beagle_init_machine(void)
{
    omap3_beagle_init_serial();
    omap3_beagle_init_interrupt();
}

/* clang-format off */
MACHINE_START(OMAP3_BEAGLE, "OMAP3 Beagle Board")
.init_machine = omap3_beagle_init_machine,
.serial_putc = omap3_disp_char,
MACHINE_END
    /* clang-format on */
