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
#include <lyos/global.h>
#include <lyos/proto.h>
#include <lyos/driver.h>

#include "libsockdriver.h"
#include "mq.h"
#include "proto.h"
#include "worker.h"

#define MAX_THREADS      128
#define WORKER_STACKSIZE ((size_t)0x4000)

static const char* name = "sockdriver_async";

static coro_mutex_t queue_event_mutex;
static coro_cond_t queue_event;

static int running = FALSE;

static size_t num_threads;
static struct worker_thread threads[MAX_THREADS];
static struct worker_thread* self = NULL;

static void enqueue(const MESSAGE* msg)
{
    if (!mq_enqueue(msg)) {
        panic("%s: message queue full", name);
    }

    if (coro_mutex_lock(&queue_event_mutex) != 0) {
        panic("%s: failed to lock event mutex", name);
    }

    if (coro_cond_signal(&queue_event) != 0) {
        panic("%s: failed to signal worker event", name);
    }

    if (coro_mutex_unlock(&queue_event_mutex) != 0) {
        panic("%s: failed to lock queue event mutex", name);
    }
}

static int dequeue(struct worker_thread* wp, MESSAGE* msg)
{
    struct worker_thread* thread;

    do {
        thread = self;

        if (coro_mutex_lock(&queue_event_mutex) != 0) {
            panic("%s: failed to lock event mutex", name);
        }

        if (coro_cond_wait(&queue_event, &queue_event_mutex) != 0) {
            panic("%s: failed to wait for worker event", name);
        }

        if (coro_mutex_unlock(&queue_event_mutex) != 0) {
            panic("%s: failed to lock queue event mutex", name);
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
        panic("%s: failed to initialize mutex", name);
    }

    if (coro_cond_init(&queue_event, NULL) != 0) {
        panic("%s: failed to initialize condition variable", name);
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

    while (running) {
        if (!mq_dequeue(&msg)) {
            if (!dequeue(self, &msg)) {
                break;
            }
        }

        self->state = WS_BUSY;

        sockdriver_process(&msg);

        self->state = WS_RUNNING;
    }

    return NULL;
}

static void create_worker(struct worker_thread* wp, worker_id_t wid)
{
    coro_attr_t attr;

    wp->id = wid;

    coro_attr_init(&attr);
    coro_attr_setstacksize(&attr, WORKER_STACKSIZE);

    if (coro_mutex_init(&wp->event_mutex, NULL) != 0) {
        panic("%s: failed to initialize mutex", name);
    }

    if (coro_cond_init(&wp->event, NULL) != 0) {
        panic("%s: failed to initialize condition variable", name);
    }

    if (coro_thread_create(&wp->thread, &attr, worker_main, wp) != 0) {
        panic("%s: failed to start worker thread", name);
    }

    coro_attr_destroy(&attr);
}

static void dispatch_message(MESSAGE* msg)
{
    int i;
    struct worker_thread* wp;

    if (msg->type == NOTIFY_MSG) {
        sockdriver_process(msg);

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
            create_worker(wp, i);
        }
    }

    enqueue(msg);
}

static void yield_all(void)
{
    coro_yield_all();

    self = NULL;
}

void sockdriver_sleep(void)
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

void sockdriver_wakeup_worker(struct worker_thread* wp)
{
    if (coro_mutex_lock(&wp->event_mutex) != 0) {
        panic("%s: failed to lock event mutex", name);
    }

    if (coro_cond_signal(&wp->event) != 0) {
        panic("%s: failed to signal worker event", name);
    }

    if (coro_mutex_unlock(&wp->event_mutex) != 0) {
        panic("%s: failed to lock queue event mutex", name);
    }
}

void sockdriver_wakeup(sockdriver_worker_id_t tid)
{
    struct worker_thread* wp = &threads[tid];

    sockdriver_wakeup_worker(wp);
}

void sockdriver_yield(void)
{
    struct worker_thread* thread = self;

    coro_yield();

    self = thread;
}

void sockdriver_set_workers(size_t num_workers)
{
    if (num_workers > MAX_THREADS) {
        num_workers = MAX_THREADS;
    }

    num_threads = num_workers;
}

struct worker_thread* sockdriver_worker(void)
{
    if (!self) {
        panic("%s: try to get worker on main thread");
    }

    return self;
}

sockdriver_worker_id_t sockdriver_worker_id(void)
{
    if (!self) {
        panic("%s: try to query worker ID on main thread");
    }

    return self->id;
}

void sockdriver_task(size_t num_workers)
{
    MESSAGE msg;

    if (!running) {
        init_main_thread(num_workers);
        running = TRUE;
    }

    while (running) {
        send_recv(RECEIVE_ASYNC, ANY, &msg);

        dispatch_message(&msg);

        yield_all();
    }
}
