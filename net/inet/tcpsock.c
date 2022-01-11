#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <lyos/idr.h>
#include <assert.h>
#include <sys/epoll.h>

#include <lwip/tcp.h>

#include "inet.h"
#include "ifdev.h"
#include "ipsock.h"

#define TCP_BUF_SIZE 512

#define TCP_DEF_SNDBUF 32768
#define TCP_DEF_RCVBUF 32768

#define TCP_POLL_REG_INTERVAL   10
#define TCP_POLL_CLOSE_INTERVAL 1

struct tcpsock {
    struct ipsock ipsock;
    struct tcp_pcb* pcb;
    struct list_head list;

    struct tcpsock* listener;

    struct list_head queue;

    struct {
        struct pbuf* head;
        struct pbuf* unsent;
        struct pbuf* tail;
        size_t len;
        size_t head_off;
        size_t unsent_off;
    } send_queue;

    struct {
        struct pbuf* head;
        struct pbuf** tailp;
        size_t len;
        size_t head_off;
        size_t unacked_len;
    } recv_queue;
};

#define to_tcpsock(sock) ((struct tcpsock*)(sock))

#define tcpsock_get_sock(tcp)     ipsock_get_sock(&(tcp)->ipsock)
#define tcpsock_is_listening(tcp) sock_is_listening(tcpsock_get_sock(tcp))
#define tcpsock_is_shutdown(tcp, mask) \
    sock_is_shutdown(tcpsock_get_sock(tcp), mask)
#define tcpsock_is_closing(tcp) sock_is_closing(tcpsock_get_sock(tcp))

#define tcpsock_get_sndbuf(tcp) ipsock_get_sndbuf(&(tcp)->ipsock)
#define tcpsock_get_rcvbuf(tcp) ipsock_get_rcvbuf(&(tcp)->ipsock)

#define tcpsock_get_flags(tcp)        ipsock_get_flags(&(tcp)->ipsock)
#define tcpsock_get_flag(tcp, flag)   ipsock_get_flag(&(tcp)->ipsock, flag)
#define tcpsock_set_flag(tcp, flag)   ipsock_set_flag(&(tcp)->ipsock, flag)
#define tcpsock_clear_flag(tcp, flag) ipsock_clear_flag(&(tcp)->ipsock, flag)

extern struct idr sock_idr;

static int tcpsock_bind(struct sock* sock, struct sockaddr* addr,
                        size_t addrlen, endpoint_t user_endpt, int flags);
static int tcpsock_listen(struct sock* sock, int backlog);
static int tcpsock_accept(struct sock* sock, struct sockaddr* addr,
                          socklen_t* addrlen, endpoint_t user_endpt, int flags,
                          struct sock** newsockp);
static int tcpsock_connect(struct sock* sock, struct sockaddr* addr,
                           size_t addrlen, endpoint_t user_endpt, int flags);
static ssize_t tcpsock_send(struct sock* sock, struct iov_grant_iter* iter,
                            size_t len, const struct sockdriver_data* ctl,
                            socklen_t ctl_len, const struct sockaddr* addr,
                            socklen_t addr_len, endpoint_t user_endpt,
                            int flags);
static ssize_t tcpsock_recv(struct sock* sock, struct iov_grant_iter* iter,
                            size_t len, const struct sockdriver_data* ctl,
                            socklen_t* ctl_len, struct sockaddr* addr,
                            socklen_t* addr_len, endpoint_t user_endpt,
                            int flags, int* rflags);
__poll_t tcpsock_poll(struct sock* sock);
static int tcpsock_close(struct sock* sock, int force);
static void tcpsock_free(struct sock* sock);

const struct sockdriver_ops tcpsock_ops = {
    .sop_bind = tcpsock_bind,
    .sop_connect = tcpsock_connect,
    .sop_listen = tcpsock_listen,
    .sop_accept = tcpsock_accept,
    .sop_send = tcpsock_send,
    .sop_recv = tcpsock_recv,
    .sop_poll = tcpsock_poll,
    .sop_close = tcpsock_close,
    .sop_free = tcpsock_free,
};

static void tcpsock_reset_send(struct tcpsock* tcp)
{
    tcp->send_queue.head = NULL;
    tcp->send_queue.unsent = NULL;
    tcp->send_queue.tail = NULL;
    tcp->send_queue.len = 0;
    tcp->send_queue.head_off = 0;
    tcp->send_queue.unsent_off = 0;
}

