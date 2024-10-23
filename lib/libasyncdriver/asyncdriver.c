#include <lyos/ipc.h>
#include <stdio.h>
#include <errno.h>
#include <lyos/const.h>
#include <lyos/sysutils.h>

#include <libcoro/libcoro.h>
#include <libasyncdriver/libasyncdriver.h>

#include "mq.h"

#define MAX_THREADS      32
#define WORKER_STACKSIZE ((size_t)0x4000)

typedef unsigned int worker_id_t;

typedef enum {
    WS_DEAD,
    WS_RUNNING,
    WS_BUSY,
} worker_state_t;

struct worker_thread {
    worker_id_t id;
    worker_state_t state;
    coro_thread_t thread;
    coro_mutex_t event_mutex;
    coro_cond_t event;
    void (*init_func)(void);

    MESSAGE* msg_recv;
    endpoint_t recv_from;
};

static coro_mutex_t queue_event_mutex;
static coro_cond_t queue_event;

static int running = FALSE;

static int inited = FALSE;
static coro_mutex_t init_event_mutex;
static coro_cond_t init_event;

static size_t num_threads;
static struct worker_thread threads[MAX_THREADS];
static struct worker_thread* self = NULL;

static const struct asyncdriver* async_drv = NULL;

static void enqueue(const MESSAGE* msg)
{
    if (!mq_enqueue(msg)) {
        panic("%s: message queue full", async_drv->name);
    }

    if (coro_mutex_lock(&queue_event_mutex) != 0) {
        panic("%s: failed to lock event mutex", async_drv->name);
    }

    if (coro_cond_signal(&queue_event) != 0) {
        panic("%s: failed to signal worker event", async_drv->name);
    }

    if (coro_mutex_unlock(&queue_event_mutex) != 0) {
        panic("%s: failed to lock queue event mutex", async_drv->name);
    }
}

static int dequeue(struct worker_thread* wp, MESSAGE* msg)
{
    struct worker_thread* thread;

    do {
        thread = self;

        if (coro_mutex_lock(&queue_event_mutex) != 0) {
            panic("%s: failed to lock event mutex", async_drv->name);
        }

        if (coro_cond_wait(&queue_event, &queue_event_mutex) != 0) {
            panic("%s: failed to wait for worker event", async_drv->name);
        }

        if (coro_mutex_unlock(&queue_event_mutex) != 0) {
            panic("%s: failed to lock queue event mutex", async_drv->name);
        }

        self = thread;

        if (!running) {
            return FALSE;
        }
    } while (!mq_dequeue(msg));

    return TRUE;
}

static void init_main_thread(const struct asyncdriver* asd, size_t num_workers)
{
    int i;

    async_drv = asd;

    coro_init();
    mq_init();

    if (coro_mutex_init(&queue_event_mutex, NULL) != 0) {
        panic("%s: failed to initialize mutex", async_drv->name);
    }

    if (coro_cond_init(&queue_event, NULL) != 0) {
        panic("%s: failed to initialize condition variable", async_drv->name);
    }

    if (coro_mutex_init(&init_event_mutex, NULL) != 0) {
        panic("%s: failed to initialize init event mutex", async_drv->name);
    }

    if (coro_cond_init(&init_event, NULL) != 0) {
        panic("%s: failed to initialize init event condition variable",
              async_drv->name);
    }

    for (i = 0; i < MAX_THREADS; i++) {
        threads[i].state = WS_DEAD;
    }

    asyncdrv_set_workers(num_workers);

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
            panic("%s: failed to lock init event mutex", async_drv->name);
        }

        inited = TRUE;

        if (coro_cond_signal(&init_event) != 0) {
            panic("%s: failed to signal init event", async_drv->name);
        }

        if (coro_mutex_unlock(&init_event_mutex) != 0) {
            panic("%s: failed to unlock init event mutex", async_drv->name);
        }
    }

    if (coro_mutex_lock(&init_event_mutex) != 0) {
        panic("%s: failed to lock init event mutex", async_drv->name);
    }
    while (!inited) {
        if (coro_cond_wait(&init_event, &init_event_mutex) != 0) {
            panic("%s: failed to wait for init event", async_drv->name);
        }
    }
    if (coro_mutex_unlock(&init_event_mutex) != 0) {
        panic("%s: failed to unlock init event mutex", async_drv->name);
    }

    while (running) {
        if (!mq_dequeue(&msg)) {
            if (!dequeue(self, &msg)) {
                break;
            }
        }

        self->state = WS_BUSY;

        async_drv->process(&msg);

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
        panic("%s: failed to initialize mutex", async_drv->name);
    }

    if (coro_cond_init(&wp->event, NULL) != 0) {
        panic("%s: failed to initialize condition variable", async_drv->name);
    }

    if (coro_thread_create(&wp->thread, &attr, worker_main, wp) != 0) {
        panic("%s: failed to start worker thread", async_drv->name);
    }

    coro_attr_destroy(&attr);
}

static void dispatch_message(MESSAGE* msg)
{
    int i;
    struct worker_thread* wp;

    if (async_drv->process_on_thread && !async_drv->process_on_thread(msg)) {
        async_drv->process(msg);

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

void asyncdrv_sleep(void)
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

static void worker_wakeup(struct worker_thread* wp)
{
    if (coro_mutex_lock(&wp->event_mutex) != 0) {
        panic("%s: failed to lock event mutex", async_drv->name);
    }

    if (coro_cond_signal(&wp->event) != 0) {
        panic("%s: failed to signal worker event", async_drv->name);
    }

    if (coro_mutex_unlock(&wp->event_mutex) != 0) {
        panic("%s: failed to unlock event mutex", async_drv->name);
    }
}

void asyncdrv_wakeup(async_worker_id_t tid)
{
    struct worker_thread* wp = &threads[tid];

    worker_wakeup(wp);
}

void asyncdrv_set_workers(size_t num_workers)
{
    if (num_workers > MAX_THREADS) {
        num_workers = MAX_THREADS;
    }

    num_threads = num_workers;
}

async_worker_id_t asyncdrv_worker_id(void)
{
    if (!self) {
        panic("%s: try to query worker ID on main thread", async_drv->name);
    }

    return self->id;
}

void asyncdrv_task(const struct asyncdriver* asd, size_t num_workers,
                   void (*init_func)(void))
{
    MESSAGE msg;

    if (!init_func) inited = TRUE;

    if (!running) {
        init_main_thread(asd, num_workers);
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

static int sendmsg(endpoint_t dest, struct worker_thread* wp)
{
    wp->recv_from = dest;

    return asyncsend3(dest, wp->msg_recv, 0);
}

int asyncdrv_sendrec(endpoint_t dest, MESSAGE* msg)
{
    int retval;

    self->msg_recv = msg;

    retval = sendmsg(dest, self);
    if (retval) {
        return retval;
    }

    asyncdrv_sleep();

    return 0;
}

void asyncdrv_reply(async_worker_id_t tid, const MESSAGE* msg)
{
    struct worker_thread* wp = &threads[tid];

    if (wp->recv_from != msg->source) return;

    if (wp->msg_recv == NULL) return;

    *wp->msg_recv = *msg;
    wp->msg_recv = NULL;
    wp->recv_from = NO_TASK;

    worker_wakeup(wp);
}
