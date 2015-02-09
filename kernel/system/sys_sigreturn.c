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
#include "lyos/type.h"
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <lyos/ipc.h>
#include <signal.h>
#include "errno.h"
#include "stackframe.h"

PUBLIC int sys_sigreturn(MESSAGE * m, struct proc* p)
{
    struct sigcontext sc;
    struct proc * p_dest = endpt_proc(m->ENDPOINT);
    if (!p_dest) return EINVAL;

    vir_copy(KERNEL, &sc, p_dest->endpoint, m->BUF, sizeof(sc));

#ifdef __i386__
    p_dest->regs.gs = sc.gs;
    p_dest->regs.fs = sc.fs;
    p_dest->regs.es = sc.es;
    p_dest->regs.ds = sc.ds;
    p_dest->regs.edi = sc.edi;
    p_dest->regs.esi = sc.esi;
    p_dest->regs.ebp = sc.ebp;
    p_dest->regs.ebx = sc.ebx;
    p_dest->regs.edx = sc.edx;
    p_dest->regs.ecx = sc.ecx;
    p_dest->regs.eax = sc.eax;
    p_dest->regs.eip = sc.eip;
    p_dest->regs.cs = sc.cs;
    p_dest->regs.eflags = sc.eflags;
    p_dest->regs.esp = sc.esp;
    p_dest->regs.ss = sc.ss;

    p_dest->seg.trap_style = sc.trap_style;
#endif

    return 0;
}
