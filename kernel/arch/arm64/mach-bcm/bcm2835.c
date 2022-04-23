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
#include <asm/pagetable.h>
#include <asm/mach.h>
#include <asm/fixmap.h>
#include <asm/proto.h>
#include <asm/arm_arch_timer.h>

#include "serial.h"

#define BCM2835_MMIO_BASE 0x3f000000

#define BCM2835_DEBUG_UART_BASE (BCM2835_MMIO_BASE + 0x201000)

void bcm2836_arm_irqchip_l1_intc_scan(void);
void bcm2835_armctrl_irqchip_scan(void);
void bcm2836_arm_irqchip_handle_irq(void);

static void bcm2835_init_serial(void)
{
    set_fixmap_io(FIX_EARLYCON_MEM_BASE, BCM2835_DEBUG_UART_BASE);
    uart_base_addr = (void*)__fix_to_virt(FIX_EARLYCON_MEM_BASE);

    kern_map_phys(BCM2835_DEBUG_UART_BASE, ARCH_PG_SIZE, KMF_WRITE | KMF_IO,
                  &uart_base_addr, NULL, NULL);
}

static void bcm2835_init_machine(void)
{
    bcm2835_init_serial();

    bcm2836_arm_irqchip_l1_intc_scan();

    arm_arch_timer_scan();

    bcm2835_armctrl_irqchip_scan();
}

static void bcm2835_init_cpu(unsigned int cpu)
{
    arm_arch_timer_setup_cpu(cpu);
}

static const char* const bcm2835_compat[] = {"brcm,bcm2835", "brcm,bcm2836",
                                             "brcm,bcm2837", NULL};

/* clang-format off */
DT_MACHINE_START(BCM2835, "BCM2835")
.dt_compat = bcm2835_compat,

.init_machine = bcm2835_init_machine,
.init_cpu = bcm2835_init_cpu,

.serial_putc = pl011_disp_char,

.handle_irq = bcm2836_arm_irqchip_handle_irq,
MACHINE_END
    /* clang-format on */
