#define _GNU_SOURCE
#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/const.h>
#include <errno.h>
#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <lyos/mgrant.h>
#include <lyos/sysutils.h>
#include <poll.h>
#include <lyos/scm.h>

#include "libsockdriver.h"
#include "proto.h"

#define SCM_CTL_BUF_MAX 4096

static inline void scm_set_cred(struct scm_data* scm,
                                const struct scm_creds* creds)
{
    scm->creds = *creds;
}

void scm_destroy(struct scm_data* scm)
{
    struct scm_fd_list* fdl = scm->fd_list;
    int i;

    scm->creds.pid = scm->creds.uid = scm->creds.gid = -1;

    if (fdl) {
        scm->fd_list = NULL;

        if (fdl->owner == SELF)
            for (i = 0; i < fdl->count; i++) {
                close(fdl->fds[i]);
            }

        free(fdl);
    }
}

static int scm_copy_fds(struct cmsghdr* cmsg, struct scm_fd_list** fdlp,
                        endpoint_t user_endpt)
{
    int *fds = (int*)CMSG_DATA(cmsg), *fdp;
    struct scm_fd_list* fdl = *fdlp;
    int num, i;

    num = (cmsg->cmsg_len - sizeof(struct cmsghdr)) / sizeof(int);

    if (num <= 0) return 0;
    if (num > SCM_FD_MAX) return -EINVAL;

    if (!fdl) {
        fdl = malloc(sizeof(*fdl));
        if (!fdl) return -ENOMEM;
        *fdlp = fdl;
        memset(fdl, 0, sizeof(*fdl));
        fdl->count = 0;
        fdl->max = SCM_FD_MAX;
        fdl->owner = user_endpt;
    } else
        assert(fdl->owner == user_endpt);

    fdp = &fdl->fds[fdl->count];

    if (fdl->count + num > fdl->max) return -EINVAL;

    for (i = 0; i < num; i++) {
        *fdp++ = *fds++;
        fdl->count++;
    }

    return num;
}

int scm_send(struct sock* sock, struct scm_creds* creds,
             const struct sockdriver_data* ctl_data, socklen_t ctl_len,
             endpoint_t user_endpt, struct scm_data* scm, int force_creds)
{
    struct msghdr msghdr;
    struct cmsghdr* cmsg;
    static char ctl_buf[SCM_CTL_BUF_MAX];
    socklen_t left;
    int retval;

    memset(scm, 0, sizeof(*scm));
    scm->creds.pid = -1;
    scm->creds.uid = -1;
    scm->creds.gid = -1;

    if (force_creds) scm_set_cred(scm, creds);

    if (ctl_len <= 0) return 0;
    if (ctl_len > sizeof(ctl_buf)) return -ENOBUFS;

    if ((retval = sockdriver_copyin(ctl_data, 0, ctl_buf, ctl_len)) != OK)
        return -retval;

    if (ctl_len < sizeof(ctl_buf))
        memset(&ctl_buf[ctl_len], 0, sizeof(ctl_buf) - ctl_len);

    memset(&msghdr, 0, sizeof(msghdr));
    msghdr.msg_control = ctl_buf;
    msghdr.msg_controllen = ctl_len;

    for (cmsg = CMSG_FIRSTHDR(&msghdr); cmsg != NULL;
         cmsg = CMSG_NXTHDR(&msghdr, cmsg)) {
        retval = -EINVAL;

        assert((socklen_t)((char*)cmsg - ctl_buf) <= ctl_len);
        left = ctl_len - (socklen_t)((char*)cmsg - ctl_buf);
        assert(left >= CMSG_LEN(0));

        if (cmsg->cmsg_len < CMSG_LEN(0) || cmsg->cmsg_len > left) goto error;

        if (cmsg->cmsg_level != SOL_SOCKET) continue;

        switch (cmsg->cmsg_type) {
        case SCM_RIGHTS:
            if (sock_domain(sock) != PF_UNIX) goto error;

            /* we do not actually copy the fds to our file table yet so that we
             * do not need to close the fds in case the send operation fails */
            retval = scm_copy_fds(cmsg, &scm->fd_list, user_endpt);
            if (retval < 0) goto error;
            break;
        case SCM_CREDENTIALS:
            break;
        default:
            goto error;
        }

        retval = 0;
    }

    if (scm->fd_list && !scm->fd_list->count) {
        free(scm->fd_list);
        scm->fd_list = NULL;
    }

    return 0;

error:
    scm_destroy(scm);
    return retval;
}

int scm_send_fds(struct scm_data* scm)
{
    struct scm_fd_list* fdl = scm->fd_list;
    endpoint_t user_endpt;
    int i, retval;

    if (!fdl) return 0;

    user_endpt = fdl->owner;
    assert(user_endpt != NO_TASK);
    assert(user_endpt != SELF);

    for (i = 0; i < fdl->count; i++) {
        if ((retval = copyfd(user_endpt, fdl->fds[i], COPYFD_FROM)) < 0) {
            for (i--; i >= 0; i--) {
                close(fdl->fds[i]);
                return retval;
            }
        }

        fdl->fds[i] = retval;
    }

    fdl->owner = SELF;
    return 0;
}

