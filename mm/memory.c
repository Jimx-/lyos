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
#include "lyos/config.h"
#include "stdio.h"
#include "unistd.h"
#include "errno.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/tty.h"
#include "lyos/console.h"
#include "lyos/global.h"
#include "lyos/keyboard.h"
#include "lyos/proto.h"

#define MEMORY_DEBUG    1

/**
 * <Ring 1> Map a physical page.
 * @param  phys_addr Physical address.
 * @param  vir_addr  Virtual address.
 * @return           Zero on success.
 */
PUBLIC int map_page(struct page_directory * pgd, void * phys_addr, void * vir_addr)
{
    unsigned long pgd_index = (unsigned long)vir_addr >> 22 & 0x03FF;
    unsigned long pt_index = (unsigned long)vir_addr >> 12 & 0x03FF;

    pte_t * pt = pgd->vir_pts[pgd_index];

    /* page table not present */
    if (pt == NULL) {
        pt = (pte_t *)alloc_vmem(PT_SIZE);
        if (pt == NULL) {
            printl("MM: map_page: failed to allocate memory for new page table\n");
            return ENOMEM;
        }

#if MEMORY_DEBUG
        printl("MM: map_page: allocated new page table\n");
#endif

        pgd->vir_addr[pgd_index] = (int)va2pa(getpid(), pt) | PG_PRESENT | PG_RW | PG_USER;
        pgd->vir_pts[pgd_index] = pt;
    }

    pt[pt_index] = ((int)phys_addr & 0xFFFFF000) | PG_PRESENT | PG_RW | PG_USER;

    return 0;
}

PUBLIC int map_memory(struct page_directory * pgd, void * phys_addr, void * vir_addr, int length)
{
    while (1) {
        map_page(pgd, phys_addr, vir_addr);

        length -= PG_SIZE;
        phys_addr += PG_SIZE;
        vir_addr += PG_SIZE;
        if (length <= 0) break;
    }

    return 0;
}

void wp_page(void){

}

void no_page(void){
    int i;
    for (i = 0; i < 4; i++) {
        current->pgd.vir_addr[i] = initial_pgd[i];
    }
}

