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

#include <lyos/ipc.h>
#include "lyos/const.h"
#include <lyos/sysutils.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <lyos/trace.h>
#include "errno.h"
#include "pmproc.h"
#include "proto.h"

int do_ptrace(MESSAGE* m)
{
    struct pmproc* pmp = pm_endpt_proc(m->source);
    if (!pmp) return EINVAL;

    int request = m->PTRACE_REQ;
    pid_t pid = m->PTRACE_PID;
    void* addr = m->PTRACE_ADDR;
    void* data = m->PTRACE_DATA;

    struct pmproc* target;
    int trace_req;

    switch (request) {
    case PTRACE_TRACEME:
        if (pmp->tracer != NO_TASK) return EBUSY;
        pmp->tracer = pmp->parent;
        m->PTRACE_RET = 0;
        return 0;
    }

    target = pm_pid_proc(pid);
    if (!target) return ESRCH;
    if (target->tracer != pmp->endpoint) return ESRCH;

    int i;
    switch (request) {
    case PTRACE_SYSCALL:
    case PTRACE_CONT:
        if ((long)data < 0 || (long)data >= NSIG) return EINVAL;

        if ((long)data > 0) sig_proc(target, (long)data, FALSE);

        for (i = 0; i < NSIG; i++) {
            if (sigismember(&target->sig_trace, i)) {
                m->PTRACE_RET = 0;
                return 0;
            }
        }

        target->flags &= ~PMPF_TRACED;

        break;
    }

    switch (request) {
    case PTRACE_SYSCALL:
        trace_req = TRACE_SYSCALL;
        break;
    case PTRACE_CONT:
        trace_req = TRACE_CONT;
        break;
    case PTRACE_PEEKTEXT:
        trace_req = TRACE_PEEKTEXT;
        break;
    case PTRACE_PEEKDATA:
        trace_req = TRACE_PEEKDATA;
        break;
    case PTRACE_PEEKUSER:
        trace_req = TRACE_PEEKUSER;
        break;
    }

    long trace_data;
    int retval = kernel_trace(trace_req, target->endpoint, addr, &trace_data);
    if (retval) return retval;

    m->PTRACE_RET = trace_data;
    return retval;
}

void trace_signal(struct pmproc* p_dest, int signo)
{
    p_dest->flags |= PMPF_TRACED;

    int retval = kernel_trace(TRACE_STOP, p_dest->endpoint, NULL, NULL);
    if (retval) panic("trace_signal: kernel_trace failed");

    struct pmproc* tracer = pm_endpt_proc(p_dest->tracer);
    if (waiting_for(tracer, p_dest)) {
        sigdelset(&p_dest->sig_trace, signo);

        tracer->flags &= ~PMPF_WAITING;

        MESSAGE msg2parent;
        msg2parent.type = SYSCALL_RET;
        msg2parent.PID = p_dest->pid;
        msg2parent.STATUS = W_STOPCODE(signo);
        send_recv(SEND_NONBLOCK, tracer->endpoint, &msg2parent);
    }
}
