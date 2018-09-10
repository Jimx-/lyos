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
#include <lyos/ipc.h>
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "stddef.h"
#include <asm/protect.h>
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "acpi.h"
#include "apic.h"
#include <asm/proto.h>
#include <asm/const.h>
#ifdef CONFIG_SMP
#include <asm/smp.h>
#endif
#include <asm/hwint.h>
#include <lyos/cpulocals.h>
#include <lyos/cpufeature.h>
#include <lyos/spinlock.h>
#include <lyos/time.h>
#include <asm/hpet.h>
#include <asm/div64.h>
#include <asm/type.h>

#if !CONFIG_SMP
#define cpuid 0
#endif

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
extern u8 cpuid2apicid[CONFIG_SMP_MAX_CPUS];

extern struct cpu_info cpu_info[CONFIG_SMP_MAX_CPUS];

PRIVATE u32 lapic_bus_freq[CONFIG_SMP_MAX_CPUS];

PUBLIC struct io_apic io_apics[MAX_IOAPICS];
PUBLIC u32 nr_ioapics;

struct irq;
typedef void (* eoi_method_t)(struct irq *);

struct irq {
    struct io_apic *    ioa;
    unsigned        pin;
    unsigned        vector;
    eoi_method_t        eoi;
#define IOAPIC_IRQ_STATE_MASKED 0x1
    unsigned        state;
};

PRIVATE struct irq io_apic_irq[NR_IRQ_VECTORS];

/* ISA IRQ -> Global system interrupt */
PRIVATE u32 isa_irq_to_gsi[NR_IRQS_LEGACY] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

PUBLIC u8 ioapic_enabled = 0;

PUBLIC u32 apicid()
{
    return lapic_read(LAPIC_ID);
}

#define IOAPIC_IOREGSEL 0x0
#define IOAPIC_IOWIN    0x10

PRIVATE u32 ioapic_read(void* ioa_base, u32 reg)
{
    *((volatile u32 *)(ioa_base + IOAPIC_IOREGSEL)) = (reg & 0xff);
    return *(volatile u32 *)(ioa_base + IOAPIC_IOWIN);
}

PRIVATE void ioapic_write(void* ioa_base, u8 reg, u32 val)
{
    *((volatile u32 *)(ioa_base + IOAPIC_IOREGSEL)) = reg;
    *((volatile u32 *)(ioa_base + IOAPIC_IOWIN)) = val;
}

PRIVATE void ioapic_redirt_entry_write(void * ioapic_addr,
                    int entry,
                    u32 hi,
                    u32 lo)
{
    ioapic_write(ioapic_addr, (u8) (IOAPIC_REDIR_TABLE + entry * 2 + 1), hi);
    ioapic_write(ioapic_addr, (u8) (IOAPIC_REDIR_TABLE + entry * 2), lo);
}

PRIVATE void ioapic_enable_pin(void* ioapic_addr, int pin)
{
    u32 lo = ioapic_read(ioapic_addr, IOAPIC_REDIR_TABLE + pin * 2);

    lo &= ~APIC_ICR_INT_MASK;
    ioapic_write(ioapic_addr, IOAPIC_REDIR_TABLE + pin * 2, lo);
}

PRIVATE void ioapic_disable_pin(void* ioapic_addr, int pin)
{
    u32 lo = ioapic_read(ioapic_addr, IOAPIC_REDIR_TABLE + pin * 2);

    lo |= APIC_ICR_INT_MASK;
    ioapic_write(ioapic_addr, IOAPIC_REDIR_TABLE + pin * 2, lo);
}

PRIVATE u32 lapic_errstatus()
{
    lapic_write(LAPIC_ESR, 0);
    return lapic_read(LAPIC_ESR);
}

