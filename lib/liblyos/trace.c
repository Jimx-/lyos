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
#include <lyos/const.h>
#include <lyos/ipc.h>
#include <stdio.h>
#include <string.h>
#include <lyos/trace.h>

PUBLIC int kernel_trace(int request, endpoint_t endpoint, void* addr, void* data)
{
    MESSAGE msg;

    msg.TRACE_REQ = request;
    msg.TRACE_ENDPOINT = endpoint;
    msg.TRACE_ADDR = addr;
    msg.TRACE_DATA = data;

    int retval = syscall_entry(NR_TRACE, &msg);
    if (data) *(long*)data = msg.TRACE_RET;

    return retval;
}
