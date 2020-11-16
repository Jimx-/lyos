#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/const.h>

#include "types.h"
#include "proto.h"

#define DEFAULT_STRLEN 32
size_t max_strlen = DEFAULT_STRLEN;

#define TCB_MAX 20
static struct tcb tcbs[TCB_MAX];
static struct tcb* last_tcb;

static int exit_tcb(struct tcb* tcp, int status);

static struct tcb* alloc_tcb(pid_t pid)
{
    int i;

    for (i = 0; i < TCB_MAX; i++)
        if (tcbs[i].pid == -1) break;

    if (i == TCB_MAX) return NULL;

    memset(&tcbs[i], 0, sizeof(tcbs[i]));
    tcbs[i].pid = pid;

    return &tcbs[i];
}

static struct tcb* get_tcb(pid_t pid)
{
    int i;

    for (i = 0; i < TCB_MAX; i++)
        if (tcbs[i].pid == pid) return &tcbs[i];

    return NULL;
}

static int copy_message_from(struct tcb* tcp, void* src_msg, void* dest_msg)
{
    if (!fetch_mem(tcp, src_msg, dest_msg, sizeof(MESSAGE))) return -1;

    return 0;
}

static void trace_sendrec_in(struct tcb* tcp)
{
    int sig, type;

    fetch_mem(tcp, tcp->msg_in.SR_MSG, &tcp->msg_in, sizeof(MESSAGE));

    type = tcp->msg_in.type;
    tcp->msg_type_in = type;

    if (type == EXIT) {
        printf("exit(%d) = ?\n",
               tcp->msg_in.STATUS); /* exit has no return value */
        exit_tcb(tcp, W_EXITCODE(tcp->msg_in.STATUS, 0));

        return;
    }

    tcp->sys_trace_ret = syscall_trace_entering(tcp, &sig);

    if (type == EXEC) {
        /* exec() has no return value */
        tcp->flags ^= TCB_ENTERING;
    }
}

static void trace_sendrec_out(struct tcb* tcp)
{
    fetch_mem(tcp, tcp->msg_out.SR_MSG, &tcp->msg_out, sizeof(MESSAGE));

    int type = tcp->msg_type_in;

    if (type == FORK) {
        pid_t new_pid = tcp->msg_out.PID;
        struct tcb* new_tcp;

        if (new_pid > 0 && !get_tcb(new_pid)) {
            new_tcp = alloc_tcb(new_pid);
            new_tcp->flags = TCB_ENTERING;
            new_tcp->msg_type_in = FORK;
            ptrace(PTRACE_SYSCALL, new_tcp->pid, 0, 0);
        }
    }

    syscall_trace_exiting(tcp);
}

#define EBX      8
#define EAX      11
#define ORIG_EAX 18
static void trace_call_in(struct tcb* tcp)
{
    if (last_tcb != tcp) {
        if (last_tcb->pid != -1 && entering(last_tcb))
            puts(" <unfinished ...>");
    }
    if (tcp->pid != tcbs[0].pid) printf("[pid %d] ", tcp->pid);

    long call_nr = ptrace(PTRACE_PEEKUSER, tcp->pid, (void*)(ORIG_EAX * 4), 0);

    void* src_msg =
        (void*)ptrace(PTRACE_PEEKUSER, tcp->pid, (void*)(EBX * 4), 0);
    copy_message_from(tcp, src_msg, &tcp->msg_in);

    switch (call_nr) {
    case NR_GETINFO:
        printf("get_sysinfo()");
        break;
    case NR_SENDREC:
        trace_sendrec_in(tcp);
        break;
    }

    fflush(stdout);
}

static void trace_call_out(struct tcb* tcp)
{
    if (last_tcb != tcp) {
        if (last_tcb->pid != -1 && entering(last_tcb)) puts("");

        if (tcp->pid != tcbs[0].pid) printf("[pid %d] ", tcp->pid);

        printf("<... %s resumed> ", tcb_sysent(tcp)->sys_name);
    }

    long call_nr = ptrace(PTRACE_PEEKUSER, tcp->pid, (void*)(ORIG_EAX * 4), 0);
    long eax = ptrace(PTRACE_PEEKUSER, tcp->pid, (void*)(EAX * 4), 0);

    void* src_msg =
        (void*)ptrace(PTRACE_PEEKUSER, tcp->pid, (void*)(EBX * 4), 0);
    copy_message_from(tcp, src_msg, &tcp->msg_out);

    switch (call_nr) {
    case NR_GETINFO:
        printf(" = 0x%lx\n",
               ptrace(PTRACE_PEEKDATA, tcp->pid, tcp->msg_out.BUF, 0));
        return;
    case NR_SENDREC:
        trace_sendrec_out(tcp);
        return;
    }

    printf(" = %ld\n", eax);
}

static int exit_tcb(struct tcb* tcp, int status)
{
    int i;

    if (tcp->pid != tcbs[0].pid) printf("[pid %d] ", tcp->pid);

    tcp->pid = -1;
    printf("+++ exited with %d +++\n", WEXITSTATUS(status));

    for (i = 0; i < TCB_MAX; i++) {
        if (tcbs[i].pid != -1) return FALSE;
    }

    return TRUE;
}

static void print_signalled(struct tcb* tcp, int sig)
{
    if (tcp->pid != tcbs[0].pid) printf("[pid %d] ", tcp->pid);

    printf("+++ caught signal %s +++\n", sprint_signame(sig));
}

static void do_trace(pid_t child, int s)
{
    int status = s;
    int wait_child;
    struct tcb* tcp;
    int restart_sig;

    last_tcb = alloc_tcb(child);

    ptrace(PTRACE_SYSCALL, child, 0, 0);

    while (1) {
        wait_child = waitpid(-1, &status, 0);
        if (wait_child < 0) continue;

        int stopsig = WSTOPSIG(status);

        tcp = get_tcb(wait_child);
        if (!tcp) {
            tcp = alloc_tcb(wait_child);
            tcp->flags = TCB_ENTERING;
            tcp->msg_type_in = FORK;
            stopsig = SIGTRAP;
        }

        if (WIFEXITED(status)) {
            if (exit_tcb(tcp, status)) break;
            continue;
        }

        restart_sig = 0;

        switch (stopsig) {
        case SIGTRAP:
            tcp->flags ^= TCB_ENTERING;

            if (entering(tcp))
                trace_call_in(tcp);
            else
                trace_call_out(tcp);

            break;

        default:
            restart_sig = stopsig;
            /* print_signalled(tcp, restart_sig); */
            break;
        }

        last_tcb = tcp;

        if (tcp->pid != -1)
            ptrace(PTRACE_SYSCALL, tcp->pid, 0, restart_sig);
        else
            ptrace(PTRACE_CONT, wait_child, 0, 0);
    }
}

int main(int argc, char* argv[], char* envp[])
{
    int i;

    if (argc == 1) return 0;

    for (i = 0; i < TCB_MAX; i++)
        tcbs[i].pid = -1;

    pid_t child;
    child = fork();

    argv++;
    if (child == 0) {
        ptrace(PTRACE_TRACEME, 0, NULL, NULL);
        exit(execve(argv[0], argv, envp));
    } else {
        int status;
        wait(&status);
        do_trace(child, status);
    }
    return 0;
}
