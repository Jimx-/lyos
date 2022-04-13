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

#define cmb() __asm__ __volatile__("" ::: "memory")

#define sev() asm volatile("sev" : : : "memory")
#define wfe() asm volatile("wfe" : : : "memory")
#define wfi() asm volatile("wfi" : : : "memory")

#define isb()    asm volatile("isb" : : : "memory")
#define dmb(opt) asm volatile("dmb " #opt : : : "memory")
#define dsb(opt) asm volatile("dsb " #opt : : : "memory")

/* in/out functions */
#define out_long(a, b) *((volatile unsigned int*)(unsigned long)(a)) = (b)
#define in_long(a)     (*((volatile unsigned int*)(unsigned long)(a)))
#define out_word(a, b) \
    *((volatile unsigned int*)(unsigned long)(a)) = ((b) | ((b) << 16))
#define in_word(a)     ((*((volatile unsigned int*)(unsigned long)(a))) & 0xffff)
#define out_byte(a, b) *((volatile unsigned char*)(unsigned long)(a)) = (b)
#define in_byte(a)     (*((volatile unsigned char*)(unsigned long)(a)))

#define mmio_write(a, b) *((volatile unsigned int*)(unsigned long)(a)) = (b)
#define mmio_read(a)     (*((volatile unsigned int*)(unsigned long)(a)))

void arch_pause();

void paging_init(void);
phys_bytes pg_alloc_lowest(kinfo_t* pk, phys_bytes size);

void* fixmap_remap_fdt(phys_bytes dt_phys, int* size, pgprot_t prot);

int init_tss(unsigned cpu, void* kernel_stack);

struct proc* arch_switch_to_user(void);
void arch_boot_proc(struct proc* p, struct boot_proc* bp);

int kern_map_phys(phys_bytes phys_addr, phys_bytes len, int flags,
                  void** mapped_addr);

static inline void flush_tlb(void)
{
    dsb(nshst);
    asm("tlbi vmalle1\n" ::);
    dsb(nsh);
    isb();
}

#endif
