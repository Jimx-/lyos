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
#include "fcntl.h"

PUBLIC int do_vfs_open(MESSAGE * p)
{
    int fd = -1;        /* return value */

    char pathname[MAX_PATH];
    /* get parameters from the message */
    int flags = p->FLAGS;   /* access mode */
    int name_len = p->NAME_LEN; /* length of filename */
    int src = p->source;    /* caller proc nr. */

    assert(name_len < MAX_PATH);
    phys_copy((void*)va2la(TASK_FS, pathname),
          (void*)va2la(src, p->PATHNAME),
          name_len);
    pathname[name_len] = 0;

    /* find a free slot in PROCESS::filp[] */
    int i;
    for (i = 0; i < NR_FILES; i++) {
        if (pcaller->filp[i] == 0) {
            fd = i;
            break;
        }
    }
    if ((fd < 0) || (fd >= NR_FILES))
        panic("filp[] is full (PID:%d)", proc2pid(pcaller));

    /* find a free slot in f_desc_table[] */
    for (i = 0; i < NR_FILE_DESC; i++)
        if (f_desc_table[i].fd_inode == 0)
            break;
    if (i >= NR_FILE_DESC)
        panic("f_desc_table[] is full (PID:%d)", proc2pid(pcaller));

    return fd;
}