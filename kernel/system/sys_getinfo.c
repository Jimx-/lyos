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
#include "stdarg.h"
#include "unistd.h"
#include "assert.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <errno.h>
#include <asm/type.h>
#include <asm/proto.h>

extern struct cpu_info cpu_info[CONFIG_SMP_MAX_CPUS];

/*======================================================================*
                               sys_getinfo
 *======================================================================*/
int sys_getinfo(MESSAGE* m, struct proc* p_proc)
{
    int request = m->REQUEST;
    void* buf = m->BUF;
    size_t buf_len = m->BUF_LEN;
    void* addr = NULL;
    size_t size = 0;
    struct sysinfo** psi;
    u64 ticks[CPU_STATES];
    unsigned int cpu;

    switch (request) {
    case GETINFO_SYSINFO:
        psi = (struct sysinfo**)buf;
        *psi = sysinfo_user;
        break;
    case GETINFO_KINFO:
        addr = (void*)&kinfo;
        size = sizeof(kinfo_t);
        break;
    case GETINFO_CMDLINE:
        addr = (void*)&kinfo.cmdline;
        size = sizeof(kinfo.cmdline);
        break;
    case GETINFO_BOOTPROCS:
        addr = (void*)&kinfo.boot_procs;
        size = sizeof(kinfo.boot_procs);
        break;
    case GETINFO_HZ:
        m->RETVAL = system_hz;
        return 0;
    case GETINFO_MACHINE:
        addr = (void*)&machine;
        size = sizeof(machine);
        break;
    case GETINFO_CPUINFO:
        addr = (void*)cpu_info;
        size = sizeof(cpu_info);
        break;
    case GETINFO_PROCTAB:
        addr = (void*)proc_table;
        size = sizeof(struct proc) * (NR_TASKS + NR_PROCS);
        break;
    case GETINFO_CPUTICKS:
        cpu = m->u.m3.m3i1;

        if (cpu >= CONFIG_SMP_MAX_CPUS) {
            return EINVAL;
        }

        get_cpu_ticks(cpu, ticks);
        addr = (void*)ticks;
        size = sizeof(ticks);
        break;
    default:
        return EINVAL;
    }

    if (buf_len > 0 && buf_len < size) return E2BIG;

    if (addr)
        data_vir_copy_check(p_proc, p_proc->endpoint, buf, KERNEL, addr, size);

    return 0;
}
