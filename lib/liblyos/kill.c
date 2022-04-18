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

int kernel_kill(endpoint_t ep, int signo)
{
    MESSAGE m;
    m.ENDPOINT = ep;
    m.SIGNR = signo;

    return syscall_entry(NR_KILL, &m);
}
