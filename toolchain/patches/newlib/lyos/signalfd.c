#include <errno.h>
#include <sys/signalfd.h>
#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/const.h>

int signalfd(int fd, const sigset_t* mask, int flags)
{
    MESSAGE msg;
    msg.type = SIGNALFD;
    msg.u.m3.m3i1 = fd;
    msg.u.m3.m3l1 = *mask;
    msg.u.m3.m3i2 = fd;

    __asm__ __volatile__("" ::: "memory");

    send_recv(BOTH, TASK_FS, &msg);

    if (msg.FD < 0) {
        errno = -msg.FD;
        return -1;
    }

    return msg.FD;
}
