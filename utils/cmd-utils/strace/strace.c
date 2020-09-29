#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/const.h>

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

static int copy_message_from(pid_t pid, void* src_msg, MESSAGE* dest_msg)
{
    long* src = (long*)src_msg;
    long* dest = (long*)dest_msg;

    while (src < (long*)((char*)src_msg + sizeof(MESSAGE))) {
        long data = ptrace(PTRACE_PEEKDATA, pid, src, 0);
        if (data == -1) return -1;
        *dest = data;
        src++;
        dest++;
    }

    return 0;
}

static void print_path(pid_t pid, char* string, int len)
{
    char* buf = (char*)malloc(len + 5);
    if (!buf) return;

    long* src = (long*)string;
    long* dest = (long*)buf;

    while (src < (long*)(string + len)) {
        long data = ptrace(PTRACE_PEEKDATA, pid, src, 0);
        if (data == -1) return;
        *dest = data;
        src++;
        dest++;
    }

    buf[len] = '\0';

    printf("\"%s\"", buf);

    free(buf);
}

static void print_str(pid_t pid, char* str, int len)
{
#define DEFAULT_BUF_LEN 32
    int truncated = 0;
    if (len > DEFAULT_BUF_LEN) {
        truncated = 1;
        len = DEFAULT_BUF_LEN;
    }

    char* buf = malloc(len + 1);
    if (!buf) return;

    long* src = (long*)str;
    long* dest = (long*)buf;

    while (src < (long*)(str + len)) {
        long data = ptrace(PTRACE_PEEKDATA, pid, src, 0);
        if (data == -1) goto out;
        *dest = data;
        src++;
        dest++;
    }

    buf[len] = '\0';
    printf("\"%s\"%s", buf, truncated ? "..." : "");

out:
    free(buf);
}

static void print_err(int err) { printf("%d %s", err, strerror(err)); }

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

static void trace_open_in(pid_t child, MESSAGE* msg)
{
    printf("open(");
    print_path(child, msg->PATHNAME, msg->NAME_LEN);
    printf(", %d)", msg->FLAGS);
}

static void trace_write_in(pid_t child, MESSAGE* msg)
{
    printf("write(%d, ", msg->FD);
    print_str(child, msg->BUF, msg->CNT);
    printf(", %d)", msg->CNT);
}

static void trace_exec_in(pid_t child, MESSAGE* msg)
{
    printf("execve(");
    print_path(child, msg->PATHNAME, msg->NAME_LEN);
    printf(", %p)", msg->BUF);
}

static void trace_stat_in(pid_t child, MESSAGE* msg)
{
    printf("stat(");
    print_path(child, msg->PATHNAME, msg->NAME_LEN);
    printf(", %p)", msg->BUF);
}

static void trace_chmod_in(pid_t child, MESSAGE* msg)
{
    printf("chmod(");
    print_path(child, msg->PATHNAME, msg->NAME_LEN);
    printf(", 0%o)", msg->MODE);
}

