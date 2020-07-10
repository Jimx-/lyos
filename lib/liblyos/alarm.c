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

#include <lyos/config.h>
#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/const.h>
#include <stdio.h>
#include <string.h>
#include <lyos/sysutils.h>

int kernel_alarm2(clock_t expire_time, int abs_time, clock_t* time_left)
{
    MESSAGE m;
    m.EXP_TIME = expire_time;
    m.ABS_TIME = abs_time;

    int retval = syscall_entry(NR_ALARM, &m);
    if (retval) return retval;

    if (time_left) *time_left = m.TIME_LEFT;
    return 0;
}
