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
#include "lyos/config.h"
#include "stdio.h"
#include "stddef.h"
#include "unistd.h"
#include "assert.h"
#include "errno.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "path.h"
#include "global.h"
#include "proto.h"
#include <sys/stat.h>

/**
 * <Ring 1> Perform the access syscall.
 * @param  p Ptr to the message.
 * @return   Zero if success. Otherwise -1.
 */
PUBLIC int do_access(MESSAGE * p)
{
    int namelen = p->NAME_LEN + 1;
    char pathname[MAX_PATH];
    if (namelen > MAX_PATH) return ENAMETOOLONG;

    data_copy(SELF, pathname, p->source, p->PATHNAME, namelen);
    //phys_copy(va2pa(getpid(), pathname), va2pa(p->source, p->PATHNAME), namelen);
    pathname[namelen] = 0;

    struct inode * pin = resolve_path(pathname, pcaller);
    if (!pin) return ENOENT;

    int retval = forbidden(pcaller, pin, p->MODE);

    put_inode(pin);

    return (retval == 0) ? 0 : -1;
}

PUBLIC int forbidden(struct proc * fp, struct inode * pin, int access)
{
    mode_t bits, perm_bits;
    int shift;

    bits = pin->i_mode;

    if (fp->uid == SU_UID) {
        if ((bits & I_TYPE) == I_DIRECTORY || bits & ((X_BIT << 6) | (X_BIT << 3) | X_BIT))
            perm_bits = R_BIT | W_BIT | X_BIT;
        else
            perm_bits = R_BIT | W_BIT;
    } else {
        if (fp->uid == pin->i_uid) shift = 6;    /* owner */
        else if (fp->gid == pin->i_gid) shift = 3;   /* group */
        else shift = 0;                 /* other */
        perm_bits = (bits >> shift) & (R_BIT | W_BIT | X_BIT); 
    }

    if ((perm_bits | access) != perm_bits) return EACCES;

    /* check for readonly filesystem */
    if (access & W_BIT) {
        if (pin->i_vmnt->m_flags & VMNT_READONLY) return EROFS;
    }

    return 0;
}

/**
 * <Ring 1> Perform the UMASK syscall.
 * @param  p Ptr to the message.
 * @return   Complement of the old mask.
 */
PUBLIC mode_t do_umask(MESSAGE * p)
{
    mode_t old = ~(pcaller->umask);
    pcaller->umask = ~((mode_t)p->MODE & RWX_MODES);
    return old;
}