static int recorded_sendrec_type;
static void trace_sendrec_in(pid_t child, MESSAGE* req_msg)
{
    MESSAGE msg;
    copy_message_from(child, req_msg->SR_MSG, &msg);

    int type = msg.type;
    recorded_sendrec_type = type;
    switch (type) {
    case OPEN:
        trace_open_in(child, &msg);
        break;
    case CLOSE:
        printf("close(%d)", msg.FD);
        break;
    case READ:
        printf("read(%d, %p, %d)", msg.FD, msg.BUF, msg.CNT);
        break;
    case WRITE:
        trace_write_in(child, &msg);
        break;
    case STAT:
        trace_stat_in(child, &msg);
        break;
    case FSTAT:
        printf("fstat(%d, %p)", msg.FD, msg.BUF);
        break;
    case EXEC:
        trace_exec_in(child, &msg);
        break;
    case BRK:
        printf("brk(%p)", msg.ADDR);
        break;
    case GETDENTS:
        printf("getdents(%d, %p, %d)", msg.FD, msg.BUF, msg.CNT);
        break;
    case EXIT:
        printf("exit(%d) = ?\n", msg.STATUS); /* exit has no return value */
        break;
    case MMAP:
        printf("mmap(%p, %ld, %d, %d, %d, %ld)", msg.u.m_mm_mmap.vaddr,
               msg.u.m_mm_mmap.length, msg.u.m_mm_mmap.prot,
               msg.u.m_mm_mmap.flags, msg.u.m_mm_mmap.fd,
               msg.u.m_mm_mmap.offset);
        break;
    case UMASK:
        printf("umask(0%o)", msg.MODE);
        break;
    case CHMOD:
        trace_chmod_in(child, &msg);
        break;
    case GETSETID:
        printf("getsetid(%d)", msg.REQUEST);
        break;
    case SYMLINK:
        printf("symlink(");
        print_str(child, msg.u.m_vfs_link.old_path,
                  msg.u.m_vfs_link.old_path_len);
        printf(", ");
        print_str(child, msg.u.m_vfs_link.new_path,
                  msg.u.m_vfs_link.new_path_len);
        printf(")");
        break;
    default:
        printf("syscall(%d)", type);
        break;
    }
}

static void trace_sendrec_out(pid_t child, MESSAGE* req_msg)
{
    MESSAGE msg;
    copy_message_from(child, req_msg->SR_MSG, &msg);

    int type = recorded_sendrec_type;
    int retval = 0;
    int base = 10;
    int err = 0;

    switch (type) {
    case OPEN:
        retval = msg.FD;
        if (retval < 0) {
            err = -retval;
            retval = -1;
        }
        break;
    case READ:
    case WRITE:
        retval = msg.CNT;
        break;
    case STAT:
    case FSTAT:
        retval = msg.RETVAL;
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
        retval = msg.RETVAL;
        break;
    case MMAP:
        base = 16;
        retval = (int)msg.u.m_mm_mmap_reply.retaddr;
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
static void trace_call_in(pid_t child)
{
    long call_nr = ptrace(PTRACE_PEEKUSER, child, (void*)(ORIG_EAX * 4), 0);

    void* src_msg = (void*)ptrace(PTRACE_PEEKUSER, child, (void*)(EBX * 4), 0);
    MESSAGE msg;
    copy_message_from(child, src_msg, &msg);

    switch (call_nr) {
    case NR_GETINFO:
        printf("get_sysinfo()");
        break;
    case NR_SENDREC:
        trace_sendrec_in(child, &msg);
        break;
    }

    fflush(stdout);
}

static void trace_call_out(pid_t child)
{
    long call_nr = ptrace(PTRACE_PEEKUSER, child, (void*)(ORIG_EAX * 4), 0);
    long eax = ptrace(PTRACE_PEEKUSER, child, (void*)(EAX * 4), 0);

    void* src_msg = (void*)ptrace(PTRACE_PEEKUSER, child, (void*)(EBX * 4), 0);
    MESSAGE msg;
    copy_message_from(child, src_msg, &msg);

    switch (call_nr) {
    case NR_GETINFO:
        printf(" = 0x%lx\n", ptrace(PTRACE_PEEKDATA, child, msg.BUF, 0));
        return;
    case NR_SENDREC:
        trace_sendrec_out(child, &msg);
        return;
    }

    printf(" = %ld\n", eax);
}

static void do_trace(pid_t child, int s)
{
    int status = s;

    int call_in = 0;

    while (1) {
        if (WIFEXITED(status)) break;

        ptrace(PTRACE_SYSCALL, child, 0, 0);

        int waitchild = wait(&status);
        if (waitchild != child) continue;
        if (WIFEXITED(status)) break;

        int stopsig = WSTOPSIG(status);
        switch (stopsig) {
        case SIGTRAP:
            call_in = ~call_in;
            if (call_in)
                trace_call_in(child);
            else
                trace_call_out(child);
            break;
        }
    }

    printf("+++ exited with %d +++\n", WEXITSTATUS(status));
}
