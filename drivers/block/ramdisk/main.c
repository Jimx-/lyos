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
    
#include "lyos/type.h"
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

#define MAX_RAMDISKS	CONFIG_BLK_DEV_RAM_COUNT

#define DEV_MEM			1
#define DEV_KMEM		2
#define DEV_NULL		3
#define DEV_ZERO		5

#define NR_CDEVS		5
int char_open_count[NR_CDEVS+1];

struct ramdisk_dev {
	char * start;
	int length;

	int rdonly;
};

PRIVATE struct ramdisk_dev ramdisks[MAX_RAMDISKS];
PRIVATE struct ramdisk_dev initramdisk;

PRIVATE void init_rd(int argc, char * argv[]);
PRIVATE int rd_open(dev_t minor, int access);
PRIVATE int rd_close(dev_t minor);
PRIVATE ssize_t rd_rdwt(dev_t minor, int do_write, u64 pos,
	  endpoint_t endpoint, char* buf, unsigned int count);
PRIVATE int rd_ioctl(dev_t minor, int request, endpoint_t endpoint, char* buf);

PRIVATE int char_open(dev_t minor, int access);
PRIVATE int char_close(dev_t minor);
PRIVATE ssize_t char_read(dev_t minor, u64 pos,
	  endpoint_t endpoint, char* buf, unsigned int count);
PRIVATE ssize_t char_write(dev_t minor, u64 pos,
	  endpoint_t endpoint, char* buf, unsigned int count);
PRIVATE int char_ioctl(dev_t minor, int request, endpoint_t endpoint, char* buf);

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
};

PUBLIC int main(int argc, char * argv[])
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

PRIVATE int rd_open(dev_t minor, int access)
{
	return 0;
}

PRIVATE int rd_close(dev_t minor)
{
	return 0;
}

PRIVATE int rd_rdwt(dev_t minor, int do_write, u64 pos,
	  endpoint_t endpoint, char* buf, unsigned int count)
{
    struct ramdisk_dev * ramdisk; 
    if (minor == MINOR_INITRD) ramdisk = &initramdisk;
    else ramdisk = ramdisks + minor;

	char * addr = ramdisk->start + (int)pos;
	
	if (pos > ramdisk->length){
		return 0;
	}

	if (do_write) {
		if (ramdisk->rdonly) return -EROFS;
		data_copy(SELF, addr, endpoint, buf, count);
	} else {
		data_copy(endpoint, buf, SELF, addr, count);
	}

	return count;
}

PRIVATE int rd_ioctl(dev_t minor, int request, endpoint_t endpoint, char* buf)
{
	return 0;
}

PRIVATE int char_open(dev_t minor, int access)
{
	if (minor <= 0 || minor > NR_CDEVS) return ENXIO;

	char_open_count[minor]++;
	return 0;
}

PRIVATE int char_close(dev_t minor)
{
	if (minor <= 0 || minor > NR_CDEVS) return ENXIO;

	char_open_count[minor]--;
	return 0;
}

PRIVATE ssize_t char_read(dev_t minor, u64 pos,
	  endpoint_t endpoint, char* buf, unsigned int count)
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

PRIVATE ssize_t char_write(dev_t minor, u64 pos,
	  endpoint_t endpoint, char* buf, unsigned int count)
{
	ssize_t retval;

	if (minor <= 0 || minor > NR_CDEVS) return -ENXIO;

	switch (minor) {
	case DEV_NULL:
	case DEV_ZERO:
		retval = count;
		break;
	}
	return 0;
}

PRIVATE int char_ioctl(dev_t minor, int request, endpoint_t endpoint, char* buf)
{
	return 0;
}

PRIVATE void init_rd(int argc, char * argv[])
{
    long base, len;
    env_get_long("initrd_base", &base, "u", 0, -1, -1);
    env_get_long("initrd_len", &len, "d", 0, -1, -1);

	initramdisk.start = (char *)base;
	initramdisk.length = len;
	initramdisk.rdonly = 1;

	printl("RAMDISK: initrd: %d bytes(%d kB), base: 0x%x\n", len, len / 1024, base);

	dm_cdev_add(MAKE_DEV(DEV_RD, DEV_MEM));
	dm_cdev_add(MAKE_DEV(DEV_RD, DEV_MEM));
	dm_cdev_add(MAKE_DEV(DEV_RD, DEV_MEM));
	dm_cdev_add(MAKE_DEV(DEV_RD, DEV_MEM));
}
