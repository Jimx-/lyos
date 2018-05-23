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
#include <lyos/portio.h>

PUBLIC int sys_sportio(MESSAGE * m, struct proc * p_proc)
{
    int dir = m->PIO_REQUEST & PIO_DIR_MASK;
    size_t buf_len = m->PIO_BUFLEN;
    void * buf = m->PIO_BUF;
    int port = m->PIO_PORT;

    if (dir == PIO_IN) {
        port_read(port, buf, buf_len);
    } else {
        port_write(port, buf, buf_len);
    }

    return 0;
}
