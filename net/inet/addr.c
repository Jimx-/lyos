#include <errno.h>

#include "inet.h"

int addr_get_inet(const struct sockaddr* addr, socklen_t addr_len, uint8_t type,
                  ip_addr_t* ipaddr, uint16_t* port)
{
    struct sockaddr_in sin;

    switch (type) {
    case IPADDR_TYPE_V4:
        if (addr_len != sizeof(sin)) return EINVAL;

        memcpy(&sin, addr, sizeof(sin));
        if (sin.sin_family != AF_INET) return EAFNOSUPPORT;

        ip_addr_set_ip4_u32(ipaddr, sin.sin_addr.s_addr);

        if (port != NULL) *port = ntohs(sin.sin_port);

        break;
    default:
        return EAFNOSUPPORT;
    }

    return 0;
}
