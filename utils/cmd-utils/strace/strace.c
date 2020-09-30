#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/const.h>

#include "types.h"
#include "proto.h"

static void do_trace(pid_t child, int s);

int main(int argc, char* argv[], char* envp[])
{
    if (argc == 1) return 0;

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
    long* src = (long*)src_msg;
    long* dest = (long*)dest_msg;

    while (src < (long*)((char*)src_msg + sizeof(MESSAGE))) {
        long data = ptrace(PTRACE_PEEKDATA, tcp->pid, src, 0);
        if (data == -1) return -1;
        *dest = data;
        src++;
        dest++;
    }

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

static void trace_open_in(struct tcb* tcp)
{
    MESSAGE* msg = &tcp->msg_in;

    printf("open(");
    print_path(tcp, msg->PATHNAME, msg->NAME_LEN);
    printf(", %d)", msg->FLAGS);
}

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
        trace_open_in(tcp);
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
    int base = 10;
    int err = 0;

    switch (type) {
    case OPEN:
        retval = tcp->msg_out.FD;
        if (retval < 0) {
            err = -retval;
            retval = -1;
        }
        break;
    case READ:
    case WRITE:
        retval = tcp->msg_out.CNT;
        break;
    case STAT:
    case FSTAT:
        if (!(tcp->sys_trace_ret & RVAL_DECODED)) {
            if (type == STAT) {
                trace_stat(tcp);
            } else {
                trace_fstat(tcp);
            }
        }

        retval = tcp->msg_out.RETVAL;
        if (retval > 0) {
            err = retval;
            retval = -1;
        }
    case CLOSE:
    case BRK:
    case GETDENTS:
    case UMASK:
    case CHMOD:
    case GETSETID:
    case SELECT:
    case MUNMAP:
    case DUP:
        retval = tcp->msg_out.RETVAL;
        break;
    case MMAP:
        base = 16;
        retval = (int)tcp->msg_out.u.m_mm_mmap_reply.retaddr;
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

    printf("\n");
}

#define EBX      8
#define EAX      11
#define ORIG_EAX 18
static void trace_call_in(struct tcb* tcp)
{
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

static void do_trace(pid_t child, int s)
{
    int status = s;
    struct tcb tcb;

    tcb.pid = child;
    tcb.flags = 0;

    while (1) {
        if (WIFEXITED(status)) break;

        ptrace(PTRACE_SYSCALL, child, 0, 0);

        int waitchild = wait(&status);
        if (waitchild != child) continue;
        if (WIFEXITED(status)) break;

        int stopsig = WSTOPSIG(status);
        switch (stopsig) {
        case SIGTRAP:
            tcb.flags ^= TCB_ENTERING;

            if (entering(&tcb))
                trace_call_in(&tcb);
            else
                trace_call_out(&tcb);
            break;
        }
    }

    printf("+++ exited with %d +++\n", WEXITSTATUS(status));
}
