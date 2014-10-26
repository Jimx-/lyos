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
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "page.h"
#include <errno.h>
#include "arch_proto.h"

PRIVATE void * find_free_pages(pde_t * pgd_phys, int nr_pages, void * start, void * end)
{
    /* default value */
    if (start == NULL) start = (void *)FIXMAP_START;
    if (end == NULL) end = (void *)FIXMAP_END;

    unsigned int start_pde = ARCH_PDE((unsigned int)start);
    unsigned int end_pde = ARCH_PDE((unsigned int)end);

    int i, j;
    pde_t * vir_pts = (pde_t *)((u32)pgd_phys + KERNEL_VMA);
    int allocated_pages = 0;
    void * retaddr = NULL;
    for (i = start_pde; i < end_pde; i++) {
        pte_t * pt_entries = (pte_t *)(((pde_t)vir_pts[i] + KERNEL_VMA) & ARCH_VM_ADDR_MASK);
        /* the pde is empty, we have I386_VM_DIR_ENTRIES free pages */
        if (pt_entries == NULL) {
            nr_pages -= I386_VM_DIR_ENTRIES;
            allocated_pages += I386_VM_DIR_ENTRIES;
            if (retaddr == NULL) retaddr = (void *)ARCH_VM_ADDRESS(i, 0, 0);
            if (nr_pages <= 0) {
                return retaddr;
            }
            continue;
        }

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

PUBLIC int sys_datacopy(MESSAGE * m, struct proc * p_proc)
{
    void * src_addr = m->SRC_ADDR;
    endpoint_t src_pid = m->SRC_PID == SELF ? proc2pid(p_proc) : m->SRC_PID;

    void * dest_addr = m->DEST_ADDR;
    endpoint_t dest_pid = m->DEST_PID == SELF ? proc2pid(p_proc) : m->DEST_PID;

    int len = m->BUF_LEN;

    return vir_copy(dest_pid, dest_addr, src_pid, src_addr, len);
}

PUBLIC int vir_copy(endpoint_t dest_pid, void * dest_addr,
                        endpoint_t src_pid, void * src_addr, int len)
{
    u32 old_cr3 = read_cr3();
    pde_t * initial_pgd_vir = (pde_t *)((u32)initial_pgd + KERNEL_VMA);

#define PGD_ENTRY(i) (pte_t *)(((pde_t)initial_pgd_vir[i] + KERNEL_VMA) & ARCH_VM_ADDR_MASK)

    write_cr3((u32)initial_pgd);
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
    u32 dest_vaddr = (u32)find_free_pages(initial_pgd, dest_pages, (void*)FIXMAP_START, (void*)FIXMAP_END);
    if (dest_addr == NULL) return ENOMEM;

    int i;
    u32 _dest_vaddr = dest_vaddr;
    u32 _dest_la = dest_la;
    for (i = 0; i < dest_pages; i++, _dest_la += PG_SIZE, _dest_vaddr += PG_SIZE)
    {
        unsigned long pgd_index = ARCH_PDE(_dest_vaddr);
        unsigned long pt_index = ARCH_PTE(_dest_vaddr);

        pte_t * pt = PGD_ENTRY(pgd_index);
        pt[pt_index] = ((u32)la2pa(dest_pid, (void *)_dest_la) & ARCH_VM_ADDR_MASK) | PG_PRESENT | PG_RW | PG_USER;
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
    u32 src_vaddr = (u32)find_free_pages(initial_pgd, src_pages, (void*)FIXMAP_START, (void*)FIXMAP_END);
    if (src_addr == NULL) return ENOMEM;

    u32 _src_vaddr = src_vaddr;
    u32 _src_la = src_la;
    for (i = 0; i < src_pages; i++, _src_la += PG_SIZE, _src_vaddr += PG_SIZE)
    {
        unsigned long pgd_index = ARCH_PDE(_src_vaddr);
        unsigned long pt_index = ARCH_PTE(_src_vaddr);

        pte_t * pt = PGD_ENTRY(pgd_index);
        pt[pt_index] = ((u32)la2pa(src_pid, (void *)_src_la) & ARCH_VM_ADDR_MASK) | PG_PRESENT | PG_RW | PG_USER;
    }

    //void * src_pa = (void *)va2pa(src_pid, src_addr);
    //void * dest_pa = (void *)va2pa(dest_pid, dest_addr);

    //phys_copy(dest_pa, src_pa, len);
    phys_copy((void *)(dest_vaddr + dest_offset), (void *)(src_vaddr + src_offset), len);

    /* unmap */
    _dest_vaddr = dest_vaddr;
    _dest_la = dest_la;
    for (i = 0; i < dest_pages; i++, _dest_la += PG_SIZE, _dest_vaddr += PG_SIZE)
    {
        unsigned long pgd_index = ARCH_PDE(_dest_vaddr);
        unsigned long pt_index = ARCH_PTE(_dest_vaddr);

        pte_t * pt = PGD_ENTRY(pgd_index);
        pt[pt_index] = 0;
    }

    _src_vaddr = src_vaddr;
    _src_la = src_la;
    for (i = 0; i < src_pages; i++, _src_la += PG_SIZE, _src_vaddr += PG_SIZE)
    {
        unsigned long pgd_index = ARCH_PDE(_src_vaddr);
        unsigned long pt_index = ARCH_PTE(_src_vaddr);

        pte_t * pt = PGD_ENTRY(pgd_index);
        pt[pt_index] = 0;
    }

    write_cr3(old_cr3);
    reload_cr3();

    return 0;
}