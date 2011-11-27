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

#ifndef _BLK_H
#define _BLK_H

struct request{
	MESSAGE * p;
	int free;
	struct request * next;
};

struct blk_dev
{
	struct request * r_current;
	void(*rq_handle)(void);
};

#define NR_REQUEST 32
#define NR_BLK_DEV	7

extern	struct request	requests[NR_REQUEST];
extern	struct blk_dev	blk_dev_table[NR_BLK_DEV];

#ifdef MAJOR_DEV
#if (MAJOR_DEV == 1)
/* ramdisk */
#define DEVICE_NAME		"ramdisk"
#define DEVICE_REQUEST	do_rd_request

#elif (MAJOR_DEV == 2)
/* floppy disk */
#define DEVICE_NAME		"floppy"
#define DEVICE_REQUEST	do_fd_request

#elif (MAJOR_DEV == 3)
/* hard disk */
#define DEVICE_NAME		"harddisk"
#define DEVICE_REQUEST	do_hd_request

#elif (MAJOR_DEV == 5)
/* scsi disk */
#define DEVICE_NAME "scsidisk"
#define DEVICE_REQUEST do_sd_request

#else
#error "unknown block device "

#endif
	
#define CURRENT (blk_dev_table[MAJOR_DEV].r_current)
#define HANDLE (blk_dev_table[MAJOR_DEV].rq_handle)
#endif

#endif

