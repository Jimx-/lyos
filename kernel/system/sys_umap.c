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
#include <asm/cpulocals.h>
#include <lyos/vm.h>

int sys_umap(MESSAGE* m, struct proc* p)
{
    endpoint_t grantee, new_granter, ep = m->UMAP_WHO;
    int type = m->UMAP_TYPE;
    vir_bytes offset = (vir_bytes)m->UMAP_SRCADDR;
    vir_bytes count = (vir_bytes)m->UMAP_SIZE;
    vir_bytes src_addr, new_offset;
    struct proc* target_proc;
    mgrant_id_t grant;
    int retval;

    if (ep == SELF) ep = p->endpoint;
    grantee = p->endpoint;

    target_proc = endpt_proc(ep);
    if (!target_proc) return EINVAL;

    switch (type) {
    case UMT_GRANT:
        grant = (mgrant_id_t)offset;
        if ((retval = verify_grant(target_proc->endpoint, grantee, grant, count,
                                   0, 0, &new_offset, &new_granter)) != 0)
            return retval;

        target_proc = endpt_proc(new_granter);
        if (!target_proc) return EFAULT;

        offset = new_offset;
        /* fall through */
    case UMT_VADDR:
        src_addr = offset;
        break;
    default:
        return EINVAL;
    }

    m->UMAP_DSTADDR = va2pa(target_proc->endpoint, (void*)src_addr);

    return 0;
}
