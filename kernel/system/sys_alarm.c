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
#include <errno.h>
#include "arch_proto.h"
#include <lyos/sysutils.h>
#include <lyos/timer.h>

PRIVATE void sig_alarm(struct timer_list* timer)
{
    msg_notify(proc_addr(CLOCK), (endpoint_t)timer->arg);
}

PUBLIC int sys_alarm(MESSAGE * m, struct proc * p_proc)
{
    clock_t expire_time = m->EXP_TIME;
    int absolute_time = m->ABS_TIME;

    if (!(p_proc->priv->flags & PRF_PRIV_PROC)) return EPERM;

    struct timer_list* tp;
    tp = &(p_proc->priv->timer);
    tp->arg = (unsigned long) p_proc->endpoint;
    tp->callback = sig_alarm;

    if ((tp->expire_time != TIMER_UNSET) && (tp->expire_time > jiffies))
        m->TIME_LEFT = tp->expire_time - jiffies;
    else 
        m->TIME_LEFT = 0;

    if (expire_time == 0) {
        reset_sys_timer(tp);
    } else {
        tp->expire_time = absolute_time ? expire_time : jiffies + expire_time;
        set_sys_timer(tp);
    }

    return 0;
}
