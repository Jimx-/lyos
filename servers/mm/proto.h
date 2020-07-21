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
#include "pagetable.h"

int mm_verify_endpt(endpoint_t ep, int* proc_nr);
struct mmproc* endpt_mmproc(endpoint_t ep);

void mem_init(phys_bytes mem_start, phys_bytes free_mem_size);
phys_bytes alloc_mem(phys_bytes memsize);
int free_mem(phys_bytes base, phys_bytes len);
phys_bytes alloc_pages(int nr_pages, int memflags);

void vmem_init(void* mem_start, size_t free_mem_size);
void* alloc_vmem(phys_bytes* phys_addr, int memsize, int reason);
void* alloc_vmpages(int nr_pages);
void free_vmem(void* base, int len);
void free_vmpages(void* base, int nr_pages);

void slabs_init();
void* slaballoc(int bytes);
void slabfree(void* mem, int bytes);
#define SLABALLOC(p)               \
    do {                           \
        p = slaballoc(sizeof(*p)); \
    } while (0)
#define SLABFREE(p)              \
    do {                         \
        slabfree(p, sizeof(*p)); \
        p = NULL;                \
    } while (0)

void pt_init();
pmd_t* pmd_create(pde_t* pde, vir_bytes addr);
int pt_create(pmd_t* pmde);
pte_t* pt_create_map(pmd_t* pmde, vir_bytes addr);
int pt_mappage(pgdir_t* pgd, phys_bytes phys_addr, vir_bytes vir_addr,
               unsigned int flags);
int pt_wppage(pgdir_t* pgd, vir_bytes vir_addr);
int pt_unwppage(pgdir_t* pgd, vir_bytes vir_addr);
int pt_writemap(pgdir_t* pgd, phys_bytes phys_addr, vir_bytes vir_addr,
                size_t length, int flags);
int pt_wp_memory(pgdir_t* pgd, vir_bytes vir_addr, size_t length);
int pt_unwp_memory(pgdir_t* pgd, vir_bytes vir_addr, size_t length);
void pt_kern_mapping_init();
int unmap_memory(pgdir_t* pgd, vir_bytes vir_addr, size_t length);
int pgd_new(pgdir_t* pgd);
int pgd_mapkernel(pgdir_t* pgd);
int pgd_bind(struct mmproc* who, pgdir_t* pgd);
int pgd_clear(pgdir_t* pgd);
void pgd_free_range(pgdir_t* pgd, vir_bytes addr, vir_bytes end,
                    vir_bytes floor, vir_bytes ceiling);
int pgd_free(pgdir_t* pgd);
int pgd_va2pa(pgdir_t* pgd, vir_bytes vir_addr, phys_bytes* phys_addr);

struct mm_struct* mm_allocate();
void mm_init(struct mm_struct* mm);
void mm_free(struct mm_struct* mm);

int phys_region_init(struct phys_region* rp, int capacity);
struct vir_region* region_new(vir_bytes vir_base, size_t vir_length, int flags);
int region_alloc_phys(struct vir_region* rp);
int region_map_phys(struct mmproc* mmp, struct vir_region* rp);
int region_set_phys(struct vir_region* rp, phys_bytes phys_addr);
int region_unmap_phys(struct mmproc* mmp, struct vir_region* rp);
struct vir_region* region_find_free_region(struct mmproc* mmp, vir_bytes minv,
                                           vir_bytes maxv, size_t len,
                                           int flags);
int region_extend_up_to(struct mmproc* mmp, vir_bytes addr);
int region_extend(struct vir_region* rp, int increment);
int region_extend_stack(struct vir_region* rp, int increment);
int region_share(struct mmproc* p_dest, struct vir_region* dest,
                 struct mmproc* p_src, struct vir_region* src, int writable);
struct vir_region* region_lookup(struct mmproc* mmp, vir_bytes addr);
int region_handle_memory(struct mmproc* mmp, struct vir_region* vr,
                         off_t offset, size_t sublen, int wrflag);
int region_handle_pf(struct mmproc* mmp, struct vir_region* vr, off_t offset,
                     int wrflag);
int region_unmap_range(struct mmproc* mmp, vir_bytes start, size_t len);
int region_free(struct vir_region* rp);

void region_init_avl(struct mm_struct* mm);
void region_avl_start_iter(struct avl_root* root, struct avl_iter* iter,
                           void* key, int flags);
struct vir_region* region_avl_get_iter(struct avl_iter* iter);
void region_avl_inc_iter(struct avl_iter* iter);
void region_avl_dec_iter(struct avl_iter* iter);

int proc_free(struct mmproc* p, int clear_proc);

/* mm/forkexit.c */
int do_fork();

/* mm/brk.c */
int do_brk();

void do_handle_fault();
void do_mmrequest();

int do_procctl();

int do_mm_getinfo();

struct vir_region* mmap_region(struct mmproc* mmp, void* addr, int mmap_flags,
                               size_t len, int vrflags);
int do_mmap();
int do_munmap();
int do_vfs_mmap();
int do_map_phys();
int do_mm_remap();

typedef void (*vfs_callback_t)(struct mmproc* mmp, MESSAGE* msg, void* arg);
int enqueue_vfs_request(struct mmproc* mmp, int req_type, int fd, void* addr,
                        off_t offset, size_t len, vfs_callback_t callback,
                        void* arg, int arg_len);
int do_vfs_reply();

struct mm_file_desc* get_mm_file_desc(int fd, dev_t dev, ino_t ino);
void file_reference(struct vir_region* vr, struct mm_file_desc* filp);
void file_unreferenced(struct mm_file_desc* filp);

void page_cache_init();
int page_cache_add(dev_t dev, off_t dev_offset, ino_t ino, off_t ino_offset,
                   void* vir_addr, struct phys_frame* frame);

#endif
