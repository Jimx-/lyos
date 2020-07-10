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

#include <lyos/types.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "lyos/const.h"
#include "stdio.h"
#include "stdarg.h"
#include "unistd.h"
#include "assert.h"
#include <lyos/sysutils.h>

#define ASYNC_SIZE 200
static async_message_t async_msg[ASYNC_SIZE];
static int first_slot = 0, next_slot = 0;
static int initialized = 0;

int asyncsend3(endpoint_t dest, MESSAGE* msg, int flags)
{
    int i;

    if (!initialized) {
        for (i = 0; i < ASYNC_SIZE; i++) {
            async_msg[i].flags = 0;
        }

        initialized = 1;
    }

    /* find first in-use slot */
    for (; first_slot < next_slot; first_slot++) {
        /* skip processed message */
        if (async_msg[first_slot].flags & ASMF_DONE) continue;

        if (async_msg[first_slot].flags != 0) break;
    }

    if (first_slot >= next_slot)
        first_slot = next_slot = 0; /* all messages processed */

    /* table full, try to clean up processed slots */
    if (next_slot >= ASYNC_SIZE) {
        send_async(NULL, 0);

        int dest_idx = 0, src_idx;
        for (src_idx = first_slot; src_idx < next_slot; src_idx++) {
            if (async_msg[src_idx].flags == 0) continue;
            if (async_msg[src_idx].flags & ASMF_DONE) continue;

            async_msg[dest_idx] = async_msg[src_idx];
            dest_idx++;
        }

        for (i = dest_idx; i < ASYNC_SIZE; i++)
            async_msg[i].flags = 0;

        first_slot = 0;
        next_slot = dest_idx;

        if (next_slot >= ASYNC_SIZE) panic("async table full\n");
    }

    async_msg[next_slot].flags = flags | ASMF_USED;
    async_msg[next_slot].msg = *msg;
    async_msg[next_slot].dest = dest;

    next_slot++;
    int len = next_slot - first_slot;

    /* tell kernel to process messages */
    return send_async(&async_msg[first_slot], len);
}

int async_sendrec(endpoint_t dest, MESSAGE* msg, int flags)
{
    int retval = asyncsend3(dest, msg, 0);
    if (retval) {
        printl("async sendrec failed\n");
        return retval;
    }

    retval = send_recv(RECEIVE, dest, msg);
    return retval;
}
