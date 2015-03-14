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
#include "assert.h"
#include <errno.h>
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "signal.h"
#include "page.h"
#include "arch_const.h"
#include "arch_proto.h"
#ifdef CONFIG_SMP
#include "arch_smp.h"
#endif
#include "lyos/cpulocals.h"

typedef int (*sys_call_handler_t)(MESSAGE * m, struct proc * p_proc);

PUBLIC int set_priv(struct proc * p, int id)
{
    struct priv * priv;
    if (id == PRIV_ID_NULL) {
        for (priv = &FIRST_DYN_PRIV; priv < &LAST_DYN_PRIV; priv++) {
            if (priv->proc_nr == NO_TASK) break;
        }
        if (priv >= &LAST_DYN_PRIV) return ENOSPC;
    } else {
        if (id < 0 || id > NR_BOOT_PROCS) return EINVAL;
        if (priv_table[id].proc_nr != NO_TASK) return EBUSY;
        priv = &priv_table[id];
    }
    p->priv = priv;
    priv->proc_nr = ENDPOINT_P(p->endpoint);

    return 0;
}

PUBLIC int dispatch_sys_call(int call_nr, MESSAGE * m_user, struct proc * p_proc)
{
    int retval;
    MESSAGE msg;

    if (call_nr > NR_SYS_CALLS || call_nr < 0) return EINVAL;

    if (copy_user_message(&msg, m_user) != 0) return EFAULT;
    msg.source = p_proc->endpoint;

    if (sys_call_table[call_nr]) {
        sys_call_handler_t handler = sys_call_table[call_nr];
        retval = handler(&msg, p_proc);
    } else {
        return EINVAL;
    }

   if (copy_user_message(m_user, &msg) != 0) return EFAULT;

    return retval;
}
