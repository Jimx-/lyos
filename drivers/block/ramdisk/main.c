/*  This file is part of Lyos.

    Lyos is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Lyos is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Lyos.  If not, see <http://www.gnu.org/licenses/>. */

#include <lyos/types.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "lyos/config.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "multiboot.h"
#include <errno.h>
#include <lyos/sysutils.h>

#include "libblockdriver/libblockdriver.h"
#include "libchardriver/libchardriver.h"
#include <libdevman/libdevman.h>

/*
  1 char	Memory devices
          1 = /dev/mem		Physical memory access
          2 = /dev/kmem		Kernel virtual memory access
          3 = /dev/null		Null device
          4 = /dev/port		I/O port access
          5 = /dev/zero		Null byte source
          7 = /dev/full		Returns ENOSPC on write
          8 = /dev/random	Nondeterministic random number gen.
          9 = /dev/urandom	Faster, less secure random number gen.
         10 = /dev/aio		Asynchronous I/O notification interface
         11 = /dev/kmsg		Writes to this come out as printk's, reads
                            export the buffered printk records.

  1 block	RAM disk
          0 = /dev/ram0		First RAM disk
          1 = /dev/ram1		Second RAM disk
            ...
        120 = /dev/initrd	Initial RAM disk
*/

#define MAX_RAMDISKS CONFIG_BLK_DEV_RAM_COUNT

#define DEV_MEM 1
#define DEV_KMEM 2
#define DEV_NULL 3
#define DEV_ZERO 5

#define NR_CDEVS 5
int char_open_count[NR_CDEVS + 1];

struct ramdisk_dev {
    char* start;
    int length;

    int rdonly;
};

static struct ramdisk_dev ramdisks[MAX_RAMDISKS];
static struct ramdisk_dev initramdisk;

static class_id_t mem_subsys_id;

static void init_rd(int argc, char* argv[]);
static int rd_open(dev_t minor, int access);
static int rd_close(dev_t minor);
static ssize_t rd_rdwt(dev_t minor, int do_write, loff_t pos,
                       endpoint_t endpoint, const struct iovec* iov,
                       size_t count);
static int rd_ioctl(dev_t minor, int request, endpoint_t endpoint, void* buf);

static int char_open(dev_t minor, int access);
static int char_close(dev_t minor);
static ssize_t char_read(dev_t minor, u64 pos, endpoint_t endpoint, char* buf,
                         unsigned int count, cdev_id_t id);
static ssize_t char_write(dev_t minor, u64 pos, endpoint_t endpoint, char* buf,
                          unsigned int count, cdev_id_t id);
static int char_ioctl(dev_t minor, int request, endpoint_t endpoint, char* buf,
                      cdev_id_t id);

struct blockdriver rd_driver = {
    .bdr_open = rd_open,
    .bdr_close = rd_close,
    .bdr_readwrite = rd_rdwt,
    .bdr_ioctl = rd_ioctl,
};

struct chardriver c_driver = {
    .cdr_open = char_open,
    .cdr_close = char_close,
    .cdr_read = char_read,
    .cdr_write = char_write,
    .cdr_ioctl = char_ioctl,
    .cdr_mmap = NULL,
};

int main(int argc, char* argv[])
{
    env_setargs(argc, argv);
    init_rd(argc, argv);

    MESSAGE msg;
    while (TRUE) {
        send_recv(RECEIVE, ANY, &msg);

        if (IS_BDEV_REQ(msg.type)) {
            blockdriver_process(&rd_driver, &msg);
        } else {
            chardriver_process(&c_driver, &msg);
        }
    }

    return 0;
}

static int rd_open(dev_t minor, int access) { return 0; }

static int rd_close(dev_t minor) { return 0; }

