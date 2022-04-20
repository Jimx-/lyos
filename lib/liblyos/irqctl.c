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
    along with Lyos.  If not, see <http://www.gnu.org/licenses/". */

#include <lyos/ipc.h>
#include <lyos/const.h>
#include <lyos/irqctl.h>
#include <string.h>

int syscall_entry(int syscall_nr, MESSAGE* m);

int irqctl(int request, int irq, int policy, int* hook_id)
{
    MESSAGE m;
    struct irqctl_request* req = (struct irqctl_request*)m.MSG_PAYLOAD;

    memset(&m, 0, sizeof(m));
    req->request = request;
    req->irq = irq;
    req->policy = policy;
    req->hook_id = *hook_id;

    int retval = syscall_entry(NR_IRQCTL, &m);

    if (request == IRQ_SETPOLICY) *hook_id = req->hook_id;

    return retval;
}

int irqctl_map_fwspec(struct irqctl_fwspec* fwspec)
{
    MESSAGE m;
    struct irqctl_request* req = (struct irqctl_request*)m.MSG_PAYLOAD;
    int retval;

    memset(&m, 0, sizeof(m));
    req->request = IRQ_MAP_FWSPEC;
    req->fwspec = *fwspec;

    retval = syscall_entry(NR_IRQCTL, &m);
    if (retval > 0) return 0;

    return req->irq;
}
