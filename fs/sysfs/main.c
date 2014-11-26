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
#include "lyos/global.h"
#include "lyos/proto.h"
#include "lyos/list.h"

PUBLIC int main()
{
	printl("sysfs: SysFS is running\n");

	MESSAGE m;

	int reply;

	while (1) {
		send_recv(RECEIVE, ANY, &m);

		int msgtype = m.type;
		int src = m.source;
		reply = 1;

		switch (msgtype) {
		case SYSFS_PUBLISH:
			break;
		default:
			printl("sysfs: unknown message\n");
			break;
		}

		/* reply */
		if (reply) {
			m.type = FSREQ_RET;
			send_recv(SEND, src, &m);
		}
	}

    return 0;
}

