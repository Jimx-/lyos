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

//#define PAGEFAULT_DEBUG

PUBLIC void do_handle_fault()
{
    if (mm_msg.FAULT_NR != 14) {
        printl("MM: unexpected fault type: %d", mm_msg.FAULT_NR);
        return;
    }

    vir_bytes pfla = mm_msg.FAULT_ADDR;
    struct mmproc * mmp = endpt_mmproc(mm_msg.FAULT_PROC);
    int err_code = mm_msg.FAULT_ERRCODE;

    int handled = 0;

    if (ARCH_PF_PROT(err_code)) {
#ifdef PAGEFAULT_DEBUG
        printl("MM: pagefault: %d protected address %x\n", mm_msg.FAULT_PROC, pfla);
#endif

        pfla = rounddown(pfla, ARCH_PG_SIZE);
        struct vir_region * vr;
        if (vr = region_lookup(mmp, pfla)) { 
            if (region_handle_pf(mmp, vr, pfla - (vir_bytes)vr->vir_addr, 1) == 0) handled = 1;
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
    } else {
        if (ARCH_PF_PROT(err_code)) {
            printl("MM: SIGSEGV %d protected address %x\n", mm_msg.FAULT_PROC, pfla);
        } else if (ARCH_PF_NOPAGE(err_code)) {
            printl("MM: SIGSEGV %d bad address %x\n", mm_msg.FAULT_PROC, pfla);
        }
        //send_sig(p, SIGSEGV); 
    }
}

PRIVATE int handle_memory(struct mmproc * mmp, vir_bytes start, vir_bytes len, int wrflag, endpoint_t caller)
{
    struct vir_region * vr;

    if (start % ARCH_PG_SIZE) {
        len += start % ARCH_PG_SIZE;
        start -= start % ARCH_PG_SIZE;
    }

    while (len > 0) {
        if (!(vr = region_lookup(mmp, start))) {
            return EFAULT;
        } else if (!(vr->flags & RF_WRITABLE) && wrflag) return EFAULT;

        vir_bytes offset = start - (vir_bytes)vr->vir_addr;
        vir_bytes sublen = len;

        if (offset + sublen > vr->length) {
            sublen = vr->length - offset;
        }

        int retval = region_handle_memory(mmp, vr, offset, sublen, wrflag);
        if (retval) return retval;

        len -= sublen;
        start += sublen;
    }

    return 0;
}

PUBLIC void do_mmrequest()
{
    endpoint_t target, caller;
    vir_bytes start, len;
    int flags;
    struct mmproc * mmp;
    int result;

    while (TRUE) {
        int type = vmctl_get_mmrequest(&target, &start, &len, 
                        &flags, &caller);

        switch (type) {
        case MMREQ_CHECK:
            mmp = endpt_mmproc(target);
            if (!mmp) return;
#ifdef PAGEFAULT_DEBUG
            printl("MM: mm request: %d address %x\n", target, start);
#endif
            result = handle_memory(mmp, start, len, flags, caller);
            break;
        default:
            return;
        }

        result = vmctl_reply_mmreq(caller, result);
        if (result) panic("reply mm request failed: %d", result);
    }
}
