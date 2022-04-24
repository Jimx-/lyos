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

#include <lyos/types.h>
#include <lyos/ipc.h>
#include "lyos/const.h"
#include "lyos/vm.h"
#include <sys/mman.h>
#include <string.h>

/*****************************************************************************
 *                                mm_map_phys
 *****************************************************************************/
void* mm_map_phys(endpoint_t who, phys_bytes phys_addr, size_t len, int flags)
{
    MESSAGE msg;
    struct mm_map_phys_request* mmp_req =
        (struct mm_map_phys_request*)&msg.MSG_PAYLOAD;

    memset(&msg, 0, sizeof(msg));
    msg.type = MM_MAP_PHYS;
    mmp_req->who = who;
    mmp_req->phys_addr = phys_addr;
    mmp_req->len = len;
    mmp_req->flags = flags;

    send_recv(BOTH, TASK_MM, &msg);

    if (msg.RETVAL != 0) return MAP_FAILED;

    return msg.ADDR;
}
