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

#include <lyos/types.h>
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

static void root_cmdline();
static void root_version();
static void root_uptime();
void root_cpuinfo();
void root_meminfo();
static void root_stat(void);

struct procfs_file root_files[] = {
    {"cmdline", S_IFREG | S_IRUSR | S_IRGRP | S_IROTH, root_cmdline},
    {"version", S_IFREG | S_IRUSR | S_IRGRP | S_IROTH, root_version},
    {"uptime", S_IFREG | S_IRUSR | S_IRGRP | S_IROTH, root_uptime},
    {"cpuinfo", S_IFREG | S_IRUSR | S_IRGRP | S_IROTH, root_cpuinfo},
    {"meminfo", S_IFREG | S_IRUSR | S_IRGRP | S_IROTH, root_meminfo},
    {"stat", S_IFREG | S_IRUSR | S_IRGRP | S_IROTH, root_stat},
    {NULL, 0, NULL},
};

static void root_cmdline()
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

static void root_version()
{
    struct utsname utsname;
    uname(&utsname);

    buf_printf("%s version %s %s\n", utsname.sysname, utsname.release,
               utsname.version);
}

static void root_uptime()
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
