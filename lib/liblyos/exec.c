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
#include <lyos/sysutils.h>

int kernel_exec(endpoint_t ep, void* sp, char* name, void* ip,
                struct ps_strings* ps)
{
    MESSAGE m;

    m.KEXEC_ENDPOINT = ep;
    m.KEXEC_SP = sp;
    m.KEXEC_NAME = name;
    m.KEXEC_IP = ip;
    m.KEXEC_PSSTR = ps;

    return syscall_entry(NR_EXEC, &m);
}
