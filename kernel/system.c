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

#include "lyos/type.h"
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "assert.h"
#include <errno.h>
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "signal.h"
#include "page.h"
#include "arch_const.h"
#include "arch_proto.h"
#ifdef CONFIG_SMP
#include "arch_smp.h"
#endif
#include "lyos/cpulocals.h"
#include <lyos/vm.h>

PRIVATE  sys_call_handler_t  sys_call_table[NR_SYS_CALLS];

PRIVATE int sys_nosys(MESSAGE * m, struct proc * p_proc)
{
    return ENOSYS;
}

PUBLIC void init_system()
{
    int i;
    for (i = 0; i < NR_SYS_CALLS; i++) {
        sys_call_table[i] = sys_nosys;
    }

    sys_call_table[NR_PRINTX] = sys_printx;
    sys_call_table[NR_SENDREC] = sys_sendrec;
    sys_call_table[NR_DATACOPY] = sys_datacopy;
    sys_call_table[NR_PRIVCTL] = sys_privctl;
    sys_call_table[NR_GETINFO] = sys_getinfo;
    sys_call_table[NR_VMCTL] = sys_vmctl;
    sys_call_table[NR_UMAP] = sys_umap;
    sys_call_table[NR_PORTIO] = sys_portio;
    sys_call_table[NR_VPORTIO] = sys_vportio;
#ifdef __i386__
    sys_call_table[NR_SPORTIO] = sys_sportio;
#endif
    sys_call_table[NR_IRQCTL] = sys_irqctl;
    sys_call_table[NR_FORK] = sys_fork;
    sys_call_table[NR_CLEAR] = sys_clear;
    sys_call_table[NR_EXEC] = sys_exec;
    sys_call_table[NR_SIGSEND] = sys_sigsend;
    sys_call_table[NR_SIGRETURN] = sys_sigreturn;
    sys_call_table[NR_KILL] = sys_kill;
    sys_call_table[NR_GETKSIG] = sys_getksig;
    sys_call_table[NR_ENDKSIG] = sys_endksig;
    sys_call_table[NR_TIMES] = sys_times;
    sys_call_table[NR_TRACE] = sys_trace;
}

PUBLIC int set_priv(struct proc * p, int id)
{
    struct priv * priv;
    if (id == PRIV_ID_NULL) {
        for (priv = &FIRST_DYN_PRIV; priv < &LAST_DYN_PRIV; priv++) {
            if (priv->proc_nr == NO_TASK) break;
        }
        if (priv >= &LAST_DYN_PRIV) return ENOSPC;
    } else {
        if (id < 0 || id > NR_BOOT_PROCS) return EINVAL;
        if (priv_table[id].proc_nr != NO_TASK) return EBUSY;
        priv = &priv_table[id];
    }
    p->priv = priv;
    priv->proc_nr = ENDPOINT_P(p->endpoint);

    return 0;
}

PRIVATE int finish_sys_call(struct proc * p_proc, MESSAGE * msg, int result)
{
    if (result == MMSUSPEND) {
        p_proc->mm_request.saved_reqmsg = *msg;
        p_proc->flags |= PF_RESUME_SYSCALL;
    } else {
        if (copy_user_message(p_proc->syscall_msg, msg) != 0) return EFAULT;
    }

    arch_set_syscall_result(p_proc, result);
    
    return result;
}

PRIVATE int dispatch_sys_call(int call_nr, MESSAGE * msg, struct proc * p_proc)
{
    int retval;
    
    if (sys_call_table[call_nr]) {
        sys_call_handler_t handler = sys_call_table[call_nr];
        retval = handler(msg, p_proc);
    } else {
        return EINVAL;
    }

    return retval;
}

PUBLIC int handle_sys_call(int call_nr, MESSAGE * m_user, struct proc * p_proc)
{
    MESSAGE msg;

    if (call_nr > NR_SYS_CALLS || call_nr < 0) return EINVAL;

    p_proc->syscall_msg = m_user;

    if (copy_user_message(&msg, m_user) != 0) {
        printk("kernel: copy message failed!(SIGSEGV #%d 0x%x)\n", p_proc->endpoint, m_user);
        ksig_proc(p_proc->endpoint, SIGSEGV);
        return EFAULT;
    }

    msg.source = p_proc->endpoint;
    msg.type = call_nr;

    /* syscall enter stop */
    if (p_proc->flags & PF_TRACE_SYSCALL) {
        /* restart the call later */
        p_proc->mm_request.saved_reqmsg = msg;
        p_proc->flags |= PF_RESUME_SYSCALL;
        p_proc->flags &= ~PF_TRACE_SYSCALL;
        ksig_proc(p_proc->endpoint, SIGTRAP);

        return 0;
    }

    int retval = dispatch_sys_call(call_nr, &msg, p_proc);

    retval = finish_sys_call(p_proc, &msg, retval);

    return retval;
}

PUBLIC int resume_sys_call(struct proc * p)
{
    int retval = dispatch_sys_call(p->mm_request.saved_reqmsg.type, &p->mm_request.saved_reqmsg, p);

    p->flags &= ~PF_RESUME_SYSCALL;

    retval = finish_sys_call(p, &p->mm_request.saved_reqmsg, retval);
    return retval;
}

PUBLIC int send_sig(endpoint_t ep, int signo)
{
    struct proc * p = endpt_proc(ep);
    if (!p) return EINVAL;

    struct priv * priv = p->priv;
    if (!priv) return ENOENT;

    sigaddset(&priv->sig_pending, signo);
    msg_notify(proc_addr(SYSTEM), p->endpoint);

    return 0;
}

PUBLIC void ksig_proc(endpoint_t ep, int signo)
{
    struct proc * p = endpt_proc(ep);
    if (!p) return;

    if (!sigismember(&p->sig_pending, signo)) {
        sigaddset(&p->sig_pending, signo);

        if (!PST_IS_SET(p, PST_SIGNALED)) {
            PST_SET(p, PST_SIGNALED | PST_SIG_PENDING);
            if (send_sig(TASK_PM, SIGKSIG) != 0) panic("sig_proc: send_sig failed");
        }
    }
}
