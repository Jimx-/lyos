#include <errno.h>
#include <sys/signalfd.h>
#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/const.h>

int signalfd(int fd, const sigset_t* mask, int flags)
{
    MESSAGE msg;
    msg.type = SIGNALFD;
    msg.u.m_vfs_signalfd.fd = fd;
    msg.u.m_vfs_signalfd.mask = *mask;
    msg.u.m_vfs_signalfd.flags = flags;

    __asm__ __volatile__("" ::: "memory");

    send_recv(BOTH, TASK_FS, &msg);

    if (msg.FD < 0) {
        errno = -msg.FD;
        return -1;
    }

    return msg.FD;
}
