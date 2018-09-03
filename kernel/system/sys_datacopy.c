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

#include <lyos/type.h>
#include <lyos/ipc.h>
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

PUBLIC int sys_datacopy(MESSAGE * m, struct proc * p_proc)
{
    void * src_addr = m->SRC_ADDR;
    endpoint_t src_ep = m->SRC_EP == SELF ? p_proc->endpoint : m->SRC_EP;

    void * dest_addr = m->DEST_ADDR;
    endpoint_t dest_ep = m->DEST_EP == SELF ? p_proc->endpoint : m->DEST_EP;

    struct vir_addr src, dest;

    src.addr = src_addr;
    src.proc_ep = src_ep;

    dest.addr = dest_addr;
    dest.proc_ep = dest_ep;

    int len = m->BUF_LEN;

    return vir_copy_check(p_proc, &dest, &src, len);
}
