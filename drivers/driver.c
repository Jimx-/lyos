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
#include "driver.h"
#include "proto.h"

PUBLIC void dev_driver_task(struct dev_driver * dd)
{
	MESSAGE msg;
	while (1) {
		send_recv(RECEIVE, ANY, &msg);

		int src = msg.source;

		switch (msg.type) {
		case DEV_OPEN:
			(*dd->dev_open)(&msg);
			break;

		case DEV_CLOSE:
			(*dd->dev_close)(&msg);
			break;

		case DEV_READ:
		case DEV_WRITE:
			(*dd->dev_rdwt)(&msg);
			break;

		case DEV_IOCTL:
			(*dd->dev_ioctl)(&msg);
			break;

		default:
			dump_msg("dev_driver_task::unknown msg", &msg);
			spin("FS::main_loop (invalid msg.type)");
			break;
		}

		if ((msg.type != DEV_READ) && (msg.type != DEV_WRITE))send_recv(SEND, src, &msg);
	}
}
