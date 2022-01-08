#include <lyos/types.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/proc.h>
#include <lyos/global.h>
#include <lyos/proto.h>
#include <lyos/driver.h>
#include <errno.h>
#include <lyos/service.h>
#include <lyos/sysutils.h>
#include <lyos/vm.h>
#include <lyos/idr.h>
#include <sys/socket.h>

#include "inet.h"

static int inet_init(void)
{
    printl("inet: INET socket driver is running\n");

    sockdriver_init();

    ndev_init();

    return 0;
}

int main()
{
    MESSAGE msg;

    serv_register_init_fresh_callback(inet_init);
    serv_init();

    while (TRUE) {
        send_recv(RECEIVE_ASYNC, ANY, &msg);

        if (msg.type == NOTIFY_MSG) {
            switch (msg.source) {
            case TASK_SYSFS:
                ndev_check();
                break;
            }
            continue;
        }

        if (IS_NDEV_CALL(msg.type)) {
            ndev_process(&msg);
        }
    }

    return 0;
}
