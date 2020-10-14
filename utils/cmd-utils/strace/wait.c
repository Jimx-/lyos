#include <stdio.h>
#include <sys/wait.h>

#include "types.h"
#include "xlat.h"
#include "proto.h"

#include "xlat/wait4_options.h"

static int print_status(int status)
{
    int exited = 0;

    if (WIFSTOPPED(status)) {
        int sig = WSTOPSIG(status);
        printf("[{WIFSTOPPED(s) && WSTOPSIG(s) == %s%s}",
               sprint_signame(sig & 0x7f), sig & 0x80 ? " | 0x80" : "");
        status &= ~W_STOPCODE(sig);
    } else if (WIFSIGNALED(status)) {
        printf("[{WIFSIGNALED(s) && WTERMSIG(s) == %s}",
               sprint_signame(WTERMSIG(status)));
        status &= ~(W_EXITCODE(0, WTERMSIG(status)) | WCOREFLAG);
    } else if (WIFEXITED(status)) {
        printf("[{WIFEXITED(s) && WEXITSTATUS(s) == %d}", WEXITSTATUS(status));
        exited = 1;
        status &= ~W_EXITCODE(WEXITSTATUS(status), 0);
    } else {
        printf("[%#x]", status);
        return 0;
    }

    printf("]");

    return exited;
}

int trace_waitpid(struct tcb* tcp)
{
    if (entering(tcp)) {
        printf("%d, ", tcp->msg_in.PID);
    } else {
        if (tcp->msg_out.PID > 0)
            print_status(tcp->msg_out.STATUS);
        else
            print_addr(0);

        printf(", ");
        print_flags(tcp->msg_in.OPTIONS, &wait4_options);
    }

    return 0;
}
