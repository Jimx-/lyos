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

#include <lyos/types.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "stddef.h"
#include "lyos/const.h"
#include "string.h"
#include <kernel/proc.h>
#include <kernel/global.h>
#include <kernel/proto.h>
#include <asm/page.h>
#include <errno.h>
#include <asm/proto.h>
#include <lyos/sysutils.h>

int sys_kill(MESSAGE* m, struct proc* p_proc)
{
    endpoint_t target = m->ENDPOINT;
    int signo = m->SIGNR;

    if (signo > NSIG) return EINVAL;
    if (!verify_endpt(target, NULL)) return EINVAL;
    if (is_kerntaske(target)) return EPERM;

    ksig_proc(target, signo);

    return 0;
}
