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
#include <stdint.h>
#include <string.h>
#include <lyos/proc.h>
#include <lyos/driver.h>
#include <lyos/fs.h>
#include <lyos/sysutils.h>
#include <sys/socket.h>

#include "const.h"
#include "types.h"
#include "global.h"
#include "proto.h"
#include "poll.h"

struct sdmap {
    char label[FS_LABEL_MAX + 1];
    unsigned int num;
    endpoint_t endpoint;
};

static struct sdmap sdmap[NR_SOCKDEVS];
static struct sdmap* pfmap[PF_MAX];

void init_sdev(void)
{
    int i;
    for (i = 0; i < NR_SOCKDEVS; i++) {
        sdmap[i].num = i + 1;
        sdmap[i].endpoint = NO_TASK;
    }

    memset(pfmap, 0, sizeof(pfmap));
}

int sdev_mapdriver(const char* label, endpoint_t endpoint, const int* domains,
                   int nr_domains)
{
    int i;
    struct sdmap* sdp = NULL;

    for (i = 0; i < NR_SOCKDEVS; i++) {
        if (sdmap[i].endpoint != NO_TASK && !strcmp(sdmap[i].label, label)) {
            sdp = &sdmap[i];
            break;
        }
    }

    for (i = 0; i < nr_domains; i++) {
        if (domains[i] <= 0 || domains[i] >= PF_MAX) return EINVAL;
        if (domains[i] == PF_UNSPEC) return EINVAL;
        if (pfmap[domains[i]] != sdp) return EINVAL;
    }

    if (sdp) {
        for (i = 0; i < PF_MAX; i++) {
            if (pfmap[i] == sdp) pfmap[i] = NULL;
        }
    }

    if (!sdp) {
        for (i = 0; i < NR_SOCKDEVS; i++) {
            if (sdmap[i].endpoint == NO_TASK) {
                sdp = &sdmap[i];
                break;
            }
        }

        if (i == NR_SOCKDEVS) return ENOMEM;
    }

    sdp->endpoint = endpoint;
    strlcpy(sdp->label, label, sizeof(sdp->label));

    for (i = 0; i < nr_domains; i++) {
        pfmap[domains[i]] = sdp;
    }

    return 0;
}

static struct sdmap* get_sdmap_by_domain(int domain)
{
    if (domain <= 0 || domain >= PF_MAX) return NULL;

    return pfmap[domain];
}

static dev_t make_sdmap_dev(struct sdmap* sdp, sockid_t sockid)
{
    return (dev_t)(((uint32_t)sdp->num << 24) | ((uint32_t)sockid & 0xffffff));
}

static int sdev_sendrec(struct sdmap* sdp, MESSAGE* msg)
{
    int retval;

    if ((retval = asyncsend3(sdp->endpoint, msg, 0)) != 0) return retval;

    self->recv_from = sdp->endpoint;
    self->msg_driver = msg;

    worker_wait();
    self->recv_from = NO_TASK;

    return 0;
}

int sdev_socket(endpoint_t src, int domain, int type, int protocol, dev_t* dev,
                int pair)
{
    struct sdmap* sdp;
    MESSAGE msg;
    int sock_id, sock_id2;
    int retval;

    if (!(sdp = get_sdmap_by_domain(domain))) return EAFNOSUPPORT;

    memset(&msg, 0, sizeof(msg));
    msg.type = pair ? SDEV_SOCKETPAIR : SDEV_SOCKET;
    msg.u.m_sockdriver_socket.req_id = src;
    msg.u.m_sockdriver_socket.endpoint = src;
    msg.u.m_sockdriver_socket.domain = domain;
    msg.u.m_sockdriver_socket.type = type;
    msg.u.m_sockdriver_socket.protocol = protocol;

    if (sdev_sendrec(sdp, &msg)) {
        panic("vfs: sdev_socket failed to send request");
    }

    if (msg.type != SDEV_SOCKET_REPLY) return EIO;

    retval = msg.u.m_sockdriver_socket_reply.status;
    if (retval) return retval;

    sock_id = msg.u.m_sockdriver_socket_reply.sock_id;
    sock_id2 = msg.u.m_sockdriver_socket_reply.sock_id2;

    dev[0] = make_sdmap_dev(sdp, sock_id);

    return 0;
}

int sdev_close(dev_t dev) { return 0; }

void sdev_reply(MESSAGE* msg)
{
    int req_id;
    struct fproc* fp;

    switch (msg->type) {
    case SDEV_SOCKET_REPLY:
        req_id = msg->u.m_sockdriver_socket_reply.req_id;
        break;
    }

    fp = vfs_endpt_proc(req_id);
    if (fp == NULL) return;

    struct worker_thread* worker = fp->worker;
    if (worker != NULL && worker->msg_driver != NULL) {
        *worker->msg_driver = *msg;
        worker->msg_driver = NULL;
        worker_wake(worker);
    }
}
