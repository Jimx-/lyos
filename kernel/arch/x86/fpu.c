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
#include <errno.h>
#include "lyos/const.h"
#include "string.h"
#include <kernel/proc.h>
#include <kernel/proto.h>
#include <asm/proto.h>
#ifdef CONFIG_SMP
#include <asm/smp.h>
#endif
#include <lyos/cpufeature.h>

static DEFINE_CPULOCAL(int, fpu_present) = 0;

static int has_osfxsr = 0;
static int has_osxsave = 0;

/* CR0 Flags */
#define CR0_MP (1L << 1) /* Monitor co-processor */
#define CR0_TS (1L << 3) /* Task switched */
#define CR0_NE (1L << 5) /* Numeric error */

/* CR4 Flags */
#define CR4_OSFXSR   (1L << 9)
#define CR4_XMMEXCPT (1L << 10)
#define CR4_OSXSAVE  (1L << 18)

#define XFEATURE_FP  (1L << 0)
#define XFEATURE_SSE (1L << 1)

/**
 * <Ring 0> Initialize FPU.
 */
void fpu_init(void)
{
    unsigned short sw, cw;

    fninit();
    sw = fnstsw();
    fnstcw(&cw);

    if ((sw & 0xff) == 0 && (cw & 0x103f) == 0x3f) {
        get_cpulocal_var(fpu_present) = 1;

        write_cr0(read_cr0() | (CR0_MP | CR0_NE));

        if (_cpufeature(_CPUF_I386_FXSR)) {
            u32 cr4 = read_cr4();
            cr4 |= CR4_OSFXSR;

            if (_cpufeature(_CPUF_I386_SSE)) {
                cr4 |= CR4_XMMEXCPT;
            }

            write_cr4(cr4);
            has_osfxsr = 1;
        }

        if (_cpufeature(_CPUF_I386_XSAVE)) {
            u32 cr4 = read_cr4();
            cr4 |= CR4_OSXSAVE;
            write_cr4(cr4);

            xsetbv(0, XFEATURE_FP | XFEATURE_SSE);
            has_osxsave = 1;
        }
    } else {
        get_cpulocal_var(fpu_present) = 0;
    }
}

void enable_fpu_exception(void)
{
    u32 cr0;

    cr0 = read_cr0();
    if (!(cr0 & CR0_TS)) {
        cr0 |= CR0_TS;
        write_cr0(cr0);
    }
}

void disable_fpu_exception(void) { clts(); }

void save_local_fpu(struct proc* p, int retain)
{
    char* state = p->seg.fpu_state;

    if (has_osxsave) {
        xsave(state, 0xffffffff, 0xffffffff);
    } else if (has_osfxsr) {
        fxsave(state);
    } else {
        fnsave(state);
        if (retain) {
            frstor(state);
        }
    }
}

void save_fpu(struct proc* p)
{
    if (get_cpulocal_var(fpu_owner) == p) {
        disable_fpu_exception();
        save_local_fpu(p, TRUE);
    }
}

static void fpu_init_xstate(struct xregs_state* xsave)
{
    xsave->header.xcomp_bv =
        XCOMP_BV_COMPACTED_FORMAT | XFEATURE_FP | XFEATURE_SSE;
}

static void fpu_init_fxstate(struct fxregs_state* fx)
{
    fx->cwd = 0x37f;
    fx->mxcsr = MXCSR_DEFAULT;
}

static void fpu_init_state(union fpregs_state* state)
{
    memset(state, 0, sizeof(*state));

    if (has_osxsave) fpu_init_xstate(&state->xsave);
    if (has_osfxsr) fpu_init_fxstate(&state->fxsave);
}

int restore_fpu(struct proc* p)
{
    int retval;
    union fpregs_state* state = (union fpregs_state*)p->seg.fpu_state;

    if (!(p->flags & PF_FPU_INITIALIZED)) {
        fninit();
        fpu_init_state(state);
        p->flags |= PF_FPU_INITIALIZED;
    }

    if (has_osxsave) {
        retval = xrstor(&state->xsave, 0xffffffff, 0xffffffff);
    } else if (has_osfxsr) {
        retval = fxrstor(&state->fxsave);
    } else {
        retval = frstor(state);
    }

    if (retval) {
        return EINVAL;
    }

    return 0;
}
