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
#include "lyos/config.h"
#include "lyos/const.h"
#include <lyos/endpoint.h>
#include <errno.h>
#include "global.h"
#include "const.h"

/**
 * @brief Find an unused pid.
 * @return The free pid found.
 */
pid_t find_free_pid()
{
    pid_t pid = INIT_PID + 1;
    int used = 0;

    do {
        struct pmproc* pmp;
        used = 0;
        pid = pid < NR_PIDS ? pid + 1 : INIT_PID + 1;
        for (pmp = &pmproc_table[0]; pmp < &pmproc_table[NR_PROCS]; pmp++) {
            if (pmp->pid == pid || pmp->procgrp == pid) {
                used = 1;
                break;
            }
        }
    } while (used);

    return pid;
}

int pm_verify_endpt(endpoint_t ep, int* proc_nr)
{
    *proc_nr = ENDPOINT_P(ep);
    return 0;
}

struct pmproc* pm_endpt_proc(endpoint_t ep)
{
    int proc_nr;
    if (pm_verify_endpt(ep, &proc_nr) == 0) return &pmproc_table[proc_nr];
    return NULL;
}

struct pmproc* pm_pid_proc(pid_t pid)
{
    int i;
    struct pmproc* pmp = pmproc_table;
    for (i = 0; i < NR_PROCS; i++, pmp++) {
        if (pmp->pid == pid) return pmp;
    }

    return NULL;
}
