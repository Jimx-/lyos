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

#define MAJOR_DEV	5
#include "../blk.h"

PUBLIC void task_scsi()
{
	MESSAGE msg;

	init_scsi();

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
			dump_msg("scsi driver: unknown msg", &msg);
			spin("FS::main_loop (invalid msg.type)");
			break;
		}

		send_recv(SEND, src, &msg);
	}
}

PUBLIC void scsi_handler(int irq)
{
}

PUBLIC void do_scsi_request()
{
}

PUBLIC void init_scsi()
{

}

