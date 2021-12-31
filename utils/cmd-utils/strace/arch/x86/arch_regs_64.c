#include <sys/ptrace.h>

#include "../../types.h"
#include "../../proto.h"

#define RBXREG    (5 * 8)
#define RAXREG    (10 * 8)
#define P_ORIGRAX (15 * 8)

void arch_get_syscall_args(struct tcb* tcp)
{
    tcp->u_args[0] = ptrace(PTRACE_PEEKUSER, tcp->pid, (void*)P_ORIGRAX, 0);
    tcp->u_args[1] = ptrace(PTRACE_PEEKUSER, tcp->pid, (void*)RBXREG, 0);
}

void arch_get_syscall_rval(struct tcb* tcp)
{
    tcp->rval = ptrace(PTRACE_PEEKUSER, tcp->pid, (void*)RAXREG, 0);
}
