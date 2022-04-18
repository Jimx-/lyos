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

int sys_exec(MESSAGE* m, struct proc* p_proc)
{
    char name[PROC_NAME_LEN];
    struct proc* p = endpt_proc(m->KEXEC_ENDPOINT);
    struct ps_strings ps;
    int retval = 0;

    if (!p) return EINVAL;

    lock_proc(p);

    p->flags &= ~PF_DELIVER_MSG;

    if (data_vir_copy(KERNEL, name, p_proc->endpoint, m->KEXEC_NAME,
                      sizeof(name) - 1) != 0)
        strncpy(name, "<unset>", sizeof(name));
    name[sizeof(name) - 1] = '\0';

    if ((retval = data_vir_copy(KERNEL, &ps, p_proc->endpoint, m->KEXEC_PSSTR,
                                sizeof(ps))) != 0)
        goto out;

    arch_init_proc(p, m->KEXEC_SP, m->KEXEC_IP, &ps, name);

    release_fpu(p);
    p->flags &= ~PF_FPU_INITIALIZED;

out:
    unlock_proc(p);
    return retval;
}