static void tcpsock_reset_recv(struct tcpsock* tcp)
{
    tcp->recv_queue.head = NULL;
    tcp->recv_queue.tailp = NULL;
    tcp->recv_queue.len = 0;
    tcp->recv_queue.head_off = 0;
    tcp->recv_queue.unacked_len = 0;
}

static void tcpsock_clear_send(struct tcpsock* tcp)
{
    struct pbuf* head;

    assert(!tcp->pcb);

    while ((head = tcp->send_queue.head) != NULL) {
        tcp->send_queue.head = head->next;
        pbuf_free(head);
    }

    tcpsock_reset_send(tcp);
}

static size_t tcpsock_clear_recv(struct tcpsock* tcp, int ack)
{
    struct pbuf* head;
    size_t len;

    len = tcp->recv_queue.len;

    while ((head = tcp->recv_queue.head) != NULL) {
        tcp->recv_queue.head = head->next;
        pbuf_free(head);
    }

    if (ack && tcp->pcb && tcp->recv_queue.unacked_len)
        tcp_recved(tcp->pcb, tcp->recv_queue.unacked_len);

    tcpsock_reset_recv(tcp);

    return len;
}

static int tcpsock_cleanup(struct tcpsock* tcp, int may_free)
{
    int release;

    assert(!tcp->pcb);

    tcpsock_clear_send(tcp);

    if (tcp->listener) {
        assert(!list_empty(&tcp->list));
        list_del(&tcp->list);
        tcp->listener = NULL;
        release = TRUE;
    } else
        release = tcpsock_is_closing(tcp);

    if (release && may_free) {
        tcpsock_clear_recv(tcp, FALSE);
        sockdriver_fire(tcpsock_get_sock(tcp), SEV_CLOSE);
    }

    return release;
}

sockid_t tcpsock_socket(int domain, int protocol, struct sock** sock,
                        const struct sockdriver_ops** ops)
{
    struct tcpsock* tcp;
    sockid_t sock_id;
    int type;

    switch (protocol) {
    case 0:
    case IPPROTO_TCP:
        break;
    default:
        return -EPROTONOSUPPORT;
    }

    sock_id = idr_alloc(&sock_idr, NULL, 1, 0);
    if (sock_id < 0) return sock_id;

    tcp = malloc(sizeof(*tcp));
    if (!tcp) return -ENOMEM;

    memset(tcp, 0, sizeof(*tcp));

    type = ipsock_socket(&tcp->ipsock, domain, TCP_DEF_SNDBUF, TCP_DEF_RCVBUF,
                         sock);

    if ((tcp->pcb = tcp_new_ip_type(type)) == NULL) return -ENOMEM;
    tcp_arg(tcp->pcb, tcp);

    tcpsock_reset_send(tcp);
    tcpsock_reset_recv(tcp);

    *ops = &tcpsock_ops;
    return sock_id;
}

static int tcpsock_clone(struct tcpsock* listener, struct tcp_pcb* pcb)
{
    struct tcpsock* tcp;
    sockid_t sock_id;

    sock_id = idr_alloc(&sock_idr, NULL, 1, 0);
    if (sock_id < 0) return sock_id;

    tcp = malloc(sizeof(*tcp));
    if (!tcp) return -ENOMEM;

    memset(tcp, 0, sizeof(*tcp));

    ipsock_clone(&listener->ipsock, &tcp->ipsock, sock_id);

    tcp->pcb = pcb;
    tcp_arg(pcb, tcp);

    tcpsock_reset_send(tcp);
    tcpsock_reset_recv(tcp);

    list_add(&tcp->list, &listener->queue);
    tcp->listener = listener;

    return 0;
}

static inline int tcpsock_may_close(struct tcpsock* tcp)
{
    assert(tcp->pcb);

    return tcpsock_get_flag(tcp, TCPF_SENT_FIN) &&
           tcpsock_get_flag(tcp, TCPF_RCVD_FIN) && tcp->send_queue.len == 0;
}

static void tcpsock_pcb_close(struct tcpsock* tcp)
{
    err_t err;

    assert(tcp->pcb);
    assert(tcp->send_queue.len == 0);

    if (!tcpsock_is_listening(tcp)) {
        tcp_recv(tcp->pcb, NULL);
        tcp_sent(tcp->pcb, NULL);
        tcp_err(tcp->pcb, NULL);
        tcp_poll(tcp->pcb, NULL, TCP_POLL_REG_INTERVAL);
    }
    tcp_arg(tcp->pcb, NULL);

    err = tcp_close(tcp->pcb);
    assert(err == ERR_OK);

    tcp->pcb = NULL;
}

