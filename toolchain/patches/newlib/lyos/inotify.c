#include <sys/inotify.h>
#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/const.h>
#include <string.h>
#include <errno.h>

int inotify_init1(int flags)
{
    MESSAGE msg;

    memset(&msg, 0, sizeof(msg));
    msg.type = INOTIFY_INIT1;
    msg.u.m_vfs_inotify.flags = flags;

    __asm__ __volatile__("" ::: "memory");

    send_recv(BOTH, TASK_FS, &msg);

    if (msg.FD < 0) {
        errno = -msg.FD;
        return -1;
    }

    return msg.FD;
}

int inotify_init(void) { return inotify_init1(0); }

int inotify_add_watch(int fd, const char* pathname, uint32_t mask)
{
    MESSAGE msg;

    memset(&msg, 0, sizeof(msg));
    msg.type = INOTIFY_ADD_WATCH;
    msg.u.m_vfs_inotify.fd = fd;
    msg.u.m_vfs_inotify.pathname = pathname;
    msg.u.m_vfs_inotify.name_len = strlen(pathname);
    msg.u.m_vfs_inotify.mask = mask;

    __asm__ __volatile__("" ::: "memory");

    send_recv(BOTH, TASK_FS, &msg);

    if (msg.RETVAL < 0) {
        errno = -msg.RETVAL;
        return -1;
    }

    return msg.RETVAL;
}

int inotify_rm_watch(int fd, int wd)
{
    MESSAGE msg;

    memset(&msg, 0, sizeof(msg));
    msg.type = INOTIFY_RM_WATCH;
    msg.u.m_vfs_inotify.fd = fd;
    msg.u.m_vfs_inotify.wd = wd;

    __asm__ __volatile__("" ::: "memory");

    send_recv(BOTH, TASK_FS, &msg);

    if (msg.RETVAL > 0) {
        errno = msg.RETVAL;
        return -1;
    }

    return 0;
}
