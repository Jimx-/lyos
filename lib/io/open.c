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
#include "lyos/const.h"
#include "lyos/ipc.h"
#include "errno.h"
#include "stdio.h"
#include "assert.h"
#include "string.h"

/*****************************************************************************
 *                                open
 *****************************************************************************/
/**
 * open/create a file.
 * 
 * @param pathname  The full path of the file to be opened/created.
 * @param flags     O_CREAT, O_RDWR, etc.
 * 
 * @return File descriptor if successful, otherwise -1.
 *****************************************************************************/
PUBLIC int open(const char *pathname, int flags)
{
	MESSAGE msg;

	msg.type	= OPEN;

	msg.PATHNAME	= (void*)pathname;
	msg.FLAGS	= flags;
	msg.NAME_LEN	= strlen(pathname);

	send_recv(BOTH, TASK_FS, &msg);
	assert(msg.type == SYSCALL_RET);

	if (msg.FD >= 0) return msg.FD;
	errno = -msg.FD;
	return -1;
}

PUBLIC int mkdir(const char *pathname)
{
	MESSAGE msg;

	msg.type	= MKDIR;

	msg.PATHNAME	= (void*)pathname;
	msg.NAME_LEN	= strlen(pathname);

	send_recv(BOTH, TASK_FS, &msg);
	assert(msg.type == SYSCALL_RET);

	return 0;
}
