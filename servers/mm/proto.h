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

PUBLIC int mm_verify_endpt(endpoint_t ep, int * proc_nr);
PUBLIC struct mmproc * endpt_mmproc(endpoint_t ep);

PUBLIC void mem_init(phys_bytes mem_start, phys_bytes free_mem_size);
PUBLIC phys_bytes  alloc_mem(phys_bytes memsize);
PUBLIC int  free_mem(phys_bytes base, phys_bytes len);
PUBLIC phys_bytes alloc_pages(int nr_pages, int memflags);

PUBLIC void vmem_init(void* mem_start, size_t free_mem_size);
PUBLIC void*  alloc_vmem(phys_bytes * phys_addr, int memsize, int reason);
PUBLIC void* alloc_vmpages(int nr_pages);
PUBLIC void  free_vmem(void* base, int len);
PUBLIC void free_vmpages(void* base, int nr_pages);

PUBLIC void slabs_init();
PUBLIC void * slaballoc(int bytes);
PUBLIC void slabfree(void * mem, int bytes);
#define SLABALLOC(p) do { p = slaballoc(sizeof(*p)); } while(0)
#define SLABFREE(p) do { slabfree(p, sizeof(*p)); p = NULL; } while(0)

PUBLIC void pt_init();
PUBLIC int pt_create(pgdir_t * pgd, int pde, u32 flags);
PUBLIC int pt_mappage(pgdir_t * pgd, phys_bytes phys_addr, void* vir_addr, unsigned int flags);
PUBLIC int pt_wppage(pgdir_t * pgd, void * vir_addr);
PUBLIC int pt_unwppage(pgdir_t * pgd, void * vir_addr);
PUBLIC int pt_writemap(pgdir_t * pgd, phys_bytes phys_addr, void* vir_addr, size_t length, int flags);
PUBLIC int pt_wp_memory(pgdir_t * pgd, void * vir_addr, size_t length);
PUBLIC int pt_unwp_memory(pgdir_t * pgd, void * vir_addr, size_t length);
PUBLIC void pt_kern_mapping_init();
PUBLIC int map_memory(pgdir_t * pgd, void * phys_addr, void * vir_addr, size_t length);
PUBLIC int unmap_memory(pgdir_t * pgd, void* vir_addr, size_t length);
PUBLIC int pgd_new(pgdir_t * pgd);
PUBLIC int pgd_mapkernel(pgdir_t * pgd);
PUBLIC int pgd_bind(struct mmproc * who, pgdir_t * pgd);
PUBLIC int pgd_clear(pgdir_t * pgd);
PUBLIC int pgd_free(pgdir_t * pgd);
PUBLIC phys_bytes pgd_va2pa(pgdir_t* pgd, void* vir_addr);
PUBLIC void* pgd_find_free_pages(pgdir_t * pgd, int nr_pages, void* minv, void* maxv);

PUBLIC struct mm_struct* mm_allocate();
PUBLIC void mm_init(struct mm_struct* mm);
PUBLIC void mm_free(struct mm_struct* mm);

PUBLIC int phys_region_init(struct phys_region * rp, int capacity);
PUBLIC struct vir_region * region_new(void* vir_base, size_t vir_length, int flags);
PUBLIC int region_alloc_phys(struct vir_region * rp);
PUBLIC int region_map_phys(struct mmproc * mmp, struct vir_region * rp);
PUBLIC int region_set_phys(struct vir_region * rp, phys_bytes phys_addr);
PUBLIC int region_unmap_phys(struct mmproc * mmp, struct vir_region * rp);
PUBLIC struct vir_region * region_find_free_region(struct mmproc * mmp,
                void* minv, void* maxv, size_t len, int flags);
PUBLIC int region_extend_up_to(struct mmproc * mmp, void* addr);
PUBLIC int region_extend(struct vir_region * rp, int increment);
PUBLIC int region_extend_stack(struct vir_region * rp, int increment);
PUBLIC int region_share(struct mmproc * p_dest, struct vir_region * dest,
                            struct mmproc * p_src, struct vir_region * src, int writable);
PUBLIC struct vir_region * region_lookup(struct mmproc * mmp, void* addr);
PUBLIC int region_handle_memory(struct mmproc * mmp, struct vir_region * vr,
        off_t offset, size_t sublen, int wrflag);
PUBLIC int region_handle_pf(struct mmproc * mmp, struct vir_region * vr,
        off_t offset, int wrflag);
PUBLIC int region_unmap_range(struct mmproc* mmp, void* start, size_t len);
PUBLIC int region_free(struct vir_region * rp);

PUBLIC void region_init_avl(struct mm_struct* mm);
PUBLIC void region_avl_start_iter(struct avl_root* root, struct avl_iter* iter, void* key, int flags);
PUBLIC struct vir_region* region_avl_get_iter(struct avl_iter* iter);
PUBLIC void region_avl_inc_iter(struct avl_iter* iter);
PUBLIC void region_avl_dec_iter(struct avl_iter* iter);

PUBLIC int proc_free(struct mmproc * p, int clear_proc);

/* mm/forkexit.c */
PUBLIC int  do_fork();

/* mm/brk.c */
PUBLIC int  do_brk();

PUBLIC void do_handle_fault();
PUBLIC void do_mmrequest();

PUBLIC int do_procctl();

PUBLIC int do_mm_getinfo();

PUBLIC struct vir_region * mmap_region(struct mmproc * mmp, void* addr,
    int mmap_flags, size_t len, int vrflags);
PUBLIC int do_mmap();
PUBLIC int do_munmap();
PUBLIC int do_vfs_mmap();
PUBLIC int do_map_phys();
PUBLIC int do_mm_remap();

typedef void (*vfs_callback_t) (struct mmproc* mmp, MESSAGE* msg, void* arg);
PUBLIC int enqueue_vfs_request(struct mmproc* mmp, int req_type, int fd, void* addr, off_t offset, size_t len, vfs_callback_t callback, void* arg, int arg_len);
PUBLIC int do_vfs_reply();

PUBLIC struct mm_file_desc* get_mm_file_desc(int fd, dev_t dev, ino_t ino);
PUBLIC void file_reference(struct vir_region* vr, struct mm_file_desc* filp);
PUBLIC void file_unreferenced(struct mm_file_desc* filp);

PUBLIC void page_cache_init();
PUBLIC int page_cache_add(dev_t dev, off_t dev_offset, ino_t ino, off_t ino_offset, void* vir_addr, struct phys_frame* frame);

PUBLIC void futex_init();
PUBLIC int do_futex();

#endif
