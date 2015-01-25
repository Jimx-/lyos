#ifndef _X86_APIC_H_
#define _X86_APIC_H_

#define LOCAL_APIC_DEF_ADDR	0xfee00000 /* default local apic address */

#define LAPIC_ID	(lapic_addr + 0x020)
#define LAPIC_VERSION	(lapic_addr + 0x030)
#define LAPIC_TPR	(lapic_addr + 0x080)
#define LAPIC_EOI	(lapic_addr + 0x0b0)
#define LAPIC_LDR	(lapic_addr + 0x0d0)
#define LAPIC_DFR	(lapic_addr + 0x0e0)
#define LAPIC_SIVR	(lapic_addr + 0x0f0)
#define LAPIC_ISR	(lapic_addr + 0x100)
#define LAPIC_TMR	(lapic_addr + 0x180)
#define LAPIC_IRR	(lapic_addr + 0x200)
#define LAPIC_ESR	(lapic_addr + 0x280)
#define LAPIC_ICR1	(lapic_addr + 0x300)
#define LAPIC_ICR2	(lapic_addr + 0x310)
#define LAPIC_LVTTR	(lapic_addr + 0x320)
#define LAPIC_LVTTMR	(lapic_addr + 0x330)
#define LAPIC_LVTPCR	(lapic_addr + 0x340)
#define LAPIC_LINT0	(lapic_addr + 0x350)
#define LAPIC_LINT1	(lapic_addr + 0x360)
#define LAPIC_LVTER	(lapic_addr + 0x370)
#define LAPIC_TIMER_ICR	(lapic_addr + 0x380)
#define LAPIC_TIMER_CCR	(lapic_addr + 0x390)
#define LAPIC_TIMER_DCR	(lapic_addr + 0x3e0)

#define APIC_TIMER_INT_VECTOR		0xf0
#define APIC_SMP_SCHED_PROC_VECTOR	0xf1
#define APIC_SMP_CPU_HALT_VECTOR	0xf2
#define APIC_ERROR_INT_VECTOR		0xfe
#define APIC_SPURIOUS_INT_VECTOR	0xff

#define IOAPIC_ID				0x0
#define IOAPIC_VERSION			0x1
#define IOAPIC_ARB				0x2
#define IOAPIC_REDIR_TABLE		0x10

#define MAX_IOAPICS		32

#define lapic_read(what)	(*((volatile u32 *)((what))))
#define lapic_write(what, data)	do {			\
									(*((volatile u32 *)((what)))) = data;		\
								} while(0)
#define apic_eoi()			do { *(volatile u32 *)lapic_eoi_addr = 0; } while(0)

PUBLIC u32 apicid();
PUBLIC int lapic_enable(unsigned cpu);
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
	unsigned	id;
	char		version;
	vir_bytes	addr;
	phys_bytes	phys_addr;
	vir_bytes	vir_addr;
	unsigned	pins;
	unsigned	gsi_base;
};

#endif
