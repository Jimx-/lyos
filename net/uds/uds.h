#ifndef _UDS_UDS_H_
#define _UDS_UDS_H_

#include <sys/types.h>
#include <sys/un.h>
#include <libsockdriver/libsockdriver.h>

struct udscred {
    pid_t pid;
    uid_t uid;
    gid_t gid;
};

struct udssock {
    struct sockdriver_sock sock;
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
    int err;

    struct list_head queue;
    size_t nr_queued;

    struct udscred cred;
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
#define uds_is_listening(uds)      sockdriver_is_listening(&(uds)->sock)
#define uds_is_shutdown(uds, mask) sockdriver_is_shutdown(&(uds)->sock, mask)

#define uds_get_type(uds) sockdriver_get_type(&(uds)->sock)

#endif
