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
#include <asm/protect.h>
#include "lyos/const.h"
#include <kernel/proto.h>
#include <kernel/irq.h>
#include <asm/const.h>

/* 8259A interrupt controller ports. */
#define INT_M_CTL                                              \
    0x20 /* I/O port for interrupt controller         <Master> \
          */
#define INT_M_CTLMASK \
    0x21               /* setting bits in this port disables ints   <Master> */
#define INT_S_CTL 0xA0 /* I/O port for second interrupt controller  <Slave> */
#define INT_S_CTLMASK \
    0xA1 /* setting bits in this port disables ints   <Slave>  */

extern void i8259_eoi_master();
extern void i8259_eoi_slave();

static struct irq_chip i8259A_chip;

/*======================================================================*
                            init_8259A
 *======================================================================*/
void init_8259A()
{
    int irq;

    for (irq = 0; irq < NR_IRQS_LEGACY; irq++) {
        irq_set_chip(irq, &i8259A_chip);
        irq_set_handler(irq, handle_simple_irq, FALSE);
    }

    out_byte(INT_M_CTL, 0x11); /* Master 8259, ICW1. */
    out_byte(INT_S_CTL, 0x11); /* Slave  8259, ICW1. */
    out_byte(INT_M_CTLMASK,
             INT_VECTOR_IRQ0); /* Master 8259, ICW2. 设置 '主8259'
                                  的中断入口地址为 0x20. */
    out_byte(INT_S_CTLMASK,
             INT_VECTOR_IRQ8);    /* Slave  8259, ICW2. 设置 '从8259'
                                     的中断入口地址为 0x28. */
    out_byte(INT_M_CTLMASK, 0x4); /* Master 8259, ICW3. IR2 对应 '从8259'. */
    out_byte(INT_S_CTLMASK, 0x2); /* Slave  8259, ICW3. 对应 '主8259' 的 IR2. */
    out_byte(INT_M_CTLMASK, 0x1); /* Master 8259, ICW4. */
    out_byte(INT_S_CTLMASK, 0x1); /* Slave  8259, ICW4. */

    out_byte(INT_M_CTLMASK, ~(1 << CASCADE_IRQ)); /* Master 8259, OCW1. */
    out_byte(INT_S_CTLMASK, 0xFF);                /* Slave  8259, OCW1. */
}

void disable_8259A()
{
    int irq;

    out_byte(INT_S_CTLMASK, 0xFF);
    out_byte(INT_M_CTLMASK, 0xFF);
    in_byte(INT_M_CTLMASK);

    for (irq = 0; irq < NR_IRQS_LEGACY; irq++) {
        irq_set_chip(irq, NULL);
        irq_set_handler(irq, NULL, FALSE);
    }
}

static void mask_i8259_irq(int irq)
{
    u32 ctl_mask = irq < 8 ? INT_M_CTLMASK : INT_S_CTLMASK;
    out_byte(ctl_mask, in_byte(ctl_mask) | (1 << (irq & 0x7)));
}

static void unmask_i8259_irq(int irq)
{
    u32 ctl_mask = irq < 8 ? INT_M_CTLMASK : INT_S_CTLMASK;
    out_byte(ctl_mask, in_byte(ctl_mask) & ~(1 << (irq & 0x7)));
}

static void i8259_eoi_irq(int irq)
{
    if (irq < 8)
        i8259_eoi_master();
    else
        i8259_eoi_slave();
}

static void i8259_mask(struct irq_data* data) { mask_i8259_irq(data->irq); }

static void i8259_unmask(struct irq_data* data) { unmask_i8259_irq(data->irq); }

static void i8259_eoi(struct irq_data* data) { i8259_eoi_irq(data->irq); }

static struct irq_chip i8259A_chip = {
    .irq_mask = i8259_mask,
    .irq_unmask = i8259_unmask,
    .irq_eoi = i8259_eoi,
};
