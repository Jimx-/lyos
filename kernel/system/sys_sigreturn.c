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

#include <lyos/config.h>
#include <lyos/types.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "lyos/const.h"
#include "string.h"
#include <kernel/proc.h>
#include <kernel/global.h>
#include <kernel/proto.h>
#include <signal.h>
#include "errno.h"
#include <asm/stackframe.h>

int sys_sigreturn(MESSAGE* m, struct proc* p)
{
    struct sigcontext sc;
    struct proc* p_dest = endpt_proc(m->ENDPOINT);
    if (!p_dest) return EINVAL;

    data_vir_copy(KERNEL, &sc, p_dest->endpoint, m->BUF, sizeof(sc));

    lock_proc(p_dest);

#ifdef __i386__
    p_dest->regs.gs = sc.gs;
    p_dest->regs.fs = sc.fs;
    p_dest->regs.es = sc.es;
    p_dest->regs.ds = sc.ds;
    p_dest->regs.di = sc.edi;
    p_dest->regs.si = sc.esi;
    p_dest->regs.bp = sc.ebp;
    p_dest->regs.bx = sc.ebx;
    p_dest->regs.dx = sc.edx;
    p_dest->regs.cx = sc.ecx;
    p_dest->regs.ax = sc.eax;
    p_dest->regs.ip = sc.eip;
    p_dest->regs.cs = sc.cs;
    p_dest->regs.flags = sc.eflags;
    p_dest->regs.sp = sc.esp;
    p_dest->regs.ss = sc.ss;

    p_dest->seg.trap_style = sc.trap_style;
#endif

    unlock_proc(p_dest);

    return 0;
}
