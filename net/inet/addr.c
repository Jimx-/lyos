#include <assert.h>
#include <errno.h>
#include <string.h>

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

void addr_set_inet(struct sockaddr* addr, socklen_t* addr_len,
                   const ip_addr_t* ipaddr, uint16_t port)
{
    struct sockaddr_in sin;

    switch (IP_GET_TYPE(ipaddr)) {
    case IPADDR_TYPE_V4:
        assert(*addr_len >= sizeof(sin));

        memset(&sin, 0, sizeof(sin));

        sin.sin_family = AF_INET;
        sin.sin_port = htons(port);
        sin.sin_addr.s_addr = ip_addr_get_ip4_u32(ipaddr);

        memcpy(addr, &sin, sizeof(sin));
        *addr_len = sizeof(sin);

        break;
    default:
        assert(FALSE);
    }
}

void addr_set_link(struct sockaddr* addr, socklen_t* addr_len,
                   unsigned int type, const uint8_t* hwaddr, size_t hwaddr_len)
{
    socklen_t len;

    if (hwaddr == NULL) hwaddr_len = 0;

    len = offsetof(struct sockaddr, sa_data) + hwaddr_len;

    assert(*addr_len > len);

    if (!hwaddr_len) {
        memset(addr, 0, *addr_len);
    } else {
        memcpy(addr->sa_data, hwaddr, hwaddr_len);
    }

    addr->sa_family = type;
    *addr_len = len;
}
