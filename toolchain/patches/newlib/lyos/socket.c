#include <errno.h>
#include <sys/socket.h>
#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/const.h>
#include <string.h>

int socket(int domain, int type, int protocol)
{
    MESSAGE msg;

    memset(&msg, 0, sizeof(msg));
    msg.type = SOCKET;
    msg.u.m_vfs_socket.domain = domain;
    msg.u.m_vfs_socket.type = type;
    msg.u.m_vfs_socket.protocol = protocol;

    __asm__ __volatile__("" ::: "memory");

    send_recv(BOTH, TASK_FS, &msg);

    if (msg.FD < 0) {
        errno = -msg.FD;
        return -1;
    }

    return msg.FD;
}
