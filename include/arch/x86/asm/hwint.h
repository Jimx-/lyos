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

#ifndef _HWINT_H_
#define _HWINT_H_

/* i8259 PIC */
void i8259_mask(int irq);
void i8259_unmask(int irq);
void i8259_eoi();
void i8259_eoi_master();
void i8259_eoi_slave();

#if CONFIG_X86_IO_APIC

void ioapic_mask(int irq);
void ioapic_unmask(int irq);
void ioapic_eoi(int irq);
void ioapic_set_irq(int irq);
void ioapic_unset_irq(int irq);

extern u8 ioapic_enabled;

#define hwint_mask(irq) ioapic_mask(irq)
#define hwint_unmask(irq) ioapic_unmask(irq)
#define hwint_ack(irq) ioapic_eoi(irq)
#define hwint_used(irq)                          \
    do {                                         \
        if (ioapic_enabled) ioapic_set_irq(irq); \
    } while (0)
#define hwint_not_used(irq)                        \
    do {                                           \
        if (ioapic_enabled) ioapic_unset_irq(irq); \
    } while (0)

#else

#define hwint_mask(irq) i8259_mask(irq)
#define hwint_unmask(irq) i8259_unmask(irq)
#define hwint_ack(irq) i8259_eoi(irq)
#define hwint_used(irq)
#define hwint_not_used(irq)

#endif

void disable_8259A();

#endif
