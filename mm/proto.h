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

#include "mmproc.h"
    
PUBLIC void mem_init(int mem_start, int free_mem_size);
PUBLIC void vmem_init(int mem_start, int free_mem_size);
/* PUBLIC int   alloc_mem(int pid, int memsize); */
PUBLIC int  alloc_mem(int memsize);
PUBLIC int  free_mem(int base, int len);
PUBLIC int alloc_pages(int nr_pages);

PUBLIC int  alloc_vmem(int memsize);
PUBLIC int alloc_vmpages(int nr_pages);
PUBLIC int  free_vmem(int base, int len);

PUBLIC void pt_init();
PUBLIC int pt_create(pgdir_t * pgd, int pde, u32 flags);
PUBLIC int pt_mappage(pgdir_t * pgd, void * phys_addr, void * vir_addr, u32 flags);
PUBLIC int pt_wppage(pgdir_t * pgd, void * vir_addr);
PUBLIC int pt_unwppage(pgdir_t * pgd, void * vir_addr);
PUBLIC int pt_writemap(pgdir_t * pgd, void * phys_addr, void * vir_addr, int length, int flags);
PUBLIC int pt_wp_memory(pgdir_t * pgd, void * vir_addr, int length);
PUBLIC int pt_unwp_memory(pgdir_t * pgd, void * vir_addr, int length);
PUBLIC void pt_kern_mapping_init();
PUBLIC int map_memory(pgdir_t * pgd, void * phys_addr, void * vir_addr, int length);
PUBLIC int unmap_memory(pgdir_t * pgd, void * vir_addr, int length);
PUBLIC int pgd_new(pgdir_t * pgd);
PUBLIC int pgd_mapkernel(pgdir_t * pgd);
PUBLIC int pgd_clear(pgdir_t * pgd);

PUBLIC int phys_region_init(struct phys_region * rp, int capacity);
PUBLIC struct vir_region * region_new(struct proc * mp, void * vir_base, int vir_length, int flags);
PUBLIC int region_alloc_phys(struct vir_region * rp);
PUBLIC int region_map_phys(struct proc * mp, struct vir_region * rp);
PUBLIC int region_unmap_phys(struct proc * mp, struct vir_region * rp);
PUBLIC int region_extend(struct vir_region * rp, int increment);
PUBLIC int region_extend_stack(struct vir_region * rp, int increment);
PUBLIC int region_share(struct vir_region * dest, struct vir_region * src);
PUBLIC int region_free(struct vir_region * rp);

PUBLIC int proc_free(struct proc * p);

PUBLIC int do_sbrk();
/* mm/forkexit.c */
PUBLIC int  do_fork();
PUBLIC void do_exit(int status);
PUBLIC void do_wait();
PUBLIC int  do_kill();

/* signal.h */
PUBLIC int  do_sigaction();
PUBLIC int  do_raise();
PUBLIC int  do_alarm();

PUBLIC void do_handle_fault();

PUBLIC  int kill_sig(pid_t source, pid_t dest, int signo);
PUBLIC int send_sig(struct proc * p, int signo);

PUBLIC int do_getsetid();
PUBLIC int do_procctl();

PUBLIC int do_mmap();

#endif
