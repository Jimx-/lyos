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
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/tty.h"
#include "lyos/console.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <lyos/driver.h>
#include "multiboot.h"
#include <errno.h>

#define MAX_RAMDISKS	CONFIG_BLK_DEV_RAM_COUNT

struct ramdisk_dev {
	char * start;
	int length;

	int rdonly;
};

PRIVATE struct ramdisk_dev ramdisks[MAX_RAMDISKS];
PRIVATE struct ramdisk_dev initramdisk;

PRIVATE void init_rd();
PRIVATE int rd_open(MESSAGE * p);
PRIVATE int rd_close(MESSAGE * p);
PRIVATE int rd_rdwt(MESSAGE * p);
PRIVATE int rd_ioctl(MESSAGE * p);

struct dev_driver rd_driver = 
{
	"ramdisk",
	rd_open,
	rd_close,
	rd_rdwt,
	rd_rdwt, 
	rd_ioctl 
};

PUBLIC void task_rd()
{
	//MESSAGE msg;

	init_rd();

	dev_driver_task(&rd_driver);
	/*
	while (1) {
	
		send_recv(RECEIVE, ANY, &msg);

		int src = msg.source;
		switch (msg.type){
		case DEV_OPEN:
			break;
		case DEV_CLOSE:
			break;
		case DEV_READ:
		case DEV_WRITE:
			msg.RETVAL = rd_rdwt(&msg);
			break;
		case DEV_IOCTL:
			break;
		default:
			dump_msg("ramdisk driver: unknown msg", &msg);
			spin("ramdisk::main_loop (invalid msg.type)");
			break;
		}

		send_recv(SEND, src, &msg);
	}*/
}

PRIVATE int rd_open(MESSAGE * p)
{
	return 0;
}

PRIVATE int rd_close(MESSAGE * p)
{
	return 0;
}

PRIVATE int rd_rdwt(MESSAGE * p)
{
	u64 pos = p->POSITION;
	int count = p->CNT;
	int dev = MINOR(p->DEVICE);

    struct ramdisk_dev * ramdisk; 
    if (dev == MINOR_INITRD) ramdisk = &initramdisk;
    else ramdisk = ramdisks + dev;

	char * addr = ramdisk->start + (int)pos;
	
	if (pos > ramdisk->length){
		p->CNT = 0;
		return 0;
	}

	if (p->type == DEV_WRITE) {
		if (ramdisk->rdonly) return EROFS;
		data_copy(getpid(), D, addr, p->PROC_NR, D, p->BUF, count);
	}else if(p->type == DEV_READ){
		data_copy(p->PROC_NR, D, p->BUF, getpid(), D, addr, count);
	}

	return 0;
}

PRIVATE int rd_ioctl(MESSAGE * p)
{
	return 0;
}

PRIVATE void init_rd()
{
	multiboot_module_t * initrd_mod = (multiboot_module_t *)mb_mod_addr;
	if (initrd_mod->pad) {
		printl("RAMDISK: invalid initrd module parameter(pad is not zero)");
	}

	initramdisk.start = (char*)(initrd_mod->mod_start + KERNEL_VMA);
	initramdisk.length = initrd_mod->mod_end - initrd_mod->mod_start;
	initramdisk.rdonly = 1;

	printl("RAMDISK: initrd: %d bytes(%d kB), base: 0x%x\n", initramdisk.length, initramdisk.length / 1024, initramdisk.start);
}

