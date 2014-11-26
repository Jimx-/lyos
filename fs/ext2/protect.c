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
#include "lyos/const.h"
#include "errno.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "lyos/list.h"
#include "ext2_fs.h"
#include "global.h"

PUBLIC int ext2_forbidden(ext2_inode_t * pin, int access)
{
    mode_t bits, perm_bits;
    int shift;

    bits = pin->i_mode;
    if (ext2_caller_uid == SU_UID) {
        if ((bits & I_TYPE) == I_DIRECTORY || bits & ((X_BIT << 6) | (X_BIT << 3) | X_BIT))
            perm_bits = R_BIT | W_BIT | X_BIT;
        else
            perm_bits = R_BIT | W_BIT;
    } else {
        if (ext2_caller_uid == pin->i_uid) shift = 6;    /* owner */
        else if (ext2_caller_gid == pin->i_gid) shift = 3;   /* group */
        else shift = 0;                 /* other */
        perm_bits = (bits >> shift) & (R_BIT | W_BIT | X_BIT); 
    }

    if ((perm_bits | access) != perm_bits) return EACCES;

    /* check for readonly filesystem */
    if (access & W_BIT) {
        if (pin->i_sb->sb_readonly) return EROFS;
    }

    return 0;
}
