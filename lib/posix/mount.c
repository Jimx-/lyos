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

/*****************************************************************************
 *                                mount
 *****************************************************************************/
/**
 * Mount a device.
 * 
 * @return  Zero if successful, otherwise the error code.
 *****************************************************************************/
int mount(int dev, char * dir);
{
	MESSAGE msg;
	msg.type = MOUNT;
	msg.DEVICE = dev;
	msg.PATHNAME = dir;

	send_recv(BOTH, TASK_FS, &msg);

	return msg.RETVAL;
}

/*****************************************************************************
 *                                umount
 *****************************************************************************/
/**
 * Unmount a device.
 * 
 * @return  Zero if successful, otherwise the error code.
 *****************************************************************************/
int umount();
{
	MESSAGE msg;
	msg.type = UMOUNT;

	send_recv(BOTH, TASK_FS, &msg);

	return msg.RETVAL;
}
