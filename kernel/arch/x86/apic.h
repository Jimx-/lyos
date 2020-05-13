#ifndef _X86_APIC_H_
#define _X86_APIC_H_

#include <asm/proto.h>

#define LOCAL_APIC_DEF_ADDR 0xfee00000 /* default local apic address */
#define APIC_BASE_MSR 0x800

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

static inline u32 apic_native_mem_read(u32 reg)
{
    return *((volatile u32*)(lapic_addr + (reg)));
}

static inline void apic_native_mem_write(u32 reg, u32 val)
{
    *((volatile u32*)(lapic_addr + reg)) = val;
}

static inline void apic_native_mem_eoi_write(void)
{
    apic_native_mem_write(LAPIC_EOI, 0);
}

static inline u32 apic_native_msr_read(u32 reg)
{
    u32 hi, lo;

    if (reg == LAPIC_DFR) {
        return -1;
    }

    ia32_read_msr(APIC_BASE_MSR + (reg >> 4), &hi, &lo);
    return lo;
}

static inline void apic_native_msr_write(u32 reg, u32 val)
{
    if (reg == LAPIC_DFR || reg == LAPIC_LDR || reg == LAPIC_ID) {
        /* not writable in x2apic */
        return;
    }

    ia32_write_msr(APIC_BASE_MSR + (reg >> 4), 0, val);
}

static inline void apic_native_msr_eoi_write(void)
{
    ia32_write_msr(APIC_BASE_MSR + (LAPIC_EOI >> 4), 0, 0);
}

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

struct apic {
    u32 (*read)(u32 reg);
    void (*write)(u32 reg, u32 val);
    void (*eoi_write)(void);
    void (*native_eoi_write)(void);
    u64 (*icr_read)(void);
    void (*icr_write)(u32 id, u32 low);
};

extern struct apic* apic;

struct io_apic {
    unsigned id;
    char version;
    void* addr;
    phys_bytes phys_addr;
    void* vir_addr;
    unsigned pins;
    unsigned gsi_base;
};

static __attribute__((always_inline)) inline u32 lapic_read(u32 reg)
{
    return apic->read(reg);
}

static __attribute__((always_inline)) inline void lapic_write(u32 reg, u32 val)
{
    apic->write(reg, val);
}

static __attribute__((always_inline)) inline void apic_eoi(void)
{
    apic->eoi_write();
}

extern u64 apic_native_icr_read(void);
extern void apic_native_icr_write(u32 id, u32 low);

static inline u64 x2apic_native_icr_read(void)
{
    u32 hi, lo;
    ia32_read_msr(APIC_BASE_MSR + (LAPIC_ICR1 >> 4), &hi, &lo);
    return make64(hi, lo);
}

static inline void x2apic_native_icr_write(u32 id, u32 low)
{
    ia32_write_msr(APIC_BASE_MSR + (LAPIC_ICR1 >> 4), id, low);
}

#endif
