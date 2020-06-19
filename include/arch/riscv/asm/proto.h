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
#define out_long(a, b) *((volatile unsigned int*)(a)) = (b)
#define in_long(a) (*((volatile unsigned int*)(a)))
#define out_word(a, b) *((volatile unsigned int*)(a)) = ((b) | ((b) << 16))
#define in_word(a) ((*((volatile unsigned int*)(a))) & 0xffff)
#define out_byte(a, b) *((volatile unsigned char*)(a)) = (b)
#define in_byte(a) (*((volatile unsigned char*)(a)))

#define mmio_write(a, b) *((volatile unsigned int*)(a)) = (b)
#define mmio_read(a) (*((volatile unsigned int*)(a)))

int init_tss(unsigned cpu, void* kernel_stack);

void arch_boot_proc(struct proc* p, struct boot_proc* bp);

int kern_map_phys(phys_bytes phys_addr, phys_bytes len, int flags,
                  void** mapped_addr);

static inline void enable_user_access()
{
    __asm__ __volatile__("csrs sstatus, %0" : : "r"(SR_SUM) : "memory");
}

static inline void disable_user_access()
{
    __asm__ __volatile__("csrc sstatus, %0" : : "r"(SR_SUM) : "memory");
}

static inline void flush_tlb()
{
    __asm__ __volatile__("sfence.vma" : : : "memory");
}

static inline phys_bytes read_ptbr()
{
    reg_t ptbr = csr_read(sptbr);
    return (phys_bytes)((ptbr & SATP_PPN) << ARCH_PG_SHIFT);
}

static inline void write_ptbr(phys_bytes ptbr)
{
    flush_tlb();
    csr_write(sptbr, (ptbr >> ARCH_PG_SHIFT) | SATP_MODE);
}

#endif
