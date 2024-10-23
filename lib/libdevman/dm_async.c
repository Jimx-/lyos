#include <lyos/sysutils.h>
#include <errno.h>

#include <libasyncdriver/libasyncdriver.h>

async_worker_id_t __asyncdrv_worker_id(void)
{
    panic("libdevman not linked with libasyncdriver\n");
    return -1;
}
async_worker_id_t asyncdrv_worker_id(void)
    __attribute__((weak, alias("__asyncdrv_worker_id")));

int __asyncdrv_sendrec(endpoint_t dest, MESSAGE* msg)
{
    panic("libdevman not linked with libasyncdriver\n");
    return ENOSYS;
}
int asyncdrv_sendrec(endpoint_t dest, MESSAGE* msg)
    __attribute__((weak, alias("__asyncdrv_sendrec")));

void __asyncdrv_reply(async_worker_id_t tid, MESSAGE* msg)
{
    panic("libdevman not linked with libasyncdriver\n");
}
void asyncdrv_reply(async_worker_id_t tid, const MESSAGE* msg)
    __attribute__((weak, alias("__asyncdrv_reply")));

void dm_async_reply(const MESSAGE* msg)
{
    async_worker_id_t tid = msg->u.m3.m3l1;
    asyncdrv_reply(tid, msg);
}
