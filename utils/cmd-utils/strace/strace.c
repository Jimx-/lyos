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

#define TCB_MAX 20
static struct tcb tcbs[TCB_MAX];
static struct tcb* last_tcb;

static int exit_tcb(struct tcb* tcp, int status);
static void do_trace(pid_t child, int s);

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

static int copy_message_from(struct tcb* tcp, void* src_msg, void* dest_msg)
{
    if (!fetch_mem(tcp, src_msg, dest_msg, sizeof(MESSAGE))) return -1;

    return 0;
}

/*
static void print_msg(MESSAGE* m)
{
    int packed = 0;
    printf("{%ssrc:%d,%stype:%d,%sm->u.m3:{0x%x, 0x%x, 0x%x, 0x%x, 0x%x,
0x%x}%s}%s", packed ? "" : "\n        ", m->source, packed ? " " : "\n        ",
           m->type,
           packed ? " " : "\n        ",
           m->u.m3.m3i1,
           m->u.m3.m3i2,
           m->u.m3.m3i3,
           m->u.m3.m3i4,
           (int)m->u.m3.m3p1,
           (int)m->u.m3.m3p2,
           packed ? "" : "\n",
           packed ? "" : "\n"
        );
}
*/

static void trace_write_in(struct tcb* tcp)
{
    MESSAGE* msg = &tcp->msg_in;

    printf("write(%d, ", msg->FD);
    print_str(tcp, msg->BUF, msg->CNT);
    printf(", %d)", msg->CNT);
}

static void trace_exec_in(struct tcb* tcp)
{
    MESSAGE* msg = &tcp->msg_in;

    printf("execve(");
    print_path(tcp, msg->PATHNAME, msg->NAME_LEN);
    printf(", %p)", msg->BUF);
}

static void trace_chmod_in(struct tcb* tcp)
{
    MESSAGE* msg = &tcp->msg_in;

    printf("chmod(");
    print_path(tcp, msg->PATHNAME, msg->NAME_LEN);
    printf(", 0%o)", msg->MODE);
}

static void trace_sendrec_in(struct tcb* tcp)
{
    copy_message_from(tcp, tcp->msg_in.SR_MSG, &tcp->msg_in);

    int type = tcp->msg_in.type;

    tcp->msg_type_in = type;

    switch (type) {
    case OPEN:
        tcp->sys_trace_ret = trace_open(tcp);
        break;
    case CLOSE:
        printf("close(%d)", tcp->msg_in.FD);
        break;
    case READ:
        printf("read(%d, %p, %d)", tcp->msg_in.FD, tcp->msg_in.BUF,
               tcp->msg_in.CNT);
        break;
    case WRITE:
        trace_write_in(tcp);
        break;
    case STAT:
        tcp->sys_trace_ret = trace_stat(tcp);
        break;
    case FSTAT:
        tcp->sys_trace_ret = trace_fstat(tcp);
        break;
    case EXEC:
        trace_exec_in(tcp);
        break;
    case BRK:
        tcp->sys_trace_ret = trace_brk(tcp);
        break;
    case GETDENTS:
        printf("getdents(%d, %p, %d)", tcp->msg_in.FD, tcp->msg_in.BUF,
               tcp->msg_in.CNT);
        break;
    case EXIT:
        printf("exit(%d) = ?\n",
               tcp->msg_in.STATUS); /* exit has no return value */
        exit_tcb(tcp, W_EXITCODE(tcp->msg_in.STATUS, 0));
        break;
    case MMAP:
        tcp->sys_trace_ret = trace_mmap(tcp);
        break;
    case MUNMAP:
        tcp->sys_trace_ret = trace_munmap(tcp);
        break;
    case DUP:
        tcp->sys_trace_ret = trace_dup(tcp);
        break;
    case UMASK:
        printf("umask(0%o)", tcp->msg_in.MODE);
        break;
    case CHMOD:
        trace_chmod_in(tcp);
        break;
    case GETSETID:
        printf("getsetid(%d)", tcp->msg_in.REQUEST);
        break;
    case SYMLINK:
        printf("symlink(");
        print_str(tcp, tcp->msg_in.u.m_vfs_link.old_path,
                  tcp->msg_in.u.m_vfs_link.old_path_len);
        printf(", ");
        print_str(tcp, tcp->msg_in.u.m_vfs_link.new_path,
                  tcp->msg_in.u.m_vfs_link.new_path_len);
        printf(")");
        break;
    case IOCTL:
        tcp->sys_trace_ret = trace_ioctl(tcp);
        break;
    case UNLINK:
        tcp->sys_trace_ret = trace_unlink(tcp);
        break;
    case PIPE2:
        tcp->sys_trace_ret = trace_pipe2(tcp);
        break;
    case FORK:
        tcp->sys_trace_ret = trace_fork(tcp);
        break;
    case WAIT:
        tcp->sys_trace_ret = trace_waitpid(tcp);
        break;
    case POLL:
        tcp->sys_trace_ret = trace_poll(tcp);
        break;
    case EVENTFD:
        tcp->sys_trace_ret = trace_eventfd(tcp);
        break;
    case SIGPROCMASK:
        tcp->sys_trace_ret = trace_sigprocmask(tcp);
        break;
    case SIGNALFD:
        tcp->sys_trace_ret = trace_signalfd(tcp);
        break;
    case KILL:
        tcp->sys_trace_ret = trace_kill(tcp);
        break;
    case TIMERFD_CREATE:
        tcp->sys_trace_ret = trace_timerfd_create(tcp);
        break;
    case TIMERFD_SETTIME:
        tcp->sys_trace_ret = trace_timerfd_settime(tcp);
        break;
    case TIMERFD_GETTIME:
        tcp->sys_trace_ret = trace_timerfd_gettime(tcp);
        break;
    case EPOLL_CREATE1:
        tcp->sys_trace_ret = trace_epoll_create1(tcp);
        break;
    case EPOLL_CTL:
        tcp->sys_trace_ret = trace_epoll_ctl(tcp);
        break;
    case EPOLL_WAIT:
        tcp->sys_trace_ret = trace_epoll_wait(tcp);
        break;
    case SOCKET:
        tcp->sys_trace_ret = trace_socket(tcp);
        break;
    case BIND:
        tcp->sys_trace_ret = trace_bind(tcp);
        break;
    case CONNECT:
        tcp->sys_trace_ret = trace_connect(tcp);
        break;
    case LISTEN:
        tcp->sys_trace_ret = trace_listen(tcp);
        break;
    case ACCEPT:
        tcp->sys_trace_ret = trace_accept(tcp);
        break;
    default:
        printf("syscall(%d)", type);
        break;
    }
}

