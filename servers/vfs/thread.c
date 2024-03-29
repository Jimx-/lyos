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
#include <lyos/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/fs.h>
#include <lyos/param.h>
#include <lyos/sysutils.h>
#include <libpthread/pthread.h>
#include <sched.h>
#include "types.h"
#include "path.h"
#include "global.h"
#include "proto.h"
#include <elf.h>
#include "global.h"
#include "thread.h"

static char __thread_stack[NR_WORKER_THREADS * DEFAULT_THREAD_STACK_SIZE]
    __attribute__((aligned(DEFAULT_THREAD_STACK_SIZE)));

static pthread_mutex_t request_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t request_queue_not_empty = PTHREAD_COND_INITIALIZER;
static DEF_LIST(request_queue);

static pthread_mutex_t response_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static DEF_LIST(response_queue);

/* producer-consumer model */
void enqueue_request(MESSAGE* msg)
{
    struct vfs_message* req =
        (struct vfs_message*)malloc(sizeof(struct vfs_message));
    if (!req) panic("enqueue_request(): Out of memory");

    memcpy(&req->msg, msg, sizeof(MESSAGE));
    INIT_LIST_HEAD(&req->list);

    pthread_mutex_lock(&request_queue_mutex);
    list_add(&req->list, &request_queue);

    pthread_cond_signal(&request_queue_not_empty);

    pthread_mutex_unlock(&request_queue_mutex);
}

static struct vfs_message* dequeue_request(struct worker_thread* thread)
{
    struct vfs_message* ret;

    pthread_mutex_lock(&request_queue_mutex);
    while (1) {

        if (list_empty(&request_queue)) {
            pthread_cond_wait(&request_queue_not_empty, &request_queue_mutex);
        }

        if (!list_empty(&request_queue)) {
            ret = list_entry(request_queue.next, struct vfs_message, list);
            list_del(&ret->list);

            if (!list_empty(&request_queue)) {
                pthread_cond_signal(&request_queue_not_empty);
            }

            pthread_mutex_unlock(&request_queue_mutex);
            return ret;
        }
    }
    pthread_mutex_unlock(&request_queue_mutex);

    return NULL;
}

static void enqueue_response(struct vfs_message* res)
{
    pthread_mutex_lock(&response_queue_mutex);
    list_add(&res->list, &response_queue);
    pthread_mutex_unlock(&response_queue_mutex);

    if (fs_sleeping) {
        MESSAGE wakeup_mess;
        wakeup_mess.type = FS_THREAD_WAKEUP;
        send_recv(SEND_NONBLOCK, TASK_FS, &wakeup_mess);
    }
}

void lock_response_queue() { pthread_mutex_lock(&response_queue_mutex); }

void unlock_response_queue() { pthread_mutex_unlock(&response_queue_mutex); }

struct vfs_message* dequeue_response()
{
    struct vfs_message* ret = NULL;

    // pthread_mutex_lock(&response_queue_mutex);
    if (!list_empty(&response_queue)) {
        ret = list_entry(response_queue.next, struct vfs_message, list);
        list_del(&ret->list);
    }
    // pthread_mutex_unlock(&response_queue_mutex);

    return ret;
}

