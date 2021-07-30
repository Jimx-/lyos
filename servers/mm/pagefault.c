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

#include <lyos/types.h>
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
#include <lyos/fs.h>
#include <signal.h>
#include "region.h"
#include "proto.h"
#include "const.h"
#include <lyos/vm.h>
#include "global.h"

#define PAGEFAULT_DEBUG 0

#if PAGEFAULT_DEBUG
#define DBG(x) x
#else
#define DBG(x)
#endif

struct pf_state {
    endpoint_t ep;
    vir_bytes pfla;
    int err;
    vir_bytes pc;
};

struct hm_state {
    endpoint_t caller;
    endpoint_t requestor;
    struct mmproc* mmp;
    vir_bytes start, len;
    int write;
    int vfs_avail;
};

static void handle_page_fault_callback(struct mmproc* mmp, MESSAGE* msg,
                                       void* arg);
static int handle_memory_state(struct hm_state* state, int retry);
static void handle_memory_callback(struct mmproc* mmp, MESSAGE* msg, void* arg);
static void handle_memory_reply(struct hm_state* state, int result);

void handle_page_fault(endpoint_t ep, vir_bytes pfla, int err, vir_bytes pc,
                       int retry)
{
    struct mmproc* mmp = endpt_mmproc(ep);
    int wrflag = !!(ARCH_PF_WRITE(err));
    int retval;
    size_t offset;
    struct vir_region* vr;

    vr = region_lookup(mmp, pfla);

    if (!vr) {
        if (ARCH_PF_PROT(err)) {
            printl("MM: SIGSEGV %d protected address %x, pc=%x\n", ep, pfla,
                   pc);
        } else if (ARCH_PF_NOPAGE(err)) {
            printl("MM: SIGSEGV %d bad address %x, pc=%x\n", ep, pfla, pc);
        }

        if (kernel_kill(ep, SIGSEGV) != 0)
            panic("pagefault: unable to kill proc");
        if (vmctl(VMCTL_PAGEFAULT_CLEAR, ep) != 0)
            panic("pagefault: vmctl failed");

        return;
    }

    if (!(vr->flags & RF_WRITE) && wrflag) {
        printl("MM: SIGSEGV %d ro address %x, pc=%x\n", ep, pfla, pc);

        if (kernel_kill(ep, SIGSEGV) != 0)
            panic("pagefault: unable to kill proc");
        if (vmctl(VMCTL_PAGEFAULT_CLEAR, ep) != 0)
            panic("pagefault: vmctl failed");

        return;
    }

    assert(pfla >= vr->vir_addr);
    assert(pfla < vr->vir_addr + vr->length);

    offset = pfla - vr->vir_addr;

    if (retry) {
        retval = region_handle_pf(mmp, vr, offset, wrflag,
                                  handle_page_fault_callback, NULL, 0);
        assert(retval != SUSPEND);
    } else {
        struct pf_state state;
        state.ep = ep;
        state.pfla = pfla;
        state.err = err;
        state.pc = pc;
        retval =
            region_handle_pf(mmp, vr, offset, wrflag,
                             handle_page_fault_callback, &state, sizeof(state));
    }

    DBG(
        if (ARCH_PF_PROT(err)) {
            printl("MM: pagefault: %d protected address %x\n", ep, pfla);
        } else if (ARCH_PF_NOPAGE(err)) {
            printl("MM: pagefault: %d bad address %x\n", ep, pfla);
        });

    if (retval == SUSPEND) return;

    if (retval != OK) {
        if (kernel_kill(ep, SIGSEGV) != 0)
            panic("pagefault: unable to kill proc");
        if (vmctl(VMCTL_PAGEFAULT_CLEAR, ep) != 0)
            panic("pagefault: vmctl failed");
    }

    vmctl(VMCTL_CLEAR_MEMCACHE, SELF);

    if (vmctl(VMCTL_PAGEFAULT_CLEAR, ep) != 0) panic("pagefault: vmctl failed");
}

static void handle_page_fault_callback(struct mmproc* mmp, MESSAGE* msg,
                                       void* arg)
{
    struct pf_state* state = arg;
    struct mmproc* fault_proc = endpt_mmproc(state->ep);
    if (!fault_proc) return;
    handle_page_fault(state->ep, state->pfla, state->err, state->pc, TRUE);
}

