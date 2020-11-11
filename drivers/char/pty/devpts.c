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
#include <lyos/const.h>
#include <lyos/ipc.h>
#include <errno.h>
#include <string.h>

#include <libsysfs/libsysfs.h>

static endpoint_t devpts_endpoint = NO_TASK;

static int devpts_sendrec(MESSAGE* msg)
{
    int retval;
    u32 v;

    if (devpts_endpoint == NO_TASK) {
        retval = sysfs_retrieve_u32("services.devpts.endpoint", &v);
        if (retval) return EDEADSRCDST;

        devpts_endpoint = (endpoint_t)v;
    }

    return send_recv(BOTH, devpts_endpoint, msg);
}

int devpts_clear(unsigned int index)
{
    MESSAGE m;
    int retval;

    memset(&m, 0, sizeof(m));
    m.type = DEVPTS_CLEAR;
    m.u.m_devpts_req.index = index;

    retval = devpts_sendrec(&m);
    if (retval) return retval;

    return m.u.m_devpts_req.status;
}

int devpts_set(unsigned int index, mode_t mode, uid_t uid, gid_t gid, dev_t dev)
{
    MESSAGE m;
    int retval;

    memset(&m, 0, sizeof(m));
    m.type = DEVPTS_SET;
    m.u.m_devpts_req.index = index;
    m.u.m_devpts_req.mode = mode;
    m.u.m_devpts_req.uid = uid;
    m.u.m_devpts_req.gid = gid;
    m.u.m_devpts_req.dev = dev;

    retval = devpts_sendrec(&m);
    if (retval) return retval;

    return m.u.m_devpts_req.status;
}
