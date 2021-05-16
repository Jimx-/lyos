#ifndef _LIBBLOCKDRIVER_H_
#define _LIBBLOCKDRIVER_H_

#include <lyos/types.h>
#include <lyos/partition.h>
#include <lyos/driver.h>
#include <sys/uio.h>

#include <libdevman/libdevman.h>

#define NR_SUB_PER_PART   16
#define NR_SUB_PER_DRIVE  (NR_SUB_PER_PART * NR_PART_PER_DRIVE)
#define NR_PRIM_PER_DRIVE (NR_PART_PER_DRIVE + 1)

#define P_PRIMARY  0
#define P_EXTENDED 1

#define CD_SECTOR_SIZE 2048

typedef unsigned int blockdriver_worker_id_t;

struct blockdriver {
    int (*bdr_open)(dev_t minor, int access);
    int (*bdr_close)(dev_t minor);
    ssize_t (*bdr_readwrite)(dev_t minor, int do_write, loff_t pos,
                             endpoint_t endpoint, const struct iovec_grant* iov,
                             size_t count);
    int (*bdr_ioctl)(dev_t minor, int request, endpoint_t endpoint,
                     mgrant_id_t grant, endpoint_t user_endpoint);
    struct part_info* (*bdr_part)(dev_t minor);
    void (*bdr_intr)(unsigned mask);
    void (*bdr_alarm)(clock_t timestamp);
    void (*bdr_other)(MESSAGE* msg);
};

void blockdriver_process(struct blockdriver* bd, MESSAGE* msg);
void blockdriver_task(struct blockdriver* bd);

void blockdriver_async_task(struct blockdriver* bd, size_t num_workers,
                            void (*init_func)(void));
blockdriver_worker_id_t blockdriver_async_worker_id(void);
void blockdriver_async_sleep(void);
void blockdriver_async_wakeup(blockdriver_worker_id_t tid);
void blockdriver_async_set_workers(size_t num_workers);

void partition(struct blockdriver* bd, int device, int style);

int blockdriver_device_register(struct device_info* devinf, device_id_t* id);

#endif
