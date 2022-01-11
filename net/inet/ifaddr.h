#ifndef _INET_IFADDR_H_
#define _INET_IFADDR_H_

int ifaddr_v4_get(struct if_device* ifdev, unsigned int num,
                  struct sockaddr_in* addr, struct sockaddr_in* mask,
                  struct sockaddr_in* bcast, struct sockaddr_in* dest);

int ifaddr_v4_add(struct if_device* ifdev, const struct sockaddr_in* addr,
                  const struct sockaddr_in* mask,
                  const struct sockaddr_in* bcast,
                  const struct sockaddr_in* dest);

#endif
