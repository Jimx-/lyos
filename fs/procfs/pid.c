/*

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
#include <errno.h>
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
#include "global.h"

static void pid_status(int slot);
static void pid_environ(int slot);

struct procfs_file pid_files[] = {
    {"status", S_IFREG | S_IRUSR | S_IRGRP | S_IROTH, pid_status},
    {"environ", S_IFREG | S_IRUSR | S_IRGRP | S_IROTH, pid_environ},
    {"fd", S_IFDIR | S_IRUSR | S_IXUSR, NULL},
    {NULL, 0, NULL},
};

static void dump_state(int pst, char* buf)
{
    int i = 0;
    if (pst == 0) {
        buf[0] = 'R';
        return;
    }

#define ADD_FLAG(f, c) \
    if (pst & f) buf[i++] = c
    ADD_FLAG(PST_BOOTINHIBIT, 'B');
    ADD_FLAG(PST_SENDING, 's');
    ADD_FLAG(PST_RECEIVING, 'r');
    ADD_FLAG(PST_NO_PRIV, 'P');
    ADD_FLAG(PST_NO_QUANTUM, 'Q');
    ADD_FLAG(PST_PAGEFAULT, 'F');
    ADD_FLAG(PST_MMINHIBIT, 'M');
    ADD_FLAG(PST_STOPPED, 'S');
}

static void pid_status(int slot)
{
    int pi = slot - NR_TASKS;
    buf_printf("Name:\t%s\n", proc[slot].name);

    char state[10];
    memset(state, 0, sizeof(state));
    dump_state(proc[slot].state, state);
    buf_printf("State:\t%s\n", state);

    if (pi >= 0) {
        buf_printf("Tgid:\t%d\n", pmproc[pi].tgid);
        buf_printf("Pid:\t%d\n", pmproc[pi].pid);
        buf_printf("Uid:\t%d\t%d\t%d\n", pmproc[pi].realuid, pmproc[pi].effuid,
                   pmproc[pi].suid);
        buf_printf("Gid:\t%d\t%d\t%d\n", pmproc[pi].realgid, pmproc[pi].effgid,
                   pmproc[pi].sgid);
        buf_printf("SigPnd:\t%016x\n", pmproc[pi].sig_pending);
    }
}

static void pid_environ(int slot)
{
    printl("environ %d, %x, %x\n", slot, pmproc[slot].frame_addr,
           pmproc[slot].frame_size);
}
