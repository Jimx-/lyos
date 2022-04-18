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
#include "lyos/const.h"
#include <string.h>

pid_t get_epinfo(endpoint_t ep, uid_t* euid, gid_t* egid)
{
    MESSAGE m;

    memset(&m, 0, sizeof(MESSAGE));
    m.type = PM_GETEPINFO;
    m.ENDPOINT = ep;

    send_recv(BOTH, TASK_PM, &m);

    if (m.RETVAL < 0) return m.RETVAL;

    if (euid) *euid = m.EP_EFFUID;
    if (egid) *egid = m.EP_EFFGID;

    return m.RETVAL;
}
