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
#include "stdio.h"
#include "lyos/const.h"
#include "string.h"
#include <kernel/proc.h>
#include <lyos/priv.h>
#include <kernel/global.h>
#include <kernel/proto.h>
#include <asm/page.h>
#include <errno.h>
#include <asm/proto.h>
#include <lyos/sysutils.h>

int sys_fork(MESSAGE* m, struct proc* p_proc)
{
    struct kfork_info* kfi = (struct kfork_info*)m->MSG_PAYLOAD;
    endpoint_t parent_ep = kfi->parent;
    void* newsp = kfi->newsp;
    void* tls = kfi->tls;
    int flags = kfi->flags;
    int child_slot = kfi->slot, parent_slot;
    int gen = ENDPOINT_G(parent_ep) + 1;
    endpoint_t child_ep = make_endpoint(gen, child_slot);
    void* old_fpu_state = NULL;
    int retval;

    if (!verify_endpt(parent_ep, &parent_slot)) return EINVAL;

    struct proc *parent = proc_addr(parent_slot),
                *child = proc_addr(child_slot);

    lock_proc(parent);
    lock_proc(child);

    save_fpu(parent);

#if defined(__i386__) || defined(__x86_64__) || defined(__aarch64__)
    old_fpu_state = child->seg.fpu_state;
#endif

    *child = *parent;

#if defined(__i386__) || defined(__x86_64__) || defined(__aarch64__)
    child->seg.fpu_state = old_fpu_state;
#endif

    snprintf(child->name, sizeof(child->name), "%s_%d", parent->name, child_ep);
    child->slot = child_slot;
    child->endpoint = child_ep;
    child->p_parent = parent_ep;

    /* child of priv proc, reset its priv structure and block it */
    if (child->priv->flags & PRF_PRIV_PROC) {
        child->priv = priv_addr(PRIV_ID_USER);
        PST_SET_LOCKED(child, PST_NO_PRIV);
    }

    /* clear message queue */
    child->q_sending = NULL;
    child->next_sending = NULL;

    child->user_time = 0;

    if (flags & KF_MMINHIBIT) PST_SET_LOCKED(child, PST_MMINHIBIT);

    if (newsp) {
        /* XXX: Relocate the receive message if the child is using a separate
         * stack in the same address space. */
        child->recv_msg = (MESSAGE*)(newsp - sizeof(MESSAGE));
    }

    retval = arch_fork_proc(child, parent, flags, newsp, tls);

    if (retval == OK) kfi->child = child_ep;

    unlock_proc(parent);
    unlock_proc(child);

    return retval;
}
