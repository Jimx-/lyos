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

PUBLIC int sys_datacopy(MESSAGE * m, struct proc * p_proc)
{
    void * src_addr = m->SRC_ADDR;
    endpoint_t src_pid = m->SRC_PID == SELF ? proc2pid(p_proc) : m->SRC_PID;

    void * dest_addr = m->DEST_ADDR;
    endpoint_t dest_pid = m->DEST_PID == SELF ? proc2pid(p_proc) : m->DEST_PID;

    int len = m->BUF_LEN;

    return vir_copy(dest_pid, dest_addr, src_pid, src_addr, len);
}
