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

#include <lyos/type.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "lyos/config.h"
#include "stdio.h"
#include "unistd.h"
#include "stddef.h"
#include "errno.h"
#include "assert.h"
#include "lyos/const.h"
#include <lyos/sysutils.h>
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

    void* pfla = mm_msg.FAULT_ADDR;
    struct mmproc * mmp = endpt_mmproc(mm_msg.FAULT_PROC);
    int err_code = mm_msg.FAULT_ERRCODE;
    int wrflag = ARCH_PF_WRITE(err_code);
    int handled = 0;

    pfla = (void*) rounddown(((uintptr_t) pfla), ARCH_PG_SIZE);

    struct vir_region * vr;
    vr = region_lookup(mmp, pfla);

    if (!vr) {
        if (ARCH_PF_PROT(err_code)) {
            printl("MM: SIGSEGV %d protected address %x\n", mm_msg.FAULT_PROC, pfla);
        } else if (ARCH_PF_NOPAGE(err_code)) {
            printl("MM: SIGSEGV %d bad address %x\n", mm_msg.FAULT_PROC, pfla);
        }

        if (kernel_kill(mm_msg.FAULT_PROC, SIGSEGV) != 0) panic("pagefault: unable to kill proc");
        if (vmctl(VMCTL_PAGEFAULT_CLEAR, mm_msg.FAULT_PROC) != 0) panic("pagefault: vmctl failed");

        return;
    }

    if (!(vr->flags & RF_WRITABLE) && wrflag) {
        printl("MM: SIGSEGV %d ro address %x\n", mm_msg.FAULT_PROC, pfla);

        if (kernel_kill(mm_msg.FAULT_PROC, SIGSEGV) != 0) panic("pagefault: unable to kill proc");
        if (vmctl(VMCTL_PAGEFAULT_CLEAR, mm_msg.FAULT_PROC) != 0) panic("pagefault: vmctl failed");

        return;
    }

    int retval;
    if ((retval = region_handle_pf(mmp, vr, pfla - (void*)vr->vir_addr, wrflag)) == 0) handled = 1;
    else if (retval == SUSPEND) return;


#ifdef PAGEFAULT_DEBUG
    if (ARCH_PF_PROT(err_code)) {
        printl("MM: pagefault: %d protected address %x\n", mm_msg.FAULT_PROC, pfla);
    } else if (ARCH_PF_NOPAGE(err_code)) {
        printl("MM: pagefault: %d bad address %x\n", mm_msg.FAULT_PROC, pfla);
    }
#endif

    vmctl(VMCTL_CLEAR_MEMCACHE, SELF);

    /* resume */
    if (handled) {
        if (vmctl(VMCTL_PAGEFAULT_CLEAR, mm_msg.FAULT_PROC) != 0) panic("pagefault: vmctl failed");
    } else {
        if (kernel_kill(mm_msg.FAULT_PROC, SIGSEGV) != 0) panic("pagefault: unable to kill proc");
        if (vmctl(VMCTL_PAGEFAULT_CLEAR, mm_msg.FAULT_PROC) != 0) panic("pagefault: vmctl failed");
    }
}

PRIVATE int handle_memory(struct mmproc * mmp, void* start, size_t len, int wrflag, endpoint_t caller)
{
    struct vir_region * vr;

    if ((uintptr_t) start % ARCH_PG_SIZE) {
        len += (uintptr_t) start % ARCH_PG_SIZE;
        start -= (uintptr_t) start % ARCH_PG_SIZE;
    }

    while (len > 0) {
        if (!(vr = region_lookup(mmp, start))) {
            return EFAULT;
        } //else if (!(vr->flags & RF_WRITABLE) && wrflag) return EFAULT;

        off_t offset = start - (void*)vr->vir_addr;
        size_t sublen = len;

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
    void* start;
    size_t len;
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
