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
#include "const.h"
#include "protect.h"
#include "string.h"
#include "fs.h"
#include "proc.h"
#include "tty.h"
#include "console.h"
#include "global.h"
#include "proto.h"
#include "fd.h"
#include "fdreg.h"

#define MAJOR_DEV	2
#include "blk.h"

PRIVATE struct floppy_struct floppy_types[] = {
	{  720, 9,2,40,0,0x2A,0x02,0xDF,0x50,"360k/PC" }, /* 360kB PC diskettes */
	{  720, 9,2,40,0,0x2A,0x02,0xDF,0x50,"360k/PC" }, /* 360kB PC diskettes */
	{ 2400,15,2,80,0,0x1B,0x00,0xDF,0x54,"1.2M" },	  /* 1.2 MB AT-diskettes */
	{  720, 9,2,40,1,0x23,0x01,0xDF,0x50,"360k/AT" }, /* 360kB in 1.2MB drive */
	{ 1440, 9,2,80,0,0x2A,0x02,0xDF,0x50,"720k" },	  /* 3.5" 720kB diskette */
	{ 1440, 9,2,80,0,0x2A,0x02,0xDF,0x50,"720k" },	  /* 3.5" 720kB diskette */
	{ 2880,18,2,80,0,0x1B,0x00,0xCF,0x6C,"1.44M" },	  /* 1.44MB diskette */
	{ 1440, 9,2,80,0,0x2A,0x02,0xDF,0x50,"720k/AT" }, /* 3.5" 720kB diskette */
};

struct floppy_struct * drive_type[4];
char * floppy_buffer = (char *)0x500;
unsigned char reply[7];

PRIVATE void fd_identify(void);
PRIVATE int read_cmos(char addr);
/*
PRIVATE int fdc_out(int val);
PRIVATE int fdc_out_command(unsigned char * command, int len);
PRIVATE int fdc_result();
PRIVATE void interrupt_wait(); */
PRIVATE struct floppy_struct *find_type(int drive,int code);

PUBLIC void task_fd()
{
	MESSAGE msg;

	init_fd();

	while (1) {
	
		send_recv(RECEIVE, ANY, &msg);

		int src = msg.source;

		switch (msg.type) {
		case DEV_OPEN:
			break;
		case DEV_CLOSE:
			break;
		case DEV_READ:
		case DEV_WRITE:
			add_request(MAJOR_DEV, &msg);
			break;
		case DEV_IOCTL:
			break;
		default:
			dump_msg("floppy driver: unknown msg", &msg);
			spin("FS::main_loop (invalid msg.type)");
			break;
		}

		send_recv(SEND, src, &msg);
	}
}

/*
PRIVATE int fdc_out(int val)
{
	out_byte(FD_DATA, val);
	return 0;
}

PRIVATE int fdc_out_command(unsigned char * command, int len)
{
	while (len > 0) {
		fdc_out(*command++);
		len--;
	}
	return 0;
}


PRIVATE int fdc_result()
{
	int i = 0;
	int counter, status;
	
	for(counter=0;counter<=10000;counter++){
		status = in_byte(FD_STATUS) & (STATUS_BUSY|STATUS_READY|STATUS_DIR);
		if(status == STATUS_READY) return i;
		if(status == (STATUS_BUSY|STATUS_READY|STATUS_DIR)){
			if(i>=7) break;
		reply[i++] = in_byte(FD_DATA);
		}
	}
	return 0;
}
*/

PRIVATE struct floppy_struct * find_type(int drive,int code)
{
	struct floppy_struct *base;

	if (code > 0 && code < 5) {
		base = &floppy_types[(code-1)*2];
		printl("fd%d is %s",drive,base->name);
		return base;
	}
	printl("fd%d is unknown type %d",drive,code);
	return 0;
}

PRIVATE void fd_identify(void)
{
	printl("Floppy drive(s): ");
	drive_type[0] = find_type(0,(read_cmos(0x10) >> 4) & 15);
	if (((read_cmos(0x14) >> 6) & 1) == 0) drive_type[0] = 0;
	else {
		printl(", ");
		drive_type[1] = find_type(1,read_cmos(0x10) & 15);
	}
	drive_type[2] = drive_type[3] = 0;
	printl("\n");
}

/*
PRIVATE void floppy_on(unsigned int nr)
{
	unsigned char mask = 0x10 << nr;

	mask |= 0x0c;

	out_byte(FD_DOR, mask);
}
*/

/*
PRIVATE void fd_rdwt(MESSAGE * p)
{
	u64 pos = p->POSITION;
	
	u32 sector = (u32)(pos >> SECTOR_SIZE_SHIFT);
	
	struct floppy_struct * floppy = floppy_types + 6; 
}
*/


PUBLIC void fd_handler(int irq)
{
	inform_int(TASK_FD);
}

/*
PRIVATE void interrupt_wait()
{
	MESSAGE msg;
	send_recv(RECEIVE, INTERRUPT, &msg);
}
* */

PUBLIC void do_fd_request()
{
}

PUBLIC void init_fd()
{
	put_irq_handler(FLOPPY_IRQ, fd_handler);
	enable_irq(FLOPPY_IRQ);
	
	fd_identify();
	
	blk_dev_table[MAJOR_DEV].rq_handle = DEVICE_REQUEST;
}

PRIVATE int read_cmos(char addr)
{
	out_byte(addr,0x70);
	return in_byte(0x71);
}
