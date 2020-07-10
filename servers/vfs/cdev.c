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
#include "const.h"
#include "types.h"
#include "global.h"
#include "proto.h"

#include <libdevman/libdevman.h>

struct cdmap cdmap[NR_DEVICES];

static void init_cdev()
{
    int i;
    for (i = 0; i < NR_DEVICES; i++) {
        cdmap[i].driver = NO_TASK;
        cdmap[i].select_busy = 0;
        cdmap[i].select_filp = NULL;
    }
}

static int cdev_update(dev_t dev)
{
    dev_t major = MAJOR(dev);
    cdmap[major].driver = dm_get_cdev_driver(dev);
    if (cdmap[major].driver < 0) cdmap[major].driver = NO_TASK;

    return cdmap[major].driver != NO_TASK;
}

struct fproc* cdev_get(dev_t dev)
{
    static int first = 1;

    if (first) {
        init_cdev();
        first = 0;
    }

    dev_t major = MAJOR(dev);
    if (cdmap[major].driver == NO_TASK)
        if (!cdev_update(dev)) return NULL;

    return vfs_endpt_proc(cdmap[major].driver);
}

struct cdmap* cdev_lookup_by_endpoint(endpoint_t driver_ep)
{
    int i;
    for (i = 0; i < NR_DEVICES; i++) {
        if (cdmap[i].driver != NO_TASK && cdmap[i].driver == driver_ep) {
            return &cdmap[i];
        }
    }

    return NULL;
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

    self->recv_from = driver->endpoint;
    self->msg_driver = &driver_msg;

    worker_wait();
    self->recv_from = NO_TASK;

    return driver_msg.u.m_vfs_cdev_reply.status;
}

int cdev_open(dev_t dev) { return cdev_opcl(CDEV_OPEN, dev); }

int cdev_close(dev_t dev) { return cdev_opcl(CDEV_CLOSE, dev); }

int cdev_io(int op, dev_t dev, endpoint_t src, void* buf, off_t pos,
            size_t count, struct fproc* fp)
{
    struct fproc* driver = cdev_get(dev);
    if (!driver) return ENXIO;

    if (op != CDEV_READ && op != CDEV_WRITE && op != CDEV_IOCTL) {
        return EINVAL;
    }

    MESSAGE driver_msg;
    driver_msg.type = op;
    driver_msg.u.m_vfs_cdev_readwrite.minor = MINOR(dev);
    driver_msg.u.m_vfs_cdev_readwrite.buf = buf;
    driver_msg.u.m_vfs_cdev_readwrite.endpoint = src;
    driver_msg.u.m_vfs_cdev_readwrite.id = src;
    if (op == CDEV_IOCTL) {
        driver_msg.u.m_vfs_cdev_readwrite.request = count;
    } else {
        driver_msg.u.m_vfs_cdev_readwrite.pos = pos;
        driver_msg.u.m_vfs_cdev_readwrite.count = count;
    }

    if (asyncsend3(driver->endpoint, &driver_msg, 0) != 0) {
        panic("vfs: cdev_io send message failed");
    }

    return SUSPEND;
}

int cdev_mmap(dev_t dev, endpoint_t src, void* vaddr, off_t offset,
              size_t length, struct fproc* fp)
{
    struct fproc* driver = cdev_get(dev);
    if (!driver) return ENXIO;

    MESSAGE driver_msg;
    driver_msg.type = CDEV_MMAP;
    driver_msg.u.m_vfs_cdev_mmap.minor = MINOR(dev);
    driver_msg.u.m_vfs_cdev_mmap.addr = (void*)vaddr;
    driver_msg.u.m_vfs_cdev_mmap.endpoint = src;
    driver_msg.u.m_vfs_cdev_mmap.pos = offset;
    driver_msg.u.m_vfs_cdev_mmap.count = length;

    if (asyncsend3(driver->endpoint, &driver_msg, 0) != 0) {
        panic("vfs: cdev_io send message failed");
    }

    return SUSPEND;
}

int cdev_select(dev_t dev, int ops, struct fproc* fp)
{
    struct fproc* driver = cdev_get(dev);
    if (!driver) return ENXIO;

    MESSAGE driver_msg;
    driver_msg.type = CDEV_SELECT;
    driver_msg.u.m_vfs_cdev_select.minor = MINOR(dev);
    driver_msg.u.m_vfs_cdev_select.ops = ops;

    if (asyncsend3(driver->endpoint, &driver_msg, 0) != 0) {
        panic("vfs: cdev_io send message failed");
    }

    return 0;
}

static void cdev_reply_generic(MESSAGE* msg)
{
    endpoint_t endpoint = msg->u.m_vfs_cdev_reply.id;
    int retval;

    struct fproc* fp = vfs_endpt_proc(endpoint);
    if (fp == NULL) return;

    struct worker_thread* worker = fp->worker;
    if (worker != NULL && worker->msg_driver != NULL) {
        *worker->msg_driver = *msg;
        worker->msg_driver = NULL;
        worker_wake(worker);
    } else {
        MESSAGE reply_msg;
        reply_msg.type = SYSCALL_RET;
        retval = msg->u.m_vfs_cdev_reply.status;
        reply_msg.RETVAL = (retval == -EINTR) ? -EAGAIN : retval;
        revive_proc(endpoint, &reply_msg);
    }
}

static void cdev_mmap_reply(endpoint_t endpoint, void* retaddr, int retval)
{
    MESSAGE reply_msg;
    memset(&reply_msg, 0, sizeof(MESSAGE));

    reply_msg.type = MM_VFS_REPLY;
    reply_msg.MMRRESULT = retval;
    reply_msg.MMRENDPOINT = endpoint;
    reply_msg.MMRBUF = retaddr;

    revive_proc(TASK_MM, &reply_msg);
}

int cdev_reply(MESSAGE* msg)
{
    switch (msg->type) {
    case CDEV_REPLY:
        cdev_reply_generic(msg);
        break;
    case CDEV_MMAP_REPLY:
        cdev_mmap_reply(msg->u.m_vfs_cdev_mmap_reply.endpoint,
                        msg->u.m_vfs_cdev_mmap_reply.retaddr,
                        msg->u.m_vfs_cdev_mmap_reply.status);
        break;
    case CDEV_SELECT_REPLY1:
        do_select_cdev_reply1(msg->source, msg->DEVICE, msg->RETVAL);
        break;
    case CDEV_SELECT_REPLY2:
        do_select_cdev_reply2(msg->source, msg->DEVICE, msg->RETVAL);
        break;
    }

    return 0;
}
