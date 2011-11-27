/*  
 * 	(c)Copyright Jimx 2011
 * 
 *  This file is part of Lyos.

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
#include "config.h"
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
#include "keyboard.h"
#include "proto.h"

PRIVATE int file_rdwt(int rw, struct file_desc * file, char * buf, int len);

PUBLIC struct file_desc * file_open(char * filename)
{
	return 0;
}

PRIVATE int file_rdwt(int rw, struct file_desc * file, char * buf, int len)
{

	int pos = file->fd_pos;

	struct inode * pin = file->fd_inode;

	int pos_end;
	if (rw == READ)
		pos_end = min(pos + len, pin->i_size);
	else		/* WRITE */
		pos_end = min(pos + len, pin->i_nr_sects * SECTOR_SIZE);

	int off = pos % SECTOR_SIZE;
	int rw_sect_min=pin->i_start_sect+(pos>>SECTOR_SIZE_SHIFT);
	int rw_sect_max=pin->i_start_sect+(pos_end>>SECTOR_SIZE_SHIFT);

	int chunk = min(rw_sect_max - rw_sect_min + 1,
			FSBUF_SIZE >> SECTOR_SIZE_SHIFT);

	int bytes_rw = 0;
	int bytes_left = len;
	int i;
	for (i = rw_sect_min; i <= rw_sect_max; i += chunk) {
		/* read/write this amount of bytes every time */
		int bytes = min(bytes_left, chunk * SECTOR_SIZE - off);
		rw_sector(DEV_READ,
			  pin->i_dev,
			  i * SECTOR_SIZE,
			  chunk * SECTOR_SIZE,
			  TASK_FS,
			  fsbuf);

		if (rw == READ) {
				phys_copy((void*)(buf + bytes_rw),
					  (void*)(fsbuf + off),
					  bytes);
		}
		else {	/* WRITE */
			phys_copy((void*)(fsbuf + off),
				  (void*)(buf + bytes_rw),
				  bytes);
			rw_sector(DEV_WRITE,
				  pin->i_dev,
				  i * SECTOR_SIZE,
				  chunk * SECTOR_SIZE,
				  TASK_FS,
				  fsbuf);
		}
		off = 0;
		bytes_rw += bytes;
		file->fd_pos += bytes;
		bytes_left -= bytes;
	}

	if (file->fd_pos > pin->i_size) {
		/* update inode::size */
		pin->i_size = file->fd_pos;
		/* write the updated i-node back to disk */
		sync_inode(pin);
	}

	return bytes_rw;
}

PUBLIC int file_read(struct file_desc * file, char * buf, int len)
{
	return file_rdwt(READ, file, buf, len);
}

PUBLIC int file_write(struct file_desc * file, char * buf, int len)
{
	return file_rdwt(WRITE, file, buf, len);
}
