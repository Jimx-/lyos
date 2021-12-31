#include <sys/ptrace.h>

#include "../../types.h"
#include "../../proto.h"

#define EBX      8
#define EAX      11
#define ORIG_EAX 18

void arch_get_syscall_args(struct tcb* tcp)
{
    tcp->u_args[0] =
        ptrace(PTRACE_PEEKUSER, tcp->pid, (void*)(ORIG_EAX * 4), 0);
    tcp->u_args[1] = ptrace(PTRACE_PEEKUSER, tcp->pid, (void*)(EBX * 4), 0);
}

void arch_get_syscall_rval(struct tcb* tcp)
{
    tcp->rval = ptrace(PTRACE_PEEKUSER, tcp->pid, (void*)(EAX * 4), 0);
}
