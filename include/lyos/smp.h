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

#ifndef _SMP_H_
#define _SMP_H_

#include <lyos/cpulocals.h>

#define CPU_IS_READY 1

struct cpu {
    u32 flags;
};

PUBLIC void set_cpu_flag(int cpu, u32 flags);
PUBLIC void clear_cpu_flag(int cpu, u32 flag);
PUBLIC int test_cpu_flag(int cpu, u32 flag);

PUBLIC void wait_for_aps_to_finish_booting();
PUBLIC void ap_finished_booting();

#endif
