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
    along with Lyos.  If not, see <http://www.gnu.org/licenses/". */

#include <lyos/type.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "lyos/const.h"
#include "stdio.h"
#include "stdarg.h"
#include "unistd.h"
#include "assert.h"
#include "lyos/vm.h"
#include <sys/mman.h>

/*****************************************************************************
 *                                mm_map_phys
 *****************************************************************************/
void* mm_map_phys(endpoint_t who, void* phys_addr, size_t len)
{
    MESSAGE msg;

    msg.type = MM_MAP_PHYS;
    msg.ENDPOINT = who;
    msg.ADDR = phys_addr;
    msg.BUF_LEN = len;

    send_recv(BOTH, TASK_MM, &msg);

    if (msg.RETVAL != 0) return MAP_FAILED;

    return msg.ADDR;
}
