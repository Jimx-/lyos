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

#ifndef _ARCH_PROTO_H_
#define _ARCH_PROTO_H_

#include <asm/csr.h>

/* in/out functions */
#define out_long(a, b) *((volatile unsigned int *)(a)) = (b)
#define in_long(a) (*((volatile unsigned int *)(a)))
#define out_word(a, b) *((volatile unsigned int *)(a)) = ((b) | ((b) << 16))
#define in_word(a) ((*((volatile unsigned int *)(a))) & 0xffff)
#define out_byte(a, b) *((volatile unsigned char *)(a)) = (b)
#define in_byte(a) (*((volatile unsigned char *)(a)))

#define mmio_write(a, b) *((volatile unsigned int *)(a)) = (b)
#define mmio_read(a) (*((volatile unsigned int *)(a)))

PUBLIC int init_tss(unsigned cpu, unsigned kernel_stack);

PUBLIC void arch_boot_proc(struct proc * p, struct boot_proc * bp);

PUBLIC int kern_map_phys(phys_bytes phys_addr, phys_bytes len, int flags, void** mapped_addr);

PRIVATE inline void enable_user_access()
{
    __asm__ __volatile__ ("csrs sstatus, %0" : : "r" (SR_SUM) : "memory");
}

PRIVATE inline void disable_user_access()
{
    __asm__ __volatile__ ("csrc sstatus, %0" : : "r" (SR_SUM) : "memory");
}

#endif
