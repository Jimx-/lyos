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

#ifndef _MM_GLOBAL_H_
#define _MM_GLOBAL_H_

#include <lyos/param.h>
#include <lyos/vm.h>
#include "mmproc.h"

/* EXTERN is extern except for global.c */
#ifdef _MM_GLOBAL_VARIABLE_HERE_
#undef EXTERN
#define EXTERN
#endif

extern struct mmproc mmproc_table[];

#define STATIC_BOOTSTRAP_PAGES 25
EXTERN struct {
    int used;
    phys_bytes phys_addr;
    void* vir_addr;
} bootstrap_pages[STATIC_BOOTSTRAP_PAGES];

extern int pt_init_done;

EXTERN kinfo_t kernel_info;

EXTERN unsigned int memory_size;
EXTERN int mem_start;

EXTERN MESSAGE mm_msg;

EXTERN struct mem_info mem_info;

extern const struct region_operations anon_map_ops;
extern const struct region_operations anon_contig_map_ops;
extern const struct region_operations file_map_ops;
extern const struct region_operations direct_phys_map_ops;
extern const struct region_operations shared_map_ops;

#endif
