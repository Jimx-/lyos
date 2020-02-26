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

#include <lyos/type.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "lyos/const.h"
#include "stdio.h"
#include "unistd.h"
#include "assert.h"
#include <lyos/interrupt.h>

int syscall_entry(int syscall_nr, MESSAGE* m);

PUBLIC int irqctl(int request, int irq, int policy, int* hook_id)
{
    MESSAGE m;

    m.IRQ_REQUEST = request;
    m.IRQ_IRQ = irq;
    m.IRQ_POLICY = policy;
    m.IRQ_HOOK_ID = *hook_id;

    int retval = syscall_entry(NR_IRQCTL, &m);

    if (request == IRQ_SETPOLICY) *hook_id = m.IRQ_HOOK_ID;

    return retval;
}
