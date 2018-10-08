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

#ifndef _ARCH_CONST_H_
#define _ARCH_CONST_H_

#define RISCV_STACK_TOP_RESERVED    (2 * sizeof(reg_t))
#define K_STACK_SIZE                ARCH_PG_SIZE

#ifndef __ASSEMBLY__

extern void* k_stacks;
extern char k_stacks_start, k_stacks_end;
#define get_k_stack_top(cpu)    ((void *)(((char*)(k_stacks)) \
                    + 2 * ((cpu) + 1) * K_STACK_SIZE))

#endif

#endif
