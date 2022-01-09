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
#include "ifdev.h"

#define MAX_THREADS 128

static sockid_t inet_socket(endpoint_t src, int domain, int type, int protocol,
                            struct sock** sock,
                            const struct sockdriver_ops** ops);
static void inet_poll(void);
static void inet_other(MESSAGE* msg);

struct idr sock_idr;

static const struct sockdriver inet_driver = {
    .sd_create = inet_socket,
    .sd_poll = inet_poll,
    .sd_other = inet_other,
};

uint32_t sys_now(void)
{
    clock_t now;
    static int first = TRUE;
    static int sys_hz;

    if (first) {
        sys_hz = get_system_hz();
        first = FALSE;
    }

    get_ticks(&now, NULL);
    return (uint32_t)(((uint64_t)now * 1000) / sys_hz);
}

uint32_t lwip_hook_rand(void) { return lrand48(); }

static sockid_t inet_socket(endpoint_t src, int domain, int type, int protocol,
                            struct sock** sock,
                            const struct sockdriver_ops** ops)
{
    switch (domain) {
    case PF_INET:
        switch (type) {
        case SOCK_STREAM:
            return tcpsock_socket(domain, protocol, sock, ops);
        default:
            return -EPROTOTYPE;
        }
        break;
    default:
        return -EAFNOSUPPORT;
    }
}

static void inet_poll(void) { ifdev_poll(); }

static void inet_other(MESSAGE* msg)
{
    if (msg->type == NOTIFY_MSG) {
        switch (msg->source) {
        case TASK_SYSFS:
            ndev_check();
            break;
        }
        return;
    }

    if (IS_NDEV_CALL(msg->type)) {
        ndev_process(msg);
    }
}

static int inet_init(void)
{
    printl("inet: INET socket driver is running\n");

    srand48(time(NULL));

    idr_init(&sock_idr);

    sockdriver_init();

    ndev_init();

    loopif_init();
    ifconf_init();

    return 0;
}

int main()
{
    serv_register_init_fresh_callback(inet_init);
    serv_init();

    sockdriver_task(&inet_driver, MAX_THREADS);

    return 0;
}
