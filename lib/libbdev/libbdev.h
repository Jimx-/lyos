#ifndef _LIBBDEV_H_
#define _LIBBDEV_H_

#include <lyos/types.h>
#include <sys/types.h>
#include <sys/uio.h>

void bdev_init();
int bdev_driver(dev_t dev);

int bdev_open(dev_t dev);
int bdev_close(dev_t dev);

ssize_t bdev_read(dev_t dev, loff_t pos, void* buf, size_t count,
                  endpoint_t endpoint);
ssize_t bdev_write(dev_t dev, loff_t pos, void* buf, size_t count,
                   endpoint_t endpoint);

ssize_t bdev_readv(dev_t dev, loff_t pos, endpoint_t endpoint,
                   const struct iovec* iov, size_t count);
ssize_t bdev_writev(dev_t dev, loff_t pos, endpoint_t endpoint,
                    const struct iovec* iov, size_t count);

#endif // _LIBBDEV_H_
