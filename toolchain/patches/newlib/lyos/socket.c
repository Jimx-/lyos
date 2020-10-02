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

ssize_t send(int sock, const void* msg, size_t len, int flags)
{
    return sendto(sock, msg, len, flags, NULL, 0);
}

ssize_t recv(int sock, void* buf, size_t len, int flags)
{
    return recvfrom(sock, buf, len, flags, NULL, NULL);
}

ssize_t sendto(int sock, const void* buf, size_t len, int flags,
               const struct sockaddr* addr, socklen_t addr_len)
{
    MESSAGE msg;
    int retval;

    memset(&msg, 0, sizeof(msg));
    msg.type = SENDTO;
    msg.u.m_vfs_sendrecv.sock_fd = sock;
    msg.u.m_vfs_sendrecv.buf = (void*)buf;
    msg.u.m_vfs_sendrecv.len = len;
    msg.u.m_vfs_sendrecv.flags = flags;
    msg.u.m_vfs_sendrecv.addr = (void*)addr;
    msg.u.m_vfs_sendrecv.addr_len = addr_len;

    __asm__ __volatile__("" ::: "memory");

    send_recv(BOTH, TASK_FS, &msg);

    retval = msg.u.m_vfs_sendrecv.status;

    if (retval < 0) {
        errno = -retval;
        return -1;
    }

    return retval;
}

ssize_t recvfrom(int sock, void* buf, size_t len, int flags,
                 struct sockaddr* addr, socklen_t* addr_len)
{
    MESSAGE msg;
    int retval;

    memset(&msg, 0, sizeof(msg));
    msg.type = RECVFROM;
    msg.u.m_vfs_sendrecv.sock_fd = sock;
    msg.u.m_vfs_sendrecv.buf = (void*)buf;
    msg.u.m_vfs_sendrecv.len = len;
    msg.u.m_vfs_sendrecv.flags = flags;
    msg.u.m_vfs_sendrecv.addr = (void*)addr;
    msg.u.m_vfs_sendrecv.addr_len = addr_len != NULL ? *addr_len : 0;

    __asm__ __volatile__("" ::: "memory");

    send_recv(BOTH, TASK_FS, &msg);

    retval = msg.u.m_vfs_sendrecv.status;

    if (retval < 0) {
        errno = -retval;
        return -1;
    }

    if (addr_len) *addr_len = msg.u.m_vfs_sendrecv.addr_len;

    return retval;
}

ssize_t sendmsg(int sock, const struct msghdr* msg, int flags)
{
    MESSAGE m;
    int retval;

    memset(&m, 0, sizeof(m));
    m.type = SENDMSG;
    m.u.m_vfs_sendrecv.sock_fd = sock;
    m.u.m_vfs_sendrecv.buf = (void*)msg;
    m.u.m_vfs_sendrecv.flags = flags;

    __asm__ __volatile__("" ::: "memory");

    send_recv(BOTH, TASK_FS, &m);

    retval = m.u.m_vfs_sendrecv.status;

    if (retval < 0) {
        errno = -retval;
        return -1;
    }

    return retval;
}

ssize_t recvmsg(int sock, struct msghdr* msg, int flags)
{
    MESSAGE m;
    int retval;

    memset(&m, 0, sizeof(m));
    m.type = RECVMSG;
    m.u.m_vfs_sendrecv.sock_fd = sock;
    m.u.m_vfs_sendrecv.buf = (void*)msg;
    m.u.m_vfs_sendrecv.flags = flags;

    __asm__ __volatile__("" ::: "memory");

    send_recv(BOTH, TASK_FS, &m);

    retval = m.u.m_vfs_sendrecv.status;

    if (retval < 0) {
        errno = -retval;
        return -1;
    }

    return retval;
}
