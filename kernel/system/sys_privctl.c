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
#include <asm/page.h>
#include <asm/const.h>
#include <asm/proto.h>
#ifdef CONFIG_SMP
#include <asm/smp.h>
#endif
#include <lyos/cpulocals.h>

int arch_privctl(MESSAGE* m, struct proc* p);

static int update_priv(struct proc* p, struct priv* priv);

int sys_privctl(MESSAGE* m, struct proc* p)
{
    endpoint_t whom = m->ENDPOINT;
    int request = m->REQUEST;
    int priv_id;
    struct priv priv;
    int r;

    struct proc* target = endpt_proc(whom);
    if (!target) return EINVAL;

    switch (request) {
    case PRIVCTL_SET_PRIV:
        if (!PST_IS_SET(target, PST_NO_PRIV)) return EPERM;

        priv_id = PRIV_ID_NULL;
        if (m->BUF) {
            memcpy(&priv, m->BUF, sizeof(struct priv));

            if (!(priv.flags & PRF_DYN_ID)) priv_id = priv.id;
        }

        if ((r = set_priv(target, priv_id)) != 0) return r;

        if (m->BUF) {
            if ((r = update_priv(target, &priv)) != 0) return r;
        }

        return 0;
    case PRIVCTL_ALLOW:
        PST_UNSET(target, PST_NO_PRIV);

        return 0;
    default:
        break;
    }

    return EINVAL;
}

static int update_priv(struct proc* p, struct priv* priv)
{
    p->priv->flags = priv->flags;
    return 0;
}
