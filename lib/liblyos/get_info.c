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

#include <lyos/type.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "lyos/const.h"
#include "stdio.h"
#include "stdarg.h"
#include "unistd.h"
#include "assert.h"
#include "lyos/vm.h"
#include <lyos/config.h>
#include <lyos/param.h>
#include <lyos/proc.h>
#include <lyos/sysutils.h>
#include <asm/type.h>

PUBLIC int getinfo(int request, void* buf)
{
    MESSAGE m;
    m.REQUEST = request;
    m.BUF = buf;
    m.BUF_LEN = 0;

    return syscall_entry(NR_GETINFO, &m);
}

PUBLIC int get_sysinfo(struct sysinfo** sysinfo)
{
    return getinfo(GETINFO_SYSINFO, sysinfo);
}

PUBLIC int get_kinfo(kinfo_t* kinfo) { return getinfo(GETINFO_KINFO, kinfo); }

PUBLIC int get_bootprocs(struct boot_proc* bp)
{
    return getinfo(GETINFO_BOOTPROCS, bp);
}

PUBLIC int get_system_hz()
{
    MESSAGE m;
    m.REQUEST = GETINFO_HZ;

    syscall_entry(NR_GETINFO, &m);

    return m.RETVAL;
}

PUBLIC int get_kernel_cmdline(char* buf, int buflen)
{
    MESSAGE m;
    m.REQUEST = GETINFO_CMDLINE;
    m.BUF = buf;
    m.BUF_LEN = buflen;

    return syscall_entry(NR_GETINFO, &m);
}

PUBLIC int get_machine(struct machine* machine)
{
    return getinfo(GETINFO_MACHINE, machine);
}

PUBLIC int get_cpuinfo(struct cpu_info* cpuinfo)
{
    return getinfo(GETINFO_CPUINFO, cpuinfo);
}

PUBLIC int get_proctab(struct proc* proc)
{
    return getinfo(GETINFO_PROCTAB, proc);
}

int get_cputicks(unsigned int cpu, u64* ticks)
{
    MESSAGE m;
    m.REQUEST = GETINFO_CPUTICKS;
    m.u.m3.m3i1 = cpu;
    m.BUF = ticks;
    m.BUF_LEN = 0;

    return syscall_entry(NR_GETINFO, &m);
}