static void handle_request(MESSAGE* msg)
{
    int msgtype = msg->type;

    /* handle kernel signals first */
    if (msg->type == NOTIFY_MSG) {
        switch (msg->source) {
        case CLOCK:
            expire_timer(msg->TIMESTAMP);
            break;
        }
        /* no reply */
        msg->RETVAL = SUSPEND;
        return;
    }

    switch (msgtype) {
    case FS_REGISTER:
        msg->RETVAL = do_register_filesystem(msg);
        break;
    case OPEN:
        msg->FD = do_open(msg);
        break;
    case CLOSE:
        msg->RETVAL = do_close(msg);
        break;
    case READ:
    case WRITE:
        msg->RETVAL = do_rdwt(msg);
        break;
    case IOCTL:
        msg->RETVAL = do_ioctl(msg);
        break;
    case STAT:
        msg->RETVAL = do_stat(msg);
        break;
    case FSTAT:
        msg->RETVAL = do_fstat(msg);
        break;
    case ACCESS:
        msg->RETVAL = do_access(msg);
        break;
    case LSEEK:
        msg->RETVAL = do_lseek(msg);
        break;
    case MKDIR:
        msg->RETVAL = do_mkdir(msg);
        break;
    case UMASK:
        msg->RETVAL = (int)do_umask(msg);
        break;
    case FCNTL:
        msg->RETVAL = do_fcntl(msg);
        break;
    case DUP:
        msg->RETVAL = do_dup(msg);
        break;
    case CHDIR:
        msg->RETVAL = do_chdir(msg);
        break;
    case FCHDIR:
        msg->RETVAL = do_fchdir(msg);
        break;
    case MOUNT:
        msg->RETVAL = do_mount(msg);
        break;
    case CHMOD:
    case FCHMOD:
        msg->RETVAL = do_chmod(msgtype, msg);
        break;
    case GETDENTS:
        msg->RETVAL = do_getdents(msg);
        break;
    case SELECT:
        msg->RETVAL = do_select(msg);
        break;
    case PM_VFS_GETSETID:
        msg->RETVAL = fs_getsetid(msg);
        break;
    case PM_VFS_FORK:
        msg->RETVAL = fs_fork(msg);
        break;
    case PM_VFS_EXEC:
        msg->RETVAL = fs_exec(msg);
        break;
    case EXIT:
        msg->RETVAL = fs_exit(msg);
        break;
    case MM_VFS_REQUEST:
        msg->RETVAL = do_mm_request(msg);
        break;
    case CDEV_REPLY:
    case CDEV_MMAP_REPLY:
    case CDEV_SELECT_REPLY1:
    case CDEV_SELECT_REPLY2:
        cdev_reply(msg);
        msg->RETVAL = SUSPEND;
        break;
    default:
        printl("VFS: unknown message type: %d\n", msgtype);
        msg->RETVAL = ENOSYS;
        break;
    }
}

static int worker_loop(void* arg)
{
    struct worker_thread* self = (struct worker_thread*)arg;
    struct vfs_message* req;

    while (TRUE) {
        req = dequeue_request(self);
        handle_request(&req->msg);
        enqueue_response(req);
    }

    return 0;
}

pid_t create_worker(int id)
{
    int retval;

    if (id >= NR_WORKER_THREADS || id < 0) return -EINVAL;

    struct worker_thread* thread = &workers[id];
    thread->id = id;
    thread->driver_msg = NULL;

    pthread_mutex_init(&thread->event_mutex, NULL);
    pthread_cond_init(&thread->event, NULL);

    /* put thread id on stack */
    char* sp = (char*)__thread_stack + (id + 1) * DEFAULT_THREAD_STACK_SIZE;
    sp -= sizeof(int);
    *(int*)sp = id;

    pid_t pid = clone(worker_loop, sp, CLONE_VM | CLONE_THREAD, (void*)thread);
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

    if ((retval = privctl(thread->endpoint, PRIVCTL_SET_PRIV, &thread->priv)) !=
        0)
        return -retval;
    if ((retval = privctl(thread->endpoint, PRIVCTL_ALLOW, NULL)) != 0)
        return -retval;

    return pid;
}

struct worker_thread* worker_self()
{
    int tmp;
    int* sp = (int*)(((uintptr_t)&tmp & (~(DEFAULT_THREAD_STACK_SIZE - 1))) +
                     DEFAULT_THREAD_STACK_SIZE - sizeof(int));
    return &workers[*sp];
}

void worker_wait()
{
    struct worker_thread* thread = worker_self();

    pthread_mutex_lock(&thread->event_mutex);
    pthread_cond_wait(&thread->event, &thread->event_mutex);
    pthread_mutex_unlock(&thread->event_mutex);
}

void worker_wake(struct worker_thread* thread)
{
    pthread_mutex_lock(&thread->event_mutex);
    pthread_cond_signal(&thread->event);
    pthread_mutex_unlock(&thread->event_mutex);
}

void revive_proc(endpoint_t endpoint, int result)
{
    /* revive a blocked process after returning from a blocking call */
    struct vfs_message* req =
        (struct vfs_message*)malloc(sizeof(struct vfs_message));
    memcpy(&req->msg, msg, sizeof(MESSAGE));
    req->msg.source = endpoint;
    enqueue_response(req);

    send_recv(SEND_NONBLOCK, fproc->endpoint, &self->msg_out);
}
