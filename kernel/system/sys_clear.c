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
#include "lyos/const.h"
#include <kernel/proc.h>
#include <kernel/proto.h>
#include <asm/page.h>
#include <errno.h>

int sys_clear(MESSAGE* m, struct proc* p_proc)
{
    endpoint_t ep = m->ENDPOINT;
    int slot;
    if (!verify_endpt(ep, &slot)) return EINVAL;

    struct proc* p = proc_addr(slot);
    PST_SETFLAGS(p, PST_FREE_SLOT);

    release_fpu(p);
    p->flags &= ~PF_FPU_INITIALIZED;

    return 0;
}
