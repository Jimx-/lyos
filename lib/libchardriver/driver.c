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

#include <lyos/type.h>
#include <sys/types.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/proc.h>
#include <lyos/global.h>
#include <lyos/driver.h>
#include <lyos/proto.h>

PUBLIC int announce_chardev(char * name, dev_t dev)
{
	MESSAGE msg;

	msg.type = ANNOUNCE_DEVICE;
	msg.BUF = name;
	msg.NAME_LEN = strlen(name);
	msg.PROC_NR = get_endpoint();
	msg.DEVICE = dev;
	msg.FLAGS = DT_CHARDEV;

	send_recv(BOTH, TASK_DEVMAN, &msg);

	return msg.RETVAL;
}
