#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

#include <lyos/const.h>
#include <lyos/type.h>

static void do_trace(pid_t child, int s);

int main(int argc, char* argv[])
{   
    if (argc == 1) return 0;

    pid_t child;
    child = fork();

    argv++;
    if(child == 0) {
        ptrace(PTRACE_TRACEME, 0, NULL, NULL);
        execve(argv[0], argv, NULL);
    }
    else {
        int status;
        wait(&status);
        do_trace(child, status);
    }
    return 0;
}

static int copy_message_from(pid_t pid, void* src_msg, MESSAGE* dest_msg)
{
    long* src = src_msg;
    long* dest = dest_msg;

    while (src < (char*)src_msg + sizeof(MESSAGE)) {
        long data = ptrace(PTRACE_PEEKDATA, pid, src, 0);
        if (data == -1) return -1;
        *dest = data;
        src++;
        dest++;
    }

    return 0;
}

static char* copy_string(pid_t pid, char* string, int len)
{
    char* buf = malloc(len + 5);
    if (!buf) return NULL;

    long* src = string;
    long* dest = buf;

    while (src < string + len) {
        long data = ptrace(PTRACE_PEEKDATA, pid, src, 0);
        if (data == -1) return -1;
        *dest = data;
        src++;
        dest++;
    }

    buf[len] = '\0';
    return buf;
}

#define DEFAULT_BUF_LEN 32
static char* copy_char_buf(pid_t pid, char* cb, int len, int* truncated)
{
    *truncated = 0;
    if (len > DEFAULT_BUF_LEN) {
        *truncated = 1;
        len = DEFAULT_BUF_LEN;
    }

    char* buf = malloc(len + 1);
    if (!buf) return NULL;

    long* src = cb;
    long* dest = buf;

    while (src < cb + len) {
        long data = ptrace(PTRACE_PEEKDATA, pid, src, 0);
        if (data == -1) return -1;
        *dest = data;
        src++;
        dest++;
    }

    buf[len] = '\0';
    return buf;
}

static void trace_open_in(pid_t child, MESSAGE* msg)
{
    char* name = copy_string(child, msg->PATHNAME, msg->NAME_LEN);
    printf("open(\"%s\", %d)", name, msg->FLAGS);
    free(name);
}

static void trace_write_in(pid_t child, MESSAGE* msg)
{
    int t;
    char* buf = copy_char_buf(child, msg->BUF, msg->CNT, &t);
    printf("write(%d, \"%s\"%s, %d)", msg->FD, buf, t?"...":"", msg->CNT);
    free(buf);
}

static void trace_sendrec_in(pid_t child, MESSAGE* req_msg)
{
    MESSAGE msg;
    copy_message_from(child, req_msg->SR_MSG, &msg);

    int type = msg.type;
    switch (type) {
    case OPEN:
        trace_open_in(child, &msg);
        break;
    case CLOSE:
        printf("close(%d)", msg.FD);
        break;
    case READ:
        printf("read(%d, 0x%x, %d)", msg.FD, msg.BUF, msg.CNT);
        break;
    case WRITE:
        trace_write_in(child, &msg);
        break;
    case FSTAT:
        printf("fstat(%d, 0x%x)", msg.FD, msg.BUF);
        break;
    case BRK:
        printf("brk(0x%x)", msg.ADDR);
        break;
    case EXIT:
        printf("exit(%d) = ?\n", msg.STATUS);   /* exit has no return value */
        break;
    }
}

static void trace_sendrec_out(pid_t child, MESSAGE* req_msg)
{
    MESSAGE msg;
    copy_message_from(child, req_msg->SR_MSG, &msg);

    int type = msg.type;
    int retval = 0;

    switch (type) {
    case OPEN:
        retval = msg.FD;
        break;
    case READ:
    case WRITE:
        retval = msg.CNT;
        break;
    case CLOSE:
    case FSTAT:
    case BRK:
        retval = msg.RETVAL;
        break;
    }
    printf(" = %d\n", retval);
}

#define EBX         8
#define EAX         11
#define ORIG_EAX    18
static void trace_call_in(pid_t child)
{
    long call_nr = ptrace(PTRACE_PEEKUSER, child, ORIG_EAX*4, 0);

    void* src_msg = (void*) ptrace(PTRACE_PEEKUSER, child, EBX*4, 0);
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
}

static void trace_call_out(pid_t child)
{
    long call_nr = ptrace(PTRACE_PEEKUSER, child, ORIG_EAX*4, 0);
    long eax = ptrace(PTRACE_PEEKUSER, child, EAX*4, 0);

    void* src_msg = (void*) ptrace(PTRACE_PEEKUSER, child, EBX*4, 0);
    MESSAGE msg;
    copy_message_from(child, src_msg, &msg);
    
    switch (call_nr) {
    case NR_SENDREC:
        trace_sendrec_out(child, &msg);
        return;
    }

    printf(" = %d\n", eax);
}

static void do_trace(pid_t child, int s)
{
    int status = s;

    int call_in = 0;

    while (1) {
        if (WIFEXITED(status)) break;

        ptrace(PTRACE_SYSCALL, child, 0, 0);

        wait(&status);
        if (WIFEXITED(status)) break;

        int stopsig = WSTOPSIG(status);
        switch (stopsig) {
        case SIGTRAP:
            call_in = ~call_in;
            if (call_in) trace_call_in(child);
            else trace_call_out(child);
            break;
        }
    }

    printf("+++ exited with %d +++\n", WEXITSTATUS(status));
}
