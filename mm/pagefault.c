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
#include <signal.h>
#include "region.h"
#include "proto.h"
#include "const.h"

#define PAGEFAULT_DEBUG

PUBLIC void do_handle_fault()
{
    if (mm_msg.FAULT_NR != 14) {
        printl("MM: unexpected fault type: %d", mm_msg.FAULT_NR);
        return;
    }

    int pfla = mm_msg.FAULT_ADDR;
    struct proc * p = proc_table + mm_msg.FAULT_PROC;
    int err_code = mm_msg.FAULT_ERRCODE;

    if (ARCH_PF_PROT(err_code)) {
        printl("MM: pagefault: %d protected address %x\n", proc2pid(p), pfla);
    } else if (ARCH_PF_NOPAGE(err_code)) {
        printl("MM: pagefault: %d not present address %x\n", proc2pid(p), pfla);
    }

    int extend = 0;
    struct vir_region * vr, * stack = NULL;
    int handled = 0;
    list_for_each_entry(vr, &(p->mem_regions), list) {
        if ((int)(vr->vir_addr) + vr->length == VM_STACK_TOP) {
            stack = vr;
            if (extend) {
                region_extend_stack(vr, STACK_GUARD_LEN);
                region_map_phys(p, vr);
            }
        }
        if (vr->flags & RF_GUARD) {
            /* pfla is in stack guard area: extend stack */
            if (pfla > (int)(vr->vir_addr) && pfla < (int)(vr->vir_addr) + vr->length) {
#ifdef PAGEFAULT_DEBUG
                printl("MM: page fault caused by not enough stack space, extending\n");
#endif
                if (!stack) { 
                    extend = 1; 
                } else {
                    region_extend_stack(stack, STACK_GUARD_LEN);
                    region_map_phys(p, stack);
                }
                vr->vir_addr = (void*)((int)vr->vir_addr - STACK_GUARD_LEN);
                handled = 1;
            }
        }
    }

    /* resume */
    if (handled) 
        p->state = 0;
    else
        send_sig(SIGSEGV, p); 
}
