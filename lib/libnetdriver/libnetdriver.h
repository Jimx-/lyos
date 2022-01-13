#ifndef _LIBNETDRIVER_H_
#define _LIBNETDRIVER_H_

#include <lyos/config.h>
#include <lyos/iov_grant_iter.h>

#include <stdint.h>

#define NDEV_LINK_UNKNOWN 0
#define NDEV_LINK_UP      1
#define NDEV_LINK_DOWN    2

typedef struct {
    uint8_t addr[NDEV_HWADDR_MAX];
} netdriver_addr_t;

struct netdriver_data {
    endpoint_t endpoint;
    unsigned int id;
    size_t size;
    struct iov_grant_iter iter;
    struct iovec_grant iovec[NDEV_IOV_MAX];
};

struct netdriver {
    const char* ndr_name;
    int (*ndr_init)(unsigned int instance, netdriver_addr_t* hwaddr);
    int (*ndr_send)(struct netdriver_data* data, size_t len);
    ssize_t (*ndr_recv)(struct netdriver_data* data, size_t max);
    void (*ndr_intr)(unsigned int mask);
    void (*ndr_other)(MESSAGE* msg);
};

int netdriver_copyin(struct netdriver_data* data, void* buf, size_t size);
int netdriver_copyout(struct netdriver_data* data, const void* buf,
                      size_t size);

void netdriver_send(void);
void netdriver_recv(void);

void netdriver_task(const struct netdriver* ndr);

#endif
