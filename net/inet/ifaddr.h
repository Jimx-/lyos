#ifndef _INET_IFADDR_H_
#define _INET_IFADDR_H_

int ifaddr_v4_add(struct if_device* ifdev, const struct sockaddr_in* addr,
                  const struct sockaddr_in* mask,
                  const struct sockaddr_in* bcast,
                  const struct sockaddr_in* dest);

#endif
