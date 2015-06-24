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
#include <sys/types.h>
#include <lyos/config.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/fs.h>
#include <lyos/proc.h>
#include <lyos/global.h>
#include <lyos/proto.h>
#include <lyos/ipc.h>
#include <lyos/param.h>
#include <lyos/sysutils.h>
#include <sched.h>
#include "path.h"
#include "global.h"
#include "proto.h"
#include <elf.h>
#include "global.h"
#include "thread.h"1

PRIVATE int worker_loop(void* arg)
{
    printl("Worker %d\n", arg);
    MESSAGE m;
        send_recv(RECEIVE, ANY, &m);
}

PUBLIC pid_t create_worker(int id)
{
    int retval;

    if (id >= NR_WORKER_THREADS || id < 0) return -EINVAL;

    struct worker_thread* thread = &workers[id];

    pid_t pid = clone(worker_loop, (char*)thread->stack + sizeof(thread->stack), CLONE_VM | CLONE_THREAD, (void*) id);
    if (pid < 0) return pid;

    retval = get_procep(pid, &thread->endpoint);
    if (retval) return -retval;

    /* prepare priv structure */
    struct priv* priv = &thread->priv;

    priv->flags = TASK_FLAGS | PRF_DYN_ID;

    /* syscall mask */
    int j;
    for (j = 0; j < BITCHUNKS(NR_SYS_CALLS); j++) {
        priv->syscall_mask[j] = ~0;
    }

    if ((retval = privctl(thread->endpoint, PRIVCTL_SET_PRIV, &thread->priv)) != 0) return -retval;
    if ((retval = privctl(thread->endpoint, PRIVCTL_ALLOW, NULL)) != 0) return -retval;

    return pid;
}