static int scm_recv_cred(struct sock* sock,
                         const struct sockdriver_data* ctl_data,
                         socklen_t ctl_len, socklen_t ctl_off,
                         endpoint_t user_endpt, struct scm_data* scm, int flags)
{
    struct msghdr msghdr;
    struct cmsghdr* cmsg;
    static char ctl_buf[SCM_CTL_BUF_MAX];
    socklen_t chunk_len, chunk_space;
    struct ucred* ucred;
    int retval;

    chunk_len = CMSG_LEN(sizeof(*ucred));
    chunk_space = CMSG_SPACE(sizeof(*ucred));

    if (chunk_len > ctl_len) return 0;
    if (chunk_space > ctl_len) chunk_space = ctl_len;

    memset(&msghdr, 0, sizeof(msghdr));
    msghdr.msg_control = ctl_buf;
    msghdr.msg_controllen = sizeof(ctl_buf);

    memset(ctl_buf, 0, chunk_len);
    cmsg = CMSG_FIRSTHDR(&msghdr);
    cmsg->cmsg_len = chunk_len;
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_CREDENTIALS;

    ucred = (struct ucred*)CMSG_DATA(cmsg);
    ucred->pid = scm->creds.pid;
    ucred->uid = scm->creds.uid;
    ucred->gid = scm->creds.gid;

    if ((retval = sockdriver_copyout(ctl_data, ctl_off, ctl_buf, chunk_len)) !=
        OK)
        retval = -retval;

    return chunk_space;
}

static int scm_recv_fds(struct sock* sock,
                        const struct sockdriver_data* ctl_data,
                        socklen_t ctl_len, socklen_t ctl_off,
                        endpoint_t user_endpt, struct scm_data* scm, int flags)
{
    struct msghdr msghdr;
    struct cmsghdr* cmsg;
    static char ctl_buf[SCM_CTL_BUF_MAX];
    struct scm_fd_list* fdl = scm->fd_list;
    socklen_t chunk_len, chunk_space;
    struct scm_data tmp;
    int fd, nfds, i, how, retval;

    scm->fd_list = NULL;

    assert(fdl);
    assert(fdl->owner == SELF);
    nfds = fdl->count;
    assert(nfds > 0);

    memset(&tmp, 0, sizeof(tmp));
    tmp.fd_list = fdl;

    chunk_len = CMSG_LEN(sizeof(int) * nfds);
    chunk_space = CMSG_SPACE(sizeof(int) * nfds);

    if (chunk_len > ctl_len) return 0;
    if (chunk_space > ctl_len) chunk_space = ctl_len;

    memset(&msghdr, 0, sizeof(msghdr));
    msghdr.msg_control = ctl_buf;
    msghdr.msg_controllen = sizeof(ctl_buf);

    memset(ctl_buf, 0, chunk_len);
    cmsg = CMSG_FIRSTHDR(&msghdr);
    cmsg->cmsg_len = chunk_len;
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;

    retval = 0;

    for (i = 0; i < nfds; i++) {
        how = COPYFD_TO;
        if (flags & MSG_CMSG_CLOEXEC) how |= COPYFD_CLOEXEC;

        if ((retval = copyfd(user_endpt, fdl->fds[i], how)) < 0) break;

        fd = retval;
        memcpy((int*)CMSG_DATA(cmsg) + i, &fd, sizeof(int));
    }

    scm_destroy(&tmp);

    if (retval >= 0)
        if ((retval = sockdriver_copyout(ctl_data, ctl_off, ctl_buf,
                                         chunk_len)) != OK)
            retval = -retval;

    if (retval < 0) {
        while (i--) {
            memcpy(&fd, (int*)CMSG_DATA(cmsg) + i, sizeof(int));
            copyfd(user_endpt, fd, COPYFD_CLOSE);
        }

        return retval;
    }

    return chunk_space;
}

int scm_recv(struct sock* sock, const struct sockdriver_data* ctl_data,
             socklen_t ctl_len, endpoint_t user_endpt, struct scm_data* scm,
             int flags)
{
    int retval;
    socklen_t ctl_off = 0;

    if (ctl_len == 0) {
        scm_destroy(scm);
        return 0;
    }

    if (sock->flags & SFL_PASSCRED) {
        retval = scm_recv_cred(sock, ctl_data, ctl_len, ctl_off, user_endpt,
                               scm, flags);

        if (retval < 0 && ctl_off == 0) return retval;

        if (retval > 0) {
            ctl_len -= retval;
            ctl_off += retval;
        }
    }

    if (!scm->fd_list) return ctl_off;

    retval =
        scm_recv_fds(sock, ctl_data, ctl_len, ctl_off, user_endpt, scm, flags);

    if (retval < 0 && ctl_off == 0) return retval;

    if (retval > 0) {
        ctl_len -= retval;
        ctl_off += retval;
    }

    return ctl_off;
}
