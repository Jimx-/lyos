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
#include "sys/types.h"
#include "lyos/config.h"
#include "errno.h"
#include "stdio.h"
#include "stddef.h"
#include "unistd.h"
#include "assert.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/tty.h"
#include "lyos/console.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "lyos/hd.h"
#include "lyos/list.h"

PUBLIC void task_initfs()
{
	printl("initfs: InitFS driver is running\n");

	MESSAGE m;

	int reply;

	while (1) {
		send_recv(RECEIVE, ANY, &m);

		int msgtype = m.type;
		int src = m.source;
		reply = 1;

		switch (msgtype) {
			/*
		case FS_LOOKUP:
			m.RET_RETVAL = ext2_lookup(&m);
            break;
		case FS_PUTINODE:
			break;
        case FS_MOUNTPOINT:
            m.RET_RETVAL = ext2_mountpoint(&m);
            break;
        case FS_READSUPER:
            m.RET_RETVAL = ext2_readsuper(&m);
            break;
        case FS_STAT:
        	m.STRET = ext2_stat(&m);
        	break;
        case FS_RDWT:
        	m.RWRET = ext2_rdwt(&m);
        	break;
        case FS_CREATE:
        	m.CRRET = ext2_create(&m);
        	break;
        case FS_FTRUNC:
        	m.RET_RETVAL = ext2_ftrunc(&m);
        	break;
        case FS_SYNC:
        	m.RET_RETVAL = ext2_sync();
        	break;*/
		default:
			dump_msg("ext2: unknown message:", &m);
			break;
		}

		/* reply */
		if (reply) {
			m.type = FSREQ_RET;
			send_recv(SEND, src, &m);
		}
	}
}
