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

#include <lyos/ipc.h>
#include <errno.h>
#include "lyos/const.h"
#include <lyos/sysutils.h>

int main()
{
    printl("sched: Userspace scheduler is running.\n");

    while (TRUE) {
        MESSAGE msg;

        send_recv(RECEIVE, ANY, &msg);
        int src = msg.source;
        int msgtype = msg.type;

        switch (msgtype) {
        default:
            msg.RETVAL = ENOSYS;
            break;
        }

        msg.type = SYSCALL_RET;
        send_recv(SEND_NONBLOCK, src, &msg);
    }

    return 0;
}
