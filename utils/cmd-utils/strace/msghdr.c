#define _GNU_SOURCE

#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>

#include "types.h"
#include "xlat.h"
#include "proto.h"

#include "xlat/msg_flags.h"
#include "xlat/socketlayers.h"
#include "xlat/scmvals.h"

typedef union {
    char* ptr;
    struct cmsghdr* cmsg;
} union_cmsghdr;

static void print_scm_creds(struct tcb* tcp, const void* cmsg_data,
                            unsigned int data_len)
{
    const struct ucred* ucred = cmsg_data;

    printf("{pid=%d, uid=%d, gid=%d}", ucred->pid, ucred->uid, ucred->pid);
}

static void print_scm_rights(struct tcb* tcp, const void* cmsg_data,
                             unsigned int data_len)
{
    const int* fds = cmsg_data;
    const unsigned int nfds = data_len / sizeof(*fds);
    unsigned int i;

    printf("[");

    for (i = 0; i < nfds; ++i) {
        if (i) printf(", ");
        if (i >= max_strlen) {
            printf("...");
            break;
        }
        printf("%d", fds[i]);
    }

    printf("]");
}

typedef void (*const cmsg_printer)(struct tcb*, const void*, unsigned int);

static const struct {
    const cmsg_printer printer;
    const unsigned int min_len;
} cmsg_socket_printers[] = {
    [SCM_CREDENTIALS] = {print_scm_creds, sizeof(struct ucred)},
    [SCM_RIGHTS] = {print_scm_rights, sizeof(int)},
};

static void print_cmsg_type_data(struct tcb* tcp, const int cmsg_level,
                                 const int cmsg_type, const void* cmsg_data,
                                 const unsigned int data_len)
{
    unsigned int utype = cmsg_type;

    switch (cmsg_level) {
    case SOL_SOCKET:
        print_flags(cmsg_type, &scmvals);

        if (utype < sizeof(cmsg_socket_printers) /
                        sizeof(cmsg_socket_printers[0]) &&
            cmsg_socket_printers[utype].printer &&
            data_len >= cmsg_socket_printers[utype].min_len) {
            printf(", cmsg_data=");
            cmsg_socket_printers[utype].printer(tcp, cmsg_data, data_len);
        }
        break;
    default:
        printf("%#x", cmsg_type);
    }
}

static void decode_msg_control(struct tcb* const tcp, const void* addr,
                               unsigned long in_control_len)
{
    if (!in_control_len) return;
    printf(", msg_control=");

    const unsigned int cmsg_size = sizeof(struct cmsghdr);
    unsigned int control_len = in_control_len;

    unsigned int buf_len = control_len;
    char* buf = buf_len < cmsg_size ? NULL : malloc(buf_len);
    if (!buf || !fetch_mem(tcp, addr, buf, buf_len)) {
        print_addr((uint64_t)addr);
        free(buf);
        return;
    }

    union_cmsghdr u = {.ptr = buf};

    printf("[");
    while (buf_len >= cmsg_size) {
        const unsigned long cmsg_len = u.cmsg->cmsg_len;
        const int cmsg_level = u.cmsg->cmsg_level;
        const int cmsg_type = u.cmsg->cmsg_type;

        if (u.ptr != buf) printf(", ");
        printf("{cmsg_len=%lu, cmsg_level=", cmsg_len);
        print_flags(cmsg_level, &socketlayers);
        printf(", cmsg_type=");

        unsigned long len = cmsg_len > buf_len ? buf_len : cmsg_len;

        print_cmsg_type_data(tcp, cmsg_level, cmsg_type,
                             (const void*)(u.ptr + cmsg_size),
                             len > cmsg_size ? len - cmsg_size : 0);
        printf("}");

        if (len < cmsg_size) {
            buf_len -= cmsg_size;
            break;
        }
        len = CMSG_ALIGN(len);
        if (len >= buf_len) {
            buf_len = 0;
            break;
        }
        u.ptr += len;
        buf_len -= len;
    }

    if (buf_len) {
        printf(", ...");
    } else if (control_len < in_control_len) {
        printf(", ...");
    }
    printf("]");
    free(buf);
}

void print_struct_msghdr(struct tcb* tcp, const struct msghdr* msg,
                         unsigned long data_size)
{
    int msg_namelen = msg->msg_namelen;

    printf("{msg_name=");
    decode_sockaddr(tcp, msg->msg_name, msg_namelen);

    printf(", msg_namelen=");
    printf("%d", msg_namelen);

    printf(", msg_iov=");
    print_iov_upto(tcp, msg->msg_iovlen, msg->msg_iov, data_size);
    printf(", msg_iovlen=");
    printf("%u", msg->msg_iovlen);

    decode_msg_control(tcp, msg->msg_control, msg->msg_controllen);
    printf(", msg_controllen=");
    printf("%u", msg->msg_controllen);

    printf(", msg_flags=");
    print_flags(msg->msg_flags, &msg_flags);
    printf("}");
}

static void decode_msghdr(struct tcb* tcp, const void* addr, size_t data_size)
{
    struct msghdr msg;

    if (addr && fetch_mem(tcp, addr, &msg, sizeof(msg)))
        print_struct_msghdr(tcp, &msg, data_size);
    else
        print_addr((uint64_t)addr);
}

int trace_sendmsg(struct tcb* tcp)
{
    MESSAGE* msg = &tcp->msg_in;

    printf("%d, ", msg->u.m_vfs_sendrecv.sock_fd);
    decode_msghdr(tcp, msg->u.m_vfs_sendrecv.buf, -1);
    printf(", ");
    print_flags(msg->u.m_vfs_sendrecv.flags, &msg_flags);

    return RVAL_DECODED | RVAL_SPECIAL;
}

int trace_recvmsg(struct tcb* tcp)
{
    MESSAGE* msg = &tcp->msg_in;

    if (entering(tcp)) {
        printf("%d, ", msg->u.m_vfs_sendrecv.sock_fd);
        return 0;
    } else {
        if (tcp->msg_out.u.m_vfs_sendrecv.status < 0)
            print_addr((uint64_t)msg->u.m_vfs_sendrecv.buf);
        else
            decode_msghdr(tcp, msg->u.m_vfs_sendrecv.buf,
                          tcp->msg_out.u.m_vfs_sendrecv.status);
    }

    printf(", ");
    print_flags(msg->u.m_vfs_sendrecv.flags, &msg_flags);

    return RVAL_DECODED | RVAL_SPECIAL;
}
