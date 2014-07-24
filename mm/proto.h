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

#ifndef _MM_PROTO_H_
#define _MM_PROTO_H_

PUBLIC int pt_create(struct page_directory * pgd, int pde, u32 flags);
PUBLIC int pt_mappage(struct page_directory * pgd, void * phys_addr, void * vir_addr, u32 flags);
PUBLIC int map_memory(struct page_directory * pgd, void * phys_addr, void * vir_addr, int length);
PUBLIC int unmap_memory(struct page_directory * pgd, void * vir_addr, int length);
PUBLIC int pgd_new(struct page_directory * pgd);
PUBLIC int pgd_mapkernel(struct page_directory * pgd);
PUBLIC int pgd_clear(struct page_directory * pgd);

PUBLIC struct vir_region * region_new(struct proc * mp, void * vir_base, int vir_length, int flags);
PUBLIC int region_alloc_phys(struct vir_region * rp);
PUBLIC int region_map_phys(struct proc * mp, struct vir_region * rp);
PUBLIC int region_unmap_phys(struct proc * mp, struct vir_region * rp);
PUBLIC int region_extend(struct vir_region * rp, int increment);
PUBLIC int region_extend_stack(struct vir_region * rp, int increment);
PUBLIC int region_free(struct vir_region * rp);

PUBLIC int proc_free(struct proc * p);

PUBLIC int do_sbrk();

PUBLIC int do_service_up();

PUBLIC void do_handle_fault();

PUBLIC int send_sig(int sig, struct proc * p);

PUBLIC int do_getsetid();
PUBLIC int do_procctl();

PUBLIC int do_mmap();

#endif
