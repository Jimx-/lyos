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

/* in/out functions */
#define out_long(a, b) *((volatile unsigned int*)(a)) = (b)
#define in_long(a) (*((volatile unsigned int*)(a)))
#define out_word(a, b) *((volatile unsigned int*)(a)) = ((b) | ((b) << 16))
#define in_word(a) ((*((volatile unsigned int*)(a))) & 0xffff)
#define out_byte(a, b) *((volatile unsigned char*)(a)) = (b)
#define in_byte(a) (*((volatile unsigned char*)(a)))

#define mmio_write(a, b) *((volatile unsigned int*)(a)) = (b)
#define mmio_read(a) (*((volatile unsigned int*)(a)))

PUBLIC int init_tss(unsigned cpu, unsigned kernel_stack);

PUBLIC void arch_boot_proc(struct proc* p, struct boot_proc* bp);

PUBLIC int kern_map_phys(phys_bytes phys_addr, phys_bytes len, int flags,
                         void** mapped_addr);

/* Data memory barrier */
static inline void dmb(void) { asm volatile("dmb" : : : "memory"); }

/* Data synchronization barrier */
static inline void dsb(void) { asm volatile("dsb" : : : "memory"); }

/* Instruction synchronization barrier */
static inline void isb(void) { asm volatile("isb" : : : "memory"); }

static inline void barrier(void)
{
    dsb();
    isb();
}

static inline void refresh_tlb(void)
{
    dsb();

    asm volatile("mcr p15, 0, %[zero], c8, c7, 0 @ TLBIALL\n\t"
                 :
                 : [ zero ] "r"(0));
    asm volatile("mcr p15, 0, %[zero], c7, c5, 0" : : [ zero ] "r"(0));
    asm volatile("mcr p15, 0, %[zero], c7, c5, 6" : : [ zero ] "r"(0));

    dsb();
    isb();
}

/* Read Translation Table Base Register 0 */
static inline u32 read_ttbr0()
{
    u32 bar;
    asm volatile("mrc p15, 0, %[bar], c2, c0, 0 @ Read TTBR0\n\t"
                 : [ bar ] "=r"(bar));

    return bar;
}

/* Write Translation Table Base Register 0 */
static inline void write_ttbr0(u32 bar)
{
    barrier();

    asm volatile("mcr p15, 0, %[bar], c2, c0, 0 @ Write TTBR0\n\t"
                 :
                 : [ bar ] "r"(bar));

    refresh_tlb();
}

static inline void write_vbar(u32 vbar)
{
    asm volatile("mcr p15, 0, %[vbar], c12, c0, 0 @ Write VBAR\n\t"
                 :
                 : [ vbar ] "r"(vbar));

    isb();
}

#endif
