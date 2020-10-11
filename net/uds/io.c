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
#include <errno.h>
#include <lyos/service.h>
#include <lyos/sysutils.h>
#include <lyos/vm.h>
#include <lyos/idr.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <lyos/eventpoll.h>

#include "uds.h"

#define UDS_BUF_SIZE 32768
#define UDS_HDR_LEN  5

static size_t uds_skb_len(const struct sk_buff* skb)
{
    return skb->data_len - UDSCB(skb).consumed;
}

int uds_io_alloc(struct udssock* uds)
{
    if ((uds->buf = mmap(NULL, UDS_BUF_SIZE, PROT_READ | PROT_WRITE,
                         MAP_ANON | MAP_PRIVATE, -1, 0)) == MAP_FAILED)
        return ENOMEM;

    uds->len = uds->tail = uds->last = 0;
    return 0;
}

void uds_io_reset(struct udssock* uds) { uds->len = uds->tail = uds->last = 0; }

void uds_io_free(struct udssock* uds)
{
    uds_io_reset(uds);

    munmap(uds->buf, UDS_BUF_SIZE);
}

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
    if (UDSCB(skb).pid) return;
    if ((uds->flags | other->flags) & UDSF_PASSCRED) {
        UDSCB(skb).pid =
            get_epinfo(user_endpt, &UDSCB(skb).uid, &UDSCB(skb).gid);
    }
}

ssize_t uds_send(struct sock* sock, struct iov_grant_iter* iter, size_t len,
                 const struct sockdriver_data* ctl, socklen_t ctl_len,
                 const struct sockaddr* addr, socklen_t addr_len,
                 endpoint_t user_endpt, int flags)
{
    struct udssock *uds = to_udssock(sock), *other;
    struct sk_buff* skb;
    size_t sent = 0;
    size_t size;
    ssize_t retval;

    if (flags & MSG_OOB) return -EOPNOTSUPP;

    if (!uds_is_connected(uds) && !uds_is_disconnected(uds)) return -ENOTCONN;

    if ((retval = uds_send_peer(uds, addr, addr_len, user_endpt, &other)) != OK)
        return -retval;
    if (!other) return -ENOTCONN;

    if (uds_is_shutdown(uds, SFL_SHUT_WR)) return -EPIPE;

    while (sent < len) {
        size = len - sent;

        if ((retval = skb_alloc(sock, size, &skb)) != OK) return -retval;

        if ((retval = skb_copy_from_iter(skb, 0, iter, size)) != 0) {
            skb_free(skb);
            return -retval;
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
    return retval;
}

ssize_t uds_recv(struct sock* sock, struct iov_grant_iter* iter, size_t len,
                 const struct sockdriver_data* ctl, socklen_t* ctl_len,
                 struct sockaddr* addr, socklen_t* addr_len,
                 endpoint_t user_endpt, int flags, int* rflags)
{
    struct udssock *uds = to_udssock(sock), *other;
    struct sk_buff* skb;
    size_t target, chunk, copied = 0;
    int retval;

    if (uds_get_type(uds) != SOCK_DGRAM) {
        if (!uds_is_connected(uds) && !uds_is_disconnected(uds))
            return -ENOTCONN;
        else if (!uds_has_conn(uds) || uds_is_shutdown(uds->conn, SFL_SHUT_WR))
            return 0;
    }

    target = sock_rcvlowat(&uds->sock, flags & MSG_WAITALL, len);

    do {
        skb = skb_peek(&uds->sock.recvq);

        if (!skb) {
            /* no data yet */
            if (copied >= target) return copied;

            /* receiving data should take precedence over socket errors */
            if ((retval = sock_error(&uds->sock)) != OK) return -retval;

            if (uds_is_shutdown(uds, SFL_SHUT_RD)) return copied;

            /* need to wait for data now */
            if (flags & MSG_DONTWAIT) return -EAGAIN;

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
            if (copied == 0) copied = -EFAULT;
            break;
        }

        copied += chunk;
        len -= chunk;

        if (!(flags & MSG_PEEK)) {
            /* mark data in the skb as consumed */
            UDSCB(skb).consumed += chunk;

            if (uds_skb_len(skb)) break;

            skb_unlink(skb, &uds->sock.recvq);
            skb_free(skb);
        }
    } while (len);

    return copied;
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
