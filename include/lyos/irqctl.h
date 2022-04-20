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

#ifndef _IRQCTL_H_
#define _IRQCTL_H_

/* irqctl requests */
#define IRQ_SETPOLICY  1
#define IRQ_ENABLE     2
#define IRQ_DISABLE    3
#define IRQ_RMPOLICY   4
#define IRQ_MAP_FWSPEC 5

/* irq policy */
#define IRQ_REENABLE 0x1

#define IRQCTL_IRQ_SPEC_PARAMS 8

struct irqctl_fwspec {
    unsigned int fwid;
    int param_count;
    u32 param[IRQCTL_IRQ_SPEC_PARAMS];
};

struct irqctl_request {
    int request;
    int irq;

    union {
        struct {
            int policy;
            int hook_id;
        };

        struct irqctl_fwspec fwspec;
    };
};

int irqctl(int request, int irq, int policy, int* hook_id);
#define irq_enable(id)                 irqctl(IRQ_ENABLE, 0, 0, id)
#define irq_disable(id)                irqctl(IRQ_DISABLE, 0, 0, id)
#define irq_setpolicy(irq, policy, id) irqctl(IRQ_SETPOLICY, irq, policy, id)
#define irq_rmpolicy(id)               irqctl(IRQ_RMPOLICY, 0, 0, id)

int irqctl_map_fwspec(struct irqctl_fwspec* fwspec);

#endif