int handle_memory(struct mmproc* mmp, vir_bytes start, vir_bytes len, int write,
                  endpoint_t caller, endpoint_t requestor, int vfs_avail)
{
    struct hm_state state;
    vir_bytes offset;
    int retval;

    offset = start % ARCH_PG_SIZE;
    if (offset) {
        start -= offset;
        len += offset;
    }
    len = roundup(len, ARCH_PG_SIZE);

    state.mmp = mmp;
    state.start = start;
    state.len = len;
    state.write = write;
    state.requestor = requestor;
    state.caller = caller;
    state.vfs_avail = vfs_avail;

    retval = handle_memory_state(&state, FALSE);

    if (retval == SUSPEND) {
        assert(caller != NO_TASK);
        assert(vfs_avail);
    } else {
        handle_memory_reply(&state, retval);
    }

    return retval;
}

static int handle_memory_state(struct hm_state* state, int retry)
{
    struct vir_region* vr;
    vir_bytes offset, len, sublen;
    int retval;

    assert(state);
    assert(!(state->start % ARCH_PG_SIZE));
    assert(!(state->len % ARCH_PG_SIZE));

    while (state->len) {
        if (!(vr = region_lookup(state->mmp, state->start)))
            return EFAULT;
        else if (state->write && !(vr->flags & RF_WRITE))
            return EFAULT;

        assert(vr);
        assert(vr->vir_addr <= state->start);
        offset = state->start - vr->vir_addr;
        len = state->len;
        if (offset + len > vr->length) len = vr->length - offset;

        while (len > 0) {
            sublen = ARCH_PG_SIZE;

            assert(sublen <= len);
            assert(offset + sublen <= vr->length);

            if ((vr->rops == &file_map_ops && (!state->vfs_avail || retry)) ||
                state->caller == NO_TASK) {
                retval = region_handle_memory(state->mmp, vr, offset, sublen,
                                              state->write, NULL, NULL, 0);
                assert(retval != SUSPEND);
            } else
                retval = region_handle_memory(
                    state->mmp, vr, offset, sublen, state->write,
                    handle_memory_callback, state, sizeof(*state));

            if (retval != OK) return retval;

            state->len -= sublen;
            state->start += sublen;

            offset += sublen;
            len -= sublen;
            retry = FALSE;
        }
    }

    return 0;
}

static void handle_memory_reply(struct hm_state* state, int result)
{
    int retval;
    assert(state);

    if (state->caller == KERNEL) {
        if ((retval = vmctl_reply_mmreq(state->requestor, result)) != OK)
            panic("mm: handle_memory_reply vmctl failed");
    } else
        panic("mm: handle_memory_reply unknown caller");
}

static void handle_memory_callback(struct mmproc* mmp, MESSAGE* msg, void* arg)
{
    struct hm_state* state = arg;
    int retval;
    assert(state);
    assert(state->caller != NO_TASK);

    if (msg->MMRRESULT != OK) {
        handle_memory_reply(state, msg->MMRRESULT);
    }

    retval = handle_memory_state(state, TRUE);

    if (retval == SUSPEND) return;

    handle_memory_reply(state, msg->MMRRESULT);
}

void do_handle_fault()
{
    handle_page_fault(mm_msg.FAULT_PROC, (vir_bytes)mm_msg.FAULT_ADDR,
                      mm_msg.FAULT_ERRCODE, (vir_bytes)mm_msg.FAULT_PC, FALSE);
}

void do_mmrequest()
{
    endpoint_t target, requestor;
    void* start;
    size_t len;
    int write, vfs_avail;
    struct mmproc* mmp;

    while (TRUE) {
        int type =
            vmctl_get_mmrequest(&target, &start, &len, &write, &requestor);

        switch (type) {
        case MMREQ_CHECK:
            mmp = endpt_mmproc(target);
            if (!mmp) return;

            vfs_avail = mmp->endpoint != TASK_FS;
            DBG(printl("MM: mm request: %d address %x\n", target, start));

            handle_memory(mmp, (vir_bytes)start, len, write, KERNEL, requestor,
                          vfs_avail);

            break;
        default:
            return;
        }
    }
}
