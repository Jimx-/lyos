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

#include <lyos/ipc.h>
#include "errno.h"
#include "lyos/const.h"
#include "string.h"
#include <lyos/fs.h>
#include <lyos/sysutils.h>
#include <asm/page.h>
#include "proto.h"
#include "global.h"

#define ARG_SIZE_MAX 80

struct vfs_request {
    MESSAGE req_msg;
    vfs_callback_t callback;
    char arg[ARG_SIZE_MAX];

    struct vfs_request* next;
};

static struct vfs_request *queue = NULL, *active = NULL;

static void process_queue()
{
    active = queue;
    queue = queue->next;

    if (asyncsend3(TASK_FS, &active->req_msg, 0) != 0)
        panic("async send vfs request failed");
}

int enqueue_vfs_request(struct mmproc* mmp, int req_type, int fd, void* addr,
                        off_t offset, size_t len, vfs_callback_t callback,
                        void* arg, int arg_len)
{
    struct vfs_request* request;
    SLABALLOC(request);
    if (!request) return ENOMEM;

    MESSAGE* m = &request->req_msg;

    m->type = MM_VFS_REQUEST;
    m->MMRTYPE = req_type;
    m->MMRFD = fd;
    m->MMRENDPOINT = mmp->endpoint;
    m->MMROFFSET = offset;
    m->MMRLENGTH = len;
    m->MMRBUF = (void*)addr;

    request->callback = callback;
    if (arg) memcpy((char*)request->arg, arg, arg_len);

    request->next = queue;
    queue = request;

    if (!active) process_queue();

    return 0;
}

int do_vfs_reply()
{
    if (!active) return EPERM;

    struct vfs_request* orig = active;
    struct mmproc* mmp = endpt_mmproc(mm_msg.MMRENDPOINT);
    vfs_callback_t cbf = active->callback;
    active = NULL;

    if (cbf) cbf(mmp, &mm_msg, orig->arg);

    SLABFREE(orig);

    if (queue && !active) process_queue();

    return SUSPEND;
}
