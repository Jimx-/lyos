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
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include <errno.h>
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <asm/const.h>
#include <asm/proto.h>
#ifdef CONFIG_SMP
#include <asm/smp.h>
#endif
#include "lyos/cpulocals.h"
#include <lyos/time.h>
#include <lyos/sysutils.h>
#include <asm/div64.h>
#include <lyos/watchdog.h>

int watchdog_enabled;
struct arch_watchdog* watchdog;

int init_profile_nmi(unsigned int freq)
{
    int retval;

    if (!watchdog_enabled) {
        return ENODEV;
    }

    if (!watchdog->init_profile) {
        stop_profile_nmi();
        return ENODEV;
    }

    retval = watchdog->init_profile(freq);
    if (retval) return retval;

    watchdog->reset_val = watchdog->profile_reset_val;

    if (watchdog->reset) {
        watchdog->reset(cpuid);
    }

    return 0;
}

void stop_profile_nmi(void)
{
    if (watchdog) {
        watchdog->reset_val = watchdog->watchdog_reset_val;
    }

    if (!watchdog_enabled) {
        arch_watchdog_stop();
    }
}

void nmi_watchdog_handler(int in_kernel, void* pc)
{
    /* no locks are allowed in NMI handler */
#if CONFIG_PROFILING
    if (kprofiling) {
        nmi_profile_handler(in_kernel, pc);
    }

    if ((watchdog_enabled || kprofiling) && watchdog->reset) {
        watchdog->reset(cpuid);
    }
#endif
}
