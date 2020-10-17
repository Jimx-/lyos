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
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/proc.h>
#include <lyos/driver.h>
#include <lyos/fs.h>
#include <lyos/sysutils.h>
#include <lyos/mgrant.h>

#include "const.h"
#include "types.h"
#include "global.h"
#include "proto.h"
#include "poll.h"

#include <libdevman/libdevman.h>

struct cdmap cdmap[NR_DEVICES];

void init_cdev(void)
{
    int i;
    for (i = 0; i < NR_DEVICES; i++) {
        cdmap[i].driver = NO_TASK;

        init_waitqueue_head(&cdmap[i].wait);
    }
}

static int cdev_update(dev_t dev)
{
    dev_t major = MAJOR(dev);
    cdmap[major].driver = dm_get_cdev_driver(dev);
    if (cdmap[major].driver < 0) cdmap[major].driver = NO_TASK;

    return cdmap[major].driver != NO_TASK;
}

static void cdev_recv(endpoint_t src, MESSAGE* msg)
{
    self->recv_from = src;
    self->msg_driver = msg;

    worker_wait(WT_BLOCKED_ON_DRV_MSG);
    self->recv_from = NO_TASK;
}

static struct cdmap* cdev_lookup(dev_t dev)
{
    dev_t major = MAJOR(dev);
    if (cdmap[major].driver == NO_TASK)
        if (!cdev_update(dev)) return NULL;

    return &cdmap[major];
}

static struct cdmap* cdev_lookup_by_endpoint(endpoint_t driver_ep)
{
    int i;
    for (i = 0; i < NR_DEVICES; i++) {
        if (cdmap[i].driver != NO_TASK && cdmap[i].driver == driver_ep) {
            return &cdmap[i];
        }
    }

    return NULL;
}

static struct fproc* cdev_get(dev_t dev)
{
    struct cdmap* pm = cdev_lookup(dev);
    if (!pm) return NULL;

    return vfs_endpt_proc(pm->driver);
}

static int cdev_opcl(int op, dev_t dev)
{
    if (op != CDEV_OPEN && op != CDEV_CLOSE) {
        return EINVAL;
    }

    struct fproc* driver = cdev_get(dev);
    if (!driver) return ENXIO;

    MESSAGE driver_msg;
    driver_msg.type = op;
    driver_msg.u.m_vfs_cdev_openclose.minor = MINOR(dev);
    driver_msg.u.m_vfs_cdev_openclose.id = fproc->endpoint;

    if (asyncsend3(driver->endpoint, &driver_msg, 0)) {
        panic("vfs: cdev_opcl send message failed");
    }

    cdev_recv(driver->endpoint, &driver_msg);

    return driver_msg.u.m_vfs_cdev_reply.status;
}

static int cdev_io(int op, dev_t dev, endpoint_t src, void* buf, off_t pos,
                   size_t count, struct fproc* fp)
{
    mgrant_id_t grant;
    struct fproc* driver = cdev_get(dev);
    int retval;
    if (!driver) return ENXIO;

    if (op != CDEV_READ && op != CDEV_WRITE && op != CDEV_IOCTL) {
        return EINVAL;
    }

    if (op == CDEV_IOCTL) {
        grant = make_ioctl_grant(driver->endpoint, src, count, buf);
    } else {
        grant = mgrant_set_proxy(driver->endpoint, src, (vir_bytes)buf, count,
                                 (op == CDEV_READ) ? MGF_WRITE : MGF_READ);
        if (grant == GRANT_INVALID)
            panic("vfs: cdev_io failed to create proxy grant");
    }

    MESSAGE driver_msg;
    driver_msg.type = op;
    driver_msg.u.m_vfs_cdev_readwrite.minor = MINOR(dev);
    driver_msg.u.m_vfs_cdev_readwrite.grant = grant;
    driver_msg.u.m_vfs_cdev_readwrite.id = src;
    if (op == CDEV_IOCTL) {
        driver_msg.u.m_vfs_cdev_readwrite.endpoint = src;
        driver_msg.u.m_vfs_cdev_readwrite.request = count;
    } else {
        driver_msg.u.m_vfs_cdev_readwrite.pos = pos;
        driver_msg.u.m_vfs_cdev_readwrite.count = count;
    }

    if (asyncsend3(driver->endpoint, &driver_msg, 0) != 0) {
        panic("vfs: cdev_io send message failed");
    }

    cdev_recv(driver->endpoint, &driver_msg);

    mgrant_revoke(grant);

    retval = driver_msg.u.m_vfs_cdev_reply.status;
    if (retval == -EINTR) {
        retval = -EAGAIN;
    }

    return retval;
}

