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
#include <assert.h>

int syscall_entry(int syscall_nr, MESSAGE * m);

/*****************************************************************************
 *                                data_copy
 *****************************************************************************/
PUBLIC int data_copy(endpoint_t dest_pid, void * dest_addr, 
    endpoint_t src_pid, void * src_addr, int len)
{
	MESSAGE m;
    
    m.SRC_EP = src_pid;
    m.SRC_ADDR = src_addr;

    m.DEST_EP = dest_pid;
    m.DEST_ADDR = dest_addr;

    m.BUF_LEN = len;

    return syscall_entry(NR_DATACOPY, &m);
}
