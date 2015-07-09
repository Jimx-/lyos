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

/**
 * Like alloc.c, however, vmalloc.c provides functions that allocate virtual memory in kernel
 * address space.
 *
 * alloc_vmem/free_vmem: allocate/free both physical and virtual memory.
 * alloc_vmpages/free_vmpages: allocate/free only virtual memory.
 */

#include "lyos/type.h"
#include "sys/types.h"
#include "lyos/config.h"
#include "stdio.h"
#include "unistd.h"
#include "stddef.h"
#include "errno.h"
#include "assert.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "page.h"
#include "region.h"
#include "proto.h"
#include "global.h"

PRIVATE struct hole hole[NR_HOLES]; /* the hole table */
PRIVATE struct hole *hole_head;	/* pointer to first hole */
PRIVATE struct hole *free_slots;/* ptr to list of unused table slots */

PRIVATE void delete_slot(struct hole *prev_ptr, struct hole *hp);
PRIVATE void merge_hole(struct hole * hp);

PUBLIC void vmem_init(int mem_start, int free_mem_size)
{
	struct hole *hp;

  	/* Put all holes on the free list. */
  	for (hp = &hole[0]; hp < &hole[NR_HOLES]; hp++) {
		hp->h_next = hp + 1;
		hp->h_base = hp->h_len = 0;
  	}
  	hole[NR_HOLES-1].h_next = NULL;
  	hole_head = NULL;
  	free_slots = &hole[0];

	/* Free memory */
	free_vmem(mem_start, free_mem_size);
}

/*****************************************************************************
 *                                alloc_vmem
 *****************************************************************************/
/**
 * Allocate a memory block for a proc.
 *
 * @param pid  Which proc the memory is for.
 * @param memsize  How many bytes is needed.
 *
 * @return  The base of the memory just allocated.
 *****************************************************************************/
PUBLIC int alloc_vmem(phys_bytes * phys_addr, int memsize)
{
	int pages = memsize / PG_SIZE;
	if (memsize % PG_SIZE != 0)
		pages++;

	/* using bootstrap pages */
	if (!pt_init_done) {
		int i, j;
		for (i = 0; i < STATIC_BOOTSTRAP_PAGES; i++) {
			if (!bootstrap_pages[i].used) break;
		}

		if (i + pages > STATIC_BOOTSTRAP_PAGES) return 0;

		*phys_addr = bootstrap_pages[i].phys_addr;
		int ret = bootstrap_pages[i].vir_addr;
		for (j = i; j < i + pages; j++) {
			bootstrap_pages[j].used = 1;
		}

		return ret;
	}

	/* allocate physical memory */
    int phys_pages = alloc_pages(pages);
 	int vir_pages = alloc_vmpages(pages);
 	int retval = vir_pages;

 	if (phys_addr != NULL) *phys_addr = (phys_bytes)phys_pages;

 	pt_writemap(&mmproc_table[TASK_MM].mm->pgd, (void *)phys_pages, (void *)vir_pages, pages * ARCH_PG_SIZE, PG_PRESENT | PG_RW | PG_USER);
 	
 	return retval;
}

/**
 * Allocate virtual memory pages.
 * @param  nr_pages How many pages are needed.
 * @return          Ptr to the memory.
 */
PUBLIC int alloc_vmpages(int nr_pages)
{
	int memsize = nr_pages * PG_SIZE;
 	struct hole *hp, *prev_ptr;
	int old_base;

    prev_ptr = NULL;
	hp = hole_head;
	
	while (hp != NULL) {
		int alignment = 0;
		if (hp->h_base % PAGE_ALIGN != 0)
			alignment = PAGE_ALIGN - (hp->h_base % PAGE_ALIGN);
		if (hp->h_len >= memsize + alignment) {
			/* We found a hole that is big enough.  Use it. */
			old_base = hp->h_base + alignment;
			hp->h_base += memsize + alignment;
			hp->h_len -= memsize - alignment;
			if (prev_ptr && prev_ptr->h_base + prev_ptr->h_len == old_base)
				prev_ptr->h_len += alignment;

			/* Delete the hole if used up completely. */
			if (hp->h_len == 0) delete_slot(prev_ptr, hp);

			/* Return the start address of the acquired block. */
			return(old_base);
		}

		prev_ptr = hp;
		hp = hp->h_next;
	}
	printl("MM: alloc_vmpages() failed.(Out of virtual memory space)\n");
  	return(-ENOMEM);
}

/*****************************************************************************
 *                                free_vmem
 *****************************************************************************/
/**
 * Free a memory block.
 *
 * @param base	Base address of block to free.
 * @param len	Number of bytes to free.
 *
 * @return  Zero if success.
 *****************************************************************************/
PUBLIC int free_vmem(int base, int len)
{
	struct hole *hp, *new_ptr, *prev_ptr;

	if (len == 0) return EINVAL;
  	if ((new_ptr = free_slots) == NULL)
  		panic("hole table full");
	new_ptr->h_base = base;
	new_ptr->h_len = len;
 	free_slots = new_ptr->h_next;
	hp = hole_head;

	/* Insert the slot to a proper place */
	if (hp == NULL || base <= hp->h_base) {
	/* If there's no hole or the block's address is less than the lowest hole,
	   put it on the front of the list */
		new_ptr->h_next = hp;
		hole_head = new_ptr;
		merge_hole(new_ptr);
		return 0;
 	}

	/* Find where it should go */
	prev_ptr = NULL;
	while (hp != NULL && base > hp->h_base) {
		prev_ptr = hp;
		hp = hp->h_next;
  	}

  	new_ptr->h_next = prev_ptr->h_next;
  	prev_ptr->h_next = new_ptr;
	merge_hole(prev_ptr);
	return 0;
}

/*******************************************************************
 *			delete_hole
 *******************************************************************/
/**
 * Remove an entry from the list.
 *
 *******************************************************************/
PRIVATE void delete_slot(struct hole *prev_ptr, struct hole *hp)
{
	if (hp == hole_head)
		hole_head = hp->h_next;
	else
		prev_ptr->h_next = hp->h_next;

  	hp->h_next = free_slots;
  	hp->h_base = hp->h_len = 0;
  	free_slots = hp;
}

/*******************************************************************
 *			merge_hole
 *******************************************************************/
/**
 * Merge contiguous holes.
 *
 *******************************************************************/
PRIVATE void merge_hole(struct hole * hp)
{
	struct hole *next_ptr;

  	if ((next_ptr = hp->h_next) == NULL) return; /* last hole */
  	if (hp->h_base + hp->h_len == next_ptr->h_base) {
		hp->h_len += next_ptr->h_len;	/* first one gets second one's mem */
		delete_slot(hp, next_ptr);
  	} else {
		hp = next_ptr;
  	}

  	if ((next_ptr = hp->h_next) == NULL) return;	/* hp is the last hole now */
  	if (hp->h_base + hp->h_len == next_ptr->h_base) {
		hp->h_len += next_ptr->h_len;
		delete_slot(hp, next_ptr);
  	}
}
