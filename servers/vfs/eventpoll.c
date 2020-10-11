/*  This file is part of Lyos.

    Lyos is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Lyos is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Lyos.  If not, see <http://www.gnu.org/licenses/>. */

#include <lyos/types.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <stddef.h>
#include <lyos/const.h>
#include <lyos/sysutils.h>
#include <string.h>
#include <lyos/fs.h>
#include <lyos/proc.h>
#include <lyos/kref.h>
#include <lyos/avl.h>
#include <errno.h>
#include <sys/syslimits.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <lyos/timer.h>
#include <lyos/eventpoll.h>
#include <fcntl.h>

#include "types.h"
#include "path.h"
#include "proto.h"
#include "global.h"
#include "poll.h"

#define EP_MAX_EVENTS   (INT_MAX / sizeof(struct epoll_event))
#define EP_PRIVATE_BITS (EPOLLWAKEUP | EPOLLONESHOT | EPOLLET | EPOLLEXCLUSIVE)

struct epoll_filefd {
    struct file_desc* file;
    int fd;
};

struct eventpoll;
struct epitem {
    struct avl_node avl;
    struct list_head rdllink;
    struct epoll_filefd ffd;

    struct eventpoll* ep;
    struct epoll_event event;
    struct list_head pwqlist;
    int nwait;
};

struct eventpoll {
    struct wait_queue_head wq;
    struct wait_queue_head poll_wait;

    struct list_head rdllist;
    struct avl_root avl_root;

    struct file_desc* file;
};

struct ep_pqueue {
    struct poll_table pt;
    struct epitem* epi;
};

struct eppoll_entry {
    struct list_head llink;
    struct epitem* epi;
    struct wait_queue_head* wq;
    struct wait_queue_entry wait;
};

struct ep_timer_cb_data {
    int expired;
    struct worker_thread* worker;
};

struct ep_send_events_data {
    int maxevents;
    void* events;
    struct fproc* fp;
    int retval;
};

static __poll_t ep_item_poll(struct epitem* epi, struct poll_table* pt);
static int ep_eventpoll_release(struct inode* pin, struct file_desc* filp);

static const struct file_operations eventpoll_fops = {
    .release = ep_eventpoll_release,
};

static inline int ep_cmp_ffd(struct epoll_filefd* p1, struct epoll_filefd* p2)
{
    return (p1->file > p2->file ? +1
                                : (p1->file < p2->file ? -1 : p1->fd - p2->fd));
}

static inline int ep_is_linked(struct epitem* epi)
{
    return !list_empty(&epi->rdllink);
}

static inline int ep_events_available(struct eventpoll* ep)
{
    return !list_empty(&ep->rdllist);
}

static __poll_t ep_scan_ready_list(struct eventpoll* ep,
                                   __poll_t (*sproc)(struct eventpoll*,
                                                     struct list_head*, void*),
                                   void* arg)
{
    struct list_head txlist;
    __poll_t retval;

    INIT_LIST_HEAD(&txlist);

    list_splice_init(&ep->rdllist, &txlist);
    retval = (*sproc)(ep, &txlist, arg);
    list_splice(&txlist, &ep->rdllist);

    return retval;
}

static __poll_t ep_send_events_proc(struct eventpoll* ep,
                                    struct list_head* head, void* arg)
{
    struct epitem *epi, *tmp;
    struct ep_send_events_data* esed = arg;
    struct epoll_event revent, *uevent = esed->events;
    struct poll_table pt;
    int revents;

    pt.qproc = NULL;
    esed->retval = 0;

    list_for_each_entry_safe(epi, tmp, head, rdllink)
    {
        if (esed->retval >= esed->maxevents) break;

        list_del(&epi->rdllink);

        revents = ep_item_poll(epi, &pt);
        if (!revents) continue;

        revent.events = revents;
        revent.data = epi->event.data;

        if (data_copy(esed->fp->endpoint, uevent, SELF, &revent,
                      sizeof(revent))) {
            esed->retval = -EFAULT;
            list_add(&epi->rdllink, head);
            break;
        }

        esed->retval++;
        uevent++;

        if (epi->event.events & EPOLLONESHOT)
            epi->event.events &= EP_PRIVATE_BITS;
        else if (!(epi->event.events & EPOLLET)) {
            list_add_tail(&epi->rdllink, &ep->rdllist);
        }
    }

    return 0;
}

static int ep_send_events(struct eventpoll* ep, void* events, int maxevents,
                          struct fproc* fp)
{
    struct ep_send_events_data esed;

