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
#include <lyos/ipc.h>
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "stddef.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <asm/page.h>
#include <errno.h>
#include <asm/proto.h>
#include <lyos/sysutils.h>

PUBLIC int sys_exec(MESSAGE* m, struct proc* p_proc)
{
    struct proc* p = endpt_proc(m->KEXEC_ENDPOINT);
    if (!p) return EINVAL;

    lock_proc(p);
    char name[PROC_NAME_LEN];
    memcpy(name, m->KEXEC_NAME, sizeof(name));
    name[sizeof(name) - 1] = '\0';

    arch_init_proc(p, m->KEXEC_SP, m->KEXEC_IP, m->KEXEC_PSSTR, m->KEXEC_NAME);

    unlock_proc(p);
    return 0;
}
