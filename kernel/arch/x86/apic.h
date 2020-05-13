#ifndef _X86_APIC_H_
#define _X86_APIC_H_

#define LOCAL_APIC_DEF_ADDR 0xfee00000 /* default local apic address */

#define LAPIC_ID 0x020
#define LAPIC_VERSION 0x030
#define LAPIC_TPR 0x080
#define LAPIC_EOI 0x0b0
#define LAPIC_LDR 0x0d0
#define LAPIC_DFR 0x0e0
#define LAPIC_SIVR 0x0f0
#define LAPIC_ISR 0x100
#define LAPIC_TMR 0x180
#define LAPIC_IRR 0x200
#define LAPIC_ESR 0x280
#define LAPIC_ICR1 0x300
#define LAPIC_ICR2 0x310
#define LAPIC_LVTTR 0x320
#define LAPIC_LVTTMR 0x330
#define LAPIC_LVTPCR 0x340
#define LAPIC_LINT0 0x350
#define LAPIC_LINT1 0x360
#define LAPIC_LVTER 0x370
#define LAPIC_TIMER_ICR 0x380
#define LAPIC_TIMER_CCR 0x390
#define LAPIC_TIMER_DCR 0x3e0

#define APIC_TIMER_INT_VECTOR 0xf0
#define APIC_SMP_SCHED_PROC_VECTOR 0xf1
#define APIC_SMP_CPU_HALT_VECTOR 0xf2
#define APIC_ERROR_INT_VECTOR 0xfe
#define APIC_SPURIOUS_INT_VECTOR 0xff

#define IOAPIC_ID 0x0
#define IOAPIC_VERSION 0x1
#define IOAPIC_ARB 0x2
#define IOAPIC_REDIR_TABLE 0x10

#define APIC_ICR_DM_MASK 0x00000700
#define APIC_ICR_VECTOR APIC_LVTT_VECTOR_MASK
#define APIC_ICR_DM_FIXED (0 << 8)
#define APIC_ICR_DM_LOWEST_PRIORITY (1 << 8)
#define APIC_ICR_DM_SMI (2 << 8)
#define APIC_ICR_DM_RESERVED (3 << 8)
#define APIC_ICR_DM_NMI (4 << 8)
#define APIC_ICR_DM_INIT (5 << 8)
#define APIC_ICR_DM_STARTUP (6 << 8)
#define APIC_ICR_DM_EXTINT (7 << 8)

#define APIC_ICR_DM_PHYSICAL (0 << 11)
#define APIC_ICR_DM_LOGICAL (1 << 11)

#define APIC_ICR_DELIVERY_PENDING (1 << 12)

#define APIC_ICR_INT_POLARITY (1 << 13)

#define APIC_ICR_LEVEL_ASSERT (1 << 14)
#define APIC_ICR_LEVEL_DEASSERT (0 << 14)

#define APIC_ICR_TRIGGER (1 << 15)

#define APIC_ICR_INT_MASK (1 << 16)

#define MAX_IOAPICS 32

#define lapic_read(what) (*((volatile u32*)(lapic_addr + (what))))

#define lapic_write(what, data)                           \
    do {                                                  \
        (*((volatile u32*)(lapic_addr + (what)))) = data; \
    } while (0)

extern void (*apic_native_eoi_write)(void);
extern void (*apic_eoi_write)(void);
#define apic_eoi()        \
    do {                  \
        apic_eoi_write(); \
    } while (0)

PUBLIC u32 apicid();
PUBLIC int lapic_enable(unsigned cpu);
void apic_set_eoi_write(void (*eoi_write)(void));
PUBLIC void apic_init_idt(int reset);
#if CONFIG_SMP
PUBLIC int apic_send_startup_ipi(unsigned cpu, phys_bytes trampoline);
PUBLIC int apic_send_init_ipi(unsigned cpu, phys_bytes trampoline);
#endif
PUBLIC void lapic_set_timer_one_shot(const u32 usec);
PUBLIC void lapic_restart_timer();
PUBLIC void lapic_stop_timer();
PUBLIC int detect_ioapics();
PUBLIC int ioapic_enable();

struct io_apic {
    unsigned id;
    char version;
    void* addr;
    phys_bytes phys_addr;
    void* vir_addr;
    unsigned pins;
    unsigned gsi_base;
};

#endif
