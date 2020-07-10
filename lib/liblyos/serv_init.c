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
    along with Lyos.  If not, see <http://www.gnu.org/licenses/". */

#include <lyos/types.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "lyos/const.h"
#include "stdio.h"
#include "stdarg.h"
#include "unistd.h"
#include "assert.h"
#include "lyos/service.h"
#include <lyos/sysutils.h>

static int init_fresh_callback_default() { return 0; }

static serv_init_fresh_callback_t init_fresh_callback =
    init_fresh_callback_default;

void serv_register_init_fresh_callback(serv_init_fresh_callback_t cb)
{
    if (cb == NULL) return;
    init_fresh_callback = cb;
}

int serv_init()
{
    MESSAGE msg;

    do {
        send_recv(RECEIVE, TASK_SERVMAN, &msg);
    } while (msg.type != SERVICE_INIT);

    int retval = 0;
    switch (msg.REQUEST) {
    case SERVICE_INIT_FRESH:
        retval = init_fresh_callback();
        break;
    default:
        panic("invalid init request from servman");
    }

    msg.type = SERVICE_INIT_REPLY;
    msg.RETVAL = retval;
    send_recv(SEND, TASK_SERVMAN, &msg);

    return retval;
}
