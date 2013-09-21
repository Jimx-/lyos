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

#include "lyos/type.h"
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "assert.h"
#include "stddef.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/tty.h"
#include "lyos/console.h"
#include "lyos/global.h"
#include "lyos/keyboard.h"
#include "lyos/proto.h"
#include "errno.h"
#include "path.h"
#include "proto.h"
#include "fcntl.h"

PUBLIC int do_dup(MESSAGE * p)
{
    int fd = p->FD;
    int newfd = p->NEWFD;

    struct file_desc * filp = pcaller->filp[fd];
    
    if (newfd == -1) {
        /* find a free slot in PROCESS::filp[] */
        int i;
        for (i = 0; i < NR_FILES; i++) {
            if (pcaller->filp[i] == 0) {
                newfd = i;
                break;
            }
        }
        if ((newfd < 0) || (newfd >= NR_FILES))
            panic("filp[] is full (PID:%d)", proc2pid(pcaller));
    }

    if (pcaller->filp[newfd] != 0) {
        /* close the file */
        p->FD = newfd;
        do_close(p);
    }

    filp->fd_cnt++;
    pcaller->filp[newfd] = filp;

    return newfd;
}
