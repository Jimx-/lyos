/*
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

#include <lyos/type.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <lyos/config.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/proc.h>
#include <lyos/global.h>
#include <lyos/proto.h>
#include <lyos/service.h>
#include <lyos/input.h>

#include <libsysfs/libsysfs.h>
#include <libchardriver/libchardriver.h>

static int init_input();
static int input_event(MESSAGE* msg);
static void input_other(MESSAGE* msg);

static struct chardriver input_driver = {
    .cdr_other = input_other,
};

int main()
{
    serv_register_init_fresh_callback(init_input);
    serv_init();

    chardriver_task(&input_driver);

    return 0;
}

static int init_input()
{
    /* tell TTY that we're up */
    MESSAGE msg;
    msg.type = INPUT_TTY_UP;

    send_recv(SEND, TASK_TTY, &msg);

    return 0;
}

static int input_event(MESSAGE* msg)
{
    MESSAGE msg2tty;

    msg2tty.type = INPUT_TTY_EVENT;
    msg2tty.u.m_input_tty_event.type = msg->u.m_inputdriver_input_event.type;
    msg2tty.u.m_input_tty_event.code = msg->u.m_inputdriver_input_event.code;
    msg2tty.u.m_input_tty_event.value = msg->u.m_inputdriver_input_event.value;

    send_recv(SEND, TASK_TTY, &msg2tty);

    return SUSPEND;
}

static void input_other(MESSAGE* msg)
{
    int src = msg->source;

    switch (msg->type) {
    case INPUT_SEND_EVENT:
        msg->RETVAL = input_event(msg);
        break;
    default:
        msg->RETVAL = ENOSYS;
        break;
    }

    if (msg->RETVAL != SUSPEND) {
        msg->type = SYSCALL_RET;
        send_recv(SEND, src, msg);
    }
}
