#ifndef _INET_ETHDEV_H_
#define _INET_ETHDEV_H_

struct eth_device;

struct eth_device* ethdev_add(unsigned int id);
int ethdev_enable(struct eth_device* ethdev, const struct ndev_hwaddr* hwaddr,
                  int hwaddr_len, int link);
void ethdev_sent(struct eth_device* ethdev, int status);
void ethdev_recv(struct eth_device* ethdev, int status);

#endif
