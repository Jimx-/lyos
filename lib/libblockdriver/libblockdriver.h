#ifndef _LIBBLOCKDRIVER_H_
#define _LIBBLOCKDRIVER_H_

#include <lyos/partition.h>
#include <lyos/driver.h>
#include <sys/uio.h>

#define NR_SUB_PER_PART 16
#define NR_SUB_PER_DRIVE (NR_SUB_PER_PART * NR_PART_PER_DRIVE)
#define NR_PRIM_PER_DRIVE (NR_PART_PER_DRIVE + 1)

#define P_PRIMARY 0
#define P_EXTENDED 1

#define CD_SECTOR_SIZE 2048

typedef unsigned int blockdriver_worker_id_t;

struct blockdriver {
    int (*bdr_open)(dev_t minor, int access);
    int (*bdr_close)(dev_t minor);
    ssize_t (*bdr_readwrite)(dev_t minor, int do_write, loff_t pos,
                             endpoint_t endpoint, const struct iovec* iov,
                             size_t count);
    int (*bdr_ioctl)(dev_t minor, int request, endpoint_t endpoint, void* buf);
    struct part_info* (*bdr_part)(dev_t minor);
    void (*bdr_intr)(unsigned mask);
    void (*bdr_alarm)(clock_t timestamp);
};

void blockdriver_process(struct blockdriver* bd, MESSAGE* msg);
void blockdriver_task(struct blockdriver* bd);

void blockdriver_async_task(struct blockdriver* bd);
blockdriver_worker_id_t blockdriver_async_worker_id(void);
void blockdriver_async_sleep(void);
void blockdriver_async_wakeup(blockdriver_worker_id_t tid);
void blockdriver_async_set_workers(size_t num_workers);

void partition(struct blockdriver* bd, int device, int style);

#endif