int cdev_mmap(dev_t dev, endpoint_t src, void* vaddr, off_t offset,
              size_t length, void** retaddr, struct fproc* fp)
{
    struct fproc* driver = cdev_get(dev);
    if (!driver) return ENXIO;

    MESSAGE driver_msg;
    driver_msg.type = CDEV_MMAP;
    driver_msg.u.m_vfs_cdev_mmap.minor = MINOR(dev);
    driver_msg.u.m_vfs_cdev_mmap.id = src;
    driver_msg.u.m_vfs_cdev_mmap.addr = (void*)vaddr;
    driver_msg.u.m_vfs_cdev_mmap.endpoint = src;
    driver_msg.u.m_vfs_cdev_mmap.pos = offset;
    driver_msg.u.m_vfs_cdev_mmap.count = length;

    if (asyncsend3(driver->endpoint, &driver_msg, 0) != 0) {
        panic("vfs: cdev_io send message failed");
    }

    fp->worker = self;
    cdev_recv(driver->endpoint, &driver_msg);
    fp->worker = NULL;

    *retaddr = driver_msg.u.m_vfs_cdev_reply.retaddr;

    return driver_msg.u.m_vfs_cdev_reply.status;
}

static int cdev_select(dev_t dev, int ops, struct fproc* fp)
{
    struct fproc* driver = cdev_get(dev);
    if (!driver) return ENXIO;

    MESSAGE driver_msg;
    driver_msg.type = CDEV_SELECT;
    driver_msg.u.m_vfs_cdev_select.minor = MINOR(dev);
    driver_msg.u.m_vfs_cdev_select.ops = ops;
    driver_msg.u.m_vfs_cdev_select.id = fp->endpoint;

    if (asyncsend3(driver->endpoint, &driver_msg, 0) != 0) {
        panic("vfs: cdev_io send message failed");
    }

    cdev_recv(driver->endpoint, &driver_msg);

    return driver_msg.u.m_vfs_cdev_reply.status;
}

static void cdev_reply_generic(MESSAGE* msg)
{
    endpoint_t endpoint = msg->u.m_vfs_cdev_reply.id;

    struct fproc* fp = vfs_endpt_proc(endpoint);
    if (fp == NULL) return;

    struct worker_thread* worker = fp->worker;

    if (worker != NULL && worker->msg_driver != NULL &&
        worker->blocked_on == WT_BLOCKED_ON_DRV_MSG) {
        *worker->msg_driver = *msg;
        worker->msg_driver = NULL;
        worker_wake(worker);
    }
}

static void cdev_poll_notify(endpoint_t driver_ep, dev_t minor, int status)
{
    struct cdmap* pm = cdev_lookup_by_endpoint(driver_ep);
    if (!pm) return;

    waitqueue_wakeup_all(&pm->wait, (void*)status);
}

int cdev_reply(MESSAGE* msg)
{
    switch (msg->type) {
    case CDEV_REPLY:
        cdev_reply_generic(msg);
        break;
    case CDEV_POLL_NOTIFY:
        cdev_poll_notify(msg->source, msg->u.m_vfs_cdev_poll_notify.minor,
                         msg->u.m_vfs_cdev_poll_notify.status);
        break;
    }

    return 0;
}

static ssize_t cdev_read(struct file_desc* filp, char* buf, size_t count,
                         loff_t* ppos, struct fproc* fp)
{
    struct inode* pin = filp->fd_inode;
    ssize_t retval;

    unlock_filp(filp);
    retval =
        cdev_io(CDEV_READ, pin->i_specdev, fp->endpoint, buf, *ppos, count, fp);
    lock_filp(filp, RWL_READ);

    return retval;
}

static ssize_t cdev_write(struct file_desc* filp, const char* buf, size_t count,
                          loff_t* ppos, struct fproc* fp)
{
    struct inode* pin = filp->fd_inode;
    ssize_t retval;

    unlock_filp(filp);
    retval = cdev_io(CDEV_WRITE, pin->i_specdev, fp->endpoint, (char*)buf,
                     *ppos, count, fp);
    lock_filp(filp, RWL_WRITE);

    return retval;
}

static int cdev_ioctl(struct inode* pin, struct file_desc* filp,
                      unsigned int cmd, unsigned long arg, struct fproc* fp)
{
    int retval;

    unlock_filp(filp);
    retval = cdev_io(CDEV_IOCTL, pin->i_specdev, fp->endpoint, (void*)arg, 0,
                     cmd, fp);
    lock_filp(filp, RWL_READ);

    return retval;
}

static __poll_t cdev_poll(struct file_desc* filp, __poll_t mask,
                          struct poll_table* wait, struct fproc* fp)
{
    struct inode* pin = filp->fd_inode;
    struct cdmap* pm = cdev_lookup(pin->i_specdev);
    int retval;

    if (!pm) return 0;

    poll_wait(filp, &pm->wait, wait);

    retval = cdev_select(pin->i_specdev, mask, fp);

    if (retval < 0) return 0;
    return retval;
}

static int cdev_open(struct inode* pin, struct file_desc* filp)
{
    return cdev_opcl(CDEV_OPEN, pin->i_specdev);
}

static int cdev_release(struct inode* pin, struct file_desc* filp)
{
    return cdev_opcl(CDEV_CLOSE, pin->i_specdev);
}

const struct file_operations cdev_fops = {
    .read = cdev_read,
    .write = cdev_write,
    .ioctl = cdev_ioctl,
    .poll = cdev_poll,
    .open = cdev_open,
    .release = cdev_release,
};
