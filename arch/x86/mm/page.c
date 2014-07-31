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

#include "lyos/type.h"
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "stddef.h"
#include "protect.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/tty.h"
#include "lyos/console.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "page.h"
#include <errno.h>

/**
 * <Ring 0> Setup identity paging for kernel
 */
PUBLIC void setup_paging(unsigned int memory_size, pde_t * pgd, pte_t * pt, int kpts)
{
    pte_t * page_table_start = pt;
    /* full 4G memory */
    int nr_page_tables = 1024;

    /* identity paging */
    int nr_pages = nr_page_tables * 1024;
    int page = PG_PRESENT | PG_RW | PG_USER;

    int i;
    for (i = 0; i < nr_pages; i++, page += PG_SIZE) {
        page_table_start[i] = page;
    }

    int nr_free_page_tables = nr_page_tables - (KERNEL_VMA / PT_MEMSIZE) - kpts;
    int free_page_start = nr_free_page_tables * 1024;
    for (i = free_page_start; i < nr_pages; i++) page_table_start[i] = 0;

    /* initialize page directory */
    int pde = (int)page_table_start | PG_PRESENT | PG_RW | PG_USER;
    for (i = 0; i < nr_page_tables; i++, pde += PT_SIZE) {
        pgd[i] = pde;
    }

    /* map 0xF0000000 ~ 0xF1000000 to 0x00000000 ~ 0x01000000 */
    for (i = KERNEL_VMA / 0x400000; i < KERNEL_VMA / 0x400000 + kpts; i++) {
        pgd[i] = pgd[i - KERNEL_VMA / 0x400000];
    }

    /* switch to the new page directory */
    switch_address_space(pgd);
    /* reload it */
    reload_cr3();
}

/* <Ring 0> */
PUBLIC void switch_address_space(pde_t * pgd) {
    asm volatile ("mov %0, %%cr3":: "r"(pgd));
}

/* <Ring 0> */
PUBLIC void disable_paging()
{
    int cr0;
    asm volatile ("mov %%cr0, %0": "=r"(cr0));
    cr0 &= ~I386_CR0_PG;
    asm volatile ("mov %0, %%cr0":: "r"(cr0));
}

PRIVATE void * find_free_pages(struct page_directory * pgd, int nr_pages, void * start, void * end)
{
    if (start == NULL) start = (void *)PG_SIZE; /* starts from the second pde by default */
    if (end == NULL) end = (void *)VM_STACK_TOP;

    int start_pde = ARCH_PDE((int)start);
    int end_pde = ARCH_PDE((int)end);

    int i, j;
    int allocated_pages = 0;
    void * retaddr = NULL;
    for (i = start_pde; i < end_pde; i++) {
        /* the pde is empty, we have I386_VM_DIR_ENTRIES free pages */
        if (pgd->vir_pts[i] == NULL) {
            nr_pages -= I386_VM_DIR_ENTRIES;
            allocated_pages += I386_VM_DIR_ENTRIES;
            if (retaddr == NULL) retaddr = (void *)ARCH_VM_ADDRESS(i, 0, 0);
            if (nr_pages <= 0) {
                return retaddr;
            }
            continue;
        }

        pte_t * pt_entries = pgd->vir_pts[i];
        for (j = 0; j < I386_VM_DIR_ENTRIES; j++) {
            if (pt_entries[j] != 0) {
                nr_pages += allocated_pages;
                retaddr = NULL;
                allocated_pages = 0;
            } else {
                nr_pages--;
                allocated_pages++;
                if (!retaddr) retaddr = (void *)ARCH_VM_ADDRESS(i, j, 0);
            }

            if (nr_pages <= 0) return retaddr;
        }
    }

    return NULL;
}

PUBLIC int sys_datacopy(int _unused1, int _unused2, MESSAGE * m, struct proc * p_proc)
{
    /* back to kernel space */
    switch_address_space(initial_pgd);
    reload_cr3();

    MESSAGE msg;
    phys_copy(&msg, va2pa(proc2pid(current), m), sizeof(MESSAGE));

    void * src_addr = msg.SRC_ADDR;
    int src_seg = (int)msg.SRC_SEG;
    endpoint_t src_pid = msg.SRC_PID;

    void * dest_addr = msg.DEST_ADDR;
    int dest_seg = (int)msg.DEST_SEG;
    endpoint_t dest_pid = msg.DEST_PID;

    int len = msg.BUF_LEN;

    msg.RETVAL = vir_copy(dest_pid, dest_seg, dest_addr, src_pid, src_seg, src_addr, len);
    phys_copy(va2pa(proc2pid(current), m), &msg, sizeof(MESSAGE));

    return 0;
}

