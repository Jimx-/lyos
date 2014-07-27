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
#include "unistd.h"
#include "stddef.h"
#include "errno.h"
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
#include "region.h"
#include "proto.h"
#include "const.h"

//#define SBRK_DEBUG

/**
 * <Ring 1> Perform the SBRK syscall.
 * @return Program brk or -1 on error.
 */
PUBLIC int do_sbrk()
{
    int src = mm_msg.source;
    int count = mm_msg.CNT;
    struct proc * p = proc_table + src;
    if (count == 0) return p->brk;
    
    int retval = 1;
    struct vir_region * vr;
    list_for_each_entry(vr, &(p->mem_regions), list) {
        if (p->brk >= (int)(vr->vir_addr) && p->brk <= (int)(vr->vir_addr) + vr->length) {
            retval = 0;
            break;
        }
    }

    if (retval) {
        panic("MM: do_sbrk: unable to find data segment for proc #%d", src);
    }

    /* enough space */
    if (p->brk + count <= (int)(vr->vir_addr) + vr->length) {
        retval = p->brk;
        p->brk += count;
#ifdef SBRK_DEBUG
        printl("MM: sbrk: proc #%d brk is now 0x%x\n", src, retval);
#endif
        return retval;
    } else {
        int increment = p->brk + count - (int)(vr->vir_addr) - vr->length;
        /*if (increment == 0) increment = PG_SIZE;   */ /* brk is at the boundary, prealloc some space */
        retval = region_extend(vr, increment);
        if (retval) {
            errno = retval;
            return -1;
        }
        region_map_phys(p, vr);
        retval = p->brk;
        p->brk += count;
#ifdef SBRK_DEBUG
        printl("MM: sbrk: proc #%d brk is now 0x%x(extended)\n", src, retval);
#endif
        return retval;
    }
    return -1;
}
