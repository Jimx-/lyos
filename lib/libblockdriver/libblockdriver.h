#ifndef _LIBBLOCKDRIVER_H_
#define _LIBBLOCKDRIVER_H_

#include <lyos/partition.h>
#include <lyos/driver.h>

#define NR_SUB_PER_PART 16
#define NR_SUB_PER_DRIVE (NR_SUB_PER_PART * NR_PART_PER_DRIVE)
#define NR_PRIM_PER_DRIVE (NR_PART_PER_DRIVE + 1)

#define P_PRIMARY 0
#define P_EXTENDED 1

#define CD_SECTOR_SIZE 2048

struct blockdriver {
    int (*bdr_open)(dev_t minor, int access);
    int (*bdr_close)(dev_t minor);
    ssize_t (*bdr_readwrite)(dev_t minor, int do_write, u64 pos,
                             endpoint_t endpoint, char* buf,
                             unsigned int count);
    int (*bdr_ioctl)(dev_t minor, int request, endpoint_t endpoint, char* buf);
    struct part_info* (*bdr_part)(dev_t minor);
    void (*bdr_intr)(unsigned mask);
    void (*bdr_alarm)(clock_t timestamp);
};

PUBLIC void blockdriver_process(struct blockdriver* bd, MESSAGE* msg);
PUBLIC void blockdriver_task(struct blockdriver* bd);

void partition(struct blockdriver* bd, int device, int style);

#endif
