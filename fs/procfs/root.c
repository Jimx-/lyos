/*
    (c)Copyright 2011 Jimx

    This file is part of Lyos.

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
#include "stdio.h"
#include "unistd.h"
#include "lyos/config.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <lyos/param.h>
#include <lyos/sysutils.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <libmemfs/libmemfs.h>
#include "type.h"
#include "proto.h"

PRIVATE void root_cmdline();
PRIVATE void root_version();
PRIVATE void root_uptime();
PUBLIC void root_cpuinfo();
PUBLIC void root_meminfo();
static void root_stat(void);

PUBLIC struct procfs_file root_files[] = {
    {"cmdline", I_REGULAR | S_IRUSR | S_IRGRP | S_IROTH, root_cmdline},
    {"version", I_REGULAR | S_IRUSR | S_IRGRP | S_IROTH, root_version},
    {"uptime", I_REGULAR | S_IRUSR | S_IRGRP | S_IROTH, root_uptime},
    {"cpuinfo", I_REGULAR | S_IRUSR | S_IRGRP | S_IROTH, root_cpuinfo},
    {"meminfo", I_REGULAR | S_IRUSR | S_IRGRP | S_IROTH, root_meminfo},
    {"stat", I_REGULAR | S_IRUSR | S_IRGRP | S_IROTH, root_stat},
    {NULL, 0, NULL},
};

PRIVATE void root_cmdline()
{
    static char kernel_cmdline[KINFO_CMDLINE_LEN];
    static int kernel_cmdline_init = 0;

    if (!kernel_cmdline_init) {
        get_kernel_cmdline(kernel_cmdline, KINFO_CMDLINE_LEN);

        int i;
        for (i = 0; i < KINFO_CMDLINE_LEN; i++) {
            if (kernel_cmdline[i] == '\0' && kernel_cmdline[i + 1] != '\0')
                kernel_cmdline[i] = ' ';
            if (kernel_cmdline[i] == '\0' && kernel_cmdline[i + 1] == '\0')
                break;
        }

        kernel_cmdline_init = 1;
    }

    buf_printf("%s\n", kernel_cmdline);
}

PRIVATE void root_version()
{
    struct utsname utsname;
    uname(&utsname);

    buf_printf("%s version %s %s\n", utsname.sysname, utsname.release,
               utsname.version);
}

PRIVATE void root_uptime()
{
    clock_t ticks, idle_ticks;
    int hz = get_system_hz();

    get_ticks(&ticks, &idle_ticks);

    buf_printf("%ld.%0.2ld ", ticks / hz, ticks % hz);
    buf_printf("%ld.%0.2ld\n", idle_ticks / hz, idle_ticks % hz);
}

static void root_stat(void)
{
    struct machine machine;
    u64 ticks[CPU_STATES];

    if (get_machine(&machine)) {
        printl("procfs: can't get machine info\n");
        return;
    }

    int i, j;
    for (i = 0; i < machine.cpu_count; i++) {
        if (get_cputicks(i, ticks) != 0) {
            return;
        }

        buf_printf("cpu%d", i);

        for (j = 0; j < CPU_STATES; j++) {
            buf_printf(" %llu", ticks[j]);
        }

        buf_printf("\n");
    }
}