static int tcpsock_finish_close(struct tcpsock* tcp)
{
    assert(tcp->send_queue.len == 0);
    assert(!tcp->listener);

    tcpsock_pcb_close(tcp);

    if (tcpsock_is_closing(tcp)) {
        assert(tcp->recv_queue.len == 0);

        sockdriver_fire(tcpsock_get_sock(tcp), SEV_CLOSE);

        return TRUE;
    }

    return FALSE;
}

static err_t tcpsock_event_poll(void* arg, struct tcp_pcb* pcb)
{
    struct tcpsock* tcp = (struct tcpsock*)arg;

    if (tcpsock_is_closing(tcp) && tcpsock_get_flag(tcp, TCPF_SENT_FIN) &&
        tcp->pcb->unsent == NULL && tcp->pcb->unacked == NULL) {
        assert(tcp->send_queue.len == 0);

        tcpsock_finish_close(tcp);
    }

    return ERR_OK;
}

static void tcpsock_pcb_abort(struct tcpsock* tcp)
{
    assert(tcp->pcb);

    tcp_recv(tcp->pcb, NULL);
    tcp_sent(tcp->pcb, NULL);
    tcp_err(tcp->pcb, NULL);
    tcp_poll(tcp->pcb, NULL, TCP_POLL_REG_INTERVAL);

    tcp_arg(tcp->pcb, NULL);

    tcp_abort(tcp->pcb);
    tcp->pcb = NULL;
}

static int tcpsock_bind(struct sock* sock, struct sockaddr* addr,
                        size_t addrlen, endpoint_t user_endpt, int flags)
{
    struct tcpsock* tcp = to_tcpsock(sock);
    ip_addr_t ipaddr;
    uint16_t port;
    err_t err;
    int retval;

    if (!tcp->pcb || tcp->pcb->state != CLOSED) return EINVAL;

    if ((retval = ipsock_get_src_addr(&tcp->ipsock, addr, addrlen, user_endpt,
                                      &tcp->pcb->local_ip, tcp->pcb->local_port,
                                      FALSE, &ipaddr, &port)) != 0)
        return retval;

    err = tcp_bind(tcp->pcb, &ipaddr, port);

    return convert_err(err);
}

static int tcpsock_pcb_enqueue(struct tcpsock* tcp)
{
    int enqueued;
    size_t space;
    struct pbuf* unsent;
    int flags;
    err_t err;

    enqueued = FALSE;

    while ((unsent = tcp->send_queue.unsent) != NULL) {
        size_t chunk;

        if (!(space = tcp_sndbuf(tcp->pcb))) break;

        assert(unsent->len >= tcp->send_queue.unsent_off);
        chunk = unsent->len - tcp->send_queue.unsent_off;
        if (!chunk) break;

        if (chunk > space) chunk = space;

        flags = 0;
        if (chunk < unsent->len || unsent->next) flags = TCP_WRITE_FLAG_MORE;

        err = tcp_write(tcp->pcb, unsent->payload + tcp->send_queue.unsent_off,
                        chunk, flags);
        if (err != ERR_OK) break;

        enqueued = TRUE;

        tcp->send_queue.unsent_off += chunk;

        if (tcp->send_queue.unsent_off < unsent->tot_len) break;

        tcp->send_queue.unsent_off = 0;
        tcp->send_queue.unsent = unsent->next;
    }

    if ((!tcp->send_queue.unsent ||
         tcp->send_queue.unsent_off == tcp->send_queue.unsent->len) &&
        tcpsock_is_shutdown(tcp, SFL_SHUT_WR) &&
        !tcpsock_get_flag(tcp, TCPF_SENT_FIN)) {
        err = tcp_shutdown(tcp->pcb, FALSE, TRUE);

        if (err == ERR_OK) {
            tcpsock_set_flag(tcp, TCPF_SENT_FIN);
            enqueued = TRUE;
        } else {
            assert(err == ERR_MEM);
            tcp->pcb->flags &= ~TF_CLOSEPEND;
            tcpsock_set_flag(tcp, TCPF_FULL);
        }
    }

    return enqueued;
}

static int tcpsock_pcb_send(struct tcpsock* tcp, int set_error)
{
    err_t err;
    int retval;

    assert(tcp->pcb);
    err = tcp_output(tcp->pcb);

    if (err != ERR_OK && err != ERR_MEM) {
        tcpsock_pcb_abort(tcp);

        retval = convert_err(err);

        if (!tcpsock_cleanup(tcp, TRUE)) {
            if (set_error) sock_set_error(tcpsock_get_sock(tcp), retval);
        }

        return retval;
    }

    return 0;
}

