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

PRIVATE int change_directory(struct inode ** ppin, char * pathname, int len);
PRIVATE int change_node(struct inode ** ppin, struct inode * pin);

/**
 * <Ring 1> Perform the DUP and DUP2 syscalls.
 */
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

/**
 * <Ring 1> Perform the CHDIR syscall.
 */
PUBLIC int do_chdir(MESSAGE * p)
{
    return change_directory(&(pcaller->pwd), p->PATHNAME, p->NAME_LEN);
}

/**
 * <Ring 1> Perform the FCHDIR syscall.
 */
PUBLIC int do_fchdir(MESSAGE * p)
{
    struct file_desc * filp = pcaller->filp[p->FD];

    if (!filp) return EINVAL;

    return change_node(&(pcaller->pwd), filp->fd_inode);
}

/**
 * <Ring 1> Change the directory.  
 * @param  ppin     Directory to be change.
 * @param  string   Pathname.
 * @param  len      Length of pathname.
 * @return          Zero on success.
 */
PRIVATE int change_directory(struct inode ** ppin, char * string, int len)
{
    char pathname[MAX_PATH];
    if (len > MAX_PATH) return ENAMETOOLONG;

    /* fetch the name */
    phys_copy(va2pa(getpid(), pathname), va2pa(proc2pid(pcaller), string), len);
    pathname[len] = '\0';

    struct inode * pin = resolve_path(pathname, pcaller);
    if (!pin) return err_code;

    int retval = change_node(ppin, pin);

    put_inode(pin);
    return retval;
}

/**
 * <Ring 1> Change ppin into pin.
 */
PRIVATE int change_node(struct inode ** ppin, struct inode * pin)
{
    int retval = 0;

    /* nothing to do */
    if (*ppin == pin) return 0;

    /* must be a directory */
    if ((pin->i_mode & I_TYPE) != I_DIRECTORY) {
        retval = ENOTDIR;
    } else {
        /* must be searchable */
        retval = forbidden(pcaller, pin, X_BIT); 
    }

    if (retval == 0) {
        put_inode(*ppin);
        pin->i_cnt++;
        *ppin = pin;
    }

    return retval;
}