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

    vir_copy(dest_pid, dest_seg, dest_addr, src_pid, src_seg, src_addr, len);

    msg.RETVAL = 0;
    phys_copy(va2pa(proc2pid(current), m), &msg, sizeof(MESSAGE));

    return 0;
}

PUBLIC int vir_copy(endpoint_t dest_pid, int dest_seg, void * dest_addr,
                        endpoint_t src_pid, int src_seg, void * src_addr, int len)
{
    pte_t * old_pgd = (pte_t *)read_cr3();

    switch_address_space(initial_pgd);
    reload_cr3();

    void * src_pa = (void *)va2pa(src_pid, src_addr);
    void * dest_pa = (void *)va2pa(dest_pid, dest_addr);

    phys_copy(dest_pa, src_pa, len);

    switch_address_space(old_pgd);
    reload_cr3();

    return 0;
}
