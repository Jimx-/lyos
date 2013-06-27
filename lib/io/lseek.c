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


/*****************************************************************************
 *                                lseek
 *****************************************************************************/
/**
 * Reposition r/w file offset.
 * 
 * @param fd      File descriptor.
 * @param offset  The offset according to `whence'.
 * @param whence  SEEK_SET, SEEK_CUR or SEEK_END.
 * 
 * @return  The resulting offset location as measured in bytes from the
 *          beginning of the file.
 *****************************************************************************/
PUBLIC int lseek(int fd, int offset, int whence)
{
	MESSAGE msg;
	msg.type   = LSEEK;
	msg.FD     = fd;
	msg.OFFSET = offset;
	msg.WHENCE = whence;

	send_recv(BOTH, TASK_FS, &msg);

	return msg.OFFSET;
}
