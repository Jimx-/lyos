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
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <signal.h>
#include "errno.h"
#include <asm/stackframe.h>

int sys_sigsend(MESSAGE* m, struct proc* p)
{
    struct siginfo si;
    struct proc* p_dest = endpt_proc(m->ENDPOINT);
    if (!p_dest) return EINVAL;

    data_vir_copy(KERNEL, &si, p->endpoint, m->BUF, sizeof(si));

    lock_proc(p_dest);

#ifdef __i386__
    si.stackptr = (void*)p_dest->regs.sp;
#endif

    struct sigframe sf, *sfp;
    sfp = (struct sigframe*)si.stackptr - 1;
    memset(&sf, 0, sizeof(sf));
    sf.scp = &sfp->sc;
    sf.scp_sigreturn = &sfp->sc;

#ifdef __i386__
    sf.sc.gs = p_dest->regs.gs;
    sf.sc.fs = p_dest->regs.fs;
    sf.sc.es = p_dest->regs.es;
    sf.sc.ds = p_dest->regs.ds;
    sf.sc.edi = p_dest->regs.di;
    sf.sc.esi = p_dest->regs.si;
    sf.sc.ebp = p_dest->regs.bp;
    sf.sc.ebx = p_dest->regs.bx;
    sf.sc.edx = p_dest->regs.dx;
    sf.sc.ecx = p_dest->regs.cx;
    sf.sc.eax = p_dest->regs.ax;
    sf.sc.eip = p_dest->regs.ip;
    sf.sc.cs = p_dest->regs.cs;
    sf.sc.eflags = p_dest->regs.flags;
    sf.sc.esp = p_dest->regs.sp;
    sf.sc.ss = p_dest->regs.ss;
    sf.signum = si.signo;
    sf.retaddr_sigreturn = si.sig_return;
    sf.retaddr = p_dest->regs.ip;

    sf.sc.trap_style = p_dest->seg.trap_style;
#endif
    sf.sc.mask = si.mask;

    data_vir_copy(p_dest->endpoint, sfp, KERNEL, &sf, sizeof(sf));

#ifdef __i386__
    p_dest->regs.sp = (reg_t)sfp;
    p_dest->regs.ip = (reg_t)si.sig_handler;
#endif

    unlock_proc(p_dest);

    return 0;
}
