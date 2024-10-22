#ifndef _LIBASYNCDRIVER_LIBASYNCDRIVER_H_
#define _LIBASYNCDRIVER_LIBASYNCDRIVER_H_

#include <lyos/types.h>
#include <lyos/ipc.h>

typedef unsigned int async_worker_id_t;

struct asyncdriver {
    const char* name;

    int (*process_on_thread)(MESSAGE* msg);
    void (*process)(MESSAGE* msg);
};

async_worker_id_t asyncdrv_worker_id(void);

void asyncdrv_sleep(void);
void asyncdrv_wakeup(async_worker_id_t tid);

int asyncdrv_sendrec(endpoint_t dest, MESSAGE* msg);
void asyncdrv_reply(async_worker_id_t tid, MESSAGE* msg);

void asyncdrv_set_workers(size_t num_workers);
void asyncdrv_task(const struct asyncdriver* asd, size_t num_workers,
                   void (*init_func)(void));

#endif
