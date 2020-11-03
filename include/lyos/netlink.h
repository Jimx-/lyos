#ifndef _LYOS_NETLINK_H_
#define _LYOS_NETLINK_H_

#include <lyos/types.h>
#include <uapi/linux/netlink.h>
#include <lyos/const.h>

enum netlink_msgtype {
    NETLINK_CREATE = NETLINK_REQ_BASE,
    NETLINK_BROADCAST,
};

struct netlink_resp {
    int status;
};

struct netlink_create_req {
    int unit;
    int groups;
};

struct netlink_transfer_req {
    int sock_id;
    u32 portid;
    u32 group;
    int nonblock;
    mgrant_id_t grant;
    size_t len;
};

#endif
