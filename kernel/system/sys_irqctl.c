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
#include "lyos/config.h"
#include "lyos/const.h"
#include <kernel/proc.h>
#include <kernel/proto.h>
#include <errno.h>
#include <string.h>
#include <lyos/irqctl.h>
#include <lyos/priv.h>
#include <kernel/irq.h>

static int generic_irq_handler(irq_hook_t* hook);

int sys_irqctl(MESSAGE* m, struct proc* p_proc)
{
    struct irqctl_request* req = (struct irqctl_request*)m->MSG_PAYLOAD;
    int irq = req->irq, hook_id = req->hook_id - 1, notify_id = req->hook_id;
    int retval = 0, i;
    irq_hook_t* hook = NULL;
    struct irq_fwspec fwspec;

    switch (req->request) {
    case IRQ_ENABLE:
    case IRQ_DISABLE:
        if (hook_id < 0 || hook_id >= NR_IRQ_HOOKS ||
            irq_hooks[hook_id].proc_ep == NO_TASK)
            return EINVAL;
        if (irq_hooks[hook_id].proc_ep != p_proc->endpoint) return EPERM;
        if (req->request == IRQ_ENABLE)
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
        hook->irq_policy = req->policy;
        put_irq_handler(irq, hook, generic_irq_handler);

        req->hook_id = hook_id + 1;

        break;

    case IRQ_MAP_FWSPEC:
        if (req->fwspec.param_count > IRQCTL_IRQ_SPEC_PARAMS) return EINVAL;

        memset(&fwspec, 0, sizeof(fwspec));
        fwspec.fwid = req->fwspec.fwid;
        fwspec.param_count = req->fwspec.param_count;
        memcpy(fwspec.param, req->fwspec.param,
               sizeof(u32) * req->fwspec.param_count);

        irq = irq_create_fwspec_mapping(&fwspec);
        if (irq == 0) return EINVAL;

        req->irq = irq;
        break;

    default:
        retval = EINVAL;
        break;
    }

    return retval;
}

static int generic_irq_handler(irq_hook_t* hook)
{
    struct proc* p = endpt_proc(hook->proc_ep);
    if (!p) panic("invalid interrupt handler: %d", hook->proc_ep);

    p->priv->int_pending |= (1 << hook->notify_id);
    msg_notify(proc_addr(INTERRUPT), hook->proc_ep);

    return hook->irq_policy & IRQ_REENABLE;
}
