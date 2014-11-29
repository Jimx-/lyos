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
#include "lyos/config.h"
#include "stdio.h"
#include "stdarg.h"
#include "unistd.h"
#include "assert.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <errno.h>

/*======================================================================*
                               sys_getinfo
 *======================================================================*/
PUBLIC int sys_getinfo(MESSAGE * m, struct proc * p_proc)
{
    int request = m->REQUEST;
    void * buf = m->BUF;
    size_t buf_len = m->BUF_LEN;
    void * addr = NULL;
    size_t size = 0;
    struct sysinfo ** psi;

    switch (request) {
    case GETINFO_SYSINFO:
        psi = (struct sysinfo **)buf;
        *psi = sysinfo_user;
        break;
    case GETINFO_KINFO:
        addr = (void *)&kinfo;
        size = sizeof(kinfo_t);
        break;
    case GETINFO_CMDLINE:
        addr = (void *)&kinfo.cmdline;
        size = sizeof(kinfo.cmdline);
        break;
    case GETINFO_BOOTPROCS:
        addr = (void *)&kinfo.boot_procs;
        size = sizeof(kinfo.boot_procs);
        break;
    default:
        return EINVAL;
    }

    if (buf_len > 0 && buf_len > size) return E2BIG;
    if (addr) memcpy(buf, addr, size);

    return 0;
}
