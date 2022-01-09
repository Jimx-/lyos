#define _GNU_SOURCE

#include <lyos/types.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/proc.h>
#include <lyos/global.h>
#include <lyos/proto.h>
#include <lyos/driver.h>
#include <unistd.h>
#include <errno.h>
#include <lyos/service.h>
#include <lyos/sysutils.h>
#include <lyos/vm.h>
#include <lyos/idr.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <lyos/eventpoll.h>

#include "uds.h"

static size_t uds_skb_len(const struct sk_buff* skb)
{
    return skb->data_len - UDSCB(skb).consumed;
}

int uds_io_alloc(struct udssock* uds) { return 0; }

void uds_io_free(struct udssock* uds) {}

static int uds_send_peer(struct udssock* uds, const struct sockaddr* addr,
                         socklen_t addr_len, endpoint_t user_endpt,
                         struct udssock** peerp)
{
    struct udssock* peer;
    int retval;

    if (uds_get_type(uds) == SOCK_DGRAM) {
        if (!uds_has_link(uds)) {

            if ((retval = uds_lookup(uds, addr, addr_len, user_endpt, &peer)) !=
                OK)
                return retval;

            if (uds_has_link(peer) && peer->link != uds) return EPERM;
        } else
            peer = uds->link;

        if (uds_is_shutdown(peer, SFL_SHUT_RD)) return ENOBUFS;
    } else {
        peer = uds->conn;
    }

    *peerp = peer;
    return 0;
}

static void uds_add_creds(struct sk_buff* skb, const struct udssock* uds,
                          const struct udssock* other, endpoint_t user_endpt)
{
    if (UDSCB(skb).pid > 0) return;
    if ((uds_get_flags(uds) | uds_get_flags(other)) & SFL_PASSCRED) {
        UDSCB(skb).pid =
            get_epinfo(user_endpt, &UDSCB(skb).uid, &UDSCB(skb).gid);
    }
}

static inline int uds_attach_fds(struct scm_data* scm, struct sk_buff* skb)
{
    UDSCB(skb).fd_list = scm->fd_list;
    scm->fd_list = NULL;

    return 0;
}

static inline int uds_detach_fds(struct scm_data* scm, struct sk_buff* skb)
{
    scm->fd_list = UDSCB(skb).fd_list;
    UDSCB(skb).fd_list = NULL;

    return 0;
}

static void uds_scm_destructor(struct sk_buff* skb)
{
    struct scm_data scm;

    memset(&scm, 0, sizeof(scm));
    if (UDSCB(skb).fd_list) uds_detach_fds(&scm, skb);

    scm_destroy(&scm);
}

static int uds_scm_to_skb(struct scm_data* scm, struct sk_buff* skb,
                          int send_fds)
{
    int retval = 0;

    UDSCB(skb).pid = scm->creds.pid;
    UDSCB(skb).uid = scm->creds.uid;
    UDSCB(skb).gid = scm->creds.gid;

    if (scm->fd_list && send_fds) {
        retval = scm_send_fds(scm);

        if (retval == OK) {
            uds_attach_fds(scm, skb);
        }
    }

    skb->destructor = uds_scm_destructor;
    return retval;
}

ssize_t uds_send(struct sock* sock, struct iov_grant_iter* iter, size_t len,
                 const struct sockdriver_data* ctl, socklen_t ctl_len,
                 const struct sockaddr* addr, socklen_t addr_len,
                 endpoint_t user_endpt, int flags, size_t min)
{
    struct udssock *uds = to_udssock(sock), *other;
    struct sk_buff* skb;
    size_t sent = 0;
    size_t size;
    struct scm_data scm;
    struct scm_creds creds;
    int fds_sent = FALSE;
    ssize_t retval;

    if (flags & MSG_OOB) return -EOPNOTSUPP;

    if (uds_get_type(uds) != SOCK_DGRAM) {
        if (!uds_is_connected(uds) && !uds_is_disconnected(uds))
            return -ENOTCONN;
        if (!uds_has_conn(uds)) return -EPIPE;
    }

    if ((retval = uds_send_peer(uds, addr, addr_len, user_endpt, &other)) != OK)
        return -retval;
    assert(other);

    if (uds_is_shutdown(uds, SFL_SHUT_WR)) return -EPIPE;

    creds.pid = uds->cred.pid;
    creds.uid = uds->cred.uid;
    creds.gid = uds->cred.gid;
    if ((retval = scm_send(&uds->sock, &creds, ctl, ctl_len, user_endpt, &scm,
                           FALSE)) != OK)
        return retval;

    while (sent < len) {
        size = len - sent;

        if ((retval = skb_alloc(sock, size, &skb)) != OK) {
            retval = -retval;
            goto out_err;
        }

        if ((retval = uds_scm_to_skb(&scm, skb, !fds_sent)) < 0) {
            skb_free(skb);
            goto out_err;
        }
        fds_sent = TRUE;

        if ((retval = skb_copy_from_iter(skb, 0, iter, size)) != 0) {
            skb_free(skb);
            retval = -retval;
            goto out_err;
        }

        if (uds_is_shutdown(other, SFL_SHUT_RD)) {
            retval = -EPIPE;
            goto out_free_skb;
        }

        uds_add_creds(skb, uds, other, user_endpt);
        skb_queue_tail(&sock_recvq(&other->sock), skb);
        sockdriver_fire(&other->sock, SEV_RECV);
        sent += size;
    }

    return sent;

out_free_skb:
    skb_free(skb);
out_err:
    scm_destroy(&scm);
    return sent ?: retval;
}