static err_t tcpsock_event_sent(void* arg, struct tcp_pcb* pcb, uint16_t len)
{
    struct tcpsock* tcp = (struct tcpsock*)arg;
    size_t left;
    struct pbuf* head;

    left = len;

    while ((head = tcp->send_queue.head) != NULL &&
           left >= ((size_t)head->len - tcp->send_queue.head_off)) {
        left -= (size_t)head->len - tcp->send_queue.head_off;

        tcp->send_queue.head = head->next;
        tcp->send_queue.head_off = 0;

        if (head == tcp->send_queue.unsent) {
            tcp->send_queue.unsent = head->next;
            tcp->send_queue.unsent_off = 0;
        }

        pbuf_free(head);
    }

    if (left) {
        assert(tcp->send_queue.head);
        tcp->send_queue.head_off += left;
        assert(tcp->send_queue.head_off < tcp->send_queue.head->len);
    }

    tcp->send_queue.len -= len;

    if (!tcp->send_queue.head) tcp->send_queue.tail = NULL;

    if (tcpsock_may_close(tcp)) {
        if (tcpsock_finish_close(tcp)) return ERR_OK;
    } else {
        tcpsock_clear_flag(tcp, TCPF_FULL);

        if (tcpsock_pcb_enqueue(tcp)) {
            if (tcpsock_may_close(tcp) && tcpsock_finish_close(tcp))
                return ERR_OK;
        }
    }

    sockdriver_fire(tcpsock_get_sock(tcp), SEV_SEND);

    return ERR_OK;
}

static void tcpsock_ack_recv(struct tcpsock* tcp)
{
    size_t rcvbuf;
    size_t left, delta, ack;

    assert(tcp->pcb);

    rcvbuf = tcpsock_get_rcvbuf(tcp);

    if (rcvbuf > tcp->recv_queue.len && tcp->recv_queue.unacked_len) {
        left = TCP_WND - tcp->recv_queue.unacked_len;
        delta = rcvbuf - tcp->recv_queue.len;

        if (left < delta) {
            ack = delta - left;

            if (ack > tcp->recv_queue.unacked_len)
                ack = tcp->recv_queue.unacked_len;

            tcp_recved(tcp->pcb, ack);

            tcp->recv_queue.unacked_len -= ack;
        }
    }
}

static int tcpsock_try_merge(struct pbuf** nextp, struct pbuf* tail,
                             struct pbuf* pbuf)
{
    assert(*nextp == tail);
    assert(tail->next == pbuf);

    if (tail->tot_len - tail->len >= pbuf->len) {
        memcpy((char*)tail->payload + tail->len, pbuf->payload, pbuf->len);
        tail->len += pbuf->len;
        tail->next = pbuf->next;

        pbuf_free(pbuf);

        return TRUE;
    }

    return FALSE;
}

static err_t tcpsock_event_recv(void* arg, struct tcp_pcb* pcb,
                                struct pbuf* pbuf, err_t err)
{
    struct tcpsock* tcp = (struct tcpsock*)arg;
    struct pbuf** prevp;
    struct pbuf* tail;
    size_t len;

    assert(tcp);
    assert(pcb == tcp->pcb);
    assert(err == ERR_OK);

    if (!pbuf) {
        tcpsock_set_flag(tcp, TCPF_RCVD_FIN);

        if (!tcpsock_is_shutdown(tcp, SFL_SHUT_RD))
            sockdriver_fire(tcpsock_get_sock(tcp), SEV_RECV);

        if (tcpsock_may_close(tcp)) tcpsock_finish_close(tcp);

        return ERR_OK;
    }

    if (tcpsock_is_closing(tcp)) {
        tcpsock_pcb_abort(tcp);
        tcpsock_cleanup(tcp, TRUE);
        pbuf_free(pbuf);

        return ERR_ABRT;
    }

    if (tcpsock_is_shutdown(tcp, SFL_SHUT_RD)) {
        tcp_recved(tcp->pcb, pbuf->tot_len);
        pbuf_free(pbuf);
        return ERR_OK;
    }

    len = pbuf->tot_len;

    if ((prevp = tcp->recv_queue.tailp) != NULL) {
        tail = *prevp;

        assert(tail);
        assert(!tail->next);
        assert(tcp->recv_queue.head);

        tail->next = pbuf;
        pbuf->tot_len = pbuf->len;

        if (tcpsock_try_merge(prevp, tail, pbuf)) {
            tail = *prevp;
            pbuf = tail->next;
        }

        if (pbuf) prevp = &tail->next;
    } else {
        assert(!tcp->recv_queue.head);
        assert(!tcp->recv_queue.head_off);

        tcp->recv_queue.head = pbuf;
        prevp = &tcp->recv_queue.head;
    }

