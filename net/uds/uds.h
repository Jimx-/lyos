#ifndef _UDS_UDS_H_
#define _UDS_UDS_H_

#include <sys/types.h>
#include <sys/un.h>
#include <sys/syslimits.h>

#include <libsockdriver/libsockdriver.h>

#define SCM_FD_MAX 253

struct scm_fd_list {
    size_t count;
    size_t max;
    endpoint_t owner;
    int fds[SCM_FD_MAX];
};

struct uds_skb_params {
    pid_t pid;
    uid_t uid;
    gid_t gid;
    struct scm_fd_list* fd_list;

    size_t consumed;
};

#define UDSCB(skb) (*(struct uds_skb_params*)&((skb)->cb))

struct udssock {
    struct sock sock;
    struct list_head hash;
    struct list_head list;
    struct udssock* conn;
    struct udssock* link;

    int backlog;

    int flags;
    char path[PATH_MAX];
    size_t path_len;
    dev_t dev;
    ino_t ino;

    char* buf;
    size_t len;
    size_t tail;
    size_t last;

    struct list_head queue;
    size_t nr_queued;

    struct uds_skb_params cred;
};

#define UDSF_CONNECTED 0x01
#define UDSF_CONNWAIT  0x02
#define UDSF_PASSCRED  0x04

#define uds_has_conn(uds) ((uds)->conn != NULL)
#define uds_has_link(uds) ((uds)->link != NULL)
#define uds_is_connecting(uds)                                \
    (uds_has_link(uds) && !((uds)->flags & UDSF_CONNECTED) && \
     uds_get_type(uds) != SOCK_DGRAM)
#define uds_is_connected(uds) \
    (((uds)->flags & UDSF_CONNECTED) && uds_has_conn(uds))
#define uds_is_disconnected(uds) \
    (((uds)->flags & UDSF_CONNECTED) && !uds_has_conn(uds))
#define uds_is_bound(uds)          ((uds)->path_len != 0)
#define uds_is_listening(uds)      sock_is_listening(&(uds)->sock)
#define uds_is_shutdown(uds, mask) sock_is_shutdown(&(uds)->sock, mask)

#define uds_get_type(uds) sock_type(&(uds)->sock)

static inline struct udssock* to_udssock(struct sock* sock)
{
    return list_entry(sock, struct udssock, sock);
}

int uds_lookup(struct udssock* uds, const struct sockaddr* addr, size_t addrlen,
               endpoint_t user_endpt, struct udssock** peerp);
void uds_make_addr(const char* path, size_t path_len, struct sockaddr* addr,
                   socklen_t* addr_len);

int uds_io_alloc(struct udssock* uds);
void uds_io_free(struct udssock* uds);
ssize_t uds_send(struct sock* sock, struct iov_grant_iter* iter, size_t len,
                 const struct sockdriver_data* ctl, socklen_t ctl_len,
                 const struct sockaddr* addr, socklen_t addr_len,
                 endpoint_t user_endpt, int flags);
ssize_t uds_recv(struct sock* sock, struct iov_grant_iter* iter, size_t len,
                 const struct sockdriver_data* ctl, socklen_t* ctl_len,
                 struct sockaddr* addr, socklen_t* addr_len,
                 endpoint_t user_endpt, int flags, int* rflags);
__poll_t uds_poll(struct sock* sock);

#endif
