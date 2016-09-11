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
#include "types.h"
#include "path.h"
#include "global.h"
#include "proto.h"
#include <elf.h>
#include "global.h"
#include "thread.h"

PRIVATE char __thread_stack[NR_WORKER_THREADS * DEFAULT_THREAD_STACK_SIZE] __attribute__ ((aligned (DEFAULT_THREAD_STACK_SIZE)));

PRIVATE pthread_mutex_t request_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
PRIVATE pthread_cond_t request_queue_not_empty = PTHREAD_COND_INITIALIZER;
PRIVATE DEF_LIST(request_queue);

PRIVATE pthread_mutex_t response_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
PRIVATE DEF_LIST(response_queue);

/* producer-consumer model */
PUBLIC void enqueue_request(MESSAGE* msg)
{
    struct vfs_message* req = (struct vfs_message*) malloc(sizeof(struct vfs_message));
    if (!req) panic("enqueue_request(): Out of memory");

    memcpy(&req->msg, msg, sizeof(MESSAGE));
    INIT_LIST_HEAD(&req->list);

    pthread_mutex_lock(&request_queue_mutex);
    list_add(&req->list, &request_queue);

    if (!list_empty(&request_queue)) {
        pthread_cond_signal(&request_queue_not_empty);
    }

    pthread_mutex_unlock(&request_queue_mutex);
}

PRIVATE struct vfs_message* dequeue_request(struct worker_thread* thread)
{
    struct vfs_message* ret;

    while (1) {
        pthread_mutex_lock(&request_queue_mutex);

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
        pthread_mutex_unlock(&request_queue_mutex);
    }

    return NULL;
}

PRIVATE void enqueue_response(struct vfs_message* res)
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

PUBLIC struct vfs_message* dequeue_response()
{
    struct vfs_message* ret = NULL;

    pthread_mutex_lock(&response_queue_mutex);
    if (!list_empty(&response_queue)) {
        ret = list_entry(response_queue.next, struct vfs_message, list);
        list_del(&ret->list);
    }
    pthread_mutex_unlock(&response_queue_mutex);

    return ret;
}

PRIVATE void handle_request(MESSAGE* msg)
{
    int msgtype = msg->type;

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
        msg->CNT = do_rdwt(msg);
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
    case RESUME_PROC:
        msg->RETVAL = 0;
        msg->source = msg->PROC_NR;
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
 
    while (TRUE) {
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

    pthread_mutex_init(&thread->event_mutex, NULL);
    pthread_cond_init(&thread->event, NULL);
    
    /* put thread id on stack */
    char* sp = (char*)__thread_stack + (id + 1) * DEFAULT_THREAD_STACK_SIZE;
    sp -= sizeof(int);
    *(int*)sp = id;

    pid_t pid = clone(worker_loop, sp, CLONE_VM | CLONE_THREAD, (void*) thread);
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

PUBLIC struct worker_thread* worker_self()
{
    int tmp;
    int* sp = (int*)(((vir_bytes)&tmp & (~(DEFAULT_THREAD_STACK_SIZE-1))) + DEFAULT_THREAD_STACK_SIZE - sizeof(int));
    return &workers[*sp];
}

PUBLIC void worker_wait()
{
    struct worker_thread* thread = worker_self();

    pthread_mutex_lock(&thread->event_mutex);
    pthread_cond_wait(&thread->event, &thread->event_mutex);
    pthread_mutex_unlock(&thread->event_mutex);
}

PUBLIC void worker_wake(struct worker_thread* thread)
{
    pthread_mutex_lock(&thread->event_mutex);
    pthread_cond_signal(&thread->event);
    pthread_mutex_unlock(&thread->event_mutex);
}