PRIVATE u32 hpet_ref_ms(u64 hpet1, u64 hpet2)
{
    if (hpet2 < hpet1)
        hpet2 += 0x100000000ULL;
    hpet2 -= hpet1;
    u64 hpet_ms = ((u64)hpet2 * hpet_readl(HPET_PERIOD));
    do_div(hpet_ms, 1000000);
    do_div(hpet_ms, 1000000);

    return (u32)hpet_ms;
}

#define CAL_MS      50
#define CAL_LATCH   (TIMER_FREQ / (1000 / CAL_MS))
PRIVATE int apic_calibrate(unsigned cpu)
{
    u32 val, lvtt;
    u64 tsc0, tsc1;
    u32 lapic_tctr0, lapic_tctr1;

    spinlock_lock(&calibrate_lock);

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

    u32 cal_ms = CAL_MS;
    u32 cal_latch = CAL_LATCH;
    u64 hpet0, hpet1, hpet_expect;
    u32 hpet = is_hpet_enabled();

    read_tsc_64(&tsc0);
    lapic_tctr0 = lapic_read(LAPIC_TIMER_CCR);

    if (hpet) {
        hpet0 = hpet_readl(HPET_COUNTER);
        hpet_expect = CAL_MS * FSEC_PER_SEC;
        do_div(hpet_expect, 1000);
        do_div(hpet_expect, hpet_readl(HPET_PERIOD));
        hpet_expect += hpet0;

        while (hpet_readl(HPET_COUNTER) < hpet_expect) { }
    } else {

        out_byte(0x61, (in_byte(0x61) & 0x02) | 0x01);

        out_byte(TIMER_MODE, 0xb0);
        out_byte(TIMER2, cal_latch & 0xff);
        out_byte(TIMER2, (u8)(cal_latch >> 8));

        while ((in_byte(0x61) & 0x20) == 0) { }
        stop_8253_timer();
    }

    if (hpet) hpet1 = hpet_readl(HPET_COUNTER);
    read_tsc_64(&tsc1);
    lapic_tctr1 = lapic_read(LAPIC_TIMER_CCR);

    if (hpet) cal_ms = hpet_ref_ms(hpet0, hpet1);

    u64 tsc_delta = tsc1 - tsc0;
    u64 cpu_freq = tsc_delta * 1000;
    do_div(cpu_freq, cal_ms);
    cpu_hz[cpuid] = cpu_freq;
    u64 cpu_mhz = cpu_freq;
    do_div(cpu_mhz, 1000000);
    printk("APIC: detected %d MHz processor\n", cpu_mhz);
    cpu_info[cpuid].freq_mhz = cpu_mhz;

    u32 lapic_delta = lapic_tctr0 - lapic_tctr1;
    lapic_bus_freq[cpuid] = lapic_delta * (1000 / cal_ms);

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

PUBLIC void lapic_set_timer_one_shot(const u32 usec)
{
    u32 lvtt;
    u32 ticks_per_us;
    const u8 cpu = cpuid;

    ticks_per_us = lapic_bus_freq[cpu] / 1000000;

    lapic_write(LAPIC_TIMER_ICR, usec * ticks_per_us);

    lvtt = APIC_TDCR_1;
    lapic_write(LAPIC_TIMER_DCR, lvtt);

    lvtt = APIC_TIMER_INT_VECTOR;
    lapic_write(LAPIC_LVTTR, lvtt);
}

PUBLIC void lapic_restart_timer()
{
    if (lapic_read(LAPIC_TIMER_CCR) == 0)
        lapic_set_timer_one_shot(1000000 / system_hz);
}

PUBLIC void lapic_stop_timer()
{
    u32 lvtt;
    lvtt = lapic_read(LAPIC_LVTTR);
    lapic_write(LAPIC_LVTTR, lvtt | APIC_LVTT_MASK);
    lapic_write(LAPIC_TIMER_ICR, 0);
    lapic_write(LAPIC_TIMER_CCR, 0);
}

PUBLIC void lapic_microsec_sleep(unsigned usec)
{
    lapic_set_timer_one_shot(usec);
    while (lapic_read(LAPIC_TIMER_CCR))
        arch_pause();
}

PRIVATE void ioapic_init_legacy_irqs()
{
    struct acpi_madt_int_src * acpi_int_src;

    while ((acpi_int_src = acpi_get_int_src_next())) {
        isa_irq_to_gsi[acpi_int_src->bus_int] = acpi_int_src->global_int;
        printk("ACPI: IRQ%d used by override\n", acpi_int_src->bus_int);
    }
}

PUBLIC int ioapic_enable()
{
    ioapic_init_legacy_irqs();

    disable_8259A();

    out_byte(0x22, 0x70);
    out_byte(0x23, 0x01);

    ioapic_enabled = 1;
    return 1;
}

PRIVATE void ioapic_enable_irq(int irq)
{
    if(!(io_apic_irq[irq].ioa)) return;

    ioapic_enable_pin(io_apic_irq[irq].ioa->addr, io_apic_irq[irq].pin);
    io_apic_irq[irq].state &= ~IOAPIC_IRQ_STATE_MASKED;
}

PRIVATE void ioapic_disable_irq(int irq)
{
    if(!(io_apic_irq[irq].ioa)) return;

    ioapic_disable_pin(io_apic_irq[irq].ioa->addr, io_apic_irq[irq].pin);
    io_apic_irq[irq].state |= IOAPIC_IRQ_STATE_MASKED;
}

PUBLIC void ioapic_mask(int irq)
{
    if (ioapic_enabled)
        ioapic_disable_irq(irq);
    else
        i8259_mask(irq);
}

PUBLIC void ioapic_unmask(int irq)
{
    if (ioapic_enabled)
        ioapic_enable_irq(irq);
    else
        i8259_unmask(irq);
}

PUBLIC void ioapic_eoi(int irq)
{
    if (ioapic_enabled) {
        io_apic_irq[irq].eoi(&io_apic_irq[irq]);
    } else {
        i8259_eoi(irq);
    }
}

PRIVATE void ioapic_eoi_edge(struct irq * irq)
{
    apic_eoi();
}

PRIVATE void ioapic_eoi_level(struct irq * irq)
{
    apic_eoi();
}

PRIVATE eoi_method_t set_eoi_method(int irq)
{
    return irq < 16 ? ioapic_eoi_edge : ioapic_eoi_level;
}

PRIVATE void set_irq_redir_low(int irq, u32 * low)
{
    u32 val = 0;

    val &= ~(APIC_ICR_VECTOR | APIC_ICR_INT_MASK |
            APIC_ICR_TRIGGER | APIC_ICR_INT_POLARITY);

    if (irq < 16) {
        /* ISA */
        val &= ~APIC_ICR_INT_POLARITY;
        val &= ~APIC_ICR_TRIGGER;
    }
    else {
        /* PCI */
        val |= APIC_ICR_INT_POLARITY;
        val |= APIC_ICR_TRIGGER;
    }

    val |= io_apic_irq[irq].vector;

    *low = val;
}

PUBLIC void ioapic_set_irq(int irq)
{
    /* shared irq */
    if (io_apic_irq[irq].ioa && io_apic_irq[irq].eoi)
        return;

    int ioa;

    for (ioa = 0; ioa < nr_ioapics; ioa++) {
        if (io_apics[ioa].gsi_base <= irq &&
                io_apics[ioa].gsi_base +
                io_apics[ioa].pins > irq) {
            u32 hi_32, low_32;

            io_apic_irq[irq].ioa = &io_apics[ioa];
            io_apic_irq[irq].pin = irq - io_apics[ioa].gsi_base;
            io_apic_irq[irq].eoi = set_eoi_method(irq);
            io_apic_irq[irq].vector = INT_VECTOR_IRQ0 + irq;

            set_irq_redir_low(irq, &low_32);
            hi_32 = cpuid << 24;
            ioapic_redirt_entry_write(io_apics[ioa].addr,
                    io_apic_irq[irq].pin, hi_32, low_32);
        }
    }
}

PUBLIC void ioapic_unset_irq(int irq)
{
    ioapic_disable_irq(irq);
    io_apic_irq[irq].ioa = NULL;
    io_apic_irq[irq].eoi = NULL;
}

PUBLIC int detect_ioapics()
{
    int n = 0;
    struct acpi_madt_ioapic * acpi_ioa;

    while (n < MAX_IOAPICS) {
        acpi_ioa = acpi_get_ioapic_next();
        if (acpi_ioa == NULL) break;

        io_apics[n].id = acpi_ioa->id;
        io_apics[n].addr = (void*) ((uintptr_t) acpi_ioa->address);
        io_apics[n].phys_addr = (phys_bytes)acpi_ioa->address;
        io_apics[n].gsi_base = acpi_ioa->global_int_base;
        io_apics[n].pins = ((ioapic_read(io_apics[n].addr,
                IOAPIC_VERSION) & 0xff0000) >> 16) + 1;
        io_apics[n].version = ioapic_read(io_apics[n].addr,
                IOAPIC_VERSION) & 0x0000ff;
        printk("IOAPIC[%d]: apic_id %d, version %d, address 0x%x, GSI %d-%d\n",
            n, io_apics[n].id, io_apics[n].version, io_apics[n].addr, io_apics[n].gsi_base, io_apics[n].gsi_base + io_apics[n].pins - 1);

        n++;
    }

    nr_ioapics = n;

    return nr_ioapics;
}

PUBLIC void apic_spurious_int_handler() {}
PUBLIC void apic_error_int_handler() {}

PUBLIC void apic_hwint00();
PUBLIC void apic_hwint01();
PUBLIC void apic_hwint02();
PUBLIC void apic_hwint03();
PUBLIC void apic_hwint04();
PUBLIC void apic_hwint05();
PUBLIC void apic_hwint06();
PUBLIC void apic_hwint07();
PUBLIC void apic_hwint08();
PUBLIC void apic_hwint09();
PUBLIC void apic_hwint10();
PUBLIC void apic_hwint11();
PUBLIC void apic_hwint12();
PUBLIC void apic_hwint13();
PUBLIC void apic_hwint14();
PUBLIC void apic_hwint15();
PUBLIC void apic_hwint16();
PUBLIC void apic_hwint17();
PUBLIC void apic_hwint18();
PUBLIC void apic_hwint19();
PUBLIC void apic_hwint20();
PUBLIC void apic_hwint21();
PUBLIC void apic_hwint22();
PUBLIC void apic_hwint23();
PUBLIC void apic_hwint24();
PUBLIC void apic_hwint25();
PUBLIC void apic_hwint26();
PUBLIC void apic_hwint27();
PUBLIC void apic_hwint28();
PUBLIC void apic_hwint29();
PUBLIC void apic_hwint30();
PUBLIC void apic_hwint31();
PUBLIC void apic_hwint32();
PUBLIC void apic_hwint33();
PUBLIC void apic_hwint34();
PUBLIC void apic_hwint35();
PUBLIC void apic_hwint36();
PUBLIC void apic_hwint37();
PUBLIC void apic_hwint38();
PUBLIC void apic_hwint39();
PUBLIC void apic_hwint40();
PUBLIC void apic_hwint41();
PUBLIC void apic_hwint42();
PUBLIC void apic_hwint43();
PUBLIC void apic_hwint44();
PUBLIC void apic_hwint45();
PUBLIC void apic_hwint46();
PUBLIC void apic_hwint47();
PUBLIC void apic_hwint48();
PUBLIC void apic_hwint49();
PUBLIC void apic_hwint50();
PUBLIC void apic_hwint51();
PUBLIC void apic_hwint52();
PUBLIC void apic_hwint53();
PUBLIC void apic_hwint54();
PUBLIC void apic_hwint55();
PUBLIC void apic_hwint56();
PUBLIC void apic_hwint57();
PUBLIC void apic_hwint58();
PUBLIC void apic_hwint59();
PUBLIC void apic_hwint60();
PUBLIC void apic_hwint61();
PUBLIC void apic_hwint62();
PUBLIC void apic_hwint63();
PUBLIC void apic_timer_int_handler();
PUBLIC void apic_spurious_intr();
PUBLIC void apic_error_intr();

PUBLIC void apic_init_idt(int reset)
{
    if (reset) {
        init_idt();
        return;
    }

    init_idt_desc(INT_VECTOR_IRQ0 + 0, DA_386IGate, apic_hwint00, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 1, DA_386IGate, apic_hwint01, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 2, DA_386IGate, apic_hwint02, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 3, DA_386IGate, apic_hwint03, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 4, DA_386IGate, apic_hwint04, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 5, DA_386IGate, apic_hwint05, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 6, DA_386IGate, apic_hwint06, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 7, DA_386IGate, apic_hwint07, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 8, DA_386IGate, apic_hwint08, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 9, DA_386IGate, apic_hwint09, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 10, DA_386IGate, apic_hwint10, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 11, DA_386IGate, apic_hwint11, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 12, DA_386IGate, apic_hwint12, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 13, DA_386IGate, apic_hwint13, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 14, DA_386IGate, apic_hwint14, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 15, DA_386IGate, apic_hwint15, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 16, DA_386IGate, apic_hwint16, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 17, DA_386IGate, apic_hwint17, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 18, DA_386IGate, apic_hwint18, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 19, DA_386IGate, apic_hwint19, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 20, DA_386IGate, apic_hwint20, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 21, DA_386IGate, apic_hwint21, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 22, DA_386IGate, apic_hwint22, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 23, DA_386IGate, apic_hwint23, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 24, DA_386IGate, apic_hwint24, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 25, DA_386IGate, apic_hwint25, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 26, DA_386IGate, apic_hwint26, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 27, DA_386IGate, apic_hwint27, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 28, DA_386IGate, apic_hwint28, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 29, DA_386IGate, apic_hwint29, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 30, DA_386IGate, apic_hwint30, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 31, DA_386IGate, apic_hwint31, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 32, DA_386IGate, apic_hwint32, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 33, DA_386IGate, apic_hwint33, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 34, DA_386IGate, apic_hwint34, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 35, DA_386IGate, apic_hwint35, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 36, DA_386IGate, apic_hwint36, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 37, DA_386IGate, apic_hwint37, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 38, DA_386IGate, apic_hwint38, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 39, DA_386IGate, apic_hwint39, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 40, DA_386IGate, apic_hwint40, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 41, DA_386IGate, apic_hwint41, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 42, DA_386IGate, apic_hwint42, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 43, DA_386IGate, apic_hwint43, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 44, DA_386IGate, apic_hwint44, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 45, DA_386IGate, apic_hwint45, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 46, DA_386IGate, apic_hwint46, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 47, DA_386IGate, apic_hwint47, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 48, DA_386IGate, apic_hwint48, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 49, DA_386IGate, apic_hwint49, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 50, DA_386IGate, apic_hwint50, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 51, DA_386IGate, apic_hwint51, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 52, DA_386IGate, apic_hwint52, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 53, DA_386IGate, apic_hwint53, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 54, DA_386IGate, apic_hwint54, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 55, DA_386IGate, apic_hwint55, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 56, DA_386IGate, apic_hwint56, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 57, DA_386IGate, apic_hwint57, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 58, DA_386IGate, apic_hwint58, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 59, DA_386IGate, apic_hwint59, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 60, DA_386IGate, apic_hwint60, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 61, DA_386IGate, apic_hwint61, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 62, DA_386IGate, apic_hwint62, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_IRQ0 + 63, DA_386IGate, apic_hwint63, PRIVILEGE_KRNL);
    init_idt_desc(APIC_SPURIOUS_INT_VECTOR, DA_386IGate, apic_spurious_intr, PRIVILEGE_KRNL);
    init_idt_desc(APIC_ERROR_INT_VECTOR, DA_386IGate, apic_error_intr, PRIVILEGE_KRNL);
    init_idt_desc(INT_VECTOR_SYS_CALL,  DA_386IGate,
              sys_call,         PRIVILEGE_USER);

    u32 val;
    val = lapic_read(LAPIC_LVTER);
    val |= APIC_ERROR_INT_VECTOR;
    val &= ~ APIC_ICR_INT_MASK;
    lapic_write(LAPIC_LVTER, val);
    lapic_read(LAPIC_LVTER);

    if (apicid() == bsp_lapic_id) {
        printk("APIC: initiating LAPIC timer handler\n");
        init_idt_desc(APIC_TIMER_INT_VECTOR,  DA_386IGate,
              apic_timer_int_handler,         PRIVILEGE_KRNL);
    }
}

#if CONFIG_SMP

PUBLIC int apic_send_startup_ipi(unsigned cpu, phys_bytes trampoline)
{
    int timeout;
    u32 errstatus = 0;
    int i;

    for (i = 0; i < 2; i++) {
        u32 val;

        lapic_errstatus();

        val = lapic_read(LAPIC_ICR2) & 0xFFFFFF;
        val |= cpuid2apicid[cpu] << 24;
        lapic_write(LAPIC_ICR2, val);

        val = lapic_read(LAPIC_ICR1) & 0xFFF32000;
        val |= APIC_ICR_LEVEL_ASSERT |APIC_ICR_DM_STARTUP;
        val |= (((u32)trampoline >> 12) & 0xff);
        lapic_write(LAPIC_ICR1, val);

        timeout = 1000;

        lapic_microsec_sleep (200);
        errstatus = 0;

        while ((lapic_read(LAPIC_ICR1) & APIC_ICR_DELIVERY_PENDING) &&
                !errstatus) {
            errstatus = lapic_errstatus();
            timeout--;
            if (!timeout) break;
        }

        if (errstatus)
            return -1;
    }

    return 0;
}

int apic_send_init_ipi(unsigned cpu, phys_bytes trampoline)
{
    u32 errstatus = 0;
    int timeout;

    lapic_errstatus();

    lapic_write(LAPIC_ICR2, (lapic_read (LAPIC_ICR2) & 0xFFFFFF) |
                    (cpuid2apicid[cpu] << 24));
    lapic_write(LAPIC_ICR1, (lapic_read (LAPIC_ICR1) & 0xFFF32000) |
        APIC_ICR_DM_INIT | APIC_ICR_TM_LEVEL | APIC_ICR_LEVEL_ASSERT);

    timeout = 1000;

    lapic_microsec_sleep(200);

    errstatus = 0;

    while ((lapic_read(LAPIC_ICR1) & APIC_ICR_DELIVERY_PENDING) && !errstatus) {
        errstatus = lapic_errstatus();
        timeout--;
        if (!timeout) break;
    }

    if (errstatus)
        return -1;

    lapic_errstatus();

    lapic_write(LAPIC_ICR2, (lapic_read (LAPIC_ICR2) & 0xFFFFFF) |
                    (cpuid2apicid[cpu] << 24));
    lapic_write(LAPIC_ICR1, (lapic_read (LAPIC_ICR1) & 0xFFF32000) |
        APIC_ICR_DEST_ALL | APIC_ICR_TM_LEVEL);

    timeout = 1000;
    errstatus = 0;

    lapic_microsec_sleep(200);

    while ((lapic_read(LAPIC_ICR1)&APIC_ICR_DELIVERY_PENDING) && !errstatus) {
        errstatus = lapic_errstatus();
        timeout--;
        if(!timeout) break;
    }

    if (errstatus)
        return -1;

    lapic_errstatus();

    /* BSP DELAY(10mSec) */
    lapic_microsec_sleep(10000);

    return 0;
}
#endif