    esed.events = events;
    esed.maxevents = maxevents;
    esed.fp = fp;

    ep_scan_ready_list(ep, ep_send_events_proc, &esed);

    return esed.retval;
}

struct epitem* ep_find(struct eventpoll* ep, struct file_desc* filp, int fd)
{
    struct avl_node* node = ep->avl_root.node;
    struct epitem* epi;
    struct epoll_filefd ffd;
    int cmp;

    ffd.fd = fd;
    ffd.file = filp;

    while (node) {
        epi = avl_entry(node, struct epitem, avl);
        cmp = ep_cmp_ffd(&ffd, &epi->ffd);

        if (!cmp) {
            return epi;
        } else if (cmp < 0) {
            node = node->left;
        } else {
            node = node->right;
        }
    }

    return NULL;
}

static int ep_poll_callback(struct wait_queue_entry* wait, void* arg)
{
    struct eppoll_entry* pwq = list_entry(wait, struct eppoll_entry, wait);
    struct epitem* epi = pwq->epi;
    struct eventpoll* ep = epi->ep;
    __poll_t mask = (__poll_t)arg;
    int ewake = 0;

    if (!(epi->event.events & ~EP_PRIVATE_BITS)) goto out;
    if (mask && !(mask & epi->event.events)) goto out;

    if (!ep_is_linked(epi)) {
        list_add_tail(&epi->rdllink, &ep->rdllist);
    }

    if (waitqueue_active(&ep->wq)) {
        waitqueue_wakeup_all(&ep->wq, NULL);
    }
    if (waitqueue_active(&ep->poll_wait)) {
        waitqueue_wakeup_all(&ep->poll_wait, (void*)EPOLLIN);
    }

out:
    if (!(epi->event.events & EPOLLEXCLUSIVE)) ewake = 1;

    if (mask & POLLFREE) {
        waitqueue_remove(pwq->wq, wait);
        pwq->wq = NULL;
    }

    return ewake;
}

static void ep_ptable_queue_proc(struct file_desc* filp,
                                 struct wait_queue_head* wq,
                                 struct poll_table* wait)
{
    struct ep_pqueue* epq = list_entry(wait, struct ep_pqueue, pt);
    struct epitem* epi = epq->epi;
    struct eppoll_entry* pwq;

    if (epi->nwait >= 0 && (pwq = malloc(sizeof(*pwq)))) {
        memset(pwq, 0, sizeof(*pwq));
        init_waitqueue_entry_func(&pwq->wait, ep_poll_callback);
        pwq->wq = wq;
        pwq->epi = epi;
        waitqueue_add(wq, &pwq->wait);
        list_add(&pwq->llink, &epi->pwqlist);
        epi->nwait++;
    } else {
        epi->nwait = -1;
    }
}

static __poll_t ep_item_poll(struct epitem* epi, struct poll_table* pt)
{
    struct file_desc* filp = epi->ffd.file;
    __poll_t mask;

    pt->mask = epi->event.events;
    if (filp->fd_fops != &eventpoll_fops) {
        mask = vfs_poll(filp, pt->mask, pt, fproc);

        return mask & epi->event.events;
    }

    return 0;
}

static int ep_insert(struct eventpoll* ep, struct epoll_event* event,
                     struct file_desc* tfilp, int fd)
{
    struct epitem* epi;
    struct ep_pqueue epq;
    __poll_t revents;

    epi = malloc(sizeof(*epi));
    if (!epi) return -ENOMEM;

    memset(epi, 0, sizeof(*epi));
    INIT_LIST_HEAD(&epi->rdllink);
    INIT_LIST_HEAD(&epi->pwqlist);
    epi->ep = ep;
    epi->ffd.file = tfilp;
    epi->ffd.fd = fd;
    epi->event = *event;

    epq.epi = epi;
    epq.pt.qproc = ep_ptable_queue_proc;

    revents = ep_item_poll(epi, &epq.pt);

    avl_insert(&epi->avl, &ep->avl_root);

    if (revents && !ep_is_linked(epi)) {
        list_add_tail(&epi->rdllink, &ep->rdllist);

        if (waitqueue_active(&ep->wq)) {
            waitqueue_wakeup_all(&ep->wq, NULL);
        }

        if (waitqueue_active(&ep->poll_wait)) {
            waitqueue_wakeup_all(&ep->poll_wait, (void*)EPOLLIN);
        }
    }

    return 0;
}

