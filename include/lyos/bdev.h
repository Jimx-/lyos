#ifndef _BDEV_H_
#define _BDEV_H_

void bdev_init();
int bdev_driver(dev_t dev);
ssize_t bdev_readwrite(int io_type, int dev, u64 pos, int bytes, int proc_nr,
                       void* buf);

#endif
