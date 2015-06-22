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

#ifndef _IPC_H_
#define _IPC_H_

#include <sys/types.h>

#define SUSPEND -1000

#define ASMF_USED   0x1
#define ASMF_DONE   0x2
typedef struct {
    endpoint_t dest;
    MESSAGE msg;
    int flags;
    int result;
} async_message_t;

int send_recv(int function, int src_dest, MESSAGE* msg);
int sendrec(int function, int src_dest, MESSAGE* p_msg);
int send_async(async_message_t* table, size_t len);

int asyncsend3(endpoint_t dest, MESSAGE* msg, int flags);
int async_sendrec(endpoint_t dest, MESSAGE* msg, int flags);

#endif