static void ep_timeout_check(struct timer_list* tp)
{
    struct ep_timer_cb_data* timer_cb = tp->arg;

    timer_cb->expired = TRUE;
    worker_wake(timer_cb->worker);
}

static int ep_poll(struct eventpoll* ep, void* events, int maxevents,
                   int timeout, struct fproc* fp)
{
    int eavail, timed_out = 0;
    struct wait_queue_entry wait;
    struct timer_list timer;
    struct ep_timer_cb_data timer_cb;
    struct file_desc* filp = ep->file;
    int retval;

    if (!timeout) {
        timed_out = 1;
        eavail = ep_events_available(ep);

        goto send_events;
    }

fetch_events:
    retval = 0;

    do {
        init_wait(&wait, self);

        eavail = ep_events_available(ep);
        if (!eavail) {
            waitqueue_add(&ep->wq, &wait);
        }

        if (eavail || retval) {
            break;
        }

        timer_cb.expired = FALSE;
        timer_cb.worker = self;

        clock_t ticks;
        ticks = (timeout * system_hz + 1000L - 1) / 1000L;
        set_timer(&timer, ticks, ep_timeout_check, &timer_cb);

        unlock_filp(filp);
        worker_wait();
        lock_filp(filp, RWL_READ);

        timed_out = timer_cb.expired;
        if (!timed_out) {
            cancel_timer(&timer);
        }

        if (timed_out) break;

        eavail = TRUE;
    } while (0);

    if (!list_empty(&wait.entry)) {
        waitqueue_remove(&ep->wq, &wait);
    }

send_events:

    if (!retval && eavail &&
        !(retval = ep_send_events(ep, events, maxevents, fp)) && !timed_out) {
        goto fetch_events;
    }

    return retval;
}

static int ep_item_key_node_comp(void* key, struct avl_node* node)
{
    struct epitem* r1 = (struct epitem*)key;
    struct epitem* r2 = avl_entry(node, struct epitem, avl);

    return ep_cmp_ffd(&r1->ffd, &r2->ffd);
}

static int ep_item_node_node_comp(struct avl_node* node1,
                                  struct avl_node* node2)
{
    struct epitem* r1 = avl_entry(node1, struct epitem, avl);
    struct epitem* r2 = avl_entry(node2, struct epitem, avl);

    return ep_cmp_ffd(&r1->ffd, &r2->ffd);
}

static struct epitem* ep_avl_get_iter(struct avl_iter* iter)
{
    struct avl_node* node = avl_get_iter(iter);
    if (!node) return NULL;
    return avl_entry(node, struct epitem, avl);
}

static int ep_alloc(struct eventpoll** epp)
{
    struct eventpoll* ep;

    ep = malloc(sizeof(*ep));
    if (!ep) return -ENOMEM;

    memset(ep, 0, sizeof(*ep));
    init_waitqueue_head(&ep->wq);
    init_waitqueue_head(&ep->poll_wait);
    INIT_LIST_HEAD(&ep->rdllist);
    INIT_AVL_ROOT(&ep->avl_root, ep_item_key_node_comp, ep_item_node_node_comp);

    *epp = ep;
    return 0;
};

static void ep_unregister_pollwait(struct eventpoll* ep, struct epitem* epi)
{
    struct eppoll_entry *pwq, *tmp;

    list_for_each_entry_safe(pwq, tmp, &epi->pwqlist, llink)
    {
        list_del(&pwq->llink);
        if (pwq->wq) {
            waitqueue_remove(pwq->wq, &pwq->wait);
        }

        free(pwq);
    }
}

static int ep_remove(struct eventpoll* ep, struct epitem* epi)
{
    ep_unregister_pollwait(ep, epi);

    avl_erase(&epi->avl, &ep->avl_root);
    if (ep_is_linked(epi)) list_del(&epi->rdllink);

    free(epi);

    return 0;
}

static void ep_free(struct eventpoll* ep)
{
    struct list_head remove_list;
    struct epitem key;
    struct avl_iter iter;
    struct epitem *epi, *tmp;

    if (waitqueue_active(&ep->poll_wait)) {
        waitqueue_wakeup_all(&ep->poll_wait, (void*)EPOLLIN);
    }

    INIT_LIST_HEAD(&remove_list);
    memset(&key.ffd, 0, sizeof(key.ffd));

    avl_start_iter(&ep->avl_root, &iter, &key, AVL_GREATER_EQUAL);
    epi = ep_avl_get_iter(&iter);

    while (epi) {
        ep_unregister_pollwait(ep, epi);
        if (ep_is_linked(epi)) {
            list_del(&epi->rdllink);
        }
        list_add(&epi->rdllink, &remove_list);

        avl_inc_iter(&iter);
        epi = ep_avl_get_iter(&iter);
    }

    list_for_each_entry_safe(epi, tmp, &remove_list, rdllink)
    {
        list_del(&epi->rdllink);

        free(epi);
    }

    free(ep);
}

