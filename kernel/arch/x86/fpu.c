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
#include "stdio.h"
#include "unistd.h"
#include "stddef.h"
#include <asm/protect.h>
#include <errno.h>
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "lyos/fs.h"
#include <asm/const.h>
#include <asm/proto.h>
#ifdef CONFIG_SMP
#include <asm/smp.h>
#endif
#include "lyos/cpulocals.h"
#include <lyos/cpufeature.h>

static DEFINE_CPULOCAL(int, fpu_present) = 0;

static int has_osfxsr = 0;

/* CR0 Flags */
#define CR0_MP (1L << 1) /* Monitor co-processor */
#define CR0_TS (1L << 3) /* Task switched */
#define CR0_NE (1L << 5) /* Numeric error */

/* CR4 Flags */
#define CR4_OSFXSR (1L << 9)
#define CR4_XMMEXCPT (1L << 10)

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
        } else {
            has_osfxsr = 0;
        }
    } else {
        get_cpulocal_var(fpu_present) = 0;
        has_osfxsr = 0;
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

    if (has_osfxsr) {
        fxsave(state);
    } else {
        fnsave(state);
        if (retain) {
            frstor(state);
        }
    }
}

int restore_fpu(struct proc* p)
{
    int retval;
    char* state = p->seg.fpu_state;

    if (!(p->flags & PF_FPU_INITIALIZED)) {
        fninit();
        p->flags |= PF_FPU_INITIALIZED;
    } else {
        if (has_osfxsr) {
            retval = fxrstor(state);
        } else {
            retval = frstor(state);
        }

        if (retval) {
            return EINVAL;
        }
    }

    return 0;
}
