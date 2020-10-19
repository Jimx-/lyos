#include <lyos/const.h>
#include <lyos/ipc.h>
#include <lyos/mgrant.h>
#include <lyos/netlink.h>
#include <string.h>
#include <errno.h>

#include "libnetlink.h"

nlsockid_t netlink_create(int unit, struct netlink_kernel_cfg* cfg)
{
    MESSAGE msg;
    struct netlink_create_req* req =
        (struct netlink_create_req*)msg.MSG_PAYLOAD;
    struct netlink_resp* resp;

    memset(&msg, 0, sizeof(msg));
    msg.type = NETLINK_CREATE;
    req->unit = unit;
    req->groups = cfg ? cfg->groups : 32;

    send_recv(BOTH, TASK_NETLINK, &msg);

    resp = (struct netlink_resp*)msg.MSG_PAYLOAD;
    return resp->status;
}

int netlink_broadcast(nlsockid_t sock, u32 portid, u32 group, const void* buf,
                      size_t len)
{
    MESSAGE msg;
    mgrant_id_t grant;
    struct netlink_transfer_req* req =
        (struct netlink_transfer_req*)msg.MSG_PAYLOAD;
    struct netlink_resp* resp;

    if ((grant = mgrant_set_direct(TASK_NETLINK, (vir_bytes)buf, len,
                                   MGF_READ)) == GRANT_INVALID)
        return -ENOMEM;

    memset(&msg, 0, sizeof(msg));
    msg.type = NETLINK_BROADCAST;
    req->sock_id = sock;
    req->portid = portid;
    req->group = group;
    req->grant = grant;
    req->len = len;

    send_recv(BOTH, TASK_NETLINK, &msg);

    mgrant_revoke(grant);

    resp = (struct netlink_resp*)msg.MSG_PAYLOAD;
    return resp->status;
}
