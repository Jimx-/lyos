/*  
    (c)Copyright 2011 Jimx
    
    This file is part of Lyos.

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
#include "lyos/config.h"
#include "lyos/const.h"
#include "string.h"
#include <errno.h>
#include "lyos/proc.h"
#include <lyos/ipc.h>
#include <lyos/sysutils.h>
#include <lyos/vm.h>
#include "global.h"
#include "proto.h"

PUBLIC int do_pm_exec(MESSAGE* m)
{
    endpoint_t ep = m->ENDPOINT;
    struct pmproc* pmp = pm_endpt_proc(ep);
    if (!pmp) return EINVAL;

    /* tell tracer */
    if (pmp->tracer != NO_TASK) {
        sig_proc(pmp, SIGTRAP, TRUE);
    }

    return 0;
}

