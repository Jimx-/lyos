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
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <signal.h>
#include "region.h"
#include "proto.h"
#include "const.h"
#include <lyos/vm.h>
#include "global.h"

#define PAGEFAULT_DEBUG

PUBLIC void do_handle_fault()
{
    if (mm_msg.FAULT_NR != 14) {
        printl("MM: unexpected fault type: %d", mm_msg.FAULT_NR);
        return;
    }

    int pfla = mm_msg.FAULT_ADDR;
    struct mmproc * mmp = mmproc_table + mm_msg.FAULT_PROC;
    int err_code = mm_msg.FAULT_ERRCODE;

    int handled = 0;

    if (ARCH_PF_PROT(err_code)) {
#ifdef PAGEFAULT_DEBUG
        printl("MM: pagefault: %d protected address %x\n", mm_msg.FAULT_PROC, pfla);
#endif

        struct vir_region * vr;
        int found = 0;
        list_for_each_entry(vr, &(mmp->mem_regions), list) {
            if (pfla >= (int)(vr->vir_addr) && pfla < (int)(vr->vir_addr) + vr->length) {
                found = 1;
                break;
            }
        }

        if (found) {
            /*struct phys_region * pregion = &(vr->phys_block);

            int fault_frame = (pfla - vr->vir_addr) / PG_SIZE;*/
        }
    } else if (ARCH_PF_NOPAGE(err_code)) {
#ifdef PAGEFAULT_DEBUG
        printl("MM: pagefault: %d bad address %x\n", mm_msg.FAULT_PROC, pfla);
#endif

        int extend = 0;
        struct vir_region * vr;
        int gd_base;
        list_for_each_entry(vr, &(mmp->mem_regions), list) {
            if (vr->flags & RF_GUARD) {
                /* pfla is in stack guard area: extend stack */
                if (pfla >= (int)(vr->vir_addr) && pfla < (int)(vr->vir_addr) + vr->length) {
#ifdef PAGEFAULT_DEBUG
                    printl("MM: page fault caused by not enough stack space, extending\n");
#endif
                    gd_base = (int)(vr->vir_addr) + vr->length;
                    vr->vir_addr = (void*)((int)vr->vir_addr - GROWSDOWN_GUARD_LEN);
                    handled = 1;
                    extend = 1;
                }
            }
        }

        if (extend) {
            list_for_each_entry(vr, &(mmp->mem_regions), list) {
                if ((vr->flags & RF_GROWSDOWN) && (gd_base == (int)vr->vir_addr)) {
                    if (region_extend_stack(vr, GROWSDOWN_GUARD_LEN) != 0) handled = 0;
                    region_map_phys(mmp, vr);
                }
            }
        }
    }

    /* resume */
    if (handled) {
        vmctl(VMCTL_PAGEFAULT_CLEAR, mm_msg.FAULT_PROC);
    }
    else {
        if (ARCH_PF_PROT(err_code)) {
            printl("MM: SIGSEGV %d protected address %x\n", mm_msg.FAULT_PROC, pfla);
        } else if (ARCH_PF_NOPAGE(err_code)) {
            printl("MM: SIGSEGV %d bad address %x\n", mm_msg.FAULT_PROC, pfla);
        }
        //send_sig(p, SIGSEGV); 
    }
}
