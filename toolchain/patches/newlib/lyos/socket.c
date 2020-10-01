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

int bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen)
{
    MESSAGE msg;

    memset(&msg, 0, sizeof(msg));
    msg.type = BIND;
    msg.u.m_vfs_bindconn.sock_fd = sockfd;
    msg.u.m_vfs_bindconn.addr = (void*)addr;
    msg.u.m_vfs_bindconn.addr_len = addrlen;

    __asm__ __volatile__("" ::: "memory");

    send_recv(BOTH, TASK_FS, &msg);

    if (msg.RETVAL) {
        errno = msg.RETVAL;
        return -1;
    }

    return 0;
}

int listen(int sockfd, int backlog)
{
    MESSAGE msg;

    memset(&msg, 0, sizeof(msg));
    msg.type = LISTEN;
    msg.u.m_vfs_listen.sock_fd = sockfd;
    msg.u.m_vfs_listen.backlog = backlog;

    __asm__ __volatile__("" ::: "memory");

    send_recv(BOTH, TASK_FS, &msg);

    if (msg.RETVAL) {
        errno = msg.RETVAL;
        return -1;
    }

    return 0;
}

int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen)
{
    MESSAGE msg;

    memset(&msg, 0, sizeof(msg));
    msg.type = CONNECT;
    msg.u.m_vfs_bindconn.sock_fd = sockfd;
    msg.u.m_vfs_bindconn.addr = (void*)addr;
    msg.u.m_vfs_bindconn.addr_len = addrlen;

    __asm__ __volatile__("" ::: "memory");

    send_recv(BOTH, TASK_FS, &msg);

    if (msg.RETVAL) {
        errno = msg.RETVAL;
        return -1;
    }

    return 0;
}

int accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen)
{
    MESSAGE msg;

    if (addr && !addrlen) {
        errno = -EFAULT;
        return -1;
    }

    memset(&msg, 0, sizeof(msg));
    msg.type = ACCEPT;
    msg.u.m_vfs_bindconn.sock_fd = sockfd;
    msg.u.m_vfs_bindconn.addr = (void*)addr;
    msg.u.m_vfs_bindconn.addr_len = addrlen ? *addrlen : 0;

    __asm__ __volatile__("" ::: "memory");

    send_recv(BOTH, TASK_FS, &msg);

    if (msg.FD < 0) {
        errno = -msg.FD;
        return -1;
    }

    if (addrlen) {
        *addrlen = msg.CNT;
    }

    return msg.FD;
}
