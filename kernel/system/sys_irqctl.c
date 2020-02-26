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
#include "lyos/config.h"
#include "stdio.h"
#include "stdarg.h"
#include "unistd.h"
#include "assert.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <errno.h>
#include <lyos/interrupt.h>

PRIVATE int generic_irq_handler(irq_hook_t* hook);

PUBLIC int sys_irqctl(MESSAGE* m, struct proc* p_proc)
{
    int irq = m->IRQ_IRQ, hook_id = m->IRQ_HOOK_ID - 1,
        notify_id = m->IRQ_HOOK_ID;
    int retval = 0, i;
    irq_hook_t* hook = NULL;

    switch (m->IRQ_REQUEST) {
    case IRQ_ENABLE:
    case IRQ_DISABLE:
        if (hook_id < 0 || hook_id >= NR_IRQ_HOOKS ||
            irq_hooks[hook_id].proc_ep == NO_TASK)
            return EINVAL;
        if (irq_hooks[hook_id].proc_ep != p_proc->endpoint) return EPERM;
        if (m->IRQ_REQUEST == IRQ_ENABLE)
            enable_irq(&irq_hooks[hook_id]);
        else
            disable_irq(&irq_hooks[hook_id]);
        break;
    case IRQ_SETPOLICY:
        if (irq < 0 || irq >= NR_IRQ) return EINVAL;

        for (i = 0; !hook && i < NR_IRQ_HOOKS; i++) {
            if (irq_hooks[i].proc_ep == p_proc->endpoint &&
                irq_hooks[i].notify_id == notify_id) {
                hook_id = i;
                hook = &irq_hooks[i];
            }
        }

        for (i = 0; !hook && i < NR_IRQ_HOOKS; i++) {
            if (irq_hooks[i].proc_ep == NO_TASK) {
                hook_id = i;
                hook = &irq_hooks[i];
            }
        }

        if (!hook) return ENOSPC;

        hook->proc_ep = p_proc->endpoint;
        hook->notify_id = notify_id;
        hook->irq_policy = m->IRQ_POLICY;
        put_irq_handler(irq, hook, generic_irq_handler);

        m->IRQ_HOOK_ID = hook_id + 1;

        break;
    default:
        retval = EINVAL;
        break;
    }

    return retval;
}

PRIVATE int generic_irq_handler(irq_hook_t* hook)
{
    struct proc* p = endpt_proc(hook->proc_ep);
    if (!p) panic("invalid interrupt handler: %d", hook->proc_ep);

    p->priv->int_pending |= (1 << hook->notify_id);
    msg_notify(proc_addr(INTERRUPT), hook->proc_ep);

    return hook->irq_policy & IRQ_REENABLE;
}
