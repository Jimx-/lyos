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
#include <asm/io.h>
#include <asm/cache.h>

#include "serial.h"

#define BCM2835_MMIO_BASE 0x3f000000

#define BCM2835_DEBUG_UART_BASE (BCM2835_MMIO_BASE + 0x215000)
#define BCM2835_GPIO_BASE       (BCM2835_MMIO_BASE + 0x200000)

#define BCM2835_MBOX_BASE (BCM2835_MMIO_BASE + 0xb880)
#define MBOX_READ         0x0
#define MBOX_POLL         0x10
#define MBOX_SENDER       0x14
#define MBOX_STATUS       0x18
#define MBOX_CONFIG       0x1c
#define MBOX_WRITE        0x20

#define MBOX_MS_FULL  BIT(31)
#define MBOX_MS_EMPTY BIT(30)

#define MBOX_CH_PROP 8

#define MBOX_REQUEST 0

#define MBOX_TAG_SETPOWER   0x28001
#define MBOX_TAG_SETCLKRATE 0x38002
#define MBOX_TAG_SETPHYWH   0x48003
#define MBOX_TAG_SETVIRTWH  0x48004
#define MBOX_TAG_SETVIRTOFF 0x48009
#define MBOX_TAG_SETDEPTH   0x48005
#define MBOX_TAG_SETPXLORDR 0x48006
#define MBOX_TAG_GETFB      0x40001
#define MBOX_TAG_GETPITCH   0x40008
#define MBOX_TAG_LAST       0

static void* mbox_base_addr;

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

static void bcm2835_mbox_call(int chan, u32 val)
{
    u32 v = (val & ~0xf) | (chan & 0xf);

    while (mmio_read(mbox_base_addr + MBOX_STATUS) & MBOX_MS_FULL)
        arch_pause();

    mmio_write(mbox_base_addr + MBOX_WRITE, v);

    while (mmio_read(mbox_base_addr + MBOX_STATUS) & MBOX_MS_EMPTY)
        arch_pause();
}

static void bcm2835_init_fb(void* arg)
{
    static u32 data[36] __attribute__((aligned(16)));

    data[0] = 35 * 4;
    data[1] = MBOX_REQUEST;

    data[2] = MBOX_TAG_SETPHYWH;
    data[3] = 8;
    data[4] = 0;
    data[5] = 1920;
    data[6] = 1080;

    data[7] = MBOX_TAG_SETVIRTWH;
    data[8] = 8;
    data[9] = 8;
    data[10] = 1920;
    data[11] = 1080;

    data[12] = MBOX_TAG_SETVIRTOFF;
    data[13] = 8;
    data[14] = 8;
    data[15] = 0;
    data[16] = 0;

    data[17] = MBOX_TAG_SETDEPTH;
    data[18] = 4;
    data[19] = 4;
    data[20] = 32;

    data[21] = MBOX_TAG_SETPXLORDR;
    data[22] = 4;
    data[23] = 4;
    data[24] = 1;

    data[25] = MBOX_TAG_GETFB;
    data[26] = 8;
    data[27] = 8;
    data[28] = 0x1000;
    data[29] = 0;

    data[30] = MBOX_TAG_GETPITCH;
    data[31] = 4;
    data[32] = 4;
    data[33] = 0;

    data[34] = MBOX_TAG_LAST;

    dcache_clean_inval_poc(data, (void*)data + sizeof(data));
    bcm2835_mbox_call(MBOX_CH_PROP, (u32)__pa_symbol(data));
    dcache_inval_poc(data, (void*)data + sizeof(data));

    if (data[20] == 32 && data[28] != 0) {
        kinfo.fb_base_phys = data[28] & 0x3fffffff;
        kinfo.fb_screen_width = data[10];
        kinfo.fb_screen_height = data[11];
        kinfo.fb_screen_pitch = data[33];
    }
}

static void bcm2835_init_mailbox(void)
{
    kern_map_phys(BCM2835_MBOX_BASE, ARCH_PG_SIZE, KMF_WRITE | KMF_IO,
                  &mbox_base_addr, bcm2835_init_fb, NULL);
}

static void bcm2835_init_machine(void)
{
    bcm2835_init_serial();

    bcm2835_init_mailbox();

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

.serial_putc = uart_aux_disp_char,

.handle_irq = bcm2836_arm_irqchip_handle_irq,
MACHINE_END
    /* clang-format on */
