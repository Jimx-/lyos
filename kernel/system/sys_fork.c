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
    void* newsp = m->BUF;
    int flags = m->FLAGS;
    int child_slot = m->PROC_NR, parent_slot, retval;

    int gen = ENDPOINT_G(parent_ep) + 1;
    endpoint_t child_ep = make_endpoint(gen, child_slot);

    if (!verify_endpt(parent_ep, &parent_slot)) return EINVAL;

    struct proc * parent = proc_addr(parent_slot), * child = proc_addr(child_slot);

    lock_proc(parent);
    lock_proc(child);

    *child = *parent;
    snprintf(child->name, sizeof(child->name), "%s_%d", parent->name, child_ep);
    child->endpoint = child_ep;
    child->p_parent = parent_ep;

    /* child of priv proc, reset its priv structure and block it */
    if (!child->priv || child->priv->flags & PRF_PRIV_PROC) {
        child->priv = NULL;
        PST_SET_LOCKED(child, PST_NO_PRIV);
    }

    /* clear message queue */
    child->q_sending = NULL;
    child->next_sending = NULL;

    if (flags & KF_MMINHIBIT) PST_SET_LOCKED(child, PST_MMINHIBIT);

#ifdef __i386__
    if (newsp != NULL) {
        child->regs.esp = (reg_t)newsp;
    }
#endif

    m->ENDPOINT = child_ep;
    unlock_proc(parent);
    unlock_proc(child);

    return 0;
}
