#include <sys/ptrace.h>

#include "../../types.h"
#include "../../proto.h"

void arch_get_syscall_args(struct tcb* tcp)
{
    tcp->u_args[0] =
        ptrace(PTRACE_PEEKUSER, tcp->pid, (void*)(8 * 8), 0);        /* w8 */
    tcp->u_args[1] = ptrace(PTRACE_PEEKUSER, tcp->pid, (void*)0, 0); /* x0 */
}

void arch_get_syscall_rval(struct tcb* tcp)
{
    tcp->rval = ptrace(PTRACE_PEEKUSER, tcp->pid, (void*)0, 0); /* x0 */
}
