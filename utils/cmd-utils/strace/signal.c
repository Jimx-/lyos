#include <stdio.h>
#include <signal.h>
#include <sys/signalfd.h>

#include "types.h"
#include "proto.h"

#include "xlat/sigprocmaskcmds.h"
#include "xlat/sfd_flags.h"

static const char* signalent[] = {
#include "signalent.h"
};
static const int nsignals = sizeof(signalent) / sizeof(signalent[0]);

static inline const char* signame(int sig)
{
    if (sig > 0 && sig < nsignals) return signalent[sig];
    return NULL;
}

const char* sprint_signame(int sig)
{
    const char* str = signame(sig);
    if (str) return str;

    static char buf[sizeof(sig) * 3 + 2];
    sprintf(buf, "%d", sig);

    return buf;
}

const char* sprint_sigmask(sigset_t mask)
{
    static char outstr[128 + 8 * (sizeof(mask) * 8 * 2 / 3)];
    char* s = outstr;
    int i;
    char sep;

    sep = '[';
    for (i = 1; i < sizeof(mask) << 3; i++) {
        if (!sigismember(&mask, i)) continue;

        *s++ = sep;

        if (i < nsignals)
            s += sprintf(s, signalent[i] + 3);
        else
            s += sprintf(s, "%u", i);

        sep = ' ';
    }

    if (sep == '[') *s++ = sep;
    *s++ = ']';
    *s = '\0';
    return outstr;
}

void print_sigmask(sigset_t mask) { printf("%s", sprint_sigmask(mask)); }

int trace_sigprocmask(struct tcb* tcp)
{
    if (entering(tcp)) {
        print_flags(tcp->msg_in.HOW, &sigprocmaskcmds);
        printf(", ");
        print_sigmask(tcp->msg_in.MASK);
        printf(", ");
    } else {
        print_sigmask(tcp->msg_out.MASK);
    }

    return 0;
}

int trace_kill(struct tcb* tcp)
{
    printf("%d, ", tcp->msg_in.PID);
    printf("%s", sprint_signame(tcp->msg_in.SIGNR));

    return RVAL_DECODED;
}

int trace_signalfd(struct tcb* tcp)
{
    MESSAGE* msg = &tcp->msg_in;

    printf("%d, ", msg->u.m_vfs_signalfd.fd);
    print_sigmask(msg->u.m_vfs_signalfd.mask);
    printf(", ");
    print_flags(msg->u.m_vfs_signalfd.flags, &sfd_flags);

    return RVAL_DECODED | RVAL_FD;
}
