#include <lyos/ipc.h>
#include <stdio.h>
#include <errno.h>
#include <lyos/const.h>
#include <lyos/sysutils.h>

#include <libcoro/libcoro.h>
#include <libblockdriver/libblockdriver.h>
#include <libasyncdriver/libasyncdriver.h>

static struct blockdriver* bdr = NULL;

static int bdr_process_on_thread(const MESSAGE* msg);
static void bdr_process(MESSAGE* msg);

static struct asyncdriver async_drv = {
    .name = "blockdriver_async",
    .process_on_thread = bdr_process_on_thread,
    .process = bdr_process,
};

static int bdr_process_on_thread(const MESSAGE* msg)
{
    return msg->type != NOTIFY_MSG;
}

static void bdr_process(MESSAGE* msg) { blockdriver_process(bdr, msg); }

void blockdriver_async_sleep(void) { asyncdrv_sleep(); }

void blockdriver_async_wakeup(blockdriver_worker_id_t tid)
{
    asyncdrv_wakeup((async_worker_id_t)tid);
}

void blockdriver_async_set_workers(size_t num_workers)
{
    asyncdrv_set_workers(num_workers);
}

blockdriver_worker_id_t blockdriver_async_worker_id(void)
{
    return (blockdriver_worker_id_t)asyncdrv_worker_id();
}

void blockdriver_async_task(struct blockdriver* bd, size_t num_workers,
                            void (*init_func)(void))
{
    bdr = bd;

    asyncdrv_task(&async_drv, num_workers, init_func);
}
