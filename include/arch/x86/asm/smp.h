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

#ifndef _ARCH_SMP_H_
#define _ARCH_SMP_H_

#include <asm/cpulocals.h>

DECLARE_CPULOCAL(unsigned int, cpu_number);

#define cpuid raw_cpulocal_read(cpu_number)

/* #define cpuid                                                \ */
/*     (((u32*)(((u32)get_stack_frame() + (K_STACK_SIZE - 1)) & \ */
/*              ~(K_STACK_SIZE - 1)))[-1]) */

#endif
