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

#define MAX_RAMDISKS	CONFIG_BLK_DEV_RAM_COUNT

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

struct blockdriver rd_driver = {
	.bdr_open = rd_open,
	.bdr_close = rd_close,
	.bdr_readwrite = rd_rdwt,
	.bdr_ioctl = rd_ioctl,
};

PUBLIC int main(int argc, char * argv[])
{
	env_setargs(argc, argv);
	init_rd(argc, argv);

	blockdriver_task(&rd_driver);

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

	return 0;
}

PRIVATE int rd_ioctl(dev_t minor, int request, endpoint_t endpoint, char* buf)
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
}

