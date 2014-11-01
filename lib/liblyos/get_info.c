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

#include "lyos/type.h"
#include "sys/types.h"
#include "lyos/const.h"
#include "stdio.h"
#include "stdarg.h"
#include "unistd.h"
#include "assert.h"
#include "lyos/vm.h"
#include "lyos/ipc.h"
#include <lyos/config.h>
#include <lyos/param.h>

int syscall_entry(int syscall_nr, MESSAGE * m);

PUBLIC int getinfo(int request, void* buf)
{
    MESSAGE m;
    m.REQUEST = request;
    m.BUF = buf;

    return syscall_entry(NR_GETINFO, &m);
}

PUBLIC int get_sysinfo(struct sysinfo ** sysinfo)
{
	return getinfo(GETINFO_SYSINFO, sysinfo);
}

PUBLIC int get_kinfo(kinfo_t * kinfo)
{
    return getinfo(GETINFO_KINFO, kinfo);
}
