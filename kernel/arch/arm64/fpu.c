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
#include <errno.h>
#include "lyos/const.h"
#include "string.h"
#include <kernel/proc.h>
#include <kernel/global.h>
#include <kernel/proto.h>
#include "lyos/fs.h"
#include <asm/const.h>
#include <asm/proto.h>
#ifdef CONFIG_SMP
#include <asm/smp.h>
#endif
#include <asm/cpulocals.h>
#include <asm/fpu.h>
#include <asm/sysreg.h>

/**
 * <Ring 0> Initialize FPU.
 */
void fpu_init() {}

void enable_fpu_exception(void)
{
    write_sysreg(CPACR_EL1_FPEN_EL1EN, cpacr_el1);
    isb();
}

void disable_fpu_exception(void)
{
    write_sysreg(CPACR_EL1_FPEN, cpacr_el1);
    isb();
}

void save_local_fpu(struct proc* p, int retain)
{
    save_fpregs((struct fpu_state*)p->seg.fpu_state);
}

void save_fpu(struct proc* p)
{
    if (get_cpulocal_var(fpu_owner) == p) {
        disable_fpu_exception();
        save_local_fpu(p, TRUE);
    }
}

static inline void fpu_init_state(struct fpu_state* state)
{
    memset(state, 0, sizeof(*state));
}

int restore_fpu(struct proc* p)
{
    if (!(p->flags & PF_FPU_INITIALIZED)) {
        fpu_init_state((struct fpu_state*)p->seg.fpu_state);
        p->flags |= PF_FPU_INITIALIZED;
    }

    load_fpregs((struct fpu_state*)p->seg.fpu_state);
    return 0;
}
