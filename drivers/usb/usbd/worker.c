#include <lyos/types.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/driver.h>
#include <lyos/sysutils.h>

#include <libasyncdriver/mq.h>

#include "worker.h"
#include "proto.h"

#define MAX_THREADS      128
#define WORKER_STACKSIZE ((size_t)0x4000)

#define NAME "usbd"

static coro_mutex_t queue_event_mutex;
static coro_cond_t queue_event;

static int running = FALSE;

static int inited = FALSE;
static coro_mutex_t init_event_mutex;
static coro_cond_t init_event;

static size_t num_threads;
static struct worker_thread threads[MAX_THREADS];
static struct worker_thread* self = NULL;

static void enqueue(const MESSAGE* msg)
{
    if (!mq_enqueue(msg)) {
        panic(NAME "%s: message queue full");
    }

    if (coro_mutex_lock(&queue_event_mutex) != 0) {
        panic(NAME ": failed to lock event mutex");
    }

    if (coro_cond_signal(&queue_event) != 0) {
        panic(NAME ": failed to signal worker event");
    }

    if (coro_mutex_unlock(&queue_event_mutex) != 0) {
        panic(NAME ": failed to lock queue event mutex");
    }
}

static int dequeue(struct worker_thread* wp, MESSAGE* msg)
{
    struct worker_thread* thread;

    do {
        thread = self;

        if (coro_mutex_lock(&queue_event_mutex) != 0) {
            panic(NAME ": failed to lock event mutex");
        }

        if (coro_cond_wait(&queue_event, &queue_event_mutex) != 0) {
            panic(NAME ": failed to wait for worker event");
        }

        if (coro_mutex_unlock(&queue_event_mutex) != 0) {
            panic(NAME ": failed to lock queue event mutex");
        }

        self = thread;

        if (!running) {
            return FALSE;
        }
    } while (!mq_dequeue(msg));

    return TRUE;
}

static void init_main_thread(size_t num_workers)
{
    int i;

    coro_init();
    mq_init();

    if (coro_mutex_init(&queue_event_mutex, NULL) != 0) {
        panic(NAME ": failed to initialize mutex");
    }

    if (coro_cond_init(&queue_event, NULL) != 0) {
        panic(NAME ": failed to initialize condition variable");
    }

    if (coro_mutex_init(&init_event_mutex, NULL) != 0) {
        panic(NAME ": failed to initialize init event mutex");
    }

    if (coro_cond_init(&init_event, NULL) != 0) {
        panic(NAME ": failed to initialize init event condition variable");
    }

    for (i = 0; i < MAX_THREADS; i++) {
        threads[i].state = WS_DEAD;
    }

    num_threads = num_workers;

    self = NULL;
}

static void* worker_main(void* arg)
{
    self = (struct worker_thread*)arg;
    MESSAGE msg;

    if (self->init_func) {
        self->state = WS_BUSY;
        self->init_func();
        self->state = WS_RUNNING;

        if (coro_mutex_lock(&init_event_mutex) != 0) {
            panic(NAME ": failed to lock init event mutex");
        }

        inited = TRUE;

        if (coro_cond_signal(&init_event) != 0) {
            panic(NAME ": failed to signal init event");
        }

        if (coro_mutex_unlock(&init_event_mutex) != 0) {
            panic(NAME ": failed to unlock init event mutex");
        }
    }

    if (coro_mutex_lock(&init_event_mutex) != 0) {
        panic(NAME ": failed to lock init event mutex");
    }
    while (!inited) {
        if (coro_cond_wait(&init_event, &init_event_mutex) != 0) {
            panic(NAME ": failed to wait for init event");
        }
    }
    if (coro_mutex_unlock(&init_event_mutex) != 0) {
        panic(NAME ": failed to unlock init event mutex");
    }

    while (running) {
        if (!mq_dequeue(&msg)) {
            if (!dequeue(self, &msg)) {
                break;
            }
        }

        self->state = WS_BUSY;

        usbd_process(&msg);

        self->state = WS_RUNNING;
    }

    return NULL;
}

