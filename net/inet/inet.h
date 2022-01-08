#ifndef _INET_INET_H_
#define _INET_INET_H_

#include <lwip/ip.h>

#include <libsockdriver/libsockdriver.h>

void ndev_init(void);
void ndev_process(MESSAGE* msg);
void ndev_check(void);

#endif
