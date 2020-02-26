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
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <stddef.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/proc.h>
#include <lyos/global.h>
#include <lyos/proto.h>
#include <errno.h>
#include <lyos/sysutils.h>
#include <lyos/profile.h>

#include <asm/proto.h>

#if CONFIG_PROFILING

DEF_SPINLOCK(kprofile_lock);

PRIVATE void clear_recorded_flag()
{
    int i;
    for (i = 0; i < NR_TASKS + NR_PROCS; i++) {
        proc_table[i].flags &= ~PF_PROFILE_RECORDED;
    }
}

PUBLIC int sys_kprofile(MESSAGE* m, struct proc* p_proc)
{
    int ret = 0;
    spinlock_lock(&kprofile_lock);

    int action = m->KP_ACTION;
    switch (action) {
    case KPROF_START: {
        int freq = m->KP_FREQ;

        if (kprofiling) {
            ret = EBUSY;
            goto out;
        }

        memset(&kprof_info, 0, sizeof(kprof_info));

        clear_recorded_flag();
        init_profile_clock(freq);

        kprofiling = 1;
        break;
    }
    case KPROF_STOP: {
        if (!kprofiling) return EBUSY;
        kprofiling = 0;

        stop_profile_clock();

        int memsize = m->KP_SIZE;
        if (memsize > kprof_info.mem_used) {
            memsize = kprof_info.mem_used;
        }

        data_vir_copy(m->KP_ENDPT, m->KP_CTL, KERNEL, &kprof_info,
                      sizeof(&kprof_info));
        data_vir_copy(m->KP_ENDPT, m->KP_BUF, KERNEL, profile_sample_buf,
                      memsize);
        break;
    }
    default:
        ret = EINVAL;
        goto out;
    }
out:
    spinlock_unlock(&kprofile_lock);
    return ret;
}

#endif
