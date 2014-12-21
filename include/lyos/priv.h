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

#ifndef _PRIV_H_
#define _PRIV_H_

/* Process privilege structure */
struct priv {
    int proc_nr;
    int id;
    int flags;

    bitchunk_t allowed_syscalls[BITCHUNKS(NR_SYS_CALLS)];

    priv_map_t notify_pending;
    irq_id_t int_pending;

    int kernlog_request;
};

#define FIRST_PRIV          priv_table[0]
#define LAST_PRIV           priv_table[NR_PRIV_PROCS]
#define FIRST_STATIC_PRIV   FIRST_PRIV
#define LAST_STATIC_PRIV    priv_table[NR_BOOT_PROCS]
#define FIRST_DYN_PRIV      LAST_STATIC_PRIV
#define LAST_DYN_PRIV       LAST_PRIV

#define priv_addr(n)        (&priv_table[n])
#define static_priv_id(n)   ((n) + NR_TASKS)

#define PRIV_ID_NULL        (-1)

#define PRF_PRIV_PROC       0x01
#define PRF_DYN_ID          0x02

#define TASK_FLAGS          (PRF_PRIV_PROC)
#define USER_FLAGS          (0)

#define NO_CALLS_ALLOWED    (-1)
#define ALL_CALLS_ALLOWED   (-2)

#define TASK_CALLS          ALL_CALLS_ALLOWED
#define KERNTASK_CALLS      NO_CALLS_ALLOWED


#endif
