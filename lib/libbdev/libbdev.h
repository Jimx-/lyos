#ifndef _LIBBDEV_H_
#define _LIBBDEV_H_

void bdev_init();
int bdev_driver(dev_t dev);

int bdev_open(dev_t dev);
int bdev_close(dev_t dev);

ssize_t bdev_read(dev_t dev, loff_t pos, void* buf, size_t count,
                  endpoint_t endpoint);
ssize_t bdev_write(dev_t dev, loff_t pos, void* buf, size_t count,
                   endpoint_t endpoint);

#endif // _LIBBDEV_H_
