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
#include <lyos/type.h>
#include <lyos/ipc.h>
#include <lyos/const.h>
#include <stdio.h>
#include <string.h>
#include <lyos/sysutils.h>

PUBLIC clock_t kernel_alarm(clock_t expire_time, int abs_time)
{
    MESSAGE m;
    m.EXP_TIME = expire_time;
    m.ABS_TIME = abs_time;

    syscall_entry(NR_ALARM, &m);

    return m.TIME_LEFT;
}
