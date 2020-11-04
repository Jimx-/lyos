#ifndef _LYOS_SCM_H
#define _LYOS_SCM_H

#include <sys/types.h>
#include <lyos/types.h>

#include <libsockdriver/libsockdriver.h>

#define SCM_FD_MAX 253

struct scm_creds {
    pid_t pid;
    uid_t uid;
    gid_t gid;
};

struct scm_fd_list {
    size_t count;
    size_t max;
    endpoint_t owner;
    int fds[SCM_FD_MAX];
};

struct scm_data {
    struct scm_creds creds;
    struct scm_fd_list* fd_list;
};

int scm_send_fds(struct scm_data* scm);

int scm_send(struct sock* sock, struct scm_creds* creds,
             const struct sockdriver_data* ctl_data, socklen_t ctl_len,
             endpoint_t user_endpt, struct scm_data* scm, int force_creds);
int scm_recv(struct sock* sock, const struct sockdriver_data* ctl_data,
             socklen_t ctl_len, endpoint_t user_endpt, struct scm_data* scm,
             int flags);

void scm_destroy(struct scm_data* scm);

#endif
