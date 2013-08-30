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
#include "lyos/const.h"
#include "lyos/tty.h"
#include "lyos/console.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"

/*****************************************************************************
 *                                sys_vircopy
 *****************************************************************************/
/**
 * <Ring 0> The core routine of system call `vircopy()'.
 * 
 * @param _unused1
 * @param _unused2
 * @param m        Ptr to the MESSAGE body.
 * @param p        The caller proc.
 * 
 * @return Zero if success.
 *****************************************************************************/
PUBLIC int sys_vircopy(int _unused1, int _unused2, MESSAGE* m, struct proc* p)
{    
    int src_pid = m->SRC_PID;
    int dest_pid = m->DEST_PID;
    void * vsrc = m->SRC_ADDR;
    void * vdest = m->DEST_ADDR;
    int len = m->BUF_LEN;

    return 0;
}