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
#include "stdio.h"
#include "assert.h"
#include "unistd.h"
#include "errno.h"
#include "lyos/const.h"
#include <lyos/fs.h>
#include "string.h"
#include <limits.h>
#include <lyos/ipc.h>
#include <lyos/input.h>
#include <libsysfs/libsysfs.h>
#include "libinputdriver/libinputdriver.h"

PRIVATE endpoint_t input_endpoint = NO_TASK;

PRIVATE void get_input_ep()
{
    if (input_endpoint == NO_TASK) {
        u32 v;

        int retval = sysfs_retrieve_u32("services.input.endpoint", &v);

        if (retval == 0) input_endpoint = (endpoint_t)v;
    }
}

PUBLIC int inputdriver_send_event(u16 type, u16 code, int value)
{
    if (input_endpoint == NO_TASK) get_input_ep();

    MESSAGE msg;

    msg.type = INPUT_SEND_EVENT;
    msg.IEV_TYPE = type;
    msg.IEV_CODE = code;
    msg.IEV_VALUE = value;

    send_recv(BOTH, input_endpoint, &msg);

    return msg.RETVAL;
}

PUBLIC int inputdriver_start(struct inputdriver* inpd)
{
    MESSAGE msg;

    while (TRUE) {
        send_recv(RECEIVE, ANY, &msg);

        int src = msg.source;

        /* notify */
        if (msg.type == NOTIFY_MSG) {
            switch (src) {
            case INTERRUPT:
                if (inpd->input_interrupt) 
                    inpd->input_interrupt(msg.INTERRUPTS);
                break;
            }
            continue;
        }

        switch (msg.type) {
        default:
            if (inpd->input_other) msg.RETVAL = inpd->input_other(&msg);
            else msg.RETVAL = ENOSYS;
            break;
        }

        msg.type = SYSCALL_RET;
        send_recv(SEND, src, &msg);
    }

    return 0;
}
