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

#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/config.h>
#include "sys/types.h"
#include "stdio.h"
#include <stdlib.h>
#include "assert.h"
#include "unistd.h"
#include "errno.h"
#include "lyos/const.h"
#include <lyos/fs.h>
#include "string.h"
#include <limits.h>
#include <lyos/sysutils.h>
#include <lyos/input.h>
#include <libsysfs/libsysfs.h>
#include <libdevman/libdevman.h>
#include <libinputdriver/libinputdriver.h>

static endpoint_t __input_endpoint = NO_TASK;

static void get_input_ep()
{
    u32 v;
    int retval;

    if (__input_endpoint == NO_TASK) {
        retval = sysfs_retrieve_u32("services.input.endpoint", &v);
        if (retval) {
            panic("failed to retrieve input endpoint (%d)", retval);
        }

        __input_endpoint = (endpoint_t)v;
    }
}

static int input_sendrec(int function, MESSAGE* msg)
{
    if (__input_endpoint == NO_TASK) get_input_ep();

    return send_recv(function, __input_endpoint, msg);
}

struct inputdriver_dev* inputdriver_allocate_device(void)
{
    struct inputdriver_dev* dev;

    dev = malloc(sizeof(*dev));
    if (dev) {
        memset(dev, 0, sizeof(*dev));
        dev->dev_id = NO_DEVICE_ID;
    }

    return dev;
}

int inputdriver_register_device(struct inputdriver_dev* dev)
{
    MESSAGE msg;
    struct input_dev_bits dev_bits;

    if (dev->registered) {
        return EINVAL;
    }

    dev_bits.evbit = dev->evbit;
    dev_bits.keybit = dev->keybit;
    dev_bits.relbit = dev->relbit;
    dev_bits.absbit = dev->absbit;
    dev_bits.mscbit = dev->mscbit;
    dev_bits.ledbit = dev->ledbit;
    dev_bits.sndbit = dev->sndbit;
    dev_bits.ffbit = dev->ffbit;
    dev_bits.swbit = dev->swbit;

    msg.type = INPUT_REGISTER_DEVICE;
    msg.u.m_inputdriver_register_device.dev_id = dev->dev_id;
    msg.u.m_inputdriver_register_device.dev_bits = &dev_bits;
    msg.u.m_inputdriver_register_device.input_id = &dev->input_id;

    input_sendrec(BOTH, &msg);

    if (msg.u.m_input_conf.status) {
        return msg.u.m_input_conf.status;
    }

    dev->id = msg.u.m_input_conf.id;
    dev->registered = TRUE;

    return 0;
}

int inputdriver_send_event(struct inputdriver_dev* dev, u16 type, u16 code,
                           int value)
{
    MESSAGE msg;

    if (!dev->registered) {
        return EINVAL;
    }

    msg.type = INPUT_SEND_EVENT;
    msg.u.m_inputdriver_input_event.id = dev->id;
    msg.u.m_inputdriver_input_event.type = type;
    msg.u.m_inputdriver_input_event.code = code;
    msg.u.m_inputdriver_input_event.value = value;

    return input_sendrec(SEND, &msg);
}

int inputdriver_start(struct inputdriver* inpd)
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
            if (inpd->input_other) {
                inpd->input_other(&msg);

                continue;
            } else
                msg.RETVAL = ENOSYS;
            break;
        }

        msg.type = SYSCALL_RET;
        send_recv(SEND, src, &msg);
    }

    return 0;
}
