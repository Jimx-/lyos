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
#include "string.h"
#include <lyos/ipc.h>
#include "libsysfs.h"

PUBLIC int sysfs_publish_domain(char * key, int flags)
{
    MESSAGE msg;

    msg.type = SYSFS_PUBLISH;

    msg.PATHNAME = key;
    msg.NAME_LEN = strlen(key);
    msg.FLAGS = flags;
    msg.MODE = ET_DOMAIN;

    send_recv(BOTH, TASK_SYSFS, &msg);

    return msg.RETVAL;
}

PUBLIC int sysfs_publish_u32(char * key, u32 value, int flags)
{
    MESSAGE msg;

    msg.type = SYSFS_PUBLISH;

    msg.PATHNAME = key;
    msg.NAME_LEN = strlen(key);
    msg.FLAGS = flags;
    msg.u.m3.m3i3 = value;

    send_recv(BOTH, TASK_SYSFS, &msg);

    return msg.RETVAL;
}
