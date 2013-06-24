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
#include "string.h"

/*****************************************************************************
 *                                unlink
 *****************************************************************************/
/**
 * Delete a file.
 * 
 * @param pathname  The full path of the file to delete.
 * 
 * @return Zero if successful, otherwise -1.
 *****************************************************************************/
PUBLIC int unlink(const char * pathname)
{
	MESSAGE msg;
	msg.type   = UNLINK;

	msg.PATHNAME	= (void*)pathname;
	msg.NAME_LEN	= strlen(pathname);

	send_recv(BOTH, TASK_FS, &msg);

	return msg.RETVAL;
}
