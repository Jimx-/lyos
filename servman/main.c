/*  
    (c)Copyright 2014 Jimx
    
    This file is part of Lyos.

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
#include "stdio.h"
#include "unistd.h"
#include "assert.h"
#include <errno.h>
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/keyboard.h"
#include "lyos/proto.h"
#include "proto.h"

PRIVATE void servman_init();

PUBLIC void task_servman()
{
    servman_init();

    while(1){
        MESSAGE msg;

        send_recv(RECEIVE, ANY, &msg);
        int src = msg.source;

        int msgtype = msg.type;

        switch (msgtype) {
        case SERVICE_UP:
            msg.RETVAL = do_service_up(&msg);
            break;
        default:
            dump_msg("SERVMAN::unknown msg", &msg);
            assert(0);
            break;
        }

        msg.type = SYSCALL_RET;
        send_recv(SEND, src, &msg);
    }
}

PRIVATE void servman_init()
{
    printl("SERVMAN: service manager is running.\n");

    serv_prepare_module_stack();
    spawn_boot_modules();
}
