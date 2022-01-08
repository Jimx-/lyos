#ifndef _INET_INET_H_
#define _INET_INET_H_

#include <lwip/ip.h>
#include <netinet/in.h>

#include <libsockdriver/libsockdriver.h>

struct ndev_hwaddr {
    u8 hwaddr[NDEV_HWADDR_MAX];
};

/* addr.c */
int addr_get_inet(const struct sockaddr* addr, socklen_t addr_len, uint8_t type,
                  ip_addr_t* ipaddr, uint16_t* port);

/* ndev.c */
void ndev_init(void);
int ndev_can_recv(unsigned int id);
int ndev_recv(unsigned int id, struct pbuf* pbuf);
void ndev_process(MESSAGE* msg);
void ndev_check(void);

/* ifconf.c */
void ifconf_init(void);

/* loopback.c */
void loopif_init(void);

#endif