    if (pbuf) {
        for (; pbuf->next; pbuf = pbuf->next) {
            pbuf->tot_len = pbuf->len;
            prevp = &pbuf->next;
        }
    }

    tcp->recv_queue.tailp = prevp;
    tcp->recv_queue.len += len;
    tcp->recv_queue.unacked_len += len;

    tcpsock_ack_recv(tcp);

    sockdriver_fire(tcpsock_get_sock(tcp), SEV_RECV);

    return ERR_OK;
}

static void tcpsock_event_err(void* arg, err_t err) {}

static err_t tcpsock_event_accept(void* arg, struct tcp_pcb* pcb, err_t err)
{
    struct tcpsock* tcp = (struct tcpsock*)arg;

    assert(tcp != NULL);
    assert(tcpsock_is_listening(tcp));

    if (pcb == NULL || err != ERR_OK) return ERR_OK;

    if (tcpsock_clone(tcp, pcb) != 0) {
        tcp_abort(pcb);
        return ERR_ABRT;
    }

    tcp_backlog_delayed(pcb);

    tcp_recv(pcb, tcpsock_event_recv);
    tcp_sent(pcb, tcpsock_event_sent);
    tcp_err(pcb, tcpsock_event_err);
    tcp_poll(pcb, tcpsock_poll, TCP_POLL_REG_INTERVAL);

    sockdriver_fire(tcpsock_get_sock(tcp), SEV_ACCEPT);

    return ERR_OK;
}

static int tcpsock_listen(struct sock* sock, int backlog)
{
    struct tcpsock* tcp = to_tcpsock(sock);
    struct tcp_pcb* pcb;
    err_t err;

    if (!tcpsock_is_listening(tcp) &&
        (tcp->pcb == NULL || tcp->pcb->state != CLOSED))
        return EINVAL;

    if (!tcpsock_is_listening(tcp)) {
        if (tcp->pcb->local_port == 0) {
            err = tcp_bind(tcp->pcb, &tcp->pcb->local_ip, 0);

            if (err != ERR_OK) return convert_err(err);
        }

        tcp_arg(tcp->pcb, NULL);

        pcb = tcp_listen_with_backlog_and_err(tcp->pcb, backlog, &err);

        if (pcb == NULL) {
            tcp_arg(tcp->pcb, tcp);

            return convert_err(err);
        }

        tcp_arg(pcb, tcp);
        tcp->pcb = pcb;

        tcp_accept(pcb, tcpsock_event_accept);

        INIT_LIST_HEAD(&tcp->queue);
    } else if (tcp->pcb != NULL)
        tcp_backlog_set(tcp->pcb, backlog);

    return 0;
}

static int tcpsock_accept(struct sock* sock, struct sockaddr* addr,
                          socklen_t* addrlen, endpoint_t user_endpt, int flags,
                          struct sock** newsockp)
{
    struct tcpsock* listener = to_tcpsock(sock);
    struct tcpsock* tcp;

restart:
    if (!tcpsock_is_listening(listener)) return -EINVAL;

    if (list_empty(&listener->queue)) {
        if (listener->pcb == NULL) return -ECONNABORTED;

        if (flags & SDEV_NONBLOCK) return -EAGAIN;

        sockdriver_suspend(sock, SEV_ACCEPT);
        goto restart;
    }

    tcp = list_first_entry(&listener->queue, struct tcpsock, list);
    assert(tcp->listener == listener);
    assert(tcp->pcb);

    list_del(&tcp->list);
    tcp->listener = NULL;

    tcp_backlog_accepted(tcp->pcb);

    ipsock_set_addr(&tcp->ipsock, addr, addrlen, &tcp->pcb->remote_ip,
                    tcp->pcb->remote_port);

    *newsockp = NULL;
    return sock_sockid(tcpsock_get_sock(tcp));
}

static err_t tcpsock_event_connected(void* arg, struct tcp_pcb* pcb, err_t err)
{
    struct tcpsock* tcp = (struct tcpsock*)arg;

    assert(tcp != NULL);
    assert(pcb == tcp->pcb);
    assert(tcpsock_get_flags(tcp) & TCPF_CONNECTING);
    assert(err == ERR_OK);

    sockdriver_fire(tcpsock_get_sock(tcp), SEV_CONNECT | SEV_SEND);

    return ERR_OK;
}

