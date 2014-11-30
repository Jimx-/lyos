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

#include "lyos/type.h"
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "stddef.h"
#include "protect.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "page.h"
#include <errno.h>
#include "arch_proto.h"
#include <lyos/sysutils.h>

PUBLIC int sys_fork(MESSAGE * m, struct proc * p_proc)
{
    endpoint_t parent_ep = m->ENDPOINT;
    int child_slot = m->PROC_NR, parent_slot, retval;

    int gen = ENDPOINT_G(parent_ep) + 1;
    endpoint_t child_ep = make_endpoint(gen, child_slot);

    if (!verify_endpt(parent_ep, &parent_slot)) return EINVAL;

    struct proc * parent = proc_addr(parent_slot), * child = proc_addr(child_slot);

    *child = *parent;
    sprintf(child->name, "%s_%d", parent->name, child_ep);
    child->endpoint = child_ep;
    child->p_parent = parent_ep;

    if (m->FLAGS & KF_MMINHIBIT) PST_SET(child, PST_MMINHIBIT);

    m->ENDPOINT = child_ep;

    return 0;
}