ssize_t uds_recv(struct sock* sock, struct iov_grant_iter* iter, size_t len,
                 const struct sockdriver_data* ctl, socklen_t* ctl_len,
                 struct sockaddr* addr, socklen_t* addr_len,
                 endpoint_t user_endpt, int flags, size_t min, int* rflags)
{
    struct udssock *uds = to_udssock(sock), *other;
    struct sk_buff* skb;
    struct scm_data scm;
    size_t target, chunk, copied = 0;
    socklen_t ctl_off;
    int retval;

    memset(&scm, 0, sizeof(scm));

    if (uds_get_type(uds) != SOCK_DGRAM) {
        if (!uds_is_connected(uds) && !uds_is_disconnected(uds))
            return -ENOTCONN;
    }

    target = sock_rcvlowat(&uds->sock, flags & MSG_WAITALL, len);

    do {
        skb = skb_peek(&uds->sock.recvq);

        if (!skb) {
            /* no data yet */
            if (copied >= target) break;

            /* receiving data should take precedence over socket errors */
            if ((retval = sock_error(&uds->sock)) != OK) {
                retval = -retval;
                break;
            }

            if (uds_is_shutdown(uds, SFL_SHUT_RD)) break;

            /* need to wait for data now */
            if (flags & MSG_DONTWAIT) {
                retval = -EAGAIN;
                break;
            }

            sockdriver_suspend(&uds->sock, SEV_RECV);
            continue;
        }

        /* copy address only once */
        if (addr) {
            other = to_udssock(skb->sock);
            uds_make_addr(other->path, other->path_len, addr, addr_len);
            addr = NULL;
        }

        chunk = min(uds_skb_len(skb), len);
        retval = skb_copy_to_iter(skb, UDSCB(skb).consumed, iter, chunk);
        if (retval != OK) {
            if (copied == 0) retval = -EFAULT;
            break;
        }

        copied += chunk;
        len -= chunk;

        memset(&scm, 0, sizeof(scm));

        scm.creds.pid = UDSCB(skb).pid;
        scm.creds.uid = UDSCB(skb).uid;
        scm.creds.gid = UDSCB(skb).gid;

        if (!(flags & MSG_PEEK)) {
            /* mark data in the skb as consumed */
            UDSCB(skb).consumed += chunk;

            if (UDSCB(skb).fd_list) uds_detach_fds(&scm, skb);

            if (uds_skb_len(skb)) break;

            skb_unlink(skb, &uds->sock.recvq);
            skb_free(skb);

            if (scm.fd_list) break;
        }
    } while (len);

    if (ctl_len && *ctl_len > 0) {
        ctl_off = scm_recv(&uds->sock, ctl, *ctl_len, user_endpt, &scm, flags);

        if (ctl_off >= 0)
            *ctl_len = ctl_off;
        else
            *ctl_len = 0;
    } else
        scm_destroy(&scm);

    return copied ?: retval;
}

__poll_t uds_poll(struct sock* sock)
{
    struct udssock* uds = to_udssock(sock);
    __poll_t mask = 0;

    if (uds->sock.err) mask |= EPOLLERR;
    if (uds_is_shutdown(uds, SFL_SHUT_RD) && uds_is_shutdown(uds, SFL_SHUT_WR))
        mask |= EPOLLHUP;
    if (uds_is_shutdown(uds, SFL_SHUT_RD))
        mask |= EPOLLRDHUP | EPOLLIN | EPOLLRDNORM;

    if (uds_is_listening(uds) && !list_empty(&uds->queue)) mask |= EPOLLIN;

    /* check for termination for connection-based sockets */
    if (uds_get_type(uds) != SOCK_DGRAM && uds_is_disconnected(uds))
        mask |= EPOLLHUP;

    if (!skb_queue_empty(&uds->sock.recvq)) mask |= EPOLLIN | EPOLLRDNORM;

    if (!uds_is_listening(uds)) mask |= EPOLLOUT | EPOLLWRNORM | EPOLLWRBAND;

    return mask;
}
