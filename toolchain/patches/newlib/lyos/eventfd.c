#include <errno.h>
#include <sys/eventfd.h>
#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/const.h>

int eventfd(unsigned int count, int flags)
{
    MESSAGE msg;
    msg.type = EVENTFD;
    msg.CNT = count;
    msg.FLAGS = flags;

    __asm__ __volatile__("" ::: "memory");

    send_recv(BOTH, TASK_FS, &msg);

    if (msg.FD < 0) {
        errno = -msg.FD;
        return -1;
    }

    return msg.FD;
}
