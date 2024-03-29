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
#include <lyos/const.h>

#include <libdevman/libdevman.h>

int dm_device_register(struct device_info* devinf, device_id_t* id)
{
    MESSAGE msg;

    msg.type = DM_DEVICE_REGISTER;
    msg.BUF = devinf;
    msg.BUF_LEN = sizeof(*devinf);

    send_recv(BOTH, TASK_DEVMAN, &msg);

    if (msg.u.m_devman_register_reply.status == 0) {
        *id = msg.u.m_devman_register_reply.id;
    }

    return msg.u.m_devman_register_reply.status;
}
