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

PUBLIC int map_memory(struct page_directory * pgd, void * phys_addr, void * vir_addr, int length);

PUBLIC struct vir_region * region_new(struct proc * mp, void * vir_base, int vir_length, int flags);
PUBLIC int region_alloc_phys(struct vir_region * rp);
PUBLIC int region_map_phys(struct proc * mp, struct vir_region * rp);

PUBLIC int proc_new(struct proc * p, void * text_vaddr, int text_memlen, void * data_vaddr, int data_memlen);

PUBLIC int do_sbrk();

#endif