static int ep_eventpoll_release(struct inode* pin, struct file_desc* filp)
{
    struct eventpoll* ep = filp->fd_private_data;
    if (ep) ep_free(ep);

    return 0;
}

static int get_epoll_flags(int eflags)
{
    int flags = 0;

    if (eflags & EPOLL_CLOEXEC) flags |= O_CLOEXEC;

    return flags;
}

int do_epoll_create1(void)
{
    int flags = self->msg_in.u.m_vfs_epoll.flags;
    int filp_flags = get_epoll_flags(flags);
    struct eventpoll* ep;
    int fd;
    int retval;

    if (flags & ~EPOLL_CLOEXEC) {
        return -EINVAL;
    }

    retval = ep_alloc(&ep);
    if (retval < 0) {
        return retval;
    }

    fd = anon_inode_get_fd(fproc, 0, &eventpoll_fops, ep,
                           O_RDWR | (filp_flags & O_CLOEXEC));

    if (fd < 0) {
        ep_free(ep);
    }

    ep->file = fproc->filp[fd];

    return fd;
}

static inline int ep_op_has_event(int op) { return op != EPOLL_CTL_DEL; }

int do_epoll_ctl(void)
{
    endpoint_t src = self->msg_in.source;
    int epfd = self->msg_in.u.m_vfs_epoll.epfd;
    int op = self->msg_in.u.m_vfs_epoll.op;
    int fd = self->msg_in.u.m_vfs_epoll.fd;
    void* event = self->msg_in.u.m_vfs_epoll.events;
    struct file_desc *tf, *f;
    struct epoll_event epds;
    struct eventpoll* ep;
    struct epitem* epi;
    int retval;

    if (ep_op_has_event(op) &&
        data_copy(SELF, &epds, src, event, sizeof(epds)) != 0) {
        return -EFAULT;
    }

    retval = -EBADF;

    f = get_filp(fproc, epfd, RWL_WRITE);
    if (!f) goto err_return;

    tf = get_filp(fproc, fd, RWL_READ);
    if (!tf) goto err_unlock_epf;

    retval = -EPERM;
    if (!file_can_poll(tf)) goto err_unlock_tf;

    retval = -EINVAL;
    if (f->fd_fops != &eventpoll_fops || f == tf) goto err_unlock_tf;

    if (ep_op_has_event(op) && (epds.events & EPOLLEXCLUSIVE)) {
        if (op == EPOLL_CTL_MOD) goto err_unlock_tf;
    }

    ep = f->fd_private_data;

    epi = ep_find(ep, tf, fd);

    retval = -EINVAL;
    switch (op) {
    case EPOLL_CTL_ADD:
        if (epi)
            retval = -EEXIST;
        else {
            epds.events |= EPOLLERR | EPOLLHUP;
            retval = ep_insert(ep, &epds, tf, fd);
        }
        break;
    case EPOLL_CTL_DEL:
        if (epi)
            retval = ep_remove(ep, epi);
        else
            retval = -ENOENT;
        break;
    }

err_unlock_tf:
    unlock_filp(tf);
err_unlock_epf:
    unlock_filp(f);
err_return:
    return retval;
}

int do_epoll_wait(void)
{
    int epfd = self->msg_in.u.m_vfs_epoll.epfd;
    int maxevents = self->msg_in.u.m_vfs_epoll.max_events;
    void* events = self->msg_in.u.m_vfs_epoll.events;
    int timeout = self->msg_in.u.m_vfs_epoll.timeout;
    struct file_desc* filp;
    struct eventpoll* ep;
    int retval;

    if (maxevents <= 0 || maxevents > EP_MAX_EVENTS) {
        return -EINVAL;
    }

    filp = get_filp(fproc, epfd, RWL_WRITE);
    if (!filp) {
        return -EBADF;
    }

    retval = -EINVAL;
    if (filp->fd_fops != &eventpoll_fops) goto err_unlock_filp;

    ep = filp->fd_private_data;

    retval = ep_poll(ep, events, maxevents, timeout, fproc);

err_unlock_filp:
    unlock_filp(filp);
    return retval;
}