static void trace_sendrec_out(struct tcb* tcp)
{
    copy_message_from(tcp, tcp->msg_out.SR_MSG, &tcp->msg_out);

    int type = tcp->msg_type_in;
    int retval = 0;
    int out_ret = 0;
    int base = 10;
    int err = 0;

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

    if (!(tcp->sys_trace_ret & RVAL_DECODED)) {
        switch (type) {
        case STAT:
            trace_stat(tcp);
            break;
        case FSTAT:
            trace_fstat(tcp);
            break;
        case PIPE2:
            trace_pipe2(tcp);
            break;
        case WAIT:
            trace_waitpid(tcp);
            break;
        case POLL:
            out_ret = trace_poll(tcp);
            break;
        case SIGPROCMASK:
            trace_sigprocmask(tcp);
            break;
        case TIMERFD_SETTIME:
            trace_timerfd_settime(tcp);
            break;
        case TIMERFD_GETTIME:
            trace_timerfd_gettime(tcp);
            break;
        case EPOLL_WAIT:
            trace_epoll_wait(tcp);
            break;
        case ACCEPT:
            trace_accept(tcp);
            break;
        }
    }

    switch (type) {
    case OPEN:
    case EVENTFD:
    case SIGNALFD:
    case TIMERFD_CREATE:
    case EPOLL_CREATE1:
    case SOCKET:
    case ACCEPT:
        retval = tcp->msg_out.FD;
        if (retval < 0) {
            err = -retval;
            retval = -1;
        }
        break;
    case READ:
    case WRITE:
    case STAT:
    case FSTAT:
    case CLOSE:
    case GETDENTS:
    case CHMOD:
    case SELECT:
    case MUNMAP:
    case DUP:
    case IOCTL:
    case PIPE2:
    case POLL:
    case SIGPROCMASK:
    case KILL:
    case TIMERFD_SETTIME:
    case TIMERFD_GETTIME:
    case EPOLL_CTL:
    case EPOLL_WAIT:
    case BIND:
    case CONNECT:
    case LISTEN:
        retval = tcp->msg_out.RETVAL;

        if (retval < 0) {
            err = -retval;
            retval = -1;
        }
        break;
    case GETSETID:
    case BRK:
    case UMASK:
        retval = tcp->msg_out.RETVAL;
        break;
    case MMAP:
        base = 16;
        retval = (int)tcp->msg_out.u.m_mm_mmap_reply.retaddr;
        break;
    case FORK:
    case WAIT:
        retval = tcp->msg_out.PID;
        break;
    }

    switch (base) {
    case 10:
        printf(" = %d ", retval);
        break;
    case 16:
        printf(" = 0x%x ", retval);
        break;
    }

    if (err != 0) {
        print_err(err);
    }

    if (out_ret & RVAL_STR) printf("(%s)", tcp->aux_str);

    printf("\n");
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

        printf("<... resumed> ");
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
            break;
        }

        last_tcb = tcp;

        if (tcp->pid != -1)
            ptrace(PTRACE_SYSCALL, tcp->pid, 0, restart_sig);
        else
            ptrace(PTRACE_CONT, wait_child, 0, 0);
    }
}
