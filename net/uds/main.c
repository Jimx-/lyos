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

#include "uds.h"

#define MAX_THREADS 32

static struct idr sock_idr;

static const struct sockdriver_ops uds_ops = {};

static int uds_alloc(struct udssock** udsp)
{
    struct udssock* uds;

    uds = malloc(sizeof(*uds));
    if (!uds) return ENOMEM;

    uds->conn = NULL;

    *udsp = uds;
    return 0;
}

static void uds_free(struct udssock* uds) { free(uds); }

static sockid_t uds_socket(endpoint_t src, int domain, int type, int protocol,
                           struct sockdriver_sock** sock,
                           const struct sockdriver_ops** ops)
{
    struct udssock* uds;
    sockid_t id;
    int retval;

    if (domain != PF_UNIX) return -EAFNOSUPPORT;

    switch (type) {
    case SOCK_STREAM:
    case SOCK_SEQPACKET:
    case SOCK_DGRAM:
        break;
    default:
        return -EPROTOTYPE;
    }

    if (protocol != 0) return -EPROTONOSUPPORT;

    if ((retval = uds_alloc(&uds)) != 0) return -retval;

    id = idr_alloc(&sock_idr, NULL, 1, 0);
    if (id < 0) {
        uds_free(uds);
    } else {
        *sock = &uds->sock;
        *ops = &uds_ops;
    }

    return id;
}

int uds_init(void)
{
    printl("uds: UNIX domain socket driver is running.\n");

    sockdriver_init(uds_socket);

    idr_init(&sock_idr);

    return 0;
}

int main()
{
    serv_register_init_fresh_callback(uds_init);
    serv_init();

    sockdriver_async_set_workers(MAX_THREADS);
    sockdriver_async_task();

    return 0;
}
