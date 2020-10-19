#ifndef _LIBNETLINK_H_
#define _LIBNETLINK_H_

#include <lyos/types.h>
#include <sys/types.h>

typedef int nlsockid_t;

struct netlink_kernel_cfg {
    int groups;
};

nlsockid_t netlink_create(int unit, struct netlink_kernel_cfg* cfg);
int netlink_broadcast(nlsockid_t sock, u32 portid, u32 group, const void* buf,
                      size_t len);

#endif
