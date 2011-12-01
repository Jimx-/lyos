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

#define DEBUG
#ifdef DEBUG
#define DEB(x) printl("Floppy driver: "); \
		x
#else
#define DEB(x)
#endif

#define MAJOR_DEV	2
#include "blk.h"

#define DRV_OF_DEV(dev) (dev & 0x03)
#define OUT_DOR(x) out_byte(FD_DOR, x)
#define READ_STATUS() in_byte(FD_STATUS)
#define NORMAL_DOR 0xc
#define GEN_DOR(ena, enb, enc, end, sel) (NORMAL_DOR + \
(ena ? 0x10 : 0) + \
(enb ? 0x20 : 0) + \
(enc ? 0x40 : 0) + \
(end ? 0x80 : 0) + sel)
#define READY(a) ((a & STATUS_READY) > 0)
#define FDC2CPU(a) (((a & STATUS_DIR) > 0) && READY(a))
#define CPU2FDC(a) (((a & STATUS_DIR) == 0) && READY(a))

PRIVATE struct floppy_struct floppy_types[] = {
	/*size, sect, head, track, stretch,  gap, rate, spec1, fmt_gap,      name */
	{  720,    9,    2,    40,       0, 0x2A, 0x02,  0xDF,    0x50,  "360k/PC" }, /* 360kB PC diskettes */
	{  720,    9,    2,    40,       0, 0x2A, 0x02,  0xDF,    0x50,  "360k/PC" }, /* 360kB PC diskettes */
	{ 2400,   15,    2,    80,       0, 0x1B, 0x00,  0xDF,    0x54,  "1.2M" },	  /* 1.2 MB AT-diskettes */
	{  720,    9,    2,    40,       1, 0x23, 0x01,  0xDF,    0x50,  "360k/AT" }, /* 360kB in 1.2MB drive */
	{ 1440,    9,    2,    80,       0, 0x2A, 0x02,  0xDF,    0x50,  "720k" },	  /* 3.5" 720kB diskette */
	{ 1440,    9,    2,    80,       0, 0x2A, 0x02,  0xDF,    0x50,  "720k" },	  /* 3.5" 720kB diskette */
	{ 2880,   18,    2,    80,       0, 0x1B, 0x00,  0xCF,    0x6C,  "1.44M" },	  /* 1.44MB diskette */
	{ 1440,    9,    2,    80,       0, 0x2A, 0x02,  0xDF,    0x50,  "720k/AT" }, /* 3.5" 720kB diskette */
};

#define SPC drive_type[selected]->sect
#define HEADS drive_type[selected]->head
#define DISK_SIZE drive_type[selected]->size

struct floppy_struct * drive_type[4];
char * floppy_buffer = (char *)0x500; // use 0x500-0x900(1024 bytes) as temp buffer
unsigned char reply[7];

unsigned char selected;
unsigned char motor[4];

PRIVATE void fd_identify(void);
PRIVATE int read_cmos(char addr);
PRIVATE void choose_drive(unsigned char which);
PRIVATE void fd_delay();
PRIVATE unsigned long read_fdc_data();
PRIVATE void write_fdc_data(unsigned char data);
PRIVATE void recalibrate();
PRIVATE void fd_rdwt(MESSAGE * p);
PRIVATE void do_DMA(int read, unsigned long buffer, unsigned long len);
PRIVATE void do_fdc_rw(int read, unsigned long start);
PRIVATE void convert_sector_nr(unsigned long sectornr, unsigned long *sec, unsigned long *track, unsigned long *head);
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
			DEB(printl("Message DEV_OPEN received.\n"));
			break;
		case DEV_CLOSE:
			DEB(printl("Message DEV_CLOSE received.\n"));
			break;
		case DEV_READ:
		case DEV_WRITE:
			DEB(printl("Message DEV_READ/WRITE received.\n"));
			/* add_request(MAJOR_DEV, &msg); */
			fd_rdwt(&msg);
			break;
		case DEV_IOCTL:
			DEB(printl("Message DEV_IOCTL received.\n"));
			break;
		default:
			dump_msg("floppy driver: unknown msg", &msg);
			spin("FD::main_loop (invalid msg.type)");
			break;
		}

		send_recv(SEND, src, &msg);
	}
}

