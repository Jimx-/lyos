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

#define ARM64_STACK_TOP_RESERVED (2 * sizeof(reg_t))
#define K_STACK_SIZE             ARCH_PG_SIZE

#ifndef __ASSEMBLY__

extern void* k_stacks;
extern char k_stacks_start, k_stacks_end;
#define get_k_stack_top(cpu) \
    ((void*)(((char*)(k_stacks)) + 2 * ((cpu) + 1) * K_STACK_SIZE))

#endif

#define PSR_F_BIT 0x00000040 /* >= V4, but not V7M */
#define PSR_I_BIT 0x00000080 /* >= V4, but not V7M */
#define PSR_A_BIT 0x00000100 /* >= V6, but not V7M */
#define PSR_E_BIT 0x00000200 /* >= V6, but not V7M */
#define PSR_J_BIT 0x01000000 /* >= V5J, but not V7M */
#define PSR_Q_BIT 0x08000000 /* >= V5E, including V7M */
#define PSR_V_BIT 0x10000000
#define PSR_C_BIT 0x20000000
#define PSR_Z_BIT 0x40000000
#define PSR_N_BIT 0x80000000

#endif
