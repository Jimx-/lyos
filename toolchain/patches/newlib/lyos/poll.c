#include <errno.h>
#include <poll.h>
#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/const.h>

int poll(struct pollfd* fds, nfds_t nfds, int timeout)
{
    MESSAGE msg;
    msg.type = POLL;
    msg.u.m_vfs_poll.fds = fds;
    msg.u.m_vfs_poll.nfds = (unsigned int)nfds;
    msg.u.m_vfs_poll.timeout_msecs = timeout;

    __asm__ __volatile__("" ::: "memory");

    send_recv(BOTH, TASK_FS, &msg);

    if (msg.RETVAL < 0) {
        errno = -msg.RETVAL;
        return -1;
    }

    return msg.RETVAL;
}
