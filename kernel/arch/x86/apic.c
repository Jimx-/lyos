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
#include "acpi.h"
#include "apic.h"
#include "arch_proto.h"
#include "arch_const.h"
#ifdef CONFIG_SMP
#include "arch_smp.h"
#endif
#include <lyos/cpulocals.h>
#include <lyos/cpufeature.h>
#include <lyos/spinlock.h>

#define APIC_ENABLE     0x100
#define APIC_FOCUS_DISABLED (1 << 9)
#define APIC_SIV        0xFF

#define APIC_TDCR_2 0x00
#define APIC_TDCR_4 0x01
#define APIC_TDCR_8 0x02
#define APIC_TDCR_16    0x03
#define APIC_TDCR_32    0x08
#define APIC_TDCR_64    0x09
#define APIC_TDCR_128   0x0a
#define APIC_TDCR_1 0x0b

#define APIC_LVTT_VECTOR_MASK   0x000000FF
#define APIC_LVTT_DS_PENDING    (1 << 12)
#define APIC_LVTT_MASK      (1 << 16)
#define APIC_LVTT_TM        (1 << 17)

#define APIC_LVT_IIPP_MASK  0x00002000
#define APIC_LVT_IIPP_AH    0x00002000
#define APIC_LVT_IIPP_AL    0x00000000

#define APIC_LVT_TM_ONESHOT     0
#define APIC_LVT_TM_PERIODIC    APIC_LVTT_TM

#define IOAPIC_REGSEL       0x0
#define IOAPIC_RW       0x10

#define APIC_ICR_DM_MASK        0x00000700
#define APIC_ICR_VECTOR         APIC_LVTT_VECTOR_MASK
#define APIC_ICR_DM_FIXED       (0 << 8)
#define APIC_ICR_DM_LOWEST_PRIORITY (1 << 8)
#define APIC_ICR_DM_SMI         (2 << 8)
#define APIC_ICR_DM_RESERVED        (3 << 8)
#define APIC_ICR_DM_NMI         (4 << 8)
#define APIC_ICR_DM_INIT        (5 << 8)
#define APIC_ICR_DM_STARTUP     (6 << 8)
#define APIC_ICR_DM_EXTINT      (7 << 8)

#define APIC_ICR_DM_PHYSICAL        (0 << 11)
#define APIC_ICR_DM_LOGICAL     (1 << 11)

#define APIC_ICR_DELIVERY_PENDING   (1 << 12)

#define APIC_ICR_INT_POLARITY       (1 << 13)
#define APIC_ICR_INTPOL_LOW         APIC_ICR_INT_POLARITY
#define APIC_ICR_INTPOL_HIGH        APIC_ICR_INT_POLARITY

#define APIC_ICR_LEVEL_ASSERT       (1 << 14)
#define APIC_ICR_LEVEL_DEASSERT     (0 << 14)

#define APIC_ICR_TRIGGER        (1 << 15)
#define APIC_ICR_TM_LEVEL       0
#define APIC_ICR_TM_EDGE        0

#define APIC_ICR_INT_MASK       (1 << 16)

#define APIC_ICR_DEST_FIELD     (0 << 18)
#define APIC_ICR_DEST_SELF      (1 << 18)
#define APIC_ICR_DEST_ALL       (2 << 18)
#define APIC_ICR_DEST_ALL_BUT_SELF  (3 << 18)

#define IA32_APIC_BASE  0x1b
#define IA32_APIC_BASE_ENABLE_BIT   11

DEF_SPINLOCK(calibrate_lock);
PRIVATE u32 probe_ticks;
#define PROBE_TICKS (system_hz / 10)

PRIVATE u64 tsc0, tsc1;
PRIVATE u32 lapic_tctr0, lapic_tctr1;
PRIVATE u32 lapic_bus_freq[CONFIG_SMP_MAX_CPUS];

PUBLIC struct io_apic io_apics[MAX_IOAPICS];
PUBLIC u32 nr_ioapics;

PUBLIC u32 apicid()
{
    return lapic_read(LAPIC_ID);
}

#define IOAPIC_IOREGSEL 0x0
#define IOAPIC_IOWIN    0x10

PRIVATE u32 ioapic_read(u32 ioa_base, u32 reg)
{
    *((volatile u32 *)(ioa_base + IOAPIC_IOREGSEL)) = (reg & 0xff);
    return *(volatile u32 *)(ioa_base + IOAPIC_IOWIN);
}

PRIVATE void ioapic_write(u32 ioa_base, u8 reg, u32 val)
{
    *((volatile u32 *)(ioa_base + IOAPIC_IOREGSEL)) = reg;
    *((volatile u32 *)(ioa_base + IOAPIC_IOWIN)) = val;
}

PRIVATE int calibrate_handler(irq_hook_t * hook)
{
    u32 tcrt;
    u64 tsc;

    probe_ticks++;
    read_tsc_64(&tsc);
    tcrt = lapic_read(LAPIC_TIMER_CCR);

    if (probe_ticks == 1) {
        lapic_tctr0 = tcrt;
        tsc0 = tsc;
    } else if (probe_ticks == PROBE_TICKS) {
        lapic_tctr1 = tcrt;
        tsc1 = tsc;
        stop_8253_timer();
    }

    return 1;
}

