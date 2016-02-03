#ifndef _MACH_OMAP2_INTERRUPT_H_
#define _MACH_OMAP2_INTERRUPT_H_

/* Interrupt controller base */
#define OMAP3_BEAGLE_INTR_BASE 0x48200000
#define OMAP3_AM335X_INTR_BASE 0x48200000

/* Interrupt controller registers */
#define OMAP3_INTCPS_SIR_IRQ      0x040 /* Active IRQ number */
#define OMAP3_INTCPS_CONTROL      0x048 /* New int agreement bits */

#define OMAP3_INTR_ACTIVEIRQ_MASK 0x7f /* Active IRQ mask for SIR_IRQ */
#define OMAP3_INTR_NEWIRQAGR      0x1  /* New IRQ Generation */

#endif
