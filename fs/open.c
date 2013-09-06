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

#define DEBUG
#ifdef DEBUG
#define DEB(x) printl("VFS: "); x
#else
#define DEB(x)
#endif

PRIVATE char mode_map[] = {R_BIT, W_BIT, R_BIT | W_BIT, 0};

PRIVATE struct inode * new_node(char * pathname, int flags, mode_t mode);

/**
 * <Ring 1> Perform the open syscall.
 * @param  p Ptr to the message.
 * @return   fd number if success, otherwise a negative error code.
 */
PUBLIC int do_open(MESSAGE * p)
{
    int fd = -1;        /* return value */

    /* get parameters from the message */
    int flags = p->FLAGS;   /* open flags */
    int name_len = p->NAME_LEN; /* length of filename */
    int src = p->source;    /* caller proc nr. */
    mode_t mode = p->MODE;  /* access mode */

    mode_t bits = mode_map[flags & O_ACCMODE];
    if (!bits) return -EINVAL;

    int exist = 1;

    char * pathname = (char *)alloc_mem(name_len + 1);
    if (!pathname) {
        err_code = -ENOMEM;
        return -1;
    }

    phys_copy((void*)va2la(TASK_FS, pathname),
          (void*)va2la(src, p->PATHNAME),
          name_len);
    pathname[name_len] = '\0';

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

    struct inode * pin = NULL;

    if (flags & O_CREAT) {
        mode = I_REGULAR;
        /* TODO: create the file */
    } else {
        pin = resolve_path(pathname, pcaller);
        printl("open file with inode_nr = %d\n", pin->i_num); 
        if (pin == NULL) return -err_code;
    }

    struct file_desc * filp = &f_desc_table[i];
    pcaller->filp[fd] = filp;
    filp->fd_cnt = 1;
    filp->fd_pos = 0;
    filp->fd_inode = pin;
    filp->fd_mode = flags;

    MESSAGE driver_msg;
    int retval = 0;
    if (exist) {
        if ((retval = forbidden(pcaller, pin, bits)) == 0) {
            switch (pin->i_mode & I_TYPE) {
                case I_REGULAR:
                    if (flags & O_TRUNC) {
                    }
                    break;
                case I_DIRECTORY:   /* directory may not be written */
                    retval = (bits & W_BIT) ? EISDIR : 0;
                    break;
                case I_CHAR_SPECIAL:    /* open char device */
                    driver_msg.type = DEV_OPEN;
                    int dev = pin->i_specdev;
                    driver_msg.DEVICE = MINOR(dev);
                    send_recv(BOTH, dd_map[MAJOR(dev)].driver_nr, &driver_msg);
                    break;
                case I_BLOCK_SPECIAL:
                    /* TODO: handle block special file */
                    break;
                default:
                break;
            }
        }
    }

    if (retval != 0) {
        pcaller->filp[fd] = NULL;
        filp->fd_cnt = 0;
        filp->fd_mode = 0;
        filp->fd_inode = 0;
        put_inode(pin);
        return -retval;
    }

    return fd;
}

/**
 * <Ring 1> Perform the close syscall.
 * @param  p Ptr to the message.
 * @return   Zero if success.
 */
PUBLIC int do_close(MESSAGE * p)
{
    int fd = p->FD;
    DEB(printl("Close file (filp[%d] of proc #%d, inode number = %d\n", fd, proc2pid(pcaller), pcaller->filp[fd]->fd_inode->i_num));
    put_inode(pcaller->filp[fd]->fd_inode);
    pcaller->filp[fd]->fd_inode = NULL;
    pcaller->filp[fd] = NULL;

    return 0;
}

/**
 * <Ring 1> Create a new inode.
 * @param  pathname Pathname.
 * @param  flags    Open flags.
 * @param  mode     Mode.
 * @return          Ptr to the inode.
 */
PRIVATE struct inode * new_node(char * pathname, int flags, mode_t mode)
{
    return NULL;
}
