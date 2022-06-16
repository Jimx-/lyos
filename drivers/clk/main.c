#include <lyos/ipc.h>
#include <errno.h>
#include <lyos/const.h>
#include <lyos/sysutils.h>

#include "proto.h"

#define NAME "clk"

#if CONFIG_OF
void* boot_params;
#endif

static int clk_init(void)
{
    struct sysinfo* sysinfo;

    printl(NAME ": Common clock framework driver is running.\n");

#if CONFIG_OF
    get_sysinfo(&sysinfo);
    boot_params = sysinfo->boot_params;
#endif

#if CONFIG_CLK_BCM2835
    bcm2835_clk_scan();
#endif

    return 0;
}

int main()
{
    serv_register_init_fresh_callback(clk_init);
    serv_init();

    while (TRUE) {
        MESSAGE msg;

        send_recv(RECEIVE, ANY, &msg);
        int src = msg.source;
        int msgtype = msg.type;

        switch (msgtype) {
        default:
            msg.RETVAL = ENOSYS;
            break;
        }

        msg.type = SYSCALL_RET;
        send_recv(SEND_NONBLOCK, src, &msg);
    }

    return 0;
}