PRIVATE void choose_drive(unsigned char which)
{
    selected = which;
    if (motor[which] == 0)
    {
        motor[which] = 1;
        OUT_DOR(GEN_DOR(motor[0], motor[1], motor[2], motor[3], which));
        fd_delay();
    }
    else
    {
        OUT_DOR(GEN_DOR(motor[0], motor[1], motor[2], motor[3], which));
    }
}

PRIVATE void fd_delay()
{
    int i;
    for (i = 0; i < 1000000; i++)
    {
    }
}

PRIVATE unsigned long read_fdc_data()
{
    unsigned long i = 0;
    while (!(FDC2CPU(READ_STATUS())))
    {
        if (i > 10000000)
        {
            return 0;
        }
        i++;
    }
    return in_byte(FD_DATA);
}

PRIVATE void write_fdc_data(unsigned char data)
{
    unsigned long i = 0;
    while (!(CPU2FDC(READ_STATUS())))
    {
        if (i > 10000000)
        {
            return;
        }
        i++;
    }
    out_byte(FD_DATA, data);
}

PRIVATE void recalibrate()
{
    DEB(printl("Floppy recalibrated.\n"));
    write_fdc_data(0x07);
    write_fdc_data(selected);
    write_fdc_data(0x07);
    write_fdc_data(selected + 4);
}

PRIVATE void fd_rdwt(MESSAGE * p)
{
	int read;
	if (p->type == DEV_READ) read = 1;
	else read = 0; 

	int start = p->POSITION;
	int len = p->CNT;
	int drive = DRV_OF_DEV(p->DEVICE);

	void * la = (void*)va2la(p->PROC_NR, p->BUF);

    	if (len > (1 * 1024))
    	{
        	printl("Floppy error: length out of range, set to 1k.");
        	len = 1 * 1024;
    	}
    	if ((start + len) > DISK_SIZE)
    	{
        	printl("Floppy error: out of disk size.");
        	return;
    	}
    	while (drive != selected)
    	{
     		choose_drive(drive);
    	}
    	if ((unsigned long)la > 0x100000)
    	{
        	if (!read)
        	{
            		memcpy((void *)floppy_buffer, la, len);
        	}
        	do_DMA(read, (unsigned long)floppy_buffer, len);
        	do_fdc_rw(read, start);
        	if (read)
        	{
           	 memcpy(la, floppy_buffer, len);
        	}
    	}
    	else
    	{
        	do_DMA(read, (unsigned long)la, len);
        	do_fdc_rw(read, start);
    	}
}

#define LOW16(data) (data & 0xFF)
#define HIGH16(data) ((data & 0xFF00) / 0x100)
#define LOW32(data) (data & 0xFFFF)
#define HIGH32(data) ((data & 0xFFFF0000) / 0x10000)

PRIVATE void do_DMA(int read, unsigned long buffer, unsigned long len)
{
	len--;
    	out_byte(0x0a, 0x06);
   	out_byte(0x0c, (read ? 0x46 : 0x4a));
    	out_byte(0x0b, (read ? 0x46 : 0x4a));
    	out_byte(0x04, LOW16(LOW32(buffer)));
    	out_byte(0x04, HIGH16(LOW32(buffer)));
    	out_byte(0x81, LOW16(HIGH32(buffer)));
    	out_byte(0x05, LOW16(len));
    	out_byte(0x05, HIGH16(len));
    	out_byte(0x0a, 0x02);
}

PRIVATE void do_fdc_rw(int read, unsigned long start)
{
	unsigned long sector, track, head;
	convert_sector_nr(start / SECTOR_SIZE, &sector, &track, &head);
	write_fdc_data((read?0xe6:0xc5));
	write_fdc_data(selected + (head<<2));
	write_fdc_data(track);
    	write_fdc_data(head);
    	write_fdc_data(sector);
    	write_fdc_data(SECTOR_SIZE / 256);
    	write_fdc_data(SPC);
    	write_fdc_data(SECTOR_SIZE / 256 + 1);
    	write_fdc_data(0);
    	int i;
    	for (i = 0; i < 7; i++)
    	{
        	read_fdc_data();
    	}
}

PRIVATE void convert_sector_nr(unsigned long sectornr, unsigned long *sec, unsigned long *track, unsigned long *head)
{
    unsigned long q;
    *sec = sectornr % SPC + 1;
    q = sectornr / SPC;
    *track = q / HEADS;
    *head = q % HEADS;
}

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
