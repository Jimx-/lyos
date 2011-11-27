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
    
#include "type.h"
#include "stdio.h"
#include "unistd.h"
#include "config.h"
#include "const.h"
#include "protect.h"
#include "string.h"
#include "fs.h"
#include "proc.h"
#include "tty.h"
#include "console.h"
#include "global.h"
#include "proto.h"

#define MAJOR_DEV	1
#include "blk.h"

PRIVATE void rd_rdwt(MESSAGE * p);

PUBLIC void task_rd()
{
	MESSAGE msg;

	while(!rd_base || !rd_length);
	init_rd();

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
			/* add_request(MAJOR_DEV, &msg); */
			rd_rdwt(&msg);
			break;
		case DEV_IOCTL:
			break;
		default:
			dump_msg("ramdisk driver: unknown msg", &msg);
			spin("FS::main_loop (invalid msg.type)");
			break;
		}

		send_recv(SEND, src, &msg);
	}
}

PUBLIC void do_rd_request()
{
	if(CURRENT->p){
		rd_rdwt(CURRENT->p);
	}
}

PRIVATE void end_request()
{
	CURRENT->p = 0;
	if ((CURRENT->next)->p){
		CURRENT->free = 1;
		CURRENT = CURRENT->next;
	}
}

PRIVATE void rd_rdwt(MESSAGE * p)
{
	u64 pos = p->POSITION;
	char * addr = (char *)(rd_base + pos);
	int count = p->CNT;
	
	if ((char *)(addr + count) > (char *)(rd_base + rd_length)){
		end_request(MAJOR_DEV);
	//	do_rd_request();
	}
	if (p->type == DEV_WRITE){
		phys_copy(addr, (void*)va2la(p->PROC_NR, p->BUF), count);
	}else if(p->type == DEV_READ){
		phys_copy((void*)va2la(p->PROC_NR, p->BUF), addr, count); 
	}
	//send_recv(SEND, p->PROC_NR, p);
	//end_request(MAJOR_DEV);
	//do_rd_request();
}

PUBLIC void init_rd()
{
	char * c;

	blk_dev_table[MAJOR_DEV].rq_handle = DEVICE_REQUEST;

	c = (char *)rd_base;

	printl("RAMDISK: %d bytes(%d kB), base: 0x%x\n", rd_length, rd_length / 1024, rd_base);
}

PUBLIC void rd_load_image(dev_t dev, int offset)
{
	RD_SECT(dev, offset);
	struct super_block * sb = (struct super_block *)fsbuf;
	
	if(sb->magic == MAGIC_V1){
		printl("RAMDISK: Lyos filesystem found at sector %d\n", offset);
	}
	else return;
	
	int nr_sects = sb->nr_sects;
	if(nr_sects * 512 > rd_length){
		printl("RAMDISK: image too big! (%d/%d sectors)\n", nr_sects, rd_length / 512);
		return;
	}
	
	printl("Loading %d bytes into ramdisk...", nr_sects * 512);
	int i;
	char rotator[4] = {'|', '/', '-', '\\'};
	int rotate = 0;
	for(i=1;i<=nr_sects;i++){
		RD_SECT(dev, i);
		WR_SECT(MAKE_DEV(DEV_RD, 0), i);
		if (!(i%16)){
			printl("\b%c", rotator[rotate & 0x03]);
			rotate++;
		}
	}
	printl("done.\n");
	ROOT_DEV = MAKE_DEV(DEV_RD, 0);
	return;
}
