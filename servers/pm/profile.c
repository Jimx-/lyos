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

#include <lyos/type.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "lyos/config.h"
#include "stdio.h"
#include "unistd.h"
#include "stddef.h"
#include "errno.h"
#include "assert.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <lyos/sysutils.h>
#include <lyos/profile.h>
#include "proto.h"
#include "const.h"
#include "global.h"

int do_pm_kprofile(MESSAGE* msg)
{
#if CONFIG_PROFILING
    switch (msg->KP_ACTION) {
    case KPROF_START:
    case KPROF_STOP:
        return kernel_kprofile(msg->KP_ACTION, msg->KP_SIZE, msg->KP_FREQ,
                               msg->source, msg->KP_CTL, msg->KP_BUF);
    }

    return EINVAL;
#else
    return ENOSYS;
#endif
}