static int tcpsock_connect(struct sock* sock, struct sockaddr* addr,
                           size_t addrlen, endpoint_t user_endpt, int flags)
{
    struct tcpsock* tcp = to_tcpsock(sock);
    ip_addr_t dst_addr;
    u16 dst_port;
    err_t err;
    int retval;

    if (tcpsock_is_listening(tcp)) return EOPNOTSUPP;

    if (!tcp->pcb) return EINVAL;

    switch (tcp->pcb->state) {
    case CLOSED:
        break;
    case SYN_SENT:
        return EALREADY;
    default:
        return EISCONN;
    }

    if ((retval = ipsock_get_dst_addr(&tcp->ipsock, addr, addrlen,
                                      &tcp->pcb->local_ip, &dst_addr,
                                      &dst_port)) != 0)
        return retval;

    err = tcp_connect(tcp->pcb, &dst_addr, dst_port, tcpsock_event_connected);
    if (err != ERR_OK) return convert_err(err);

    tcp_recv(tcp->pcb, tcpsock_event_recv);
    tcp_sent(tcp->pcb, tcpsock_event_sent);
    tcp_err(tcp->pcb, tcpsock_event_err);
    tcp_poll(tcp->pcb, tcpsock_event_poll, TCP_POLL_REG_INTERVAL);

    tcpsock_set_flag(tcp, TCPF_CONNECTING);

    if (flags & SDEV_NONBLOCK) return EINPROGRESS;

    sockdriver_suspend(sock, SEV_CONNECT);
    return sock_error(sock);
}

static ssize_t tcpsock_send(struct sock* sock, struct iov_grant_iter* iter,
                            size_t len, const struct sockdriver_data* ctl,
                            socklen_t ctl_len, const struct sockaddr* addr,
                            socklen_t addr_len, endpoint_t user_endpt,
                            int flags)
{
    struct tcpsock* tcp = to_tcpsock(sock);
    size_t sndbuf, min;
    int need_wait;
    struct pbuf *first, *last, *tail, *next;
    size_t left, off, tail_off, sent = 0;
    ssize_t retval;

    sndbuf = tcpsock_get_sndbuf(tcp);
    min = sock_sndlowat(tcpsock_get_sock(tcp), len);
    if (min > sndbuf) min = sndbuf;
    assert(min > 0);

restart:
    need_wait = FALSE;

    if (!tcp->pcb) return -EPIPE;

    switch (tcp->pcb->state) {
    case CLOSED:
    case LISTEN:
        return -ENOTCONN;
    case SYN_SENT:
    case SYN_RCVD:
        need_wait = TRUE;
    case ESTABLISHED:
    case CLOSE_WAIT:
        break;
    default:
        return -EPIPE;
    }

    if (!need_wait) {
        need_wait = tcp->send_queue.len + min > sndbuf;
    }

    if (need_wait) {
        if (flags & MSG_DONTWAIT) {
            if (sent)
                return sent;
            else
                return -EAGAIN;
        }

        sockdriver_suspend(sock, SEV_SEND);

        if ((retval = sock_error(sock)) != 0 || (sock->flags & SFL_SHUT_WR)) {
            if (sent > 0) return off;
            if (retval != 0)
                return -retval;
            else
                return -EPIPE;
        }

        goto restart;
    }

    if (len == 0) return 0;

    assert(sndbuf > tcp->send_queue.len);
    left = sndbuf - tcp->send_queue.len;
    if (left > len) left = len;

    if (((tail = tcp->send_queue.tail) != NULL) && tail->len < tail->tot_len) {
        /* Append data to the send queue tail buffer if there is any room. */
        assert(tail->len);
        tail_off = tail->len;

        off = tail->tot_len - tail->len;
        if (off > len) off = len;
        tail->len += off;
    } else {
        tail = NULL;
        tail_off = 0;
        off = 0;
    }

    first = last = NULL;
    while (off < left) {
        /* Allocate new buffers for the send data. */
        size_t chunk;

        next = pbuf_alloc(PBUF_RAW, TCP_BUF_SIZE, PBUF_RAM);
        if (!next) {
            tcpsock_set_flag(tcp, TCPF_OOM);
            break;
        }

        if (!first)
            first = next;
        else
            last->next = next;
        last = next;

        chunk = next->tot_len;
        if (chunk > left - off) chunk = left - off;
        next->len = chunk;
        off += chunk;
    }

    /* Copy data from user or sleep if we cannot meet the send low watermark. */
    if (off >= min) {
        if (tail) {
            tail->next = first;
            next = tail;
        } else {
            next = first;
        }

        retval = copy_socket_data(iter, off, next, tail_off, TRUE);
        if (retval < 0) return retval;

        retval = 0;
    } else {
        need_wait = TRUE;
    }

    if (retval < 0 || need_wait) {
        /* Revert changes and restart. */
        while (first) {
            next = first->next;
            pbuf_free(first);
            first = next;
        }

        if (tail) {
            tail->next = NULL;
            tail->len = tail_off;
        }

        if (retval < 0) return retval;

        assert(need_wait);

        if (flags & MSG_DONTWAIT) {
            if (sent)
                return sent;
            else
                return -EAGAIN;
        }

        sockdriver_suspend(sock, SEV_SEND);

        if ((retval = sock_error(sock)) != 0 || (sock->flags & SFL_SHUT_WR)) {
            if (sent > 0) return sent;
            if (retval != 0)
                return -retval;
            else
                return -EPIPE;
        }

        goto restart;
    }

    /* Commit the buffers. */
    if (first) {
        tail = tcp->send_queue.tail;
        if (tail) tail->next = first;

        tcp->send_queue.tail = last;

        if (!tcp->send_queue.head) tcp->send_queue.head = first;
        if (!tcp->send_queue.unsent) tcp->send_queue.unsent = first;
    }

    tcp->send_queue.len += off;

    if (tcpsock_pcb_enqueue(tcp) &&
        (retval = tcpsock_pcb_send(tcp, FALSE)) != 0) {
        if (sent > 0) {
            sock_set_error(tcpsock_get_sock(tcp), retval);
            return 0;
        } else
            return -retval;
    }

    sent += off;

    if (sent < len) {
        if (flags & MSG_DONTWAIT) {
            if (sent)
                return sent;
            else
                return -EAGAIN;
        }

        sockdriver_suspend(sock, SEV_SEND);

        if ((retval = sock_error(sock)) != 0 || (sock->flags & SFL_SHUT_WR)) {
            if (sent > 0) return off;
            if (retval != 0)
                return -retval;
            else
                return -EPIPE;
        }

        goto restart;
    }

    return sent;
}

