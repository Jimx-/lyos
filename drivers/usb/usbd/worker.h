#ifndef _USBD_WORKER_H_
#define _USBD_WORKER_H_

#include <lyos/list.h>
#include <libcoro/libcoro.h>

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

    unsigned int events;
    struct list_head list;
};

void worker_main_thread(void (*init_func)(void));

worker_id_t current_worker_id(void);

void worker_async_sleep(void);
void worker_async_wakeup(worker_id_t tid);

#endif