PRIVATE int apic_calibrate(unsigned cpu)
{
    u32 val, lvtt;
    irq_hook_t calibrate_hook;

    spinlock_lock(&calibrate_lock);

    probe_ticks = 0;

    val = 0xffffffff;
    lapic_write(LAPIC_TIMER_ICR, val);

    val = 0;
    lapic_write(LAPIC_TIMER_CCR, val);
    
    lvtt = lapic_read(LAPIC_TIMER_DCR) & ~0x0b;
    lvtt = APIC_TDCR_1;
    lapic_write(LAPIC_TIMER_DCR, lvtt);

    lvtt = lapic_read(LAPIC_LVTTR);
    lvtt |= APIC_LVTT_MASK;
    lapic_write(LAPIC_LVTTR, lvtt);

    put_irq_handler(CLOCK_IRQ, &calibrate_hook, calibrate_handler);
    init_8253_timer(system_hz);

    enable_int();
    while (probe_ticks < PROBE_TICKS) enable_int();
    disable_int();

    rm_irq_handler(&calibrate_hook);

    u32 tsc_delta = tsc1 - tsc0;
    u32 cpu_freq = (tsc_delta / (PROBE_TICKS - 1)) * make64(0, system_hz);
    cpu_hz[cpuid] = cpu_freq;
    printk("APIC: detected %d MHz processor\n", cpu_freq / 1000000);

    u32 lapic_delta = lapic_tctr0 - lapic_tctr1;
    lapic_bus_freq[cpuid] = system_hz * lapic_delta / (PROBE_TICKS - 1);

    spinlock_unlock(&calibrate_lock);

    return 0;
}

PUBLIC int lapic_enable_msr()
{
    u32 hi, lo;
    ia32_read_msr(IA32_APIC_BASE, &hi, &lo);
    lo |= 1 << IA32_APIC_BASE_ENABLE_BIT;
    ia32_write_msr(IA32_APIC_BASE, hi, lo);

    return 1;
}

PUBLIC int lapic_enable(unsigned cpu)
{
    u32 val, nlvt;

    if (!lapic_addr) return 0;
    
    if (!lapic_enable_msr()) return 0;

    lapic_eoi_addr = LAPIC_EOI;
    
    lapic_write(LAPIC_TPR, 0x0);

    val = lapic_read(LAPIC_SIVR);
    val |= APIC_ENABLE | APIC_SPURIOUS_INT_VECTOR;
    val &= ~APIC_FOCUS_DISABLED;
    lapic_write(LAPIC_SIVR, val);
    lapic_read(LAPIC_SIVR);

    apic_eoi();
    
    val = lapic_read(LAPIC_LDR) & ~0xFF000000;
    val |= (cpu & 0xFF) << 24;
    lapic_write(LAPIC_LDR, val);

    val = lapic_read(LAPIC_DFR) | 0xF0000000;
    lapic_write (LAPIC_DFR, val);

    val = lapic_read (LAPIC_LVTER) & 0xFFFFFF00;
    lapic_write (LAPIC_LVTER, val);

    nlvt = (lapic_read(LAPIC_VERSION) >> 16) & 0xFF;

    if(nlvt >= 4) {
        val = lapic_read(LAPIC_LVTTMR);
        lapic_write(LAPIC_LVTTMR, val | APIC_ICR_INT_MASK);
    }

    if(nlvt >= 5) {
        val = lapic_read(LAPIC_LVTPCR);
        lapic_write(LAPIC_LVTPCR, val | APIC_ICR_INT_MASK);
    }

    val = lapic_read (LAPIC_TPR);
    lapic_write (LAPIC_TPR, val & ~0xFF);

    lapic_read(LAPIC_SIVR);
    apic_eoi();

    apic_calibrate(cpu);

    return 1;
}

PUBLIC int detect_ioapics()
{
    int n = 0;
    struct acpi_madt_ioapic * acpi_ioa;

    while (n < MAX_IOAPICS) {
        acpi_ioa = acpi_get_ioapic_next();
        if (acpi_ioa == NULL) break;

        io_apics[n].id = acpi_ioa->id;
        io_apics[n].addr = acpi_ioa->address;
        io_apics[n].phys_addr = (phys_bytes)acpi_ioa->address;
        io_apics[n].gsi_base = acpi_ioa->global_int_base;
        io_apics[n].pins = ((ioapic_read(io_apics[n].addr,
                IOAPIC_VERSION) & 0xff0000) >> 16) + 1; 
        io_apics[n].version = ioapic_read(io_apics[n].addr,
                IOAPIC_VERSION) & 0x0000ff;
        printk("IOAPIC[%d]: apic_id %d, version %d, address 0x%x, GSI %d-%d\n", 
            n, io_apics[n].id, io_apics[n].version, io_apics[n].addr, io_apics[n].gsi_base, io_apics[n].pins - 1);

        n++;
    }

    nr_ioapics = n;

    return nr_ioapics;
}