static int tcpsock_may_wait(struct tcpsock* tcp)
{
    return tcp->pcb && !(tcpsock_get_flag(tcp, TCPF_RCVD_FIN));
}

static ssize_t tcpsock_recv(struct sock* sock, struct iov_grant_iter* iter,
                            size_t len, const struct sockdriver_data* ctl,
                            socklen_t* ctl_len, struct sockaddr* addr,
                            socklen_t* addr_len, endpoint_t user_endpt,
                            int flags, int* rflags)
{
    struct tcpsock* tcp = to_tcpsock(sock);
    int may_wait;
    size_t copied = 0, target;
    ssize_t retval;

    target = sock_rcvlowat(sock, flags & MSG_WAITALL, len);
    if (target > tcpsock_get_rcvbuf(tcp)) target = tcpsock_get_rcvbuf(tcp);

    do {
        size_t chunk;

        if (tcp->pcb != NULL &&
            (tcp->pcb->state == CLOSED || tcp->pcb->state == LISTEN))
            return -ENOTCONN;

        may_wait = tcpsock_may_wait(tcp);
        if (!may_wait) target = 1;

        if (!tcp->recv_queue.len) {
            /* No data in receive queue. */

            if (!may_wait) return 0; /* EOF */

            if (copied >= target) break;

            if ((retval = sock_error(sock)) != OK) {
                retval = -retval;
                break;
            }

            if (flags & MSG_DONTWAIT) {
                retval = -EAGAIN;
                break;
            }

            sockdriver_suspend(sock, SEV_RECV);
            continue;
        }

        chunk = tcp->recv_queue.len;
        if (chunk > len) chunk = len;

        assert(tcp->recv_queue.head != NULL);
        assert(tcp->recv_queue.head_off < tcp->recv_queue.head->len);

        retval = copy_socket_data(iter, chunk, tcp->recv_queue.head,
                                  tcp->recv_queue.head_off, FALSE);
        if (retval < 0) return retval;
        retval = 0;

        len -= chunk;
        copied += chunk;

        if (!(flags & MSG_PEEK)) {
            /* Clean up data we have read. */
            struct pbuf* tail;
            size_t left = chunk;

            while ((tail = tcp->recv_queue.head) != NULL &&
                   left >= (size_t)tail->len - tcp->recv_queue.head_off) {
                left -= (size_t)tail->len - tcp->recv_queue.head_off;

                tcp->recv_queue.head = tail->next;
                tcp->recv_queue.head_off = 0;

                if (tcp->recv_queue.head == NULL)
                    tcp->recv_queue.tailp = NULL;
                else if (tcp->recv_queue.tailp == &tail->next)
                    tcp->recv_queue.tailp = &tcp->recv_queue.head;

                pbuf_free(tail);
            }

            if (left > 0) {
                assert(tcp->recv_queue.head);
                tcp->recv_queue.head_off += left;
                assert(tcp->recv_queue.head_off < tcp->recv_queue.head->len);
            }

            tcp->recv_queue.len -= chunk;

            if (tcp->pcb) tcpsock_ack_recv(tcp);
        }
    } while (len > 0);

    return copied ?: retval;
}

