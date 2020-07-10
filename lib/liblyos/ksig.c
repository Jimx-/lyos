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
#include "sys/types.h"
#include "lyos/const.h"
#include "stdio.h"
#include "stdarg.h"
#include "unistd.h"
#include "assert.h"
#include <signal.h>
#include "lyos/vm.h"
#include <lyos/config.h>
#include <lyos/param.h>
#include <lyos/sysutils.h>

int get_ksig(endpoint_t* ep, sigset_t* set)
{
    MESSAGE m;

    int retval = syscall_entry(NR_GETKSIG, &m);

    *ep = m.ENDPOINT;
    *set = m.SIGSET;

    return retval;
}

int end_ksig(endpoint_t ep)
{
    MESSAGE m;
    m.ENDPOINT = ep;

    return syscall_entry(NR_ENDKSIG, &m);
}
