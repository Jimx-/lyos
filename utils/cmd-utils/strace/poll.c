#include <stdio.h>
#include <poll.h>
#include <string.h>

#include "types.h"
#include "xlat.h"
#include "proto.h"

#include "xlat/pollflags.h"

static int print_pollfd(struct tcb* tcp, void* elem_buf, size_t elem_size,
                        void* data)
{
    const struct pollfd* fds = elem_buf;

    printf("{fd=");
    printf("%d", fds->fd);
    if (fds->fd >= 0) {
        printf(", events=");
        print_flags((unsigned short)fds->events, &pollflags);
    }
    printf("}");

    return 1;
}

static void decode_poll_entering(struct tcb* tcp)
{
    void* fds = tcp->msg_in.u.m_vfs_poll.fds;
    int nfds = tcp->msg_in.u.m_vfs_poll.nfds;
    struct pollfd pfd;

    print_array(tcp, fds, nfds, &pfd, sizeof(pfd), fetch_mem, print_pollfd,
                NULL);
    printf(", %u, ", nfds);
    printf("%d)", tcp->msg_in.u.m_vfs_poll.timeout_msecs);
}

static int decode_poll_exiting(struct tcb* tcp)
{

    struct pollfd pfd;
    void* start = tcp->msg_in.u.m_vfs_poll.fds;
    int nfds = tcp->msg_in.u.m_vfs_poll.nfds;
    size_t size = nfds * sizeof(pfd);
    void *end = start + size, *cur;
    MESSAGE* msg = &tcp->msg_out;
    int printed;

    static char outstr[1024];
    char *outptr, *end_outstr = outstr + sizeof(outstr);

    if (msg->RETVAL < 0) return 0;

    if (msg->RETVAL == 0) {
        tcp->aux_str = "Timeout";
        return RVAL_STR;
    }

    if (!start || !nfds || size / sizeof(pfd) != nfds || end < start) return 0;

    outptr = outstr;

    for (printed = 0, cur = start; cur < end; cur += sizeof(pfd)) {
        if (!fetch_mem(tcp, cur, &pfd, sizeof(pfd))) {
            if (outptr == outstr)
                *outptr++ = '[';
            else {
                *outptr++ = ',';
                *outptr++ = ' ';
            }
            sprintf(outptr, "%lx", cur);
            break;
        }

        if (!pfd.revents) continue;
        if (outptr == outstr)
            *outptr++ = '[';
        else {
            *outptr++ = ',';
            *outptr++ = ' ';
        }

        static const char fmt[] = "{fd=%d, revents=";
        char fdstr[sizeof(fmt) + sizeof(int) * 3];
        sprintf(fdstr, fmt, pfd.fd);

        const char* flagstr =
            sprint_flags((unsigned short)pfd.revents, &pollflags);

        if (outptr + strlen(fdstr) + strlen(flagstr) + 1 >=
            end_outstr - (2 + 2 * sizeof(long) + sizeof(", ], ..."))) {
            outptr = stpcpy(outptr, "...");
            break;
        }
        outptr = stpcpy(outptr, fdstr);
        outptr = stpcpy(outptr, flagstr);
        *outptr++ = '}';
        ++printed;
    }

    if (outptr != outstr) *outptr++ = ']';

    *outptr = '\0';

    if (outptr == outstr) return 0;

    tcp->aux_str = outstr;
    return RVAL_STR;
}

int trace_poll(struct tcb* tcp)
{
    if (entering(tcp)) {
        printf("poll(");
        decode_poll_entering(tcp);

        return 0;
    } else
        return decode_poll_exiting(tcp);
}
