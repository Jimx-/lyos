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

#include <lyos/ipc.h>
#include <errno.h>
#include "lyos/const.h"
#include <kernel/proto.h>
#include <asm/page.h>
#include <asm/proto.h>
#include <asm/hwint.h>
#include <kernel/irq.h>
#include <lyos/spinlock.h>

static spinlock_t irq_handlers_lock;
static irq_hook_t* irq_handlers[NR_IRQ] = {0};

static unsigned long irq_actids[NR_IRQ];

/*****************************************************************************
 *                                init_irq
 *****************************************************************************/
/**
 * <Ring 0> Initializes IRQ subsystem.
 *
 *****************************************************************************/
void init_irq(void)
{
    int i;
    for (i = 0; i < NR_IRQ_HOOKS; i++) {
        irq_hooks[i].proc_ep = NO_TASK;
    }
    spinlock_init(&irq_handlers_lock);

    arch_init_irq();
}

/*****************************************************************************
 *                                put_irq_handler
 *****************************************************************************/
/**
 * <Ring 0> Register an IRQ handler.
 *
 *****************************************************************************/
void put_irq_handler(int irq, irq_hook_t* hook, irq_handler_t handler)
{
    if (irq < 0 || irq >= NR_IRQ) panic("invalid irq %d", irq);

    spinlock_lock(&irq_handlers_lock);
    irq_hook_t** line = &irq_handlers[irq];

    unsigned long used_ids = 0;
    while (*line != NULL) {
        if (hook == *line) {
            spinlock_unlock(&irq_handlers_lock);
            return;
        }
        used_ids |= (*line)->id;
        line = &(*line)->next;
    }

    unsigned long id;
    for (id = 1; id != 0; id <<= 1)
        if ((used_ids & id) == 0) break;

    if (id == 0) panic("too many handlers for irq %d", irq);

    hook->next = NULL;
    hook->handler = handler;
    hook->id = id;
    hook->irq = irq;
    *line = hook;

    if ((irq_actids[hook->irq] &= ~hook->id) == 0) {
        hwint_used(irq);
        hwint_unmask(irq);
    }

    spinlock_unlock(&irq_handlers_lock);
}

void rm_irq_handler(irq_hook_t* hook)
{
    int irq = hook->irq;
    unsigned long id = hook->id;

    spinlock_lock(&irq_handlers_lock);
    irq_hook_t** line = &irq_handlers[irq];
    while (*line != NULL) {
        if ((*line)->id == id) {
            (*line) = (*line)->next;
            if (irq_actids[irq] & id) irq_actids[irq] &= ~id;
        } else
            line = &(*line)->next;
    }

    if (irq_handlers[irq] == NULL) {
        hwint_mask(irq);
        hwint_not_used(irq);
    } else if (irq_actids[irq] == 0)
        hwint_unmask(irq);

    spinlock_unlock(&irq_handlers_lock);
}

void irq_handle(int irq)
{
    spinlock_lock(&irq_handlers_lock);
    irq_hook_t* hook = irq_handlers[irq];

    hwint_mask(irq);

    while (hook != NULL) {
        irq_actids[irq] |= hook->id;

        if ((*hook->handler)(hook)) /* reenable int */
            irq_actids[hook->irq] &= ~hook->id;

        hook = hook->next;
    }

    if (irq_actids[irq] == 0) hwint_unmask(irq);

    hwint_ack(irq);
    spinlock_unlock(&irq_handlers_lock);
}

void enable_irq(irq_hook_t* hook)
{
    if ((irq_actids[hook->irq] &= ~hook->id) == 0) {
        hwint_unmask(hook->irq);
    }
}

int disable_irq(irq_hook_t* hook)
{
    if (irq_actids[hook->irq] & hook->id) return FALSE;

    irq_actids[hook->irq] |= hook->id;
    hwint_mask(hook->irq);
    return TRUE;
}
