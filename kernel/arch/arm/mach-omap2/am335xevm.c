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
#include <sys/types.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/proc.h>
#include <lyos/global.h>
#include <lyos/proto.h>
#include <lyos/vm.h>
#include "arch.h"
#include "common.h"
#include "serial.h"
#include "interrupt.h"
    
PRIVATE void am335x_init_serial(void)
{
    uart_base_addr = OMAP3_AM335X_DEBUG_UART_BASE;
    kern_map_phys(OMAP3_AM335X_DEBUG_UART_BASE, ARCH_PG_SIZE, KMF_WRITE, &uart_base_addr);
}

PRIVATE void am335x_init_interrupt(void)
{
    intr_base_addr = OMAP3_AM335X_INTR_BASE;
    kern_map_phys(OMAP3_AM335X_INTR_BASE, ARCH_PG_SIZE, KMF_WRITE, &intr_base_addr);
}

PRIVATE void am335x_init_machine(void)
{
    am335x_init_serial();
    am335x_init_interrupt();
}

MACHINE_START(TI_AM335X_EVM, "TI AM335X EVM")
    .init_machine = am335x_init_machine,
    .serial_putc = omap3_disp_char,
MACHINE_END
