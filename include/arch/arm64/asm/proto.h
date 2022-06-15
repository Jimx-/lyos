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

#include <asm/barrier.h>

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
void pg_map(phys_bytes phys_addr, void* vir_addr, void* vir_end, kinfo_t* pk);
phys_bytes pg_alloc_lowest(kinfo_t* pk, phys_bytes size);

void* fixmap_remap_fdt(phys_bytes dt_phys, int* size, pgprot_t prot);

int init_tss(unsigned cpu, void* kernel_stack);

struct proc* arch_switch_to_user(void);
int arch_get_kern_mapping(int index, caddr_t* addr, int* len, int* flags);
int arch_reply_kern_mapping(int index, void* vir_addr);
int arch_vmctl(MESSAGE* m, struct proc* p);
int arch_init_proc(struct proc* p, void* sp, void* ip, struct ps_strings* ps,
                   char* name);
int arch_fork_proc(struct proc* p, struct proc* parent, int flags, void* newsp,
                   void* tls);
int arch_reset_proc(struct proc* p);
void arch_boot_proc(struct proc* p, struct boot_proc* bp);
void arch_set_syscall_result(struct proc* p, int result);

void clear_memcache();

void arch_setup_cpulocals(void);

int kern_map_phys(phys_bytes phys_addr, phys_bytes len, int flags,
                  void** mapped_addr, void (*callback)(void*), void* arg);

static inline void flush_tlb(void)
{
    dsb(nshst);
    asm("tlbi vmalle1\n" ::);
    dsb(nsh);
    isb();
}

#endif