__poll_t tcpsock_poll(struct sock* sock)
{
    struct tcpsock* tcp = to_tcpsock(sock);
    int state = tcp->pcb->state;
    __poll_t mask = 0;

    if (state == LISTEN) {
        /* LISTEN is a special case. */
        return list_empty(&tcp->queue) ? 0 : (EPOLLIN | EPOLLRDNORM);
    }

    if ((tcpsock_is_shutdown(tcp, SFL_SHUT_RD) &&
         tcpsock_is_shutdown(tcp, SFL_SHUT_WR)) ||
        state == CLOSED)
        mask |= EPOLLHUP;

    if (tcpsock_is_shutdown(tcp, SFL_SHUT_RD) ||
        tcpsock_get_flag(tcp, TCPF_RCVD_FIN))
        mask |= EPOLLIN | EPOLLRDNORM | EPOLLRDHUP;

    if (state != SYN_SENT && state != SYN_RCVD) {
        int target = sock_rcvlowat(sock, FALSE, INT_MAX);

        if (tcp->recv_queue.len >= target) mask |= EPOLLIN | EPOLLRDNORM;

        if (!tcpsock_is_shutdown(tcp, SFL_SHUT_WR)) {
            size_t sndbuf = tcpsock_get_sndbuf(tcp);
            target = sock_sndlowat(sock, sndbuf);

            assert(tcp->send_queue.len <= sndbuf);
            if (target <= sndbuf - tcp->send_queue.len)
                mask |= EPOLLOUT | EPOLLWRNORM;
        } else
            mask |= EPOLLOUT | EPOLLWRNORM;
    }

    if (tcpsock_get_sock(tcp)->err) mask |= EPOLLERR;

    return mask;
}

static void tcpsock_send_fin(struct tcpsock* tcp)
{
    sockdriver_set_shutdown(tcpsock_get_sock(tcp), SFL_SHUT_WR);

    if (tcpsock_pcb_enqueue(tcp)) {
        if (tcpsock_may_close(tcp))
            tcpsock_finish_close(tcp);
        else
            tcpsock_pcb_send(tcp, TRUE);
    }
}

static int tcpsock_close(struct sock* sock, int force)
{
    struct tcpsock* tcp = to_tcpsock(sock);
    struct tcpsock *queued, *tmp;
    size_t rlen;

    assert(!tcp->listener);

    if (tcpsock_is_listening(tcp)) {
        list_for_each_entry_safe(queued, tmp, &tcp->queue, list)
        {
            tcpsock_pcb_abort(queued);
            tcpsock_cleanup(queued, TRUE);
        }
    }

    rlen = tcpsock_clear_recv(tcp, FALSE);

    sockdriver_set_shutdown(tcpsock_get_sock(tcp), SFL_SHUT_RD);

    if (tcp->pcb != NULL) {
        switch (tcp->pcb->state) {
        case CLOSE_WAIT:
        case CLOSING:
        case LAST_ACK:
        case SYN_RCVD:
        case ESTABLISHED:
        case FIN_WAIT_1:
            if (force || rlen > 0) break;

            if (!tcpsock_is_shutdown(tcp, SFL_SHUT_WR))
                tcpsock_send_fin(tcp);
            else if (tcpsock_may_close(tcp))
                tcpsock_pcb_close(tcp);

            if (tcp->pcb == NULL) return 0;

            tcp_poll(tcp->pcb, tcpsock_event_poll, TCP_POLL_CLOSE_INTERVAL);

            return SUSPEND;

        default:
            tcpsock_pcb_close(tcp);
            break;
        }
    }

    if (tcp->pcb) tcpsock_pcb_abort(tcp);
    tcpsock_cleanup(tcp, FALSE);

    return 0;
}

static void tcpsock_free(struct sock* sock)
{
    struct tcpsock* tcp = to_tcpsock(sock);
    idr_remove(&sock_idr, sock_sockid(sock));
    free(tcp);
}