static ssize_t rd_rdwt(dev_t minor, int do_write, loff_t pos,
                       endpoint_t endpoint, const struct iovec* iov,
                       size_t count)
{
    int i;
    ssize_t total = 0;
    size_t bytes;

    struct ramdisk_dev* ramdisk;
    if (minor == MINOR_INITRD)
        ramdisk = &initramdisk;
    else
        ramdisk = ramdisks + minor;

    for (i = 0; i < count; i++, iov++) {
        if (pos > ramdisk->length) {
            return total;
        }

        bytes = iov->iov_len;
        if (pos + bytes > ramdisk->length) {
            bytes = ramdisk->length - pos;
        }

        char* addr = ramdisk->start + pos;

        if (do_write) {
            if (ramdisk->rdonly) return -EROFS;
            data_copy(SELF, addr, endpoint, iov->iov_base, bytes);
        } else {
            data_copy(endpoint, iov->iov_base, SELF, addr, bytes);
        }

        pos += bytes;
    }

    return total;
}

static int rd_ioctl(dev_t minor, int request, endpoint_t endpoint, void* buf)
{
    return 0;
}

static int char_open(dev_t minor, int access)
{
    if (minor <= 0 || minor > NR_CDEVS) return ENXIO;

    char_open_count[minor]++;
    return 0;
}

static int char_close(dev_t minor)
{
    if (minor <= 0 || minor > NR_CDEVS) return ENXIO;

    char_open_count[minor]--;
    return 0;
}

static ssize_t char_read(dev_t minor, u64 pos, endpoint_t endpoint, char* buf,
                         unsigned int count, cdev_id_t id)
{
    ssize_t retval;

    if (minor <= 0 || minor > NR_CDEVS) return -ENXIO;

    switch (minor) {
    case DEV_NULL:
        retval = 0;
        break;
    }

    return retval;
}

static ssize_t char_write(dev_t minor, u64 pos, endpoint_t endpoint, char* buf,
                          unsigned int count, cdev_id_t id)
{
    ssize_t retval;

    if (minor <= 0 || minor > NR_CDEVS) return -ENXIO;

    switch (minor) {
    case DEV_NULL:
    case DEV_ZERO:
        retval = count;
        break;
    }

    return retval;
}

static int char_ioctl(dev_t minor, int request, endpoint_t endpoint, char* buf,
                      cdev_id_t id)
{
    return 0;
}

static void init_rd(int argc, char* argv[])
{
    long base, len;
    struct device_info devinf;
    device_id_t dev_id;
    int retval;

    env_get_long("initrd_base", &base, "u", 0, -1, -1);
    env_get_long("initrd_len", &len, "d", 0, -1, -1);

    initramdisk.start = (char*)base;
    initramdisk.length = len;
    initramdisk.rdonly = 1;

    printl("RAMDISK: initrd: %d bytes(%d kB), base: 0x%x\n", len, len / 1024,
           base);

    retval = dm_class_register("mem", &mem_subsys_id);
    if (retval) panic("ramdisk: cannot register mem subsystem");

    memset(&devinf, 0, sizeof(devinf));
    devinf.bus = NO_BUS_ID;
    devinf.class = mem_subsys_id;
    devinf.parent = NO_DEVICE_ID;
    devinf.type = DT_CHARDEV;

    devinf.devt = MAKE_DEV(DEV_RD, DEV_MEM);
    strlcpy(devinf.name, "mem", sizeof(devinf.name));
    dm_cdev_add(devinf.devt);
    dm_device_register(&devinf, &dev_id);

    devinf.devt = MAKE_DEV(DEV_RD, DEV_KMEM);
    strlcpy(devinf.name, "kmem", sizeof(devinf.name));
    dm_cdev_add(devinf.devt);
    dm_device_register(&devinf, &dev_id);

    devinf.devt = MAKE_DEV(DEV_RD, DEV_NULL);
    strlcpy(devinf.name, "null", sizeof(devinf.name));
    dm_cdev_add(devinf.devt);
    dm_device_register(&devinf, &dev_id);

    devinf.devt = MAKE_DEV(DEV_RD, DEV_ZERO);
    strlcpy(devinf.name, "zero", sizeof(devinf.name));
    dm_cdev_add(devinf.devt);
    dm_device_register(&devinf, &dev_id);
}