PUBLIC int vir_copy(endpoint_t dest_pid, int dest_seg, void * dest_addr,
                        endpoint_t src_pid, int src_seg, void * src_addr, int len)
{
    pte_t * old_pgd = (pte_t *)read_cr3();
    struct page_directory * kernel_pgd = &(proc_table[TASK_MM].pgd);
    pde_t * kernel_pd = (pde_t *)((int)initial_pgd + KERNEL_VMA);

    switch_address_space(initial_pgd);
    reload_cr3();

    /* map in dest address */
    u32 dest_la = (u32)va2la(dest_pid, dest_addr);
    u32 dest_offset = dest_la % PG_SIZE, dest_len = len;
    if (dest_offset != 0) {
        dest_la -= dest_offset;
        dest_len += dest_offset;
    }

    if (dest_len % PG_SIZE != 0) {
        dest_len += PG_SIZE - (dest_len % PG_SIZE);
    }

    int dest_pages = dest_len / PG_SIZE;
    u32 dest_vaddr = (u32)find_free_pages(kernel_pgd, dest_pages, (void*)((u32)KERNEL_VMA + (u32)PROCS_BASE), (void*)0xffffffff);
    if (dest_addr == NULL) return ENOMEM;

    int i;
    u32 _dest_vaddr = dest_vaddr;
    u32 _dest_la = dest_la;
    for (i = 0; i < dest_pages; i++, _dest_la += PG_SIZE, _dest_vaddr += PG_SIZE)
    {
        unsigned long pgd_index = ARCH_PDE(_dest_vaddr);
        unsigned long pt_index = ARCH_PTE(_dest_vaddr);

        pte_t * pt = (pte_t *)((u32)(kernel_pd[pgd_index] & ARCH_VM_ADDR_MASK) + KERNEL_VMA);
        pt[pt_index] = ((u32)la2pa(dest_pid, _dest_la) & ARCH_VM_ADDR_MASK) | PG_PRESENT | PG_RW | PG_USER;
    }

    /* map in source address */
    u32 src_la = (u32)va2la(src_pid, src_addr);
    u32 src_offset = src_la % PG_SIZE, src_len = len;
    if (src_offset != 0) {
        src_la -= src_offset;
        src_len += src_offset;
    }

    if (src_len % PG_SIZE != 0) {
        src_len += PG_SIZE - (src_len % PG_SIZE);
    }

    u32 src_pages = src_len / PG_SIZE;
    u32 src_vaddr = (u32)find_free_pages(kernel_pgd, src_pages, (void*)((u32)KERNEL_VMA + (u32)PROCS_BASE), (void*)0xffffffff);
    if (src_addr == NULL) return ENOMEM;

    u32 _src_vaddr = src_vaddr;
    u32 _src_la = src_la;
    for (i = 0; i < src_pages; i++, _src_la += PG_SIZE, _src_vaddr += PG_SIZE)
    {
        unsigned long pgd_index = ARCH_PDE(_src_vaddr);
        unsigned long pt_index = ARCH_PTE(_src_vaddr);

        pte_t * pt = (pte_t *)((u32)(kernel_pd[pgd_index] & ARCH_VM_ADDR_MASK) + KERNEL_VMA);
        pt[pt_index] = ((u32)la2pa(src_pid, _src_la) & ARCH_VM_ADDR_MASK) | PG_PRESENT | PG_RW | PG_USER;
    }

    void * src_pa = (void *)va2pa(src_pid, src_addr);
    void * dest_pa = (void *)va2pa(dest_pid, dest_addr);

    //phys_copy(dest_pa, src_pa, len);
    phys_copy(dest_vaddr + dest_offset, src_vaddr + src_offset, len);
    if (dest_pages > 1) {
        printl("%x -> %x(%x->%x)\n", va2pa(TASK_MM, src_vaddr + src_offset), va2pa(TASK_MM, dest_vaddr + dest_offset), src_vaddr, dest_vaddr);
        printl("%x -> %x(%x->%x)\n", src_pa, dest_pa, src_vaddr, dest_vaddr);
    }
    /* unmap */
    _dest_vaddr = dest_vaddr;
    _dest_la = dest_la;
    for (i = 0; i < dest_pages; i++, _dest_la += PG_SIZE, _dest_vaddr += PG_SIZE)
    {
        unsigned long pgd_index = ARCH_PDE(_dest_vaddr);
        unsigned long pt_index = ARCH_PTE(_dest_vaddr);

        pte_t * pt = (pte_t *)((u32)(kernel_pd[pgd_index] & ARCH_VM_ADDR_MASK) + KERNEL_VMA);
        pt[pt_index] = 0;
    }

    _src_vaddr = src_vaddr;
    _src_la = src_la;
    for (i = 0; i < src_pages; i++, _src_la += PG_SIZE, _src_vaddr += PG_SIZE)
    {
        unsigned long pgd_index = ARCH_PDE(_src_vaddr);
        unsigned long pt_index = ARCH_PTE(_src_vaddr);

        pte_t * pt = (pte_t *)((u32)(kernel_pd[pgd_index] & ARCH_VM_ADDR_MASK) + KERNEL_VMA);
        pt[pt_index] = 0;
    }

    switch_address_space(old_pgd);
    reload_cr3();

    return 0;
}