static void create_worker(struct worker_thread* wp, worker_id_t wid,
                          void (*init_func)(void))
{
    coro_attr_t attr;

    wp->id = wid;
    wp->init_func = init_func;

    coro_attr_init(&attr);
    coro_attr_setstacksize(&attr, WORKER_STACKSIZE);

    if (coro_mutex_init(&wp->event_mutex, NULL) != 0) {
        panic(NAME ": failed to initialize mutex");
    }

    if (coro_cond_init(&wp->event, NULL) != 0) {
        panic(NAME ": failed to initialize condition variable");
    }

    if (coro_thread_create(&wp->thread, &attr, worker_main, wp) != 0) {
        panic(NAME ": failed to start worker thread");
    }

    coro_attr_destroy(&attr);
}

static void dispatch_message(MESSAGE* msg)
{
    int i;
    struct worker_thread* wp;

    if (msg->type == NOTIFY_MSG) {
        usbd_process(msg);

        return;
    }

    for (i = 0; i < num_threads; i++) {
        if (threads[i].state != WS_BUSY) {
            break;
        }
    }

    if (i < num_threads) {
        wp = &threads[i];

        if (wp->state == WS_DEAD) {
            create_worker(wp, i, NULL);
        }
    }

    enqueue(msg);
}

static void yield_all(void)
{
    coro_yield_all();

    self = NULL;
}

void worker_sleep(void)
{
    struct worker_thread* thread = self;

    if (coro_mutex_lock(&thread->event_mutex) != 0) {
        panic(NAME ": failed to lock event mutex");
    }

    if (coro_cond_wait(&thread->event, &thread->event_mutex) != 0) {
        panic(NAME ": failed to wait on worker event");
    }

    if (coro_mutex_unlock(&thread->event_mutex) != 0) {
        panic(NAME ": failed to lock event mutex");
    }

    self = thread;
}

void worker_wakeup(struct worker_thread* wp)
{
    if (coro_mutex_lock(&wp->event_mutex) != 0) {
        panic(NAME ": failed to lock event mutex");
    }

    if (coro_cond_signal(&wp->event) != 0) {
        panic(NAME ": failed to signal worker event");
    }

    if (coro_mutex_unlock(&wp->event_mutex) != 0) {
        panic(NAME ": failed to lock queue event mutex");
    }
}

void worker_yield(void)
{
    struct worker_thread* thread = self;

    coro_yield();

    self = thread;
}

void worker_set_workers(size_t num_workers)
{
    if (num_workers > MAX_THREADS) {
        num_workers = MAX_THREADS;
    }

    num_threads = num_workers;
}

struct worker_thread* worker_self(void)
{
    if (!self) {
        panic("%s: try to get worker on main thread");
    }

    return self;
}

void worker_main_thread(void (*init_func)(void))
{
    MESSAGE msg;

    if (!init_func) inited = TRUE;

    if (!running) {
        init_main_thread(MAX_THREADS);
        running = TRUE;
    }

    if (init_func) {
        struct worker_thread* wp = &threads[0];
        create_worker(wp, 0, init_func);
        yield_all();
    }

    while (running) {
        send_recv(RECEIVE_ASYNC, ANY, &msg);

        dispatch_message(&msg);

        yield_all();
    }
}

worker_id_t current_worker_id(void)
{
    if (!self) {
        panic(NAME ": try to query worker ID on main thread");
    }

    return self->id;
}

void worker_async_sleep(void)
{
    struct worker_thread* thread = self;

    if (coro_mutex_lock(&thread->event_mutex) != 0) {
        panic("failed to lock event mutex");
    }

    if (coro_cond_wait(&thread->event, &thread->event_mutex) != 0) {
        panic("failed to wait on worker event");
    }

    if (coro_mutex_unlock(&thread->event_mutex) != 0) {
        panic("failed to lock event mutex");
    }

    self = thread;
}

void worker_async_wakeup(worker_id_t tid)
{
    struct worker_thread* wp = &threads[tid];

    if (coro_mutex_lock(&wp->event_mutex) != 0) {
        panic(NAME ": failed to lock event mutex");
    }

    if (coro_cond_signal(&wp->event) != 0) {
        panic(NAME ": failed to signal worker event");
    }

    if (coro_mutex_unlock(&wp->event_mutex) != 0) {
        panic(NAME ": failed to unlock event mutex");
    }
}
