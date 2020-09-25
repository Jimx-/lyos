#include <errno.h>
#include <sys/timerfd.h>
#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/const.h>

int timerfd_create(clockid_t clock_id, int flags)
{
    MESSAGE msg;
    msg.type = TIMERFD_CREATE;
    msg.u.m_vfs_timerfd.clock_id = clock_id;
    msg.u.m_vfs_timerfd.flags = flags;

    __asm__ __volatile__("" ::: "memory");

    send_recv(BOTH, TASK_FS, &msg);

    if (msg.FD < 0) {
        errno = -msg.FD;
        return -1;
    }

    return msg.FD;
}

int timerfd_settime(int ufd, int flags, const struct itimerspec* utmr,
                    struct itimerspec* otmr)
{
    MESSAGE msg;
    msg.type = TIMERFD_SETTIME;
    msg.u.m_vfs_timerfd.fd = ufd;
    msg.u.m_vfs_timerfd.flags = flags;
    msg.u.m_vfs_timerfd.new_value = utmr;
    msg.u.m_vfs_timerfd.old_value = otmr;

    __asm__ __volatile__("" ::: "memory");

    send_recv(BOTH, TASK_FS, &msg);

    if (msg.RETVAL) {
        errno = msg.RETVAL;
        return -1;
    }

    return 0;
}

int timerfd_gettime(int ufd, struct itimerspec* otmr)
{
    MESSAGE msg;
    msg.type = TIMERFD_GETTIME;
    msg.u.m_vfs_timerfd.fd = ufd;
    msg.u.m_vfs_timerfd.old_value = otmr;

    __asm__ __volatile__("" ::: "memory");

    send_recv(BOTH, TASK_FS, &msg);

    if (msg.RETVAL) {
        errno = msg.RETVAL;
        return -1;
    }

    return 0;
}
