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
void addr_set_inet(struct sockaddr* addr, socklen_t* addr_len,
                   const ip_addr_t* ipaddr, uint16_t port);

/* ndev.c */
void ndev_init(void);
int ndev_can_recv(unsigned int id);
int ndev_recv(unsigned int id, struct pbuf* pbuf);
void ndev_process(MESSAGE* msg);
void ndev_check(void);

/* ifconf.c */
void ifconf_init(void);
int ifconf_ioctl(struct sock* sock, unsigned long request,
                 const struct sockdriver_data* data, endpoint_t user_endpt,
                 int flags);

/* loopback.c */
void loopif_init(void);

/* tcpsock.c */
sockid_t tcpsock_socket(int domain, int protocol, struct sock** sock,
                        const struct sockdriver_ops** ops);

/* util.c */
int is_superuser(endpoint_t endpoint);
int convert_err(err_t err);
ssize_t copy_socket_data(struct iov_grant_iter* iter, size_t len,
                         const struct pbuf* pbuf, size_t skip, int copy_in);

#endif
