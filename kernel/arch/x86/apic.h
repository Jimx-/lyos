#ifndef _X86_APIC_H_
#define _X86_APIC_H_

#define LOCAL_APIC_DEF_ADDR	0xfee00000 /* default local apic address */

#define LAPIC_ID	(lapic_addr + 0x020)

#define lapic_read(what)	(*((volatile u32 *)((what))))

PUBLIC u32 apicid();

#endif
