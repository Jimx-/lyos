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

#include <lyos/type.h>
#include <sys/types.h>
#include <lyos/config.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/fs.h>
#include <lyos/proc.h>
#include <lyos/global.h>
#include <lyos/proto.h>
#include <lyos/ipc.h>
#include <lyos/param.h>
#include <lyos/sysutils.h>
#include <sched.h>
#include "path.h"
#include "global.h"
#include "proto.h"
#include <elf.h>
#include "global.h"
#include "thread.h"

PRIVATE DEF_SPINLOCK(request_queue_lock);
PRIVATE DEF_LIST(request_queue);
int queued_request = 0;

PRIVATE DEF_SPINLOCK(response_queue_lock);
PRIVATE DEF_LIST(response_queue);

PRIVATE void thread_wait(struct worker_thread* thread)
{
    MESSAGE wait_mess;

    thread->state = WT_WAITING;
    send_recv(RECEIVE, TASK_FS, &wait_mess);
    thread->state = WT_RUNNING;
    
    if (wait_mess.type != FS_THREAD_WAKEUP) panic("[Thread %d]: unknown wakeup message from vfs(%d)\n", wait_mess.type);
}

PRIVATE void thread_wakeup(struct worker_thread* thread)
{
    if (thread->state == WT_RUNNING) return;

    MESSAGE wakeup_mess;
    wakeup_mess.type = FS_THREAD_WAKEUP;
    send_recv(SEND_NONBLOCK, thread->endpoint, &wakeup_mess);
}

PUBLIC void enqueue_request(MESSAGE* msg)
{
    struct vfs_message* req = (struct vfs_message*) malloc(sizeof(struct vfs_message));
    if (!req) panic("enqueue_request(): Out of memory");

    memcpy(&req->msg, msg, sizeof(MESSAGE));
    INIT_LIST_HEAD(&req->list);

    spinlock_lock(&request_queue_lock);
    list_add(&req->list, &request_queue);
    queued_request++;
    spinlock_unlock(&request_queue_lock);

    /* try to wake up some threads */
    int num = queued_request < nr_workers ? queued_request : nr_workers;
    int i;
    for (i = 0; i < NR_WORKER_THREADS && num > 0; i++) {
        if (workers[i].state != WT_WAITING) continue;
        num--;
        thread_wakeup(&workers[i]);
    }
}

PRIVATE struct vfs_message* dequeue_request(struct worker_thread* thread)
{
    struct vfs_message* ret;

    while (1) {
        spinlock_lock(&request_queue_lock);
        if (!list_empty(&request_queue)) {
            ret = list_entry(request_queue.next, struct vfs_message, list);
            list_del(&ret->list);
            queued_request--;
            spinlock_unlock(&request_queue_lock);
            return ret;
        }
        spinlock_unlock(&request_queue_lock);

        thread_wait(thread);
    }

    return NULL;
}

PRIVATE void enqueue_response(struct vfs_message* res)
{
    spinlock_lock(&response_queue_lock);
    list_add(&res->list, &response_queue);
    spinlock_unlock(&response_queue_lock);

    if (fs_sleeping) {
        MESSAGE wakeup_mess;
        wakeup_mess.type = FS_THREAD_WAKEUP;
        send_recv(SEND_NONBLOCK, TASK_FS, &wakeup_mess);
    }
}

PUBLIC struct vfs_message* dequeue_response()
{
    struct vfs_message* ret = NULL;

    spinlock_lock(&response_queue_lock);
    if (!list_empty(&response_queue)) {
        ret = list_entry(response_queue.next, struct vfs_message, list);
        list_del(&ret->list);
    }
    spinlock_unlock(&response_queue_lock);

    return ret;
}

PRIVATE void handle_request(MESSAGE* msg)
{
    int msgtype = msg->type;

    switch (msgtype) {
    case OPEN:
        msg->FD = do_open(msg);
        break;
    case CLOSE:
        msg->RETVAL = do_close(msg);
        break;
    case RESUME_PROC:
        msg->RETVAL = 0;
        msg->source = msg->PROC_NR;
        break;
    case PM_VFS_EXEC:
        msg->RETVAL = fs_exec(msg);
        break;
    default:
        msg->RETVAL = ENOSYS;
        break;
    }
}

PRIVATE int worker_loop(void* arg)
{
    struct worker_thread* self = (struct worker_thread*) arg;
    struct vfs_message* req;
    
    while (1) {
        req = dequeue_request(self);
        handle_request(&req->msg);
        enqueue_response(req);
    }
}

PUBLIC pid_t create_worker(int id)
{
    int retval;

    if (id >= NR_WORKER_THREADS || id < 0) return -EINVAL;

    struct worker_thread* thread = &workers[id];
    thread->id = id;
    thread->state = WT_WAITING;

    pid_t pid = clone(worker_loop, (char*)thread->stack + sizeof(thread->stack), CLONE_VM | CLONE_THREAD, (void*) thread);
    if (pid < 0) return pid;

    retval = get_procep(pid, &thread->endpoint);
    if (retval) return -retval;

    /* prepare priv structure */
    struct priv* priv = &thread->priv;

    priv->flags = TASK_FLAGS | PRF_DYN_ID;

    /* syscall mask */
    int j;
    for (j = 0; j < BITCHUNKS(NR_SYS_CALLS); j++) {
        priv->syscall_mask[j] = ~0;
    }

    if ((retval = privctl(thread->endpoint, PRIVCTL_SET_PRIV, &thread->priv)) != 0) return -retval;
    if ((retval = privctl(thread->endpoint, PRIVCTL_ALLOW, NULL)) != 0) return -retval;

    return pid;
}
